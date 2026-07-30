#include "minisnn_worlds_kernel.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define MINISNN_WORLDS_KERNEL_CONFIG_MINIMUM_READABLE_SIZE \
    (offsetof(MiniSNNWorldsKernelConfig, format_version) + \
     sizeof(((MiniSNNWorldsKernelConfig *)0)->format_version))

#define MINISNN_WORLDS_KERNEL_CONFIG_MASTER_SEED_READABLE_SIZE \
    (offsetof(MiniSNNWorldsKernelConfig, master_seed) + \
     sizeof(((MiniSNNWorldsKernelConfig *)0)->master_seed))

#define MINISNN_WORLDS_KERNEL_PCG32_MULTIPLIER UINT64_C(6364136223846793005)
#define MINISNN_WORLDS_KERNEL_FNV1A_OFFSET UINT64_C(14695981039346656037)
#define MINISNN_WORLDS_KERNEL_FNV1A_PRIME UINT64_C(1099511628211)

typedef struct
{
    MiniSNNWorldsKernelEntityId entity_id;
    MiniSNNWorldsTick creation_tick;
    MiniSNNWorldsTick destruction_tick;
    int alive;
} EntityRecord;

typedef struct
{
    MiniSNNWorldsKernelRandomStreamKey key;
    uint64_t state;
    uint64_t sequence;
    uint64_t generated_u32_count;
} RandomStreamRecord;

typedef struct
{
    MiniSNNWorldsKernelCommandInfo *ordered_commands;
    size_t command_count;
    EntityRecord *planned_entities;
    size_t planned_entity_capacity;
    size_t planned_entity_count;
    size_t planned_alive_entity_count;
    MiniSNNWorldsKernelEvent *next_events;
    MiniSNNWorldsKernelEntityId next_entity_id;
    MiniSNNWorldsKernelEventId next_event_id;
    uint64_t applied_count;
    uint64_t rejected_count;
    uint64_t created_count;
    uint64_t destroyed_count;
} StepPlan;

struct MiniSNNWorldsKernel
{
    MiniSNNWorldsTick tick;
    MiniSNNWorldsKernelState state;
    MiniSNNWorldsKernelError last_error;
    EntityRecord *entities;
    size_t entity_count;
    size_t entity_capacity;
    size_t alive_entity_count;
    MiniSNNWorldsKernelCommandInfo *pending_commands;
    size_t pending_command_count;
    size_t pending_command_capacity;
    MiniSNNWorldsKernelEvent *last_tick_events;
    size_t last_tick_event_count;
    MiniSNNWorldsKernelEntityId next_entity_id;
    MiniSNNWorldsKernelCommandId next_command_id;
    MiniSNNWorldsKernelEventId next_event_id;
    uint64_t total_entities_created;
    uint64_t total_entities_destroyed;
    uint64_t total_commands_submitted;
    uint64_t total_commands_applied;
    uint64_t total_commands_rejected;
    uint64_t total_events_emitted;
    uint64_t master_seed;
    RandomStreamRecord *random_streams;
    size_t random_stream_count;
    size_t random_stream_capacity;
    uint64_t total_random_u32_generated;
};

#ifdef MINISNN_WORLDS_KERNEL_TESTING
static size_t testing_allocation_fail_after = SIZE_MAX;
#endif

static void assign_error(
    MiniSNNWorldsKernelError *out_error,
    MiniSNNWorldsKernelError error)
{
    if (out_error != NULL)
    {
        *out_error = error;
    }
}

static void set_last_error(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelError error)
{
    if (kernel != NULL)
    {
        kernel->last_error = error;
    }
}

static int config_is_valid(const MiniSNNWorldsKernelConfig *config)
{
    const unsigned char *bytes;
    uint32_t struct_size;
    uint32_t format_version;

    if (config == NULL)
    {
        return 0;
    }

    bytes = (const unsigned char *)config;
    memcpy(&struct_size, bytes + offsetof(MiniSNNWorldsKernelConfig, struct_size),
           sizeof(struct_size));
    if (struct_size < (uint32_t)MINISNN_WORLDS_KERNEL_CONFIG_MINIMUM_READABLE_SIZE)
    {
        return 0;
    }

    memcpy(&format_version,
           bytes + offsetof(MiniSNNWorldsKernelConfig, format_version),
           sizeof(format_version));
    return format_version == MINISNN_WORLDS_KERNEL_CONFIG_VERSION;
}

static uint64_t config_master_seed(const MiniSNNWorldsKernelConfig *config)
{
    const unsigned char *bytes = (const unsigned char *)config;
    uint32_t struct_size;
    uint64_t master_seed;

    memcpy(&struct_size, bytes + offsetof(MiniSNNWorldsKernelConfig, struct_size),
           sizeof(struct_size));
    if (struct_size < (uint32_t)MINISNN_WORLDS_KERNEL_CONFIG_MASTER_SEED_READABLE_SIZE)
    {
        return MINISNN_WORLDS_KERNEL_DEFAULT_MASTER_SEED;
    }
    memcpy(&master_seed, bytes + offsetof(MiniSNNWorldsKernelConfig, master_seed),
           sizeof(master_seed));
    return master_seed;
}

static void *kernel_allocate(size_t size)
{
#ifdef MINISNN_WORLDS_KERNEL_TESTING
    if (testing_allocation_fail_after != SIZE_MAX)
    {
        if (testing_allocation_fail_after == 0U)
        {
            testing_allocation_fail_after = SIZE_MAX;
            return NULL;
        }
        --testing_allocation_fail_after;
    }
#endif
    return malloc(size);
}

static int next_capacity(
    size_t current_capacity,
    size_t required_capacity,
    size_t element_size,
    size_t *out_capacity)
{
    size_t capacity;

    if (out_capacity == NULL || element_size == 0U)
    {
        return 0;
    }
    capacity = current_capacity == 0U ? 4U : current_capacity;
    while (capacity < required_capacity)
    {
        if (capacity > SIZE_MAX / 2U)
        {
            capacity = required_capacity;
            break;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / element_size)
    {
        return 0;
    }
    *out_capacity = capacity;
    return 1;
}

static void *allocate_expanded_copy(
    const void *existing,
    size_t existing_count,
    size_t capacity,
    size_t element_size)
{
    void *copy;

    if (capacity == 0U || element_size == 0U || capacity > SIZE_MAX / element_size)
    {
        return NULL;
    }
    copy = kernel_allocate(capacity * element_size);
    if (copy != NULL && existing_count != 0U)
    {
        memcpy(copy, existing, existing_count * element_size);
    }
    return copy;
}

static int random_stream_key_is_valid(MiniSNNWorldsKernelRandomStreamKey key)
{
    return key.namespace_id != 0U || key.stream_id != 0U;
}

static int random_stream_compare(
    MiniSNNWorldsKernelRandomStreamKey left,
    MiniSNNWorldsKernelRandomStreamKey right)
{
    if (left.namespace_id < right.namespace_id)
    {
        return -1;
    }
    if (left.namespace_id > right.namespace_id)
    {
        return 1;
    }
    if (left.stream_id < right.stream_id)
    {
        return -1;
    }
    if (left.stream_id > right.stream_id)
    {
        return 1;
    }
    return 0;
}

static size_t random_stream_insert_index(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelRandomStreamKey key,
    int *out_exists)
{
    size_t index;

    for (index = 0U; index < kernel->random_stream_count; ++index)
    {
        int comparison = random_stream_compare(kernel->random_streams[index].key, key);

        if (comparison >= 0)
        {
            *out_exists = comparison == 0;
            return index;
        }
    }
    *out_exists = 0;
    return kernel->random_stream_count;
}

static uint64_t splitmix64_permute(uint64_t value)
{
    value += UINT64_C(0x9E3779B97F4A7C15);
    value = (value ^ (value >> 30U)) * UINT64_C(0xBF58476D1CE4E5B9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94D049BB133111EB);
    return value ^ (value >> 31U);
}

static RandomStreamRecord random_stream_initial_record(
    uint64_t master_seed,
    MiniSNNWorldsKernelRandomStreamKey key)
{
    RandomStreamRecord record;
    uint64_t state_input = master_seed ^ UINT64_C(0xA0761D6478BD642F);
    uint64_t sequence_input = master_seed ^ UINT64_C(0xE7037ED1A0B428DB);

    state_input ^= splitmix64_permute(key.namespace_id);
    state_input ^= splitmix64_permute(key.stream_id ^ UINT64_C(0x8EBC6AF09C88C6E3));
    sequence_input ^= splitmix64_permute(key.stream_id);
    sequence_input ^= splitmix64_permute(key.namespace_id ^ UINT64_C(0x589965CC75374CC3));
    record.key = key;
    record.state = splitmix64_permute(state_input);
    record.sequence = splitmix64_permute(sequence_input) | UINT64_C(1);
    record.generated_u32_count = 0U;
    return record;
}

static uint32_t pcg32_next(RandomStreamRecord *record)
{
    uint64_t old_state = record->state;
    uint32_t xorshifted;
    uint32_t rotation;

    record->state = old_state * MINISNN_WORLDS_KERNEL_PCG32_MULTIPLIER +
                    record->sequence;
    xorshifted = (uint32_t)(((old_state >> 18U) ^ old_state) >> 27U);
    rotation = (uint32_t)(old_state >> 59U);
    return (xorshifted >> rotation) | (xorshifted << ((UINT32_C(0) - rotation) & 31U));
}

static int random_draw_raw(
    RandomStreamRecord *record,
    uint64_t *in_out_total_generated,
    uint32_t *out_value)
{
    if (record->generated_u32_count == UINT64_MAX ||
        *in_out_total_generated == UINT64_MAX)
    {
        return 0;
    }
    *out_value = pcg32_next(record);
    ++record->generated_u32_count;
    ++*in_out_total_generated;
    return 1;
}

static MiniSNNWorldsKernelError random_prepare_candidate(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelRandomStreamKey key,
    size_t *out_index,
    int *out_is_new,
    RandomStreamRecord *out_candidate,
    RandomStreamRecord **out_expanded_streams,
    size_t *out_expanded_capacity)
{
    int exists;
    size_t index;
    size_t capacity;

    index = random_stream_insert_index(kernel, key, &exists);
    *out_index = index;
    *out_is_new = exists == 0;
    *out_expanded_streams = NULL;
    *out_expanded_capacity = kernel->random_stream_capacity;
    if (exists != 0)
    {
        *out_candidate = kernel->random_streams[index];
        return MINISNN_WORLDS_KERNEL_ERROR_NONE;
    }
    if (kernel->random_stream_count == SIZE_MAX ||
        !next_capacity(kernel->random_stream_capacity, kernel->random_stream_count + 1U,
                       sizeof(**out_expanded_streams), &capacity))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    *out_expanded_streams = allocate_expanded_copy(
        kernel->random_streams, kernel->random_stream_count, capacity,
        sizeof(**out_expanded_streams));
    if (*out_expanded_streams == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    *out_expanded_capacity = capacity;
    *out_candidate = random_stream_initial_record(kernel->master_seed, key);
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static void random_commit_candidate(
    MiniSNNWorldsKernel *kernel,
    size_t index,
    int is_new,
    const RandomStreamRecord *candidate,
    RandomStreamRecord *expanded_streams,
    size_t expanded_capacity,
    uint64_t total_generated)
{
    if (is_new != 0)
    {
        memmove(&expanded_streams[index + 1U], &expanded_streams[index],
                (kernel->random_stream_count - index) * sizeof(*expanded_streams));
        expanded_streams[index] = *candidate;
        free(kernel->random_streams);
        kernel->random_streams = expanded_streams;
        kernel->random_stream_capacity = expanded_capacity;
        ++kernel->random_stream_count;
    }
    else
    {
        kernel->random_streams[index] = *candidate;
    }
    kernel->total_random_u32_generated = total_generated;
}

static int entity_id_is_alive(
    const EntityRecord *entities,
    size_t entity_count,
    MiniSNNWorldsKernelEntityId entity_id)
{
    size_t index;

    if (entity_id.value == 0U)
    {
        return 0;
    }
    for (index = 0U; index < entity_count; ++index)
    {
        if (entities[index].entity_id.value == entity_id.value)
        {
            return entities[index].alive != 0;
        }
    }
    return 0;
}

static EntityRecord *find_entity_record(
    EntityRecord *entities,
    size_t entity_count,
    MiniSNNWorldsKernelEntityId entity_id)
{
    size_t index;

    if (entity_id.value == 0U)
    {
        return NULL;
    }
    for (index = 0U; index < entity_count; ++index)
    {
        if (entities[index].entity_id.value == entity_id.value)
        {
            return &entities[index];
        }
    }
    return NULL;
}

static int command_compare(
    const MiniSNNWorldsKernelCommandInfo *left,
    const MiniSNNWorldsKernelCommandInfo *right)
{
    if (left->target_tick < right->target_tick)
    {
        return -1;
    }
    if (left->target_tick > right->target_tick)
    {
        return 1;
    }
    if (left->priority < right->priority)
    {
        return -1;
    }
    if (left->priority > right->priority)
    {
        return 1;
    }
    if (left->issuer.value < right->issuer.value)
    {
        return -1;
    }
    if (left->issuer.value > right->issuer.value)
    {
        return 1;
    }
    if (left->command_id.value < right->command_id.value)
    {
        return -1;
    }
    if (left->command_id.value > right->command_id.value)
    {
        return 1;
    }
    return 0;
}

static int command_qsort_compare(const void *left, const void *right)
{
    return command_compare((const MiniSNNWorldsKernelCommandInfo *)left,
                           (const MiniSNNWorldsKernelCommandInfo *)right);
}

static int identifiers_available(uint64_t next_identifier, size_t required_count)
{
    uint64_t count;

    if (required_count == 0U)
    {
        return 1;
    }
    if (required_count > UINT64_MAX)
    {
        return 0;
    }
    count = (uint64_t)required_count;
    return next_identifier != 0U && next_identifier <= UINT64_MAX - count;
}

static void step_plan_destroy(StepPlan *plan)
{
    if (plan != NULL)
    {
        free(plan->ordered_commands);
        free(plan->planned_entities);
        free(plan->next_events);
        memset(plan, 0, sizeof(*plan));
    }
}

static MiniSNNWorldsKernelError prepare_step_plan(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick next_tick,
    StepPlan *out_plan)
{
    size_t index;
    size_t due_count = 0U;
    size_t potential_create_count = 0U;
    size_t ordered_index = 0U;
    size_t capacity;

    memset(out_plan, 0, sizeof(*out_plan));
    for (index = 0U; index < kernel->pending_command_count; ++index)
    {
        if (kernel->pending_commands[index].target_tick == next_tick)
        {
            ++due_count;
            if (kernel->pending_commands[index].type ==
                MINISNN_WORLDS_KERNEL_COMMAND_CREATE_ENTITY)
            {
                ++potential_create_count;
            }
        }
    }
    if (!identifiers_available(kernel->next_event_id.value, due_count))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_IDENTIFIER_OVERFLOW;
    }
    if (kernel->entity_count > SIZE_MAX - potential_create_count)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    if (due_count != 0U)
    {
        if (due_count > SIZE_MAX / sizeof(*out_plan->ordered_commands) ||
            due_count > SIZE_MAX / sizeof(*out_plan->next_events))
        {
            return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
        }
        out_plan->ordered_commands = kernel_allocate(
            due_count * sizeof(*out_plan->ordered_commands));
        if (out_plan->ordered_commands == NULL)
        {
            return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
        }
        for (index = 0U; index < kernel->pending_command_count; ++index)
        {
            if (kernel->pending_commands[index].target_tick == next_tick)
            {
                out_plan->ordered_commands[ordered_index] = kernel->pending_commands[index];
                ++ordered_index;
            }
        }
        qsort(out_plan->ordered_commands, due_count,
              sizeof(*out_plan->ordered_commands), command_qsort_compare);
        out_plan->next_events = kernel_allocate(due_count * sizeof(*out_plan->next_events));
        if (out_plan->next_events == NULL)
        {
            step_plan_destroy(out_plan);
            return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
        }
    }
    out_plan->command_count = due_count;
    if (due_count != 0U &&
        (kernel->entity_count != 0U || potential_create_count != 0U))
    {
        if (!next_capacity(kernel->entity_capacity,
                           kernel->entity_count + potential_create_count,
                           sizeof(*out_plan->planned_entities), &capacity))
        {
            step_plan_destroy(out_plan);
            return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
        }
        out_plan->planned_entities = allocate_expanded_copy(
            kernel->entities, kernel->entity_count, capacity,
            sizeof(*out_plan->planned_entities));
        if (out_plan->planned_entities == NULL)
        {
            step_plan_destroy(out_plan);
            return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
        }
        out_plan->planned_entity_capacity = capacity;
    }
    out_plan->planned_entity_count = kernel->entity_count;
    out_plan->planned_alive_entity_count = kernel->alive_entity_count;
    out_plan->next_entity_id = kernel->next_entity_id;
    out_plan->next_event_id = kernel->next_event_id;
    for (index = 0U; index < due_count; ++index)
    {
        MiniSNNWorldsKernelCommandInfo *command = &out_plan->ordered_commands[index];
        MiniSNNWorldsKernelEvent *event = &out_plan->next_events[index];
        MiniSNNWorldsKernelCommandRejection rejection =
            MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_NONE;
        MiniSNNWorldsKernelEventType event_type =
            MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED;
        MiniSNNWorldsKernelEntityId subject = command->target_entity;

        event->event_id = out_plan->next_event_id;
        event->tick = next_tick;
        event->command_id = command->command_id;
        event->issuer = command->issuer;
        if (command->issuer.value != 0U &&
            !entity_id_is_alive(out_plan->planned_entities,
                                out_plan->planned_entity_count, command->issuer))
        {
            rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_ISSUER_NOT_ALIVE;
        }
        else if (command->type == MINISNN_WORLDS_KERNEL_COMMAND_CREATE_ENTITY)
        {
            EntityRecord *record;

            if (!identifiers_available(out_plan->next_entity_id.value, 1U))
            {
                step_plan_destroy(out_plan);
                return MINISNN_WORLDS_KERNEL_ERROR_IDENTIFIER_OVERFLOW;
            }
            record = &out_plan->planned_entities[out_plan->planned_entity_count];
            record->entity_id = out_plan->next_entity_id;
            record->creation_tick = next_tick;
            record->destruction_tick = MINISNN_WORLDS_TICK_INITIAL;
            record->alive = 1;
            subject = out_plan->next_entity_id;
            ++out_plan->planned_entity_count;
            ++out_plan->planned_alive_entity_count;
            ++out_plan->next_entity_id.value;
            ++out_plan->created_count;
            ++out_plan->applied_count;
            event_type = MINISNN_WORLDS_KERNEL_EVENT_ENTITY_CREATED;
        }
        else if (command->type == MINISNN_WORLDS_KERNEL_COMMAND_DESTROY_ENTITY)
        {
            EntityRecord *record = find_entity_record(
                out_plan->planned_entities, out_plan->planned_entity_count,
                command->target_entity);

            if (record == NULL || record->alive == 0)
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_NOT_ALIVE;
            }
            else
            {
                record->alive = 0;
                record->destruction_tick = next_tick;
                --out_plan->planned_alive_entity_count;
                ++out_plan->destroyed_count;
                ++out_plan->applied_count;
                event_type = MINISNN_WORLDS_KERNEL_EVENT_ENTITY_DESTROYED;
            }
        }
        else
        {
            rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_INVALID_COMMAND;
        }
        if (rejection != MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_NONE)
        {
            ++out_plan->rejected_count;
        }
        event->type = event_type;
        event->subject = subject;
        event->rejection = rejection;
        ++out_plan->next_event_id.value;
    }
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static MiniSNNWorldsKernelError queue_command(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelCommandType type,
    MiniSNNWorldsKernelEntityId target_entity,
    MiniSNNWorldsKernelCommandId *out_command_id)
{
    MiniSNNWorldsKernelCommandInfo *expanded_commands = NULL;
    size_t capacity;

    if (kernel == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (out_command_id == NULL)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT);
        return kernel->last_error;
    }
    if (kernel->state != MINISNN_WORLDS_KERNEL_STATE_READY)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE);
        return kernel->last_error;
    }
    if (target_tick <= kernel->tick)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_TICK);
        return kernel->last_error;
    }
    if (type != MINISNN_WORLDS_KERNEL_COMMAND_CREATE_ENTITY &&
        type != MINISNN_WORLDS_KERNEL_COMMAND_DESTROY_ENTITY)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_COMMAND);
        return kernel->last_error;
    }
    if (type == MINISNN_WORLDS_KERNEL_COMMAND_DESTROY_ENTITY &&
        target_entity.value == 0U)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_ENTITY_ID);
        return kernel->last_error;
    }
    if (!identifiers_available(kernel->next_command_id.value, 1U))
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_IDENTIFIER_OVERFLOW);
        return kernel->last_error;
    }
    if (kernel->pending_command_count == SIZE_MAX)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
        return kernel->last_error;
    }
    if (kernel->pending_command_count + 1U > kernel->pending_command_capacity)
    {
        if (!next_capacity(kernel->pending_command_capacity,
                           kernel->pending_command_count + 1U,
                           sizeof(*expanded_commands), &capacity))
        {
            set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
            return kernel->last_error;
        }
        expanded_commands = allocate_expanded_copy(
            kernel->pending_commands, kernel->pending_command_count, capacity,
            sizeof(*expanded_commands));
        if (expanded_commands == NULL)
        {
            set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
            return kernel->last_error;
        }
    }
    if (expanded_commands != NULL)
    {
        free(kernel->pending_commands);
        kernel->pending_commands = expanded_commands;
        kernel->pending_command_capacity = capacity;
    }
    kernel->pending_commands[kernel->pending_command_count].command_id =
        kernel->next_command_id;
    kernel->pending_commands[kernel->pending_command_count].target_tick = target_tick;
    kernel->pending_commands[kernel->pending_command_count].priority = priority;
    kernel->pending_commands[kernel->pending_command_count].issuer = issuer;
    kernel->pending_commands[kernel->pending_command_count].type = type;
    kernel->pending_commands[kernel->pending_command_count].target_entity = target_entity;
    *out_command_id = kernel->next_command_id;
    ++kernel->pending_command_count;
    ++kernel->next_command_id.value;
    ++kernel->total_commands_submitted;
    kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_NONE;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelConfig minisnn_worlds_kernel_config_default(void)
{
    MiniSNNWorldsKernelConfig config;

    config.struct_size = (uint32_t)sizeof(config);
    config.format_version = MINISNN_WORLDS_KERNEL_CONFIG_VERSION;
    config.master_seed = MINISNN_WORLDS_KERNEL_DEFAULT_MASTER_SEED;
    return config;
}

MiniSNNWorldsKernel *minisnn_worlds_kernel_create(
    const MiniSNNWorldsKernelConfig *config,
    MiniSNNWorldsKernelError *out_error)
{
    MiniSNNWorldsKernelConfig default_config;
    MiniSNNWorldsKernel *kernel;

    assign_error(out_error, MINISNN_WORLDS_KERNEL_ERROR_NONE);
    if (config == NULL)
    {
        default_config = minisnn_worlds_kernel_config_default();
        config = &default_config;
    }
    if (!config_is_valid(config))
    {
        assign_error(out_error, MINISNN_WORLDS_KERNEL_ERROR_INVALID_CONFIG);
        return NULL;
    }

    kernel = kernel_allocate(sizeof(*kernel));
    if (kernel == NULL)
    {
        assign_error(out_error, MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
        return NULL;
    }
    memset(kernel, 0, sizeof(*kernel));
    kernel->state = MINISNN_WORLDS_KERNEL_STATE_READY;
    kernel->next_entity_id.value = UINT64_C(1);
    kernel->next_command_id.value = UINT64_C(1);
    kernel->next_event_id.value = UINT64_C(1);
    kernel->master_seed = config_master_seed(config);
    return kernel;
}

void minisnn_worlds_kernel_destroy(MiniSNNWorldsKernel *kernel)
{
    if (kernel != NULL)
    {
        free(kernel->entities);
        free(kernel->pending_commands);
        free(kernel->last_tick_events);
        free(kernel->random_streams);
        free(kernel);
    }
}

MiniSNNWorldsTick minisnn_worlds_kernel_tick(const MiniSNNWorldsKernel *kernel)
{
    return kernel == NULL ? MINISNN_WORLDS_TICK_INITIAL : kernel->tick;
}

MiniSNNWorldsKernelState minisnn_worlds_kernel_state(
    const MiniSNNWorldsKernel *kernel)
{
    return kernel == NULL ? MINISNN_WORLDS_KERNEL_STATE_FAULTED : kernel->state;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_last_error(
    const MiniSNNWorldsKernel *kernel)
{
    return kernel == NULL ? MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT :
                            kernel->last_error;
}

uint64_t minisnn_worlds_kernel_master_seed(const MiniSNNWorldsKernel *kernel)
{
    return kernel == NULL ? MINISNN_WORLDS_KERNEL_DEFAULT_MASTER_SEED :
                            kernel->master_seed;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_random_u32(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelRandomStreamKey key,
    uint32_t *out_value)
{
    RandomStreamRecord candidate;
    RandomStreamRecord *expanded_streams;
    size_t index;
    size_t expanded_capacity;
    uint64_t total_generated;
    uint32_t value;
    int is_new;
    MiniSNNWorldsKernelError error;

    if (kernel == NULL || out_value == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (kernel->state != MINISNN_WORLDS_KERNEL_STATE_READY)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE);
        return kernel->last_error;
    }
    if (!random_stream_key_is_valid(key))
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_RANDOM_STREAM_KEY);
        return kernel->last_error;
    }
    error = random_prepare_candidate(kernel, key, &index, &is_new, &candidate,
                                     &expanded_streams, &expanded_capacity);
    if (error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        set_last_error(kernel, error);
        return error;
    }
    total_generated = kernel->total_random_u32_generated;
    if (!random_draw_raw(&candidate, &total_generated, &value))
    {
        free(expanded_streams);
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_IDENTIFIER_OVERFLOW);
        return kernel->last_error;
    }
    random_commit_candidate(kernel, index, is_new, &candidate, expanded_streams,
                            expanded_capacity, total_generated);
    *out_value = value;
    kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_NONE;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_random_u64(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelRandomStreamKey key,
    uint64_t *out_value)
{
    RandomStreamRecord candidate;
    RandomStreamRecord *expanded_streams;
    size_t index;
    size_t expanded_capacity;
    uint64_t total_generated;
    uint32_t low;
    uint32_t high;
    int is_new;
    MiniSNNWorldsKernelError error;

    if (kernel == NULL || out_value == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (kernel->state != MINISNN_WORLDS_KERNEL_STATE_READY)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE);
        return kernel->last_error;
    }
    if (!random_stream_key_is_valid(key))
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_RANDOM_STREAM_KEY);
        return kernel->last_error;
    }
    error = random_prepare_candidate(kernel, key, &index, &is_new, &candidate,
                                     &expanded_streams, &expanded_capacity);
    if (error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        set_last_error(kernel, error);
        return error;
    }
    total_generated = kernel->total_random_u32_generated;
    if (!random_draw_raw(&candidate, &total_generated, &low) ||
        !random_draw_raw(&candidate, &total_generated, &high))
    {
        free(expanded_streams);
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_IDENTIFIER_OVERFLOW);
        return kernel->last_error;
    }
    random_commit_candidate(kernel, index, is_new, &candidate, expanded_streams,
                            expanded_capacity, total_generated);
    *out_value = ((uint64_t)high << 32U) | (uint64_t)low;
    kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_NONE;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_random_bounded_u32(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelRandomStreamKey key,
    uint32_t exclusive_upper_bound,
    uint32_t *out_value)
{
    RandomStreamRecord candidate;
    RandomStreamRecord *expanded_streams;
    size_t index;
    size_t expanded_capacity;
    uint64_t total_generated;
    uint32_t threshold;
    uint32_t value;
    int is_new;
    MiniSNNWorldsKernelError error;

    if (kernel == NULL || out_value == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (kernel->state != MINISNN_WORLDS_KERNEL_STATE_READY)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE);
        return kernel->last_error;
    }
    if (!random_stream_key_is_valid(key))
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_RANDOM_STREAM_KEY);
        return kernel->last_error;
    }
    if (exclusive_upper_bound == 0U)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_BOUND);
        return kernel->last_error;
    }
    error = random_prepare_candidate(kernel, key, &index, &is_new, &candidate,
                                     &expanded_streams, &expanded_capacity);
    if (error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        set_last_error(kernel, error);
        return error;
    }
    total_generated = kernel->total_random_u32_generated;
    threshold = (UINT32_C(0) - exclusive_upper_bound) % exclusive_upper_bound;
    do
    {
        if (!random_draw_raw(&candidate, &total_generated, &value))
        {
            free(expanded_streams);
            set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_IDENTIFIER_OVERFLOW);
            return kernel->last_error;
        }
    } while (value < threshold);
    random_commit_candidate(kernel, index, is_new, &candidate, expanded_streams,
                            expanded_capacity, total_generated);
    *out_value = value % exclusive_upper_bound;
    kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_NONE;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

size_t minisnn_worlds_kernel_random_stream_count(const MiniSNNWorldsKernel *kernel)
{
    return kernel == NULL ? 0U : kernel->random_stream_count;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_random_stream_at(
    const MiniSNNWorldsKernel *kernel,
    size_t canonical_index,
    MiniSNNWorldsKernelRandomStreamInfo *out_stream)
{
    if (kernel == NULL || out_stream == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (canonical_index >= kernel->random_stream_count)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INDEX_OUT_OF_RANGE;
    }
    out_stream->key = kernel->random_streams[canonical_index].key;
    out_stream->state = kernel->random_streams[canonical_index].state;
    out_stream->sequence = kernel->random_streams[canonical_index].sequence;
    out_stream->generated_u32_count =
        kernel->random_streams[canonical_index].generated_u32_count;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

bool minisnn_worlds_kernel_entity_exists(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId entity_id)
{
    return kernel != NULL && entity_id_is_alive(kernel->entities, kernel->entity_count,
                                                entity_id) != 0;
}

size_t minisnn_worlds_kernel_entity_count(const MiniSNNWorldsKernel *kernel)
{
    return kernel == NULL ? 0U : kernel->alive_entity_count;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_entity_at(
    const MiniSNNWorldsKernel *kernel,
    size_t alive_index,
    MiniSNNWorldsKernelEntityId *out_entity_id)
{
    size_t index;
    size_t found = 0U;

    if (kernel == NULL || out_entity_id == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    for (index = 0U; index < kernel->entity_count; ++index)
    {
        if (kernel->entities[index].alive != 0)
        {
            if (found == alive_index)
            {
                *out_entity_id = kernel->entities[index].entity_id;
                return MINISNN_WORLDS_KERNEL_ERROR_NONE;
            }
            ++found;
        }
    }
    return MINISNN_WORLDS_KERNEL_ERROR_INDEX_OUT_OF_RANGE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_create_entity(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelCommandId *out_command_id)
{
    MiniSNNWorldsKernelEntityId no_target = { UINT64_C(0) };

    return queue_command(kernel, target_tick, priority, issuer,
                         MINISNN_WORLDS_KERNEL_COMMAND_CREATE_ENTITY, no_target,
                         out_command_id);
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_destroy_entity(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelEntityId target_entity,
    MiniSNNWorldsKernelCommandId *out_command_id)
{
    return queue_command(kernel, target_tick, priority, issuer,
                         MINISNN_WORLDS_KERNEL_COMMAND_DESTROY_ENTITY, target_entity,
                         out_command_id);
}

size_t minisnn_worlds_kernel_pending_command_count(
    const MiniSNNWorldsKernel *kernel)
{
    return kernel == NULL ? 0U : kernel->pending_command_count;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_pending_command_at(
    const MiniSNNWorldsKernel *kernel,
    size_t canonical_index,
    MiniSNNWorldsKernelCommandInfo *out_command)
{
    size_t candidate_index;

    if (kernel == NULL || out_command == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (canonical_index >= kernel->pending_command_count)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INDEX_OUT_OF_RANGE;
    }
    for (candidate_index = 0U; candidate_index < kernel->pending_command_count;
         ++candidate_index)
    {
        size_t other_index;
        size_t rank = 0U;

        for (other_index = 0U; other_index < kernel->pending_command_count;
             ++other_index)
        {
            if (command_compare(&kernel->pending_commands[other_index],
                                &kernel->pending_commands[candidate_index]) < 0)
            {
                ++rank;
            }
        }
        if (rank == canonical_index)
        {
            *out_command = kernel->pending_commands[candidate_index];
            return MINISNN_WORLDS_KERNEL_ERROR_NONE;
        }
    }
    return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
}

size_t minisnn_worlds_kernel_last_tick_event_count(
    const MiniSNNWorldsKernel *kernel)
{
    return kernel == NULL ? 0U : kernel->last_tick_event_count;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_last_tick_event_at(
    const MiniSNNWorldsKernel *kernel,
    size_t canonical_index,
    MiniSNNWorldsKernelEvent *out_event)
{
    if (kernel == NULL || out_event == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (canonical_index >= kernel->last_tick_event_count)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INDEX_OUT_OF_RANGE;
    }
    *out_event = kernel->last_tick_events[canonical_index];
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static void fnv1a_append_byte(uint64_t *in_out_hash, uint8_t value)
{
    *in_out_hash ^= (uint64_t)value;
    *in_out_hash *= MINISNN_WORLDS_KERNEL_FNV1A_PRIME;
}

static void fnv1a_append_u32(uint64_t *in_out_hash, uint32_t value)
{
    size_t index;

    for (index = 0U; index < 4U; ++index)
    {
        fnv1a_append_byte(in_out_hash, (uint8_t)(value >> (index * 8U)));
    }
}

static void fnv1a_append_u64(uint64_t *in_out_hash, uint64_t value)
{
    size_t index;

    for (index = 0U; index < 8U; ++index)
    {
        fnv1a_append_byte(in_out_hash, (uint8_t)(value >> (index * 8U)));
    }
}

static void fnv1a_append_literal(uint64_t *in_out_hash, const char *text)
{
    while (*text != '\0')
    {
        fnv1a_append_byte(in_out_hash, (uint8_t)*text);
        ++text;
    }
}

static const EntityRecord *canonical_entity_at(
    const MiniSNNWorldsKernel *kernel,
    size_t canonical_index)
{
    size_t candidate_index;

    for (candidate_index = 0U; candidate_index < kernel->entity_count;
         ++candidate_index)
    {
        size_t other_index;
        size_t rank = 0U;

        for (other_index = 0U; other_index < kernel->entity_count; ++other_index)
        {
            if (kernel->entities[other_index].entity_id.value <
                kernel->entities[candidate_index].entity_id.value)
            {
                ++rank;
            }
        }
        if (rank == canonical_index)
        {
            return &kernel->entities[candidate_index];
        }
    }
    return NULL;
}

static const MiniSNNWorldsKernelCommandInfo *canonical_pending_command_at(
    const MiniSNNWorldsKernel *kernel,
    size_t canonical_index)
{
    size_t candidate_index;

    for (candidate_index = 0U; candidate_index < kernel->pending_command_count;
         ++candidate_index)
    {
        size_t other_index;
        size_t rank = 0U;

        for (other_index = 0U; other_index < kernel->pending_command_count;
             ++other_index)
        {
            if (command_compare(&kernel->pending_commands[other_index],
                                &kernel->pending_commands[candidate_index]) < 0)
            {
                ++rank;
            }
        }
        if (rank == canonical_index)
        {
            return &kernel->pending_commands[candidate_index];
        }
    }
    return NULL;
}

static const RandomStreamRecord *canonical_random_stream_at(
    const MiniSNNWorldsKernel *kernel,
    size_t canonical_index)
{
    size_t candidate_index;

    for (candidate_index = 0U; candidate_index < kernel->random_stream_count;
         ++candidate_index)
    {
        size_t other_index;
        size_t rank = 0U;

        for (other_index = 0U; other_index < kernel->random_stream_count;
             ++other_index)
        {
            if (random_stream_compare(kernel->random_streams[other_index].key,
                                      kernel->random_streams[candidate_index].key) < 0)
            {
                ++rank;
            }
        }
        if (rank == canonical_index)
        {
            return &kernel->random_streams[candidate_index];
        }
    }
    return NULL;
}

static uint64_t compute_state_hash(const MiniSNNWorldsKernel *kernel)
{
    uint64_t hash = MINISNN_WORLDS_KERNEL_FNV1A_OFFSET;
    size_t index;

    fnv1a_append_literal(&hash, "MSWK_STATE_V1");
    fnv1a_append_u32(&hash, MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION);
    fnv1a_append_u32(&hash, MINISNN_WORLDS_KERNEL_PRNG_VERSION);
    fnv1a_append_u32(&hash, MINISNN_WORLDS_KERNEL_CONFIG_VERSION);
    fnv1a_append_u64(&hash, kernel->master_seed);
    fnv1a_append_u64(&hash, kernel->tick);
    fnv1a_append_u64(&hash, kernel->next_entity_id.value);
    fnv1a_append_u64(&hash, kernel->next_command_id.value);
    fnv1a_append_u64(&hash, kernel->next_event_id.value);

    fnv1a_append_u64(&hash, (uint64_t)kernel->entity_count);
    for (index = 0U; index < kernel->entity_count; ++index)
    {
        const EntityRecord *record = canonical_entity_at(kernel, index);

        fnv1a_append_u64(&hash, record->entity_id.value);
        fnv1a_append_byte(&hash, record->alive != 0 ? UINT8_C(1) : UINT8_C(0));
        fnv1a_append_u64(&hash, record->creation_tick);
        fnv1a_append_u64(&hash, record->destruction_tick);
    }

    fnv1a_append_u64(&hash, (uint64_t)kernel->pending_command_count);
    for (index = 0U; index < kernel->pending_command_count; ++index)
    {
        const MiniSNNWorldsKernelCommandInfo *command =
            canonical_pending_command_at(kernel, index);

        fnv1a_append_u64(&hash, command->command_id.value);
        fnv1a_append_u64(&hash, command->target_tick);
        fnv1a_append_u32(&hash, command->priority);
        fnv1a_append_u64(&hash, command->issuer.value);
        fnv1a_append_u32(&hash, (uint32_t)command->type);
        fnv1a_append_u64(&hash, command->target_entity.value);
    }

    fnv1a_append_u64(&hash, (uint64_t)kernel->last_tick_event_count);
    for (index = 0U; index < kernel->last_tick_event_count; ++index)
    {
        const MiniSNNWorldsKernelEvent *event = &kernel->last_tick_events[index];

        fnv1a_append_u64(&hash, event->event_id.value);
        fnv1a_append_u64(&hash, event->tick);
        fnv1a_append_u32(&hash, (uint32_t)event->type);
        fnv1a_append_u64(&hash, event->command_id.value);
        fnv1a_append_u64(&hash, event->issuer.value);
        fnv1a_append_u64(&hash, event->subject.value);
        fnv1a_append_u32(&hash, (uint32_t)event->rejection);
    }

    fnv1a_append_u64(&hash, kernel->total_entities_created);
    fnv1a_append_u64(&hash, kernel->total_entities_destroyed);
    fnv1a_append_u64(&hash, kernel->total_commands_submitted);
    fnv1a_append_u64(&hash, kernel->total_commands_applied);
    fnv1a_append_u64(&hash, kernel->total_commands_rejected);
    fnv1a_append_u64(&hash, kernel->total_events_emitted);
    fnv1a_append_u64(&hash, kernel->total_random_u32_generated);

    fnv1a_append_u64(&hash, (uint64_t)kernel->random_stream_count);
    for (index = 0U; index < kernel->random_stream_count; ++index)
    {
        const RandomStreamRecord *record = canonical_random_stream_at(kernel, index);

        fnv1a_append_u64(&hash, record->key.namespace_id);
        fnv1a_append_u64(&hash, record->key.stream_id);
        fnv1a_append_u64(&hash, record->state);
        fnv1a_append_u64(&hash, record->sequence);
        fnv1a_append_u64(&hash, record->generated_u32_count);
    }
    return hash;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_state_hash(
    const MiniSNNWorldsKernel *kernel,
    uint64_t *out_hash)
{
    if (kernel == NULL || out_hash == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (kernel->state != MINISNN_WORLDS_KERNEL_STATE_READY)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE;
    }
    *out_hash = compute_state_hash(kernel);
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_step(MiniSNNWorldsKernel *kernel)
{
    StepPlan plan;
    MiniSNNWorldsTick next_tick;
    size_t index;
    MiniSNNWorldsKernelError error;

    if (kernel == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (kernel->state != MINISNN_WORLDS_KERNEL_STATE_READY)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE);
        return kernel->last_error;
    }
    if (kernel->tick == UINT64_MAX)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_TICK_OVERFLOW);
        return kernel->last_error;
    }

    next_tick = kernel->tick + UINT64_C(1);
    kernel->state = MINISNN_WORLDS_KERNEL_STATE_STEPPING;
    error = prepare_step_plan(kernel, next_tick, &plan);
    if (error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        kernel->state = MINISNN_WORLDS_KERNEL_STATE_READY;
        kernel->last_error = error;
        return error;
    }

    if (plan.command_count != 0U)
    {
        free(kernel->entities);
        kernel->entities = plan.planned_entities;
        kernel->entity_capacity = plan.planned_entity_capacity;
        plan.planned_entities = NULL;
    }
    kernel->entity_count = plan.planned_entity_count;
    kernel->alive_entity_count = plan.planned_alive_entity_count;
    kernel->next_entity_id = plan.next_entity_id;
    kernel->next_event_id = plan.next_event_id;
    kernel->total_entities_created += plan.created_count;
    kernel->total_entities_destroyed += plan.destroyed_count;
    kernel->total_commands_applied += plan.applied_count;
    kernel->total_commands_rejected += plan.rejected_count;
    kernel->total_events_emitted += (uint64_t)plan.command_count;
    free(kernel->last_tick_events);
    kernel->last_tick_events = plan.next_events;
    kernel->last_tick_event_count = plan.command_count;
    plan.next_events = NULL;
    if (plan.command_count != 0U)
    {
        size_t retained_count = 0U;

        for (index = 0U; index < kernel->pending_command_count; ++index)
        {
            if (kernel->pending_commands[index].target_tick != next_tick)
            {
                kernel->pending_commands[retained_count] = kernel->pending_commands[index];
                ++retained_count;
            }
        }
        kernel->pending_command_count = retained_count;
    }
    kernel->tick = next_tick;
    kernel->state = MINISNN_WORLDS_KERNEL_STATE_READY;
    kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_NONE;
    step_plan_destroy(&plan);
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_get_diagnostics(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelDiagnostics *out_diagnostics)
{
    MiniSNNWorldsKernelDiagnostics diagnostics;

    if (kernel == NULL || out_diagnostics == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    diagnostics.completed_ticks = kernel->tick;
    diagnostics.state = kernel->state;
    diagnostics.last_error = kernel->last_error;
    diagnostics.alive_entities = (uint64_t)kernel->alive_entity_count;
    diagnostics.total_entities_created = kernel->total_entities_created;
    diagnostics.total_entities_destroyed = kernel->total_entities_destroyed;
    diagnostics.pending_commands = (uint64_t)kernel->pending_command_count;
    diagnostics.total_commands_submitted = kernel->total_commands_submitted;
    diagnostics.total_commands_applied = kernel->total_commands_applied;
    diagnostics.total_commands_rejected = kernel->total_commands_rejected;
    diagnostics.last_tick_events = (uint64_t)kernel->last_tick_event_count;
    diagnostics.total_events_emitted = kernel->total_events_emitted;
    diagnostics.master_seed = kernel->master_seed;
    diagnostics.random_streams = (uint64_t)kernel->random_stream_count;
    diagnostics.total_random_u32_generated = kernel->total_random_u32_generated;
    diagnostics.current_state_hash = compute_state_hash(kernel);
    diagnostics.state_hash_version = MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION;
    diagnostics.prng_version = MINISNN_WORLDS_KERNEL_PRNG_VERSION;
    *out_diagnostics = diagnostics;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_capture_trace_point(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelTracePoint *out_trace)
{
    MiniSNNWorldsKernelTracePoint trace;

    if (kernel == NULL || out_trace == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (kernel->state != MINISNN_WORLDS_KERNEL_STATE_READY)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE;
    }
    trace.tick = kernel->tick;
    trace.state_hash = compute_state_hash(kernel);
    trace.alive_entities = (uint64_t)kernel->alive_entity_count;
    trace.pending_commands = (uint64_t)kernel->pending_command_count;
    trace.last_tick_events = (uint64_t)kernel->last_tick_event_count;
    trace.random_streams = (uint64_t)kernel->random_stream_count;
    trace.total_random_u32_generated = kernel->total_random_u32_generated;
    *out_trace = trace;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

#ifdef MINISNN_WORLDS_KERNEL_TESTING
void minisnn_worlds_kernel_testing_fail_next_allocation(void)
{
    testing_allocation_fail_after = 0U;
}

void minisnn_worlds_kernel_testing_fail_allocation_after(size_t successful_allocations)
{
    testing_allocation_fail_after = successful_allocations;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_set_tick(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick tick)
{
    if (kernel == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (kernel->state != MINISNN_WORLDS_KERNEL_STATE_READY)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE);
        return kernel->last_error;
    }
    kernel->tick = tick;
    kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_NONE;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_set_next_entity_id(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId entity_id)
{
    if (kernel == NULL || entity_id.value == 0U)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    kernel->next_entity_id = entity_id;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_set_next_command_id(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelCommandId command_id)
{
    if (kernel == NULL || command_id.value == 0U)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    kernel->next_command_id = command_id;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_set_next_event_id(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEventId event_id)
{
    if (kernel == NULL || event_id.value == 0U)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    kernel->next_event_id = event_id;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_set_random_counts(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelRandomStreamKey key,
    uint64_t generated_u32_count,
    uint64_t total_random_u32_generated)
{
    int exists;
    size_t index;

    if (kernel == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (kernel->state != MINISNN_WORLDS_KERNEL_STATE_READY)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE);
        return kernel->last_error;
    }
    if (!random_stream_key_is_valid(key))
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_RANDOM_STREAM_KEY);
        return kernel->last_error;
    }
    index = random_stream_insert_index(kernel, key, &exists);
    if (exists == 0 || total_random_u32_generated < generated_u32_count)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_RANDOM_STREAM_KEY);
        return kernel->last_error;
    }
    kernel->random_streams[index].generated_u32_count = generated_u32_count;
    kernel->total_random_u32_generated = total_random_u32_generated;
    kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_NONE;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}
#endif
