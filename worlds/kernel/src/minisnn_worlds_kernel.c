#include "minisnn_worlds_kernel.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define MINISNN_WORLDS_KERNEL_CONFIG_MINIMUM_READABLE_SIZE \
    (offsetof(MiniSNNWorldsKernelConfig, format_version) + \
     sizeof(((MiniSNNWorldsKernelConfig *)0)->format_version))

typedef struct
{
    MiniSNNWorldsKernelEntityId entity_id;
    MiniSNNWorldsTick creation_tick;
    MiniSNNWorldsTick destruction_tick;
    int alive;
} EntityRecord;

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
    return kernel;
}

void minisnn_worlds_kernel_destroy(MiniSNNWorldsKernel *kernel)
{
    if (kernel != NULL)
    {
        free(kernel->entities);
        free(kernel->pending_commands);
        free(kernel->last_tick_events);
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
    *out_diagnostics = diagnostics;
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
#endif
