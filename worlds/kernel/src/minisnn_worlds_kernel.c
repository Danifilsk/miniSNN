#include "minisnn_worlds_kernel.h"
#include "minisnn_worlds_kernel_internal.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>


#define MINISNN_WORLDS_KERNEL_CONFIG_MINIMUM_READABLE_SIZE \
    (offsetof(MiniSNNWorldsKernelConfig, format_version) + \
     sizeof(((MiniSNNWorldsKernelConfig *)0)->format_version))

#define MINISNN_WORLDS_KERNEL_CONFIG_MASTER_SEED_READABLE_SIZE \
    (offsetof(MiniSNNWorldsKernelConfig, master_seed) + \
     sizeof(((MiniSNNWorldsKernelConfig *)0)->master_seed))

#define MINISNN_WORLDS_KERNEL_CONFIG_SPACE_BOUNDS_READABLE_SIZE \
    (offsetof(MiniSNNWorldsKernelConfig, space_bounds) + \
     sizeof(((MiniSNNWorldsKernelConfig *)0)->space_bounds))

#define MINISNN_WORLDS_KERNEL_PCG32_MULTIPLIER UINT64_C(6364136223846793005)
#define MINISNN_WORLDS_KERNEL_FNV1A_OFFSET UINT64_C(14695981039346656037)
#define MINISNN_WORLDS_KERNEL_FNV1A_PRIME UINT64_C(1099511628211)


static int find_spatial_parent_link(
    const SpatialLinkRecord *links,
    size_t link_count,
    MiniSNNWorldsKernelEntityId child,
    SpatialLinkRecord *out_link);

typedef struct
{
    MiniSNNWorldsKernelEntityId entity;
    MiniSNNWorldsKernelEntityId immediate_parent;
    size_t entity_index;
    MiniSNNWorldsKernelTransform previous_transform;
    MiniSNNWorldsKernelTransform destination_transform;
} SubtreeMoveMember;

typedef struct
{
    SubtreeMoveMember *members;
    unsigned char *member_flags;
    size_t count;
    size_t capacity;
} SubtreeMovePlan;


typedef struct
{
    MiniSNNWorldsKernelCommandInfo *ordered_commands;
    size_t command_count;
    EntityRecord *planned_entities;
    size_t planned_entity_capacity;
    SpatialLinkRecord *planned_spatial_links;
    size_t planned_spatial_link_capacity;
    size_t planned_spatial_link_count;
    size_t planned_entity_count;
    size_t planned_alive_entity_count;
    MiniSNNWorldsKernelEvent *next_events;
    size_t next_event_count;
    size_t next_event_capacity;
    MiniSNNWorldsKernelEntityId next_entity_id;
    MiniSNNWorldsKernelEventId next_event_id;
    uint64_t applied_count;
    uint64_t rejected_count;
    uint64_t created_count;
    uint64_t destroyed_count;
    uint64_t placed_count;
    uint64_t removed_from_space_count;
    size_t planned_placed_entity_count;
    size_t planned_entities_with_occupancy;
    size_t planned_active_occupancies;
    size_t planned_blocking_occupancies;
    uint64_t occupancies_set_count;
    uint64_t occupancies_cleared_count;
    uint64_t occupancy_conflicts_rejected_count;
    uint64_t movement_commands_processed_count;
    uint64_t moved_count;
    uint64_t movement_overflows_rejected_count;
    uint64_t spatial_links_created_count;
    uint64_t spatial_links_removed_count;
    uint64_t spatial_link_commands_processed_count;
} StepPlan;


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

static int scalar_add_checked(
    MiniSNNWorldsKernelScalar left,
    MiniSNNWorldsKernelScalar right,
    MiniSNNWorldsKernelScalar *out_result);

static int scalar_subtract_checked(
    MiniSNNWorldsKernelScalar left,
    MiniSNNWorldsKernelScalar right,
    MiniSNNWorldsKernelScalar *out_result);
static MiniSNNWorldsKernelTransform zero_transform(void)
{
    MiniSNNWorldsKernelTransform transform;

    transform.position.x = MINISNN_WORLDS_KERNEL_SCALAR_ZERO;
    transform.position.y = MINISNN_WORLDS_KERNEL_SCALAR_ZERO;
    transform.orientation = UINT32_C(0);
    return transform;
}

static MiniSNNWorldsKernelPosition zero_position(void)
{
    MiniSNNWorldsKernelPosition position;

    position.x = MINISNN_WORLDS_KERNEL_SCALAR_ZERO;
    position.y = MINISNN_WORLDS_KERNEL_SCALAR_ZERO;
    return position;
}

static MiniSNNWorldsKernelOccupancy zero_occupancy(void)
{
    MiniSNNWorldsKernelOccupancy occupancy;

    occupancy.half_extent_x = MINISNN_WORLDS_KERNEL_SCALAR_ZERO;
    occupancy.half_extent_y = MINISNN_WORLDS_KERNEL_SCALAR_ZERO;
    occupancy.category_bits = UINT32_C(0);
    occupancy.blocking_mask = UINT32_C(0);
    return occupancy;
}

static MiniSNNWorldsKernelSpatialLink zero_spatial_link(void)
{
    MiniSNNWorldsKernelSpatialLink link;

    link.parent.value = UINT64_C(0);
    link.child.value = UINT64_C(0);
    link.offset_x = MINISNN_WORLDS_KERNEL_SCALAR_ZERO;
    link.offset_y = MINISNN_WORLDS_KERNEL_SCALAR_ZERO;
    return link;
}

static MiniSNNWorldsKernelSpatialLinkEndpoints zero_spatial_link_endpoints(void)
{
    MiniSNNWorldsKernelSpatialLinkEndpoints endpoints;

    endpoints.parent.value = UINT64_C(0);
    endpoints.child.value = UINT64_C(0);
    return endpoints;
}
static int occupancy_is_valid(MiniSNNWorldsKernelOccupancy occupancy)
{
    return occupancy.half_extent_x > MINISNN_WORLDS_KERNEL_SCALAR_ZERO &&
           occupancy.half_extent_y > MINISNN_WORLDS_KERNEL_SCALAR_ZERO &&
           occupancy.category_bits != UINT32_C(0);
}

static int occupancy_fits_space(
    MiniSNNWorldsKernelOccupancy occupancy,
    MiniSNNWorldsKernelSpaceBounds bounds)
{
    MiniSNNWorldsKernelScalar min_center_x;
    MiniSNNWorldsKernelScalar max_center_x;
    MiniSNNWorldsKernelScalar min_center_y;
    MiniSNNWorldsKernelScalar max_center_y;

    return occupancy_is_valid(occupancy) &&
           scalar_add_checked(bounds.min_x, occupancy.half_extent_x, &min_center_x) &&
           scalar_subtract_checked(bounds.max_x, occupancy.half_extent_x, &max_center_x) &&
           scalar_add_checked(bounds.min_y, occupancy.half_extent_y, &min_center_y) &&
           scalar_subtract_checked(bounds.max_y, occupancy.half_extent_y, &max_center_y) &&
           min_center_x <= max_center_x && min_center_y <= max_center_y;
}

static int occupancy_envelope(
    MiniSNNWorldsKernelTransform transform,
    MiniSNNWorldsKernelOccupancy occupancy,
    MiniSNNWorldsKernelSpaceBounds *out_envelope)
{
    MiniSNNWorldsKernelSpaceBounds envelope;

    if (out_envelope == NULL || !occupancy_is_valid(occupancy) ||
        !scalar_subtract_checked(transform.position.x, occupancy.half_extent_x,
                                 &envelope.min_x) ||
        !scalar_add_checked(transform.position.x, occupancy.half_extent_x,
                            &envelope.max_x) ||
        !scalar_subtract_checked(transform.position.y, occupancy.half_extent_y,
                                 &envelope.min_y) ||
        !scalar_add_checked(transform.position.y, occupancy.half_extent_y,
                            &envelope.max_y))
    {
        return 0;
    }
    *out_envelope = envelope;
    return 1;
}

static int occupancy_envelope_is_within_bounds(
    MiniSNNWorldsKernelTransform transform,
    MiniSNNWorldsKernelOccupancy occupancy,
    MiniSNNWorldsKernelSpaceBounds bounds)
{
    MiniSNNWorldsKernelSpaceBounds envelope;

    return occupancy_envelope(transform, occupancy, &envelope) &&
           envelope.min_x >= bounds.min_x && envelope.max_x <= bounds.max_x &&
           envelope.min_y >= bounds.min_y && envelope.max_y <= bounds.max_y;
}

static int aabb_has_positive_overlap(
    MiniSNNWorldsKernelSpaceBounds left,
    MiniSNNWorldsKernelSpaceBounds right)
{
    return left.min_x < right.max_x && left.max_x > right.min_x &&
           left.min_y < right.max_y && left.max_y > right.min_y;
}

static int occupancies_block_each_other(
    MiniSNNWorldsKernelOccupancy left,
    MiniSNNWorldsKernelOccupancy right)
{
    return (left.blocking_mask & right.category_bits) != UINT32_C(0) ||
           (right.blocking_mask & left.category_bits) != UINT32_C(0);
}
static MiniSNNWorldsKernelSpaceBounds default_space_bounds(void)
{
    MiniSNNWorldsKernelSpaceBounds bounds;

    bounds.min_x = INT64_C(-1000000);
    bounds.min_y = INT64_C(-1000000);
    bounds.max_x = INT64_C(1000000);
    bounds.max_y = INT64_C(1000000);
    return bounds;
}

static int scalar_add_checked(
    MiniSNNWorldsKernelScalar left,
    MiniSNNWorldsKernelScalar right,
    MiniSNNWorldsKernelScalar *out_result)
{
    if (out_result == NULL ||
        (right > 0 && left > INT64_MAX - right) ||
        (right < 0 && left < INT64_MIN - right))
    {
        return 0;
    }
    *out_result = left + right;
    return 1;
}

static int scalar_subtract_checked(
    MiniSNNWorldsKernelScalar left,
    MiniSNNWorldsKernelScalar right,
    MiniSNNWorldsKernelScalar *out_result)
{
    if (right == INT64_MIN)
    {
        if (left >= 0)
        {
            return 0;
        }
        *out_result = left - right;
        return 1;
    }
    return scalar_add_checked(left, -right, out_result);
}

static int space_bounds_are_valid(MiniSNNWorldsKernelSpaceBounds bounds)
{
    MiniSNNWorldsKernelScalar unused;

    if (bounds.min_x >= bounds.max_x || bounds.min_y >= bounds.max_y)
    {
        return 0;
    }
    /* Width overflow is observable to callers only through checked arithmetic. */
    (void)scalar_subtract_checked(bounds.max_x, bounds.min_x, &unused);
    (void)scalar_subtract_checked(bounds.max_y, bounds.min_y, &unused);
    return 1;
}

static int transform_orientation_is_valid(MiniSNNWorldsKernelTransform transform)
{
    return transform.orientation < MINISNN_WORLDS_KERNEL_ORIENTATION_FULL_TURN;
}

static int transform_is_within_bounds(
    MiniSNNWorldsKernelTransform transform,
    MiniSNNWorldsKernelSpaceBounds bounds)
{
    return transform.position.x >= bounds.min_x && transform.position.x <= bounds.max_x &&
           transform.position.y >= bounds.min_y && transform.position.y <= bounds.max_y;
}

static MiniSNNWorldsKernelSpaceBounds config_space_bounds(
    const MiniSNNWorldsKernelConfig *config)
{
    const unsigned char *bytes = (const unsigned char *)config;
    uint32_t struct_size;
    MiniSNNWorldsKernelSpaceBounds bounds;

    memcpy(&struct_size, bytes + offsetof(MiniSNNWorldsKernelConfig, struct_size),
           sizeof(struct_size));
    if (struct_size <
        (uint32_t)MINISNN_WORLDS_KERNEL_CONFIG_SPACE_BOUNDS_READABLE_SIZE)
    {
        return default_space_bounds();
    }
    memcpy(&bounds, bytes + offsetof(MiniSNNWorldsKernelConfig, space_bounds),
           sizeof(bounds));
    return bounds;
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
    return format_version == MINISNN_WORLDS_KERNEL_CONFIG_VERSION &&
           space_bounds_are_valid(config_space_bounds(config));
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

void *minisnn_worlds_kernel_internal_allocate(size_t size)
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
    copy = minisnn_worlds_kernel_internal_allocate(capacity * element_size);
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

static int find_entity_record_index(
    const EntityRecord *entities,
    size_t entity_count,
    MiniSNNWorldsKernelEntityId entity_id,
    size_t *out_index)
{
    size_t index;

    if (entities == NULL || out_index == NULL || entity_id.value == UINT64_C(0))
    {
        return 0;
    }
    for (index = 0U; index < entity_count; ++index)
    {
        if (entities[index].entity_id.value == entity_id.value)
        {
            *out_index = index;
            return 1;
        }
    }
    return 0;
}

static void subtree_move_plan_destroy(SubtreeMovePlan *plan)
{
    if (plan != NULL)
    {
        free(plan->members);
        free(plan->member_flags);
        memset(plan, 0, sizeof(*plan));
    }
}

/* Collects a parent-first breadth-first traversal from canonically sorted links. */
static MiniSNNWorldsKernelError collect_spatial_subtree(
    const EntityRecord *entities,
    size_t entity_count,
    const SpatialLinkRecord *links,
    size_t link_count,
    MiniSNNWorldsKernelEntityId root,
    SubtreeMovePlan *out_plan)
{
    size_t root_index;
    size_t member_index;

    if (entities == NULL || out_plan == NULL || entity_count == 0U ||
        !find_entity_record_index(entities, entity_count, root, &root_index))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }
    if (entity_count > SIZE_MAX / sizeof(*out_plan->members))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    memset(out_plan, 0, sizeof(*out_plan));
    out_plan->members = minisnn_worlds_kernel_internal_allocate(entity_count * sizeof(*out_plan->members));
    if (out_plan->members == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    out_plan->member_flags = minisnn_worlds_kernel_internal_allocate(entity_count * sizeof(*out_plan->member_flags));
    if (out_plan->member_flags == NULL)
    {
        subtree_move_plan_destroy(out_plan);
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    memset(out_plan->member_flags, 0, entity_count * sizeof(*out_plan->member_flags));
    if (entities[root_index].alive == 0 || entities[root_index].has_transform == 0)
    {
        subtree_move_plan_destroy(out_plan);
        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }
    out_plan->members[0U].entity = root;
    out_plan->members[0U].immediate_parent.value = UINT64_C(0);
    out_plan->members[0U].entity_index = root_index;
    out_plan->members[0U].previous_transform = entities[root_index].transform;
    out_plan->count = 1U;
    out_plan->capacity = entity_count;
    out_plan->member_flags[root_index] = UINT8_C(1);

    for (member_index = 0U; member_index < out_plan->count; ++member_index)
    {
        size_t link_index;
        MiniSNNWorldsKernelEntityId parent = out_plan->members[member_index].entity;

        for (link_index = 0U; link_index < link_count; ++link_index)
        {
            size_t child_index;
            SpatialLinkRecord parent_link;

            if (links[link_index].parent.value != parent.value)
            {
                continue;
            }
            if (!find_entity_record_index(entities, entity_count, links[link_index].child,
                                          &child_index) ||
                entities[child_index].alive == 0 || entities[child_index].has_transform == 0 ||
                !find_spatial_parent_link(links, link_count, links[link_index].child,
                                          &parent_link) ||
                parent_link.parent.value != parent.value ||
                out_plan->member_flags[child_index] != UINT8_C(0) ||
                out_plan->count >= out_plan->capacity)
            {
                subtree_move_plan_destroy(out_plan);
                return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
            }
            out_plan->members[out_plan->count].entity = links[link_index].child;
            out_plan->members[out_plan->count].immediate_parent = parent;
            out_plan->members[out_plan->count].entity_index = child_index;
            out_plan->members[out_plan->count].previous_transform =
                entities[child_index].transform;
            out_plan->member_flags[child_index] = UINT8_C(1);
            ++out_plan->count;
        }
    }
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static MiniSNNWorldsKernelError reserve_step_events(
    StepPlan *plan,
    size_t additional_count)
{
    size_t required_count;
    size_t capacity;
    MiniSNNWorldsKernelEvent *expanded_events;

    if (plan == NULL || additional_count > SIZE_MAX - plan->next_event_count ||
        additional_count > UINT64_MAX || plan->next_event_id.value == UINT64_C(0) ||
        (uint64_t)additional_count > UINT64_MAX - plan->next_event_id.value)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_IDENTIFIER_OVERFLOW;
    }
    required_count = plan->next_event_count + additional_count;
    if (required_count <= plan->next_event_capacity)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NONE;
    }
    if (!next_capacity(plan->next_event_capacity, required_count,
                       sizeof(*plan->next_events), &capacity))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    expanded_events = allocate_expanded_copy(plan->next_events, plan->next_event_count,
                                              capacity, sizeof(*plan->next_events));
    if (expanded_events == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    free(plan->next_events);
    plan->next_events = expanded_events;
    plan->next_event_capacity = capacity;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static MiniSNNWorldsKernelError append_step_event(
    StepPlan *plan,
    MiniSNNWorldsKernelEvent event)
{
    MiniSNNWorldsKernelError error;

    error = reserve_step_events(plan, 1U);
    if (error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return error;
    }
    event.event_id = plan->next_event_id;
    plan->next_events[plan->next_event_count] = event;
    ++plan->next_event_count;
    ++plan->next_event_id.value;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}
static int spatial_link_compare(
    MiniSNNWorldsKernelEntityId left_parent,
    MiniSNNWorldsKernelEntityId left_child,
    MiniSNNWorldsKernelEntityId right_parent,
    MiniSNNWorldsKernelEntityId right_child)
{
    if (left_parent.value < right_parent.value)
    {
        return -1;
    }
    if (left_parent.value > right_parent.value)
    {
        return 1;
    }
    if (left_child.value < right_child.value)
    {
        return -1;
    }
    if (left_child.value > right_child.value)
    {
        return 1;
    }
    return 0;
}

static size_t spatial_link_insert_index(
    const SpatialLinkRecord *links,
    size_t link_count,
    MiniSNNWorldsKernelEntityId parent,
    MiniSNNWorldsKernelEntityId child,
    int *out_exists)
{
    size_t index;

    for (index = 0U; index < link_count; ++index)
    {
        int comparison = spatial_link_compare(links[index].parent, links[index].child,
                                              parent, child);

        if (comparison >= 0)
        {
            *out_exists = comparison == 0;
            return index;
        }
    }
    *out_exists = 0;
    return link_count;
}

static int find_spatial_link(
    const SpatialLinkRecord *links,
    size_t link_count,
    MiniSNNWorldsKernelEntityId parent,
    MiniSNNWorldsKernelEntityId child,
    size_t *out_index)
{
    int exists;
    size_t index;

    if (out_index == NULL)
    {
        return 0;
    }
    index = spatial_link_insert_index(links, link_count, parent, child, &exists);
    if (exists != 0)
    {
        *out_index = index;
        return 1;
    }
    return 0;
}

static int find_spatial_parent_link(
    const SpatialLinkRecord *links,
    size_t link_count,
    MiniSNNWorldsKernelEntityId child,
    SpatialLinkRecord *out_link)
{
    size_t index;

    for (index = 0U; index < link_count; ++index)
    {
        if (links[index].child.value == child.value)
        {
            if (out_link != NULL)
            {
                *out_link = links[index];
            }
            return 1;
        }
    }
    return 0;
}

static int spatial_link_has_child(
    const SpatialLinkRecord *links,
    size_t link_count,
    MiniSNNWorldsKernelEntityId parent,
    MiniSNNWorldsKernelEntityId *out_smallest_child)
{
    size_t index;
    int found = 0;

    if (out_smallest_child != NULL)
    {
        out_smallest_child->value = UINT64_C(0);
    }
    for (index = 0U; index < link_count; ++index)
    {
        if (links[index].parent.value == parent.value)
        {
            if (out_smallest_child != NULL &&
                (found == 0 || links[index].child.value < out_smallest_child->value))
            {
                *out_smallest_child = links[index].child;
            }
            found = 1;
        }
    }
    return found;
}

static int find_incident_spatial_link_endpoint(
    const SpatialLinkRecord *links,
    size_t link_count,
    MiniSNNWorldsKernelEntityId entity,
    MiniSNNWorldsKernelEntityId *out_related_entity)
{
    size_t index;
    int found = 0;

    if (out_related_entity == NULL)
    {
        return 0;
    }
    out_related_entity->value = UINT64_C(0);
    for (index = 0U; index < link_count; ++index)
    {
        MiniSNNWorldsKernelEntityId counterpart;

        if (links[index].parent.value == entity.value)
        {
            counterpart = links[index].child;
        }
        else if (links[index].child.value == entity.value)
        {
            counterpart = links[index].parent;
        }
        else
        {
            continue;
        }
        if (found == 0 || counterpart.value < out_related_entity->value)
        {
            *out_related_entity = counterpart;
        }
        found = 1;
    }
    return found;
}

/* Returns 1 for a requested cycle, 0 when the chain reaches a root, and -1 for corruption. */
static int spatial_link_would_create_cycle(
    const SpatialLinkRecord *links,
    size_t link_count,
    size_t entity_count,
    MiniSNNWorldsKernelEntityId parent,
    MiniSNNWorldsKernelEntityId child)
{
    MiniSNNWorldsKernelEntityId current = parent;
    size_t visited;

    for (visited = 0U; visited < entity_count; ++visited)
    {
        SpatialLinkRecord parent_link;

        if (current.value == child.value)
        {
            return 1;
        }
        if (!find_spatial_parent_link(links, link_count, current, &parent_link))
        {
            return 0;
        }
        current = parent_link.parent;
    }
    return -1;
}

static void insert_spatial_link(
    SpatialLinkRecord *links,
    size_t *in_out_count,
    size_t index,
    SpatialLinkRecord link)
{
    memmove(&links[index + 1U], &links[index],
            (*in_out_count - index) * sizeof(*links));
    links[index] = link;
    ++*in_out_count;
}

static void remove_spatial_link(
    SpatialLinkRecord *links,
    size_t *in_out_count,
    size_t index)
{
    memmove(&links[index], &links[index + 1U],
            (*in_out_count - index - 1U) * sizeof(*links));
    --*in_out_count;
}
static int find_occupancy_conflict(
    const EntityRecord *entities,
    size_t entity_count,
    MiniSNNWorldsKernelEntityId candidate_entity,
    MiniSNNWorldsKernelTransform candidate_transform,
    MiniSNNWorldsKernelOccupancy candidate_occupancy,
    MiniSNNWorldsKernelEntityId *out_related_entity)
{
    MiniSNNWorldsKernelSpaceBounds candidate_envelope;
    size_t index;

    if (out_related_entity == NULL ||
        !occupancy_envelope(candidate_transform, candidate_occupancy,
                            &candidate_envelope))
    {
        return 0;
    }
    out_related_entity->value = UINT64_C(0);
    for (index = 0U; index < entity_count; ++index)
    {
        const EntityRecord *record = &entities[index];
        MiniSNNWorldsKernelSpaceBounds existing_envelope;

        if (record->entity_id.value == candidate_entity.value ||
            record->alive == 0 || record->has_transform == 0 ||
            record->has_occupancy == 0 ||
            !occupancies_block_each_other(candidate_occupancy, record->occupancy) ||
            !occupancy_envelope(record->transform, record->occupancy,
                                &existing_envelope) ||
            !aabb_has_positive_overlap(candidate_envelope, existing_envelope))
        {
            continue;
        }
        if (out_related_entity->value == UINT64_C(0) ||
            record->entity_id.value < out_related_entity->value)
        {
            *out_related_entity = record->entity_id;
        }
    }
    return out_related_entity->value != UINT64_C(0);
}
static int uint64_addition_fits(uint64_t value, uint64_t addition)
{
    return addition <= UINT64_MAX - value;
}

static MiniSNNWorldsKernelError validate_step_counter_promotion(
    const MiniSNNWorldsKernel *kernel,
    const StepPlan *plan)
{
    if (kernel == NULL || plan == NULL || plan->next_event_count > UINT64_MAX ||
        !uint64_addition_fits(kernel->total_entities_created, plan->created_count) ||
        !uint64_addition_fits(kernel->total_entities_destroyed, plan->destroyed_count) ||
        !uint64_addition_fits(kernel->total_entities_placed, plan->placed_count) ||
        !uint64_addition_fits(kernel->total_entities_removed_from_space,
                              plan->removed_from_space_count) ||
        !uint64_addition_fits(kernel->total_occupancies_set, plan->occupancies_set_count) ||
        !uint64_addition_fits(kernel->total_occupancies_cleared,
                              plan->occupancies_cleared_count) ||
        !uint64_addition_fits(kernel->total_occupancy_conflicts_rejected,
                              plan->occupancy_conflicts_rejected_count) ||
        !uint64_addition_fits(kernel->total_movement_commands_processed,
                              plan->movement_commands_processed_count) ||
        !uint64_addition_fits(kernel->total_entities_moved, plan->moved_count) ||
        !uint64_addition_fits(kernel->total_movement_overflows_rejected,
                              plan->movement_overflows_rejected_count) ||
        !uint64_addition_fits(kernel->total_spatial_links_created,
                              plan->spatial_links_created_count) ||
        !uint64_addition_fits(kernel->total_spatial_links_removed,
                              plan->spatial_links_removed_count) ||
        !uint64_addition_fits(kernel->total_spatial_link_commands_processed,
                              plan->spatial_link_commands_processed_count) ||
        !uint64_addition_fits(kernel->total_commands_applied, plan->applied_count) ||
        !uint64_addition_fits(kernel->total_commands_rejected, plan->rejected_count) ||
        !uint64_addition_fits(kernel->total_events_emitted,
                              (uint64_t)plan->next_event_count))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static MiniSNNWorldsKernelError validate_spatial_state(
    const EntityRecord *entities,
    size_t entity_count,
    size_t alive_entity_count,
    size_t placed_entity_count,
    size_t entities_with_occupancy,
    size_t active_occupancies,
    size_t blocking_occupancies,
    const SpatialLinkRecord *links,
    size_t link_count,
    MiniSNNWorldsKernelSpaceBounds bounds)
{
    size_t index;
    size_t alive_count = 0U;
    size_t placed_count = 0U;
    size_t occupancy_count = 0U;
    size_t active_occupancy_count = 0U;
    size_t blocking_occupancy_count = 0U;

    if ((entity_count != 0U && entities == NULL) || (link_count != 0U && links == NULL))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }
    for (index = 0U; index < entity_count; ++index)
    {
        const EntityRecord *record = &entities[index];

        if (record->entity_id.value == UINT64_C(0) ||
            (index != 0U && entities[index - 1U].entity_id.value >= record->entity_id.value))
        {
            return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
        }
        if (record->alive == 0)
        {
            if (record->has_transform != 0 || record->has_occupancy != 0)
            {
                return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
            }
            continue;
        }
        ++alive_count;
        if (record->has_transform != 0)
        {
            ++placed_count;
            if (!transform_orientation_is_valid(record->transform) ||
                !transform_is_within_bounds(record->transform, bounds))
            {
                return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
            }
        }
        if (record->has_occupancy != 0)
        {
            ++occupancy_count;
            if (!occupancy_is_valid(record->occupancy) ||
                !occupancy_fits_space(record->occupancy, bounds))
            {
                return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
            }
            if (record->has_transform != 0)
            {
                MiniSNNWorldsKernelEntityId conflict;

                ++active_occupancy_count;
                if (record->occupancy.blocking_mask != UINT32_C(0))
                {
                    ++blocking_occupancy_count;
                }
                if (!occupancy_envelope_is_within_bounds(record->transform,
                                                         record->occupancy, bounds) ||
                    find_occupancy_conflict(entities, entity_count, record->entity_id,
                                            record->transform, record->occupancy, &conflict))
                {
                    return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
                }
            }
        }
    }
    if (alive_count != alive_entity_count || placed_count != placed_entity_count ||
        occupancy_count != entities_with_occupancy ||
        active_occupancy_count != active_occupancies ||
        blocking_occupancy_count != blocking_occupancies)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }
    for (index = 0U; index < link_count; ++index)
    {
        const SpatialLinkRecord *link = &links[index];
        size_t parent_index;
        size_t child_index;
        MiniSNNWorldsKernelScalar expected_x;
        MiniSNNWorldsKernelScalar expected_y;
        size_t other_index;

        if (link->parent.value == UINT64_C(0) || link->child.value == UINT64_C(0) ||
            link->parent.value == link->child.value ||
            (index != 0U && spatial_link_compare(links[index - 1U].parent,
                                                  links[index - 1U].child,
                                                  link->parent, link->child) >= 0) ||
            !find_entity_record_index(entities, entity_count, link->parent, &parent_index) ||
            !find_entity_record_index(entities, entity_count, link->child, &child_index) ||
            entities[parent_index].alive == 0 || entities[child_index].alive == 0 ||
            entities[parent_index].has_transform == 0 || entities[child_index].has_transform == 0 ||
            !scalar_subtract_checked(entities[child_index].transform.position.x,
                                     entities[parent_index].transform.position.x, &expected_x) ||
            !scalar_subtract_checked(entities[child_index].transform.position.y,
                                     entities[parent_index].transform.position.y, &expected_y) ||
            expected_x != link->offset_x || expected_y != link->offset_y ||
            spatial_link_would_create_cycle(links, link_count, entity_count,
                                            link->parent, link->child) != 0)
        {
            return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
        }
        for (other_index = index + 1U; other_index < link_count; ++other_index)
        {
            if (links[other_index].child.value == link->child.value)
            {
                return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
            }
        }
    }
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_internal_validate_invariants(
    const MiniSNNWorldsKernel *kernel)
{
    size_t index;
    MiniSNNWorldsKernelError error;

    if (kernel == NULL || (kernel->state != MINISNN_WORLDS_KERNEL_STATE_READY &&
                           kernel->state != MINISNN_WORLDS_KERNEL_STATE_STEPPING) ||
        !space_bounds_are_valid(kernel->space_bounds) ||
        kernel->next_entity_id.value == UINT64_C(0) ||
        kernel->next_command_id.value == UINT64_C(0) ||
        kernel->next_event_id.value == UINT64_C(0))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (kernel->command_batch_active != 0)
    {
        if (kernel->command_batch_begin_tick > kernel->tick ||
            kernel->tick - kernel->command_batch_begin_tick > UINT64_C(1) ||
            kernel->command_batch_next_command_id.value == UINT64_C(0))
        {
            return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
        }
        if (kernel->tick == kernel->command_batch_begin_tick &&
            kernel->command_batch_pending_command_count > kernel->pending_command_count)
        {
            return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
        }
    }
    error = validate_spatial_state(kernel->entities, kernel->entity_count,
                                   kernel->alive_entity_count, kernel->placed_entity_count,
                                   kernel->entities_with_occupancy, kernel->active_occupancies,
                                   kernel->blocking_occupancies, kernel->spatial_links,
                                   kernel->spatial_link_count, kernel->space_bounds);
    if (error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }

    /*
     * These relations describe the live Kernel state. The import boundary
     * validates stronger next-ID provenance rules, but those must remain
     * outside this general validator so identifier-overflow test hooks can
     * exercise their public preflight paths.
     */
    if (kernel->total_entities_created < kernel->total_entities_destroyed ||
        kernel->total_entities_created != (uint64_t)kernel->entity_count ||
        kernel->total_entities_created - kernel->total_entities_destroyed !=
            (uint64_t)kernel->alive_entity_count ||
        kernel->total_spatial_links_created < kernel->total_spatial_links_removed ||
        kernel->total_spatial_links_created - kernel->total_spatial_links_removed !=
            (uint64_t)kernel->spatial_link_count ||
        kernel->total_commands_submitted < (uint64_t)kernel->pending_command_count ||
        !uint64_addition_fits(kernel->total_commands_applied,
                              kernel->total_commands_rejected) ||
        kernel->total_commands_submitted - (uint64_t)kernel->pending_command_count !=
            kernel->total_commands_applied + kernel->total_commands_rejected)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }

    if (kernel->last_tick_event_count != 0U && kernel->last_tick_events == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }
    for (index = 0U; index < kernel->last_tick_event_count; ++index)
    {
        const MiniSNNWorldsKernelEvent *event = &kernel->last_tick_events[index];

        if (event->event_id.value == UINT64_C(0) || event->tick != kernel->tick ||
            (index != 0U && event->event_id.value !=
                                kernel->last_tick_events[index - 1U].event_id.value + UINT64_C(1)))
        {
            return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
        }
        if (event->type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED && index != 0U &&
            event->command_id.value == kernel->last_tick_events[index - 1U].command_id.value)
        {
            size_t earlier;
            int parent_seen = 0;

            if (event->related_entity.value == UINT64_C(0))
            {
                return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
            }
            for (earlier = 0U; earlier < index; ++earlier)
            {
                if (kernel->last_tick_events[earlier].command_id.value == event->command_id.value &&
                    kernel->last_tick_events[earlier].subject.value == event->related_entity.value)
                {
                    parent_seen = 1;
                    break;
                }
            }
            if (parent_seen == 0)
            {
                return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
            }
        }
    }
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

#ifdef MINISNN_WORLDS_KERNEL_C3_INVARIANT_TESTING
static MiniSNNWorldsKernelError validate_planned_invariants(
    const MiniSNNWorldsKernel *kernel,
    const StepPlan *plan)
{
    if (kernel == NULL || plan == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    return validate_spatial_state(plan->planned_entities, plan->planned_entity_count,
                                  plan->planned_alive_entity_count,
                                  plan->planned_placed_entity_count,
                                  plan->planned_entities_with_occupancy,
                                  plan->planned_active_occupancies,
                                  plan->planned_blocking_occupancies,
                                  plan->planned_spatial_links,
                                  plan->planned_spatial_link_count,
                                  kernel->space_bounds);
}
#endif
static void select_smallest_entity(
    MiniSNNWorldsKernelEntityId candidate,
    MiniSNNWorldsKernelEntityId *in_out_selected)
{
    if (in_out_selected->value == UINT64_C(0) ||
        candidate.value < in_out_selected->value)
    {
        *in_out_selected = candidate;
    }
}

static MiniSNNWorldsKernelError calculate_subtree_destinations(
    SubtreeMovePlan *subtree,
    const EntityRecord *entities,
    MiniSNNWorldsKernelPosition displacement,
    MiniSNNWorldsKernelSpaceBounds bounds,
    MiniSNNWorldsKernelCommandRejection *out_rejection,
    MiniSNNWorldsKernelEntityId *out_affected_entity)
{
    size_t index;
    MiniSNNWorldsKernelEntityId overflow_entity = { UINT64_C(0) };
    MiniSNNWorldsKernelEntityId position_entity = { UINT64_C(0) };
    MiniSNNWorldsKernelEntityId occupancy_bounds_entity = { UINT64_C(0) };

    if (subtree == NULL || entities == NULL || out_rejection == NULL ||
        out_affected_entity == NULL || subtree->count == 0U)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }
    *out_rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_NONE;
    out_affected_entity->value = UINT64_C(0);
    for (index = 0U; index < subtree->count; ++index)
    {
        const EntityRecord *record = &entities[subtree->members[index].entity_index];
        MiniSNNWorldsKernelTransform destination = subtree->members[index].previous_transform;
        MiniSNNWorldsKernelSpaceBounds envelope;

        if (record->alive == 0 || record->has_transform == 0 ||
            record->entity_id.value != subtree->members[index].entity.value ||
            !transform_orientation_is_valid(destination))
        {
            return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
        }
        if (!scalar_add_checked(destination.position.x, displacement.x,
                                &destination.position.x) ||
            !scalar_add_checked(destination.position.y, displacement.y,
                                &destination.position.y))
        {
            select_smallest_entity(record->entity_id, &overflow_entity);
            continue;
        }
        subtree->members[index].destination_transform = destination;
        if (!transform_is_within_bounds(destination, bounds))
        {
            select_smallest_entity(record->entity_id, &position_entity);
        }
        if (record->has_occupancy != 0)
        {
            if (!occupancy_envelope(destination, record->occupancy, &envelope))
            {
                select_smallest_entity(record->entity_id, &overflow_entity);
            }
            else if (envelope.min_x < bounds.min_x || envelope.max_x > bounds.max_x ||
                     envelope.min_y < bounds.min_y || envelope.max_y > bounds.max_y)
            {
                select_smallest_entity(record->entity_id, &occupancy_bounds_entity);
            }
        }
    }
    if (overflow_entity.value != UINT64_C(0))
    {
        *out_rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_DESTINATION_OVERFLOW;
        *out_affected_entity = overflow_entity;
    }
    else if (position_entity.value != UINT64_C(0))
    {
        *out_rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_POSITION_OUT_OF_BOUNDS;
        *out_affected_entity = position_entity;
    }
    else if (occupancy_bounds_entity.value != UINT64_C(0))
    {
        *out_rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_OUT_OF_BOUNDS;
        *out_affected_entity = occupancy_bounds_entity;
    }
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static MiniSNNWorldsKernelError find_subtree_external_occupancy_conflict(
    const EntityRecord *entities,
    size_t entity_count,
    const SubtreeMovePlan *subtree,
    MiniSNNWorldsKernelEntityId *out_blocker,
    MiniSNNWorldsKernelEntityId *out_affected_entity,
    int *out_found)
{
    size_t external_index;
    MiniSNNWorldsKernelEntityId blocker = { UINT64_C(0) };
    MiniSNNWorldsKernelEntityId affected = { UINT64_C(0) };

    if (entities == NULL || subtree == NULL || subtree->member_flags == NULL ||
        out_blocker == NULL || out_affected_entity == NULL || out_found == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }
    for (external_index = 0U; external_index < entity_count; ++external_index)
    {
        const EntityRecord *external = &entities[external_index];
        MiniSNNWorldsKernelSpaceBounds external_envelope;
        size_t member_index;

        if (subtree->member_flags[external_index] != UINT8_C(0) || external->alive == 0 ||
            external->has_transform == 0 || external->has_occupancy == 0)
        {
            continue;
        }
        if (!occupancy_envelope(external->transform, external->occupancy, &external_envelope))
        {
            return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
        }
        for (member_index = 0U; member_index < subtree->count; ++member_index)
        {
            const EntityRecord *member =
                &entities[subtree->members[member_index].entity_index];
            MiniSNNWorldsKernelSpaceBounds destination_envelope;

            if (member->has_occupancy == 0 ||
                !occupancies_block_each_other(member->occupancy, external->occupancy))
            {
                continue;
            }
            if (!occupancy_envelope(subtree->members[member_index].destination_transform,
                                    member->occupancy, &destination_envelope))
            {
                return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
            }
            if (!aabb_has_positive_overlap(destination_envelope, external_envelope))
            {
                continue;
            }
            if (blocker.value == UINT64_C(0) || external->entity_id.value < blocker.value ||
                (external->entity_id.value == blocker.value &&
                 member->entity_id.value < affected.value))
            {
                blocker = external->entity_id;
                affected = member->entity_id;
            }
        }
    }
    *out_blocker = blocker;
    *out_affected_entity = affected;
    *out_found = blocker.value != UINT64_C(0);
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
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
        free(plan->planned_spatial_links);
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
    size_t potential_spatial_link_create_count = 0U;
    size_t ordered_index = 0U;
    size_t capacity;

    memset(out_plan, 0, sizeof(*out_plan));
#ifdef MINISNN_WORLDS_KERNEL_C3_INVARIANT_TESTING
    if (minisnn_worlds_kernel_internal_validate_invariants(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {

        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }
#endif
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
            else if (kernel->pending_commands[index].type ==
                     MINISNN_WORLDS_KERNEL_COMMAND_CREATE_SPATIAL_LINK)
            {
                ++potential_spatial_link_create_count;
            }
        }
    }
    if (!identifiers_available(kernel->next_event_id.value, due_count))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_IDENTIFIER_OVERFLOW;
    }
    if (kernel->entity_count > SIZE_MAX - potential_create_count ||
        kernel->spatial_link_count > SIZE_MAX - potential_spatial_link_create_count)
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
        out_plan->ordered_commands = minisnn_worlds_kernel_internal_allocate(
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
        out_plan->next_events = minisnn_worlds_kernel_internal_allocate(due_count * sizeof(*out_plan->next_events));
        if (out_plan->next_events == NULL)
        {
            step_plan_destroy(out_plan);
            return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
        }
        out_plan->next_event_capacity = due_count;
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
    if (due_count != 0U &&
        (kernel->spatial_link_count != 0U || potential_spatial_link_create_count != 0U))
    {
        if (!next_capacity(kernel->spatial_link_capacity,
                           kernel->spatial_link_count + potential_spatial_link_create_count,
                           sizeof(*out_plan->planned_spatial_links), &capacity))
        {
            step_plan_destroy(out_plan);
            return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
        }
        out_plan->planned_spatial_links = allocate_expanded_copy(
            kernel->spatial_links, kernel->spatial_link_count, capacity,
            sizeof(*out_plan->planned_spatial_links));
        if (out_plan->planned_spatial_links == NULL)
        {
            step_plan_destroy(out_plan);
            return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
        }
        out_plan->planned_spatial_link_capacity = capacity;
    }
    out_plan->planned_entity_count = kernel->entity_count;
    out_plan->planned_spatial_link_count = kernel->spatial_link_count;
    out_plan->planned_alive_entity_count = kernel->alive_entity_count;
    out_plan->planned_placed_entity_count = kernel->placed_entity_count;
    out_plan->planned_entities_with_occupancy = kernel->entities_with_occupancy;
    out_plan->planned_active_occupancies = kernel->active_occupancies;
    out_plan->planned_blocking_occupancies = kernel->blocking_occupancies;
    out_plan->next_entity_id = kernel->next_entity_id;
    out_plan->next_event_id = kernel->next_event_id;
    for (index = 0U; index < due_count; ++index)
    {
        MiniSNNWorldsKernelCommandInfo *command = &out_plan->ordered_commands[index];
        MiniSNNWorldsKernelEvent event_storage;
        MiniSNNWorldsKernelEvent *event = &event_storage;
        MiniSNNWorldsKernelCommandRejection rejection =
            MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_NONE;
        MiniSNNWorldsKernelEventType event_type =
            MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED;
        MiniSNNWorldsKernelEntityId subject = command->target_entity;
        MiniSNNWorldsKernelError event_error;
        int event_appended = 0;

        memset(event, 0, sizeof(*event));
        event->tick = next_tick;
        event->command_id = command->command_id;
        event->issuer = command->issuer;
        event->previous_transform = zero_transform();
        event->transform = zero_transform();
        event->displacement = zero_position();
        event->occupancy = zero_occupancy();
        event->spatial_link = zero_spatial_link();
        event->affected_entity.value = UINT64_C(0);
        if ((command->type == MINISNN_WORLDS_KERNEL_COMMAND_CREATE_SPATIAL_LINK ||
             command->type == MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_SPATIAL_LINK) &&
            command->has_spatial_link_endpoints)
        {
            event->related_entity = command->spatial_link_endpoints.child;
        }
        if (command->type == MINISNN_WORLDS_KERNEL_COMMAND_MOVE_ENTITY)
        {
            ++out_plan->movement_commands_processed_count;
        }
        if (command->type == MINISNN_WORLDS_KERNEL_COMMAND_CREATE_SPATIAL_LINK ||
            command->type == MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_SPATIAL_LINK)
        {
            if (out_plan->spatial_link_commands_processed_count >=
                UINT64_MAX - kernel->total_spatial_link_commands_processed)
            {
                step_plan_destroy(out_plan);
                return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
            }
            ++out_plan->spatial_link_commands_processed_count;
        }
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
            memset(record, 0, sizeof(*record));
            record->entity_id = out_plan->next_entity_id;
            record->creation_tick = next_tick;
            record->destruction_tick = MINISNN_WORLDS_TICK_INITIAL;
            record->alive = 1;
            record->transform = zero_transform();
            record->occupancy = zero_occupancy();
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
            else if (find_incident_spatial_link_endpoint(
                         out_plan->planned_spatial_links,
                         out_plan->planned_spatial_link_count,
                         command->target_entity, &event->related_entity))
            {
                rejection =
                    MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_HAS_SPATIAL_LINKS;
            }
            else
            {
                int was_active_occupancy =
                    record->has_transform != 0 && record->has_occupancy != 0;

                if (record->has_transform != 0)
                {
                    event->has_transform = true;
                    event->transform = record->transform;
                    record->has_transform = 0;
                    record->transform = zero_transform();
                    --out_plan->planned_placed_entity_count;
                }
                if (record->has_occupancy != 0)
                {
                    if (was_active_occupancy != 0)
                    {
                        --out_plan->planned_active_occupancies;
                        if (record->occupancy.blocking_mask != UINT32_C(0))
                        {
                            --out_plan->planned_blocking_occupancies;
                        }
                    }
                    --out_plan->planned_entities_with_occupancy;
                    record->has_occupancy = 0;
                    record->occupancy = zero_occupancy();
                }
                record->alive = 0;
                record->destruction_tick = next_tick;
                --out_plan->planned_alive_entity_count;
                ++out_plan->destroyed_count;
                ++out_plan->applied_count;
                event_type = MINISNN_WORLDS_KERNEL_EVENT_ENTITY_DESTROYED;
            }
        }
        else if (command->type == MINISNN_WORLDS_KERNEL_COMMAND_PLACE_ENTITY)
        {
            EntityRecord *record = find_entity_record(
                out_plan->planned_entities, out_plan->planned_entity_count,
                command->target_entity);

            if (record == NULL || record->alive == 0)
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_NOT_ALIVE;
            }
            else if (record->has_transform != 0)
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_ALREADY_PLACED;
            }
            else if (!transform_orientation_is_valid(command->transform))
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_INVALID_ORIENTATION;
            }
            else if (!transform_is_within_bounds(command->transform, kernel->space_bounds))
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_POSITION_OUT_OF_BOUNDS;
            }
            else if (record->has_occupancy != 0 &&
                     !occupancy_envelope_is_within_bounds(
                         command->transform, record->occupancy, kernel->space_bounds))
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_OUT_OF_BOUNDS;
            }
            else if (record->has_occupancy != 0 &&
                     find_occupancy_conflict(out_plan->planned_entities,
                                             out_plan->planned_entity_count,
                                             record->entity_id, command->transform,
                                             record->occupancy, &event->related_entity))
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_CONFLICT;
                ++out_plan->occupancy_conflicts_rejected_count;
            }
            else
            {
                record->has_transform = 1;
                record->transform = command->transform;
                ++out_plan->planned_placed_entity_count;
                if (record->has_occupancy != 0)
                {
                    ++out_plan->planned_active_occupancies;
                    if (record->occupancy.blocking_mask != UINT32_C(0))
                    {
                        ++out_plan->planned_blocking_occupancies;
                    }
                }
                ++out_plan->placed_count;
                ++out_plan->applied_count;
                event->has_transform = true;
                event->transform = command->transform;
                event_type = MINISNN_WORLDS_KERNEL_EVENT_ENTITY_PLACED;
            }
        }
        else if (command->type == MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_ENTITY_FROM_SPACE)
        {
            EntityRecord *record = find_entity_record(
                out_plan->planned_entities, out_plan->planned_entity_count,
                command->target_entity);

            if (record == NULL || record->alive == 0)
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_NOT_ALIVE;
            }
            else if (record->has_transform == 0)
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_NOT_PLACED;
            }
            else if (find_incident_spatial_link_endpoint(
                         out_plan->planned_spatial_links,
                         out_plan->planned_spatial_link_count,
                         command->target_entity, &event->related_entity))
            {
                rejection =
                    MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_HAS_SPATIAL_LINKS;
            }
            else
            {
                event->has_transform = true;
                event->transform = record->transform;
                if (record->has_occupancy != 0)
                {
                    --out_plan->planned_active_occupancies;
                    if (record->occupancy.blocking_mask != UINT32_C(0))
                    {
                        --out_plan->planned_blocking_occupancies;
                    }
                }
                record->has_transform = 0;
                record->transform = zero_transform();
                --out_plan->planned_placed_entity_count;
                ++out_plan->removed_from_space_count;
                ++out_plan->applied_count;
                event_type = MINISNN_WORLDS_KERNEL_EVENT_ENTITY_REMOVED_FROM_SPACE;
            }
        }
        else if (command->type == MINISNN_WORLDS_KERNEL_COMMAND_CREATE_SPATIAL_LINK)
        {
            EntityRecord *parent_record;
            EntityRecord *child_record;
            SpatialLinkRecord link;
            size_t insertion_index;
            int exact_link_exists;
            int cycle_result;

            if (!command->has_spatial_link_endpoints ||
                command->target_entity.value !=
                    command->spatial_link_endpoints.parent.value)
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_INVALID_COMMAND;
            }
            else
            {
                parent_record = find_entity_record(
                    out_plan->planned_entities, out_plan->planned_entity_count,
                    command->spatial_link_endpoints.parent);
                child_record = find_entity_record(
                    out_plan->planned_entities, out_plan->planned_entity_count,
                    command->spatial_link_endpoints.child);
                if (parent_record == NULL || parent_record->alive == 0)
                {
                    rejection =
                        MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_PARENT_NOT_ALIVE;
                }
                else if (child_record == NULL || child_record->alive == 0)
                {
                    rejection =
                        MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_CHILD_NOT_ALIVE;
                }
                else if (parent_record->entity_id.value == child_record->entity_id.value)
                {
                    rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_SELF;
                }
                else if (parent_record->has_transform == 0)
                {
                    rejection =
                        MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_PARENT_NOT_PLACED;
                }
                else if (child_record->has_transform == 0)
                {
                    rejection =
                        MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_CHILD_NOT_PLACED;
                }
                else
                {
                    insertion_index = spatial_link_insert_index(
                        out_plan->planned_spatial_links,
                        out_plan->planned_spatial_link_count,
                        parent_record->entity_id, child_record->entity_id,
                        &exact_link_exists);
                    if (exact_link_exists != 0)
                    {
                        rejection =
                            MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_DUPLICATE;
                    }
                    else if (find_spatial_parent_link(
                                 out_plan->planned_spatial_links,
                                 out_plan->planned_spatial_link_count,
                                 child_record->entity_id, NULL))
                    {
                        rejection =
                            MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_CHILD_HAS_PARENT;
                    }
                    else
                    {
                        cycle_result = spatial_link_would_create_cycle(
                            out_plan->planned_spatial_links,
                            out_plan->planned_spatial_link_count,
                            out_plan->planned_entity_count,
                            parent_record->entity_id, child_record->entity_id);
                        if (cycle_result < 0)
                        {
                            step_plan_destroy(out_plan);
                            return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
                        }
                        if (cycle_result != 0)
                        {
                            rejection =
                                MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_CYCLE;
                        }
                        else if (!scalar_subtract_checked(
                                     child_record->transform.position.x,
                                     parent_record->transform.position.x,
                                     &link.offset_x) ||
                                 !scalar_subtract_checked(
                                     child_record->transform.position.y,
                                     parent_record->transform.position.y,
                                     &link.offset_y))
                        {
                            rejection =
                                MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_OFFSET_OVERFLOW;
                        }
                        else if (out_plan->planned_spatial_link_count >=
                                 out_plan->planned_spatial_link_capacity ||
                                 out_plan->spatial_links_created_count >=
                                     UINT64_MAX - kernel->total_spatial_links_created)
                        {
                            step_plan_destroy(out_plan);
                            return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
                        }
                        else
                        {
                            link.parent = parent_record->entity_id;
                            link.child = child_record->entity_id;
                            insert_spatial_link(out_plan->planned_spatial_links,
                                                &out_plan->planned_spatial_link_count,
                                                insertion_index, link);
                            event->has_spatial_link = true;
                            event->spatial_link = link;
                            ++out_plan->spatial_links_created_count;
                            ++out_plan->applied_count;
                            event_type =
                                MINISNN_WORLDS_KERNEL_EVENT_SPATIAL_LINK_CREATED;
                        }
                    }
                }
            }
        }
        else if (command->type == MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_SPATIAL_LINK)
        {
            EntityRecord *parent_record;
            EntityRecord *child_record;
            size_t link_index;

            if (!command->has_spatial_link_endpoints ||
                command->target_entity.value !=
                    command->spatial_link_endpoints.parent.value)
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_INVALID_COMMAND;
            }
            else
            {
                parent_record = find_entity_record(
                    out_plan->planned_entities, out_plan->planned_entity_count,
                    command->spatial_link_endpoints.parent);
                child_record = find_entity_record(
                    out_plan->planned_entities, out_plan->planned_entity_count,
                    command->spatial_link_endpoints.child);
                if (parent_record == NULL || parent_record->alive == 0)
                {
                    rejection =
                        MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_PARENT_NOT_ALIVE;
                }
                else if (child_record == NULL || child_record->alive == 0)
                {
                    rejection =
                        MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_CHILD_NOT_ALIVE;
                }
                else if (parent_record->entity_id.value == child_record->entity_id.value)
                {
                    rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_SELF;
                }
                else if (parent_record->has_transform == 0)
                {
                    rejection =
                        MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_PARENT_NOT_PLACED;
                }
                else if (child_record->has_transform == 0)
                {
                    rejection =
                        MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_CHILD_NOT_PLACED;
                }
                else if (!find_spatial_link(out_plan->planned_spatial_links,
                                            out_plan->planned_spatial_link_count,
                                            parent_record->entity_id, child_record->entity_id,
                                            &link_index))
                {
                    rejection =
                        MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_NOT_FOUND;
                }
                else if (out_plan->spatial_links_removed_count >=
                         UINT64_MAX - kernel->total_spatial_links_removed)
                {
                    step_plan_destroy(out_plan);
                    return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
                }
                else
                {
                    event->has_spatial_link = true;
                    event->spatial_link = out_plan->planned_spatial_links[link_index];
                    remove_spatial_link(out_plan->planned_spatial_links,
                                        &out_plan->planned_spatial_link_count, link_index);
                    ++out_plan->spatial_links_removed_count;
                    ++out_plan->applied_count;
                    event_type = MINISNN_WORLDS_KERNEL_EVENT_SPATIAL_LINK_REMOVED;
                }
            }
        }
        else if (command->type == MINISNN_WORLDS_KERNEL_COMMAND_MOVE_ENTITY)
        {
            EntityRecord *record = find_entity_record(
                out_plan->planned_entities, out_plan->planned_entity_count,
                command->target_entity);
            MiniSNNWorldsKernelTransform destination;
            MiniSNNWorldsKernelSpaceBounds destination_envelope;
            SpatialLinkRecord parent_link;

            if (record == NULL || record->alive == 0)
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_NOT_ALIVE;
            }
            else if (!command->has_displacement)
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_INVALID_COMMAND;
            }
            else if (record->has_transform == 0)
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_NOT_PLACED;
            }
            else if (find_spatial_parent_link(out_plan->planned_spatial_links,
                                              out_plan->planned_spatial_link_count,
                                              record->entity_id, &parent_link))
            {
                rejection =
                    MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_HAS_SPATIAL_PARENT;
                event->related_entity = parent_link.parent;
            }
            else if (command->displacement.x == MINISNN_WORLDS_KERNEL_SCALAR_ZERO &&
                     command->displacement.y == MINISNN_WORLDS_KERNEL_SCALAR_ZERO)
            {
                event->has_previous_transform = true;
                event->previous_transform = record->transform;
                event->has_transform = true;
                event->transform = record->transform;
                event->has_displacement = true;
                event->displacement = command->displacement;
                ++out_plan->applied_count;
                event_type = MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED;
            }
            else if (spatial_link_has_child(out_plan->planned_spatial_links,
                                             out_plan->planned_spatial_link_count,
                                             record->entity_id, NULL))
            {
                SubtreeMovePlan subtree;
                MiniSNNWorldsKernelError subtree_error;
                MiniSNNWorldsKernelCommandRejection subtree_rejection;
                MiniSNNWorldsKernelEntityId affected_entity;
                MiniSNNWorldsKernelEntityId blocker;
                int has_conflict;
                uint64_t moved_member_count;
                size_t member_index;

                memset(&subtree, 0, sizeof(subtree));
                subtree_error = collect_spatial_subtree(
                    out_plan->planned_entities, out_plan->planned_entity_count,
                    out_plan->planned_spatial_links, out_plan->planned_spatial_link_count,
                    record->entity_id, &subtree);
                if (subtree_error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
                {
                    subtree_move_plan_destroy(&subtree);
                    step_plan_destroy(out_plan);
                    return subtree_error;
                }
                subtree_error = calculate_subtree_destinations(
                    &subtree, out_plan->planned_entities, command->displacement,
                    kernel->space_bounds, &subtree_rejection, &affected_entity);
                if (subtree_error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
                {
                    subtree_move_plan_destroy(&subtree);
                    step_plan_destroy(out_plan);
                    return subtree_error;
                }
                if (subtree_rejection != MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_NONE)
                {
                    rejection = subtree_rejection;
                    event->affected_entity = affected_entity;
                    if (rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_DESTINATION_OVERFLOW)
                    {
                        ++out_plan->movement_overflows_rejected_count;
                    }
                }
                else
                {
                    subtree_error = find_subtree_external_occupancy_conflict(
                        out_plan->planned_entities, out_plan->planned_entity_count, &subtree,
                        &blocker, &affected_entity, &has_conflict);
                    if (subtree_error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
                    {
                        subtree_move_plan_destroy(&subtree);
                        step_plan_destroy(out_plan);
                        return subtree_error;
                    }
                    if (has_conflict != 0)
                    {
                        rejection =
                            MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_CONFLICT;
                        event->related_entity = blocker;
                        event->affected_entity = affected_entity;
                        ++out_plan->occupancy_conflicts_rejected_count;
                    }
                    else
                    {
                        moved_member_count = (uint64_t)subtree.count;
                        if ((size_t)moved_member_count != subtree.count ||
                            out_plan->moved_count >
                                UINT64_MAX - kernel->total_entities_moved ||
                            moved_member_count > UINT64_MAX -
                                kernel->total_entities_moved - out_plan->moved_count)
                        {
                            subtree_move_plan_destroy(&subtree);
                            step_plan_destroy(out_plan);
                            return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
                        }
                        subtree_error = reserve_step_events(out_plan, subtree.count);
                        if (subtree_error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
                        {
                            subtree_move_plan_destroy(&subtree);
                            step_plan_destroy(out_plan);
                            return subtree_error;
                        }
                        for (member_index = 0U; member_index < subtree.count; ++member_index)
                        {
                            out_plan->planned_entities[subtree.members[member_index].entity_index]
                                .transform = subtree.members[member_index].destination_transform;
                        }
                        event->has_previous_transform = true;
                        event->previous_transform = subtree.members[0U].previous_transform;
                        event->has_transform = true;
                        event->transform = subtree.members[0U].destination_transform;
                        event->has_displacement = true;
                        event->displacement = command->displacement;
                        event->type = MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED;
                        event->subject = subtree.members[0U].entity;
                        event->related_entity.value = UINT64_C(0);
                        event->affected_entity.value = UINT64_C(0);
                        event->rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_NONE;
                        subtree_error = append_step_event(out_plan, *event);
                        if (subtree_error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
                        {
                            subtree_move_plan_destroy(&subtree);
                            step_plan_destroy(out_plan);
                            return subtree_error;
                        }
                        event_appended = 1;
                        for (member_index = 1U; member_index < subtree.count; ++member_index)
                        {
                            MiniSNNWorldsKernelEvent causal_event = *event;

                            causal_event.subject = subtree.members[member_index].entity;
                            causal_event.related_entity =
                                subtree.members[member_index].immediate_parent;
                            causal_event.previous_transform =
                                subtree.members[member_index].previous_transform;
                            causal_event.transform =
                                subtree.members[member_index].destination_transform;
                            causal_event.affected_entity.value = UINT64_C(0);
                            subtree_error = append_step_event(out_plan, causal_event);
                            if (subtree_error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
                            {
                                subtree_move_plan_destroy(&subtree);
                                step_plan_destroy(out_plan);
                                return subtree_error;
                            }
                        }
                        out_plan->moved_count += moved_member_count;
                        ++out_plan->applied_count;
                    }
                }
                subtree_move_plan_destroy(&subtree);
            }
            else if (!scalar_add_checked(record->transform.position.x,
                                         command->displacement.x,
                                         &destination.position.x) ||
                     !scalar_add_checked(record->transform.position.y,
                                         command->displacement.y,
                                         &destination.position.y))
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_DESTINATION_OVERFLOW;
                ++out_plan->movement_overflows_rejected_count;
            }
            else
            {
                destination.orientation = record->transform.orientation;
                if (!transform_orientation_is_valid(destination))
                {
                    rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_INVALID_ORIENTATION;
                }
                else if (!transform_is_within_bounds(destination, kernel->space_bounds))
                {
                    rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_POSITION_OUT_OF_BOUNDS;
                }
                else if (record->has_occupancy != 0 &&
                         !occupancy_envelope(destination, record->occupancy,
                                             &destination_envelope))
                {
                    rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_DESTINATION_OVERFLOW;
                    ++out_plan->movement_overflows_rejected_count;
                }
                else if (record->has_occupancy != 0 &&
                         (destination_envelope.min_x < kernel->space_bounds.min_x ||
                          destination_envelope.max_x > kernel->space_bounds.max_x ||
                          destination_envelope.min_y < kernel->space_bounds.min_y ||
                          destination_envelope.max_y > kernel->space_bounds.max_y))
                {
                    rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_OUT_OF_BOUNDS;
                }
                else if (record->has_occupancy != 0 &&
                         find_occupancy_conflict(out_plan->planned_entities,
                                                 out_plan->planned_entity_count,
                                                 record->entity_id, destination,
                                                 record->occupancy,
                                                 &event->related_entity))
                {
                    rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_CONFLICT;
                    ++out_plan->occupancy_conflicts_rejected_count;
                }
                else
                {
                    event->has_previous_transform = true;
                    event->previous_transform = record->transform;
                    record->transform = destination;
                    event->has_transform = true;
                    event->transform = destination;
                    event->has_displacement = true;
                    event->displacement = command->displacement;
                    ++out_plan->moved_count;
                    ++out_plan->applied_count;
                    event_type = MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED;
                }
            }
        }        else if (command->type == MINISNN_WORLDS_KERNEL_COMMAND_SET_OCCUPANCY)
        {
            EntityRecord *record = find_entity_record(
                out_plan->planned_entities, out_plan->planned_entity_count,
                command->target_entity);

            if (record == NULL || record->alive == 0)
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_NOT_ALIVE;
            }
            else if (!command->has_occupancy || !occupancy_is_valid(command->occupancy) ||
                     !occupancy_fits_space(command->occupancy, kernel->space_bounds))
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_INVALID_OCCUPANCY;
            }
            else if (record->has_transform != 0 &&
                     !occupancy_envelope_is_within_bounds(
                         record->transform, command->occupancy, kernel->space_bounds))
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_OUT_OF_BOUNDS;
            }
            else if (record->has_transform != 0 &&
                     find_occupancy_conflict(out_plan->planned_entities,
                                             out_plan->planned_entity_count,
                                             record->entity_id, record->transform,
                                             command->occupancy, &event->related_entity))
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_CONFLICT;
                ++out_plan->occupancy_conflicts_rejected_count;
            }
            else
            {
                if (record->has_occupancy == 0)
                {
                    ++out_plan->planned_entities_with_occupancy;
                }
                else if (record->has_transform != 0)
                {
                    --out_plan->planned_active_occupancies;
                    if (record->occupancy.blocking_mask != UINT32_C(0))
                    {
                        --out_plan->planned_blocking_occupancies;
                    }
                }
                record->has_occupancy = 1;
                record->occupancy = command->occupancy;
                if (record->has_transform != 0)
                {
                    ++out_plan->planned_active_occupancies;
                    if (record->occupancy.blocking_mask != UINT32_C(0))
                    {
                        ++out_plan->planned_blocking_occupancies;
                    }
                }
                event->has_occupancy = true;
                event->occupancy = command->occupancy;
                ++out_plan->occupancies_set_count;
                ++out_plan->applied_count;
                event_type = MINISNN_WORLDS_KERNEL_EVENT_OCCUPANCY_SET;
            }
        }
        else if (command->type == MINISNN_WORLDS_KERNEL_COMMAND_CLEAR_OCCUPANCY)
        {
            EntityRecord *record = find_entity_record(
                out_plan->planned_entities, out_plan->planned_entity_count,
                command->target_entity);

            if (record == NULL || record->alive == 0)
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_NOT_ALIVE;
            }
            else if (record->has_occupancy == 0)
            {
                rejection = MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_HAS_NO_OCCUPANCY;
            }
            else
            {
                event->has_occupancy = true;
                event->occupancy = record->occupancy;
                if (record->has_transform != 0)
                {
                    --out_plan->planned_active_occupancies;
                    if (record->occupancy.blocking_mask != UINT32_C(0))
                    {
                        --out_plan->planned_blocking_occupancies;
                    }
                }
                record->has_occupancy = 0;
                record->occupancy = zero_occupancy();
                --out_plan->planned_entities_with_occupancy;
                ++out_plan->occupancies_cleared_count;
                ++out_plan->applied_count;
                event_type = MINISNN_WORLDS_KERNEL_EVENT_OCCUPANCY_CLEARED;
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
        if (event_appended == 0)
        {
            event_error = append_step_event(out_plan, *event);
            if (event_error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
            {
                step_plan_destroy(out_plan);
                return event_error;
            }
        }
#ifdef MINISNN_WORLDS_KERNEL_C3_INVARIANT_TESTING
        if (validate_planned_invariants(kernel, out_plan) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {

            step_plan_destroy(out_plan);
            return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
        }
#endif
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
    int has_transform,
    MiniSNNWorldsKernelTransform transform,
    int has_displacement,
    MiniSNNWorldsKernelPosition displacement,
    int has_occupancy,
    MiniSNNWorldsKernelOccupancy occupancy,
    int has_spatial_link_endpoints,
    MiniSNNWorldsKernelSpatialLinkEndpoints spatial_link_endpoints,
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
        type != MINISNN_WORLDS_KERNEL_COMMAND_DESTROY_ENTITY &&
        type != MINISNN_WORLDS_KERNEL_COMMAND_PLACE_ENTITY &&
        type != MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_ENTITY_FROM_SPACE &&
        type != MINISNN_WORLDS_KERNEL_COMMAND_SET_OCCUPANCY &&
        type != MINISNN_WORLDS_KERNEL_COMMAND_CLEAR_OCCUPANCY &&
        type != MINISNN_WORLDS_KERNEL_COMMAND_MOVE_ENTITY &&
        type != MINISNN_WORLDS_KERNEL_COMMAND_CREATE_SPATIAL_LINK &&
        type != MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_SPATIAL_LINK)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_COMMAND);
        return kernel->last_error;
    }
    if (type == MINISNN_WORLDS_KERNEL_COMMAND_CREATE_ENTITY &&
        target_entity.value != 0U)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_ENTITY_ID);
        return kernel->last_error;
    }
    if (type != MINISNN_WORLDS_KERNEL_COMMAND_CREATE_ENTITY &&
        target_entity.value == 0U)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_ENTITY_ID);
        return kernel->last_error;
    }
    if (type == MINISNN_WORLDS_KERNEL_COMMAND_CREATE_SPATIAL_LINK ||
        type == MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_SPATIAL_LINK)
    {
        if (has_spatial_link_endpoints == 0 ||
            spatial_link_endpoints.parent.value != target_entity.value)
        {
            set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_COMMAND);
            return kernel->last_error;
        }
        if (spatial_link_endpoints.parent.value == UINT64_C(0) ||
            spatial_link_endpoints.child.value == UINT64_C(0))
        {
            set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_ENTITY_ID);
            return kernel->last_error;
        }
    }
    else
    {
        has_spatial_link_endpoints = 0;
        spatial_link_endpoints = zero_spatial_link_endpoints();
    }
    if (type == MINISNN_WORLDS_KERNEL_COMMAND_PLACE_ENTITY)
    {
        if (has_transform == 0 || !transform_orientation_is_valid(transform) ||
            !transform_is_within_bounds(transform, kernel->space_bounds))
        {
            set_last_error(kernel, !transform_orientation_is_valid(transform) ?
                MINISNN_WORLDS_KERNEL_ERROR_INVALID_TRANSFORM :
                MINISNN_WORLDS_KERNEL_ERROR_INVALID_BOUND);
            return kernel->last_error;
        }
    }
    else
    {
        has_transform = 0;
        transform = zero_transform();
    }
    if (type == MINISNN_WORLDS_KERNEL_COMMAND_MOVE_ENTITY)
    {
        if (has_displacement == 0)
        {
            set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_COMMAND);
            return kernel->last_error;
        }
    }
    else
    {
        has_displacement = 0;
        displacement = zero_position();
    }
    if (type == MINISNN_WORLDS_KERNEL_COMMAND_SET_OCCUPANCY)
    {
        if (has_occupancy == 0 || !occupancy_is_valid(occupancy) ||
            !occupancy_fits_space(occupancy, kernel->space_bounds))
        {
            set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_OCCUPANCY);
            return kernel->last_error;
        }
    }
    else
    {
        has_occupancy = 0;
        occupancy = zero_occupancy();
    }
    if (!identifiers_available(kernel->next_command_id.value, 1U))
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_IDENTIFIER_OVERFLOW);
        return kernel->last_error;
    }
    if (kernel->total_commands_submitted == UINT64_MAX)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INTERNAL);
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
    memset(&kernel->pending_commands[kernel->pending_command_count], 0,
           sizeof(*kernel->pending_commands));
    kernel->pending_commands[kernel->pending_command_count].command_id =
        kernel->next_command_id;
    kernel->pending_commands[kernel->pending_command_count].target_tick = target_tick;
    kernel->pending_commands[kernel->pending_command_count].priority = priority;
    kernel->pending_commands[kernel->pending_command_count].issuer = issuer;
    kernel->pending_commands[kernel->pending_command_count].type = type;
    kernel->pending_commands[kernel->pending_command_count].target_entity = target_entity;
    kernel->pending_commands[kernel->pending_command_count].has_transform =
        has_transform != 0;
    kernel->pending_commands[kernel->pending_command_count].transform = transform;
    kernel->pending_commands[kernel->pending_command_count].has_displacement =
        has_displacement != 0;
    kernel->pending_commands[kernel->pending_command_count].displacement = displacement;
    kernel->pending_commands[kernel->pending_command_count].has_occupancy =
        has_occupancy != 0;
    kernel->pending_commands[kernel->pending_command_count].occupancy = occupancy;
    kernel->pending_commands[kernel->pending_command_count].has_spatial_link_endpoints =
        has_spatial_link_endpoints != 0;
    kernel->pending_commands[kernel->pending_command_count].spatial_link_endpoints =
        spatial_link_endpoints;
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
    config.space_bounds = default_space_bounds();
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

    kernel = minisnn_worlds_kernel_internal_allocate(sizeof(*kernel));
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
    kernel->space_bounds = config_space_bounds(config);
    return kernel;
}

void minisnn_worlds_kernel_destroy(MiniSNNWorldsKernel *kernel)
{
    if (kernel != NULL)
    {
        free(kernel->entities);
        free(kernel->spatial_links);
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

MiniSNNWorldsKernelError minisnn_worlds_kernel_space_bounds(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelSpaceBounds *out_bounds)
{
    if (kernel == NULL || out_bounds == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    *out_bounds = kernel->space_bounds;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
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

bool minisnn_worlds_kernel_entity_is_placed(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId entity_id)
{
    size_t index;

    if (kernel == NULL || entity_id.value == 0U)
    {
        return false;
    }
    for (index = 0U; index < kernel->entity_count; ++index)
    {
        const EntityRecord *record = &kernel->entities[index];

        if (record->entity_id.value == entity_id.value)
        {
            return record->alive != 0 && record->has_transform != 0;
        }
    }
    return false;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_entity_transform(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId entity_id,
    MiniSNNWorldsKernelTransform *out_transform)
{
    size_t index;

    if (kernel == NULL || out_transform == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    for (index = 0U; index < kernel->entity_count; ++index)
    {
        const EntityRecord *record = &kernel->entities[index];

        if (record->entity_id.value == entity_id.value)
        {
            if (record->alive == 0 || record->has_transform == 0)
            {
                return MINISNN_WORLDS_KERNEL_ERROR_ENTITY_NOT_PLACED;
            }
            *out_transform = record->transform;
            return MINISNN_WORLDS_KERNEL_ERROR_NONE;
        }
    }
    return MINISNN_WORLDS_KERNEL_ERROR_INVALID_ENTITY_ID;
}

size_t minisnn_worlds_kernel_placed_entity_count(
    const MiniSNNWorldsKernel *kernel)
{
    return kernel == NULL ? 0U : kernel->placed_entity_count;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_placed_entity_at(
    const MiniSNNWorldsKernel *kernel,
    size_t canonical_index,
    MiniSNNWorldsKernelEntityId *out_entity_id,
    MiniSNNWorldsKernelTransform *out_transform)
{
    size_t index;
    size_t found = 0U;

    if (kernel == NULL || out_entity_id == NULL || out_transform == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (canonical_index >= kernel->placed_entity_count)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INDEX_OUT_OF_RANGE;
    }
    for (index = 0U; index < kernel->entity_count; ++index)
    {
        const EntityRecord *record = &kernel->entities[index];

        if (record->alive != 0 && record->has_transform != 0)
        {
            if (found == canonical_index)
            {
                *out_entity_id = record->entity_id;
                *out_transform = record->transform;
                return MINISNN_WORLDS_KERNEL_ERROR_NONE;
            }
            ++found;
        }
    }
    return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
}

bool minisnn_worlds_kernel_entity_has_occupancy(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId entity_id)
{
    const EntityRecord *record = find_entity_record((EntityRecord *)
        (kernel == NULL ? NULL : kernel->entities),
        kernel == NULL ? 0U : kernel->entity_count, entity_id);

    return record != NULL && record->alive != 0 && record->has_occupancy != 0;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_entity_occupancy(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId entity_id,
    MiniSNNWorldsKernelOccupancy *out_occupancy)
{
    const EntityRecord *record;

    if (kernel == NULL || out_occupancy == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    record = find_entity_record((EntityRecord *)kernel->entities,
                                kernel->entity_count, entity_id);
    if (record == NULL || record->alive == 0)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INVALID_ENTITY_ID;
    }
    if (record->has_occupancy == 0)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_ENTITY_HAS_NO_OCCUPANCY;
    }
    *out_occupancy = record->occupancy;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

size_t minisnn_worlds_kernel_occupied_entity_count(
    const MiniSNNWorldsKernel *kernel)
{
    return kernel == NULL ? 0U : kernel->entities_with_occupancy;
}

bool minisnn_worlds_kernel_entity_has_spatial_parent(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId child)
{
    const EntityRecord *record;

    if (kernel == NULL || child.value == UINT64_C(0))
    {
        return false;
    }
    record = find_entity_record((EntityRecord *)kernel->entities,
                                kernel->entity_count, child);
    return record != NULL && record->alive != 0 &&
           find_spatial_parent_link(kernel->spatial_links, kernel->spatial_link_count,
                                    child, NULL);
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_entity_spatial_parent(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId child,
    MiniSNNWorldsKernelSpatialLink *out_link)
{
    const EntityRecord *record;

    if (kernel == NULL || out_link == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    record = find_entity_record((EntityRecord *)kernel->entities,
                                kernel->entity_count, child);
    if (record == NULL || record->alive == 0)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INVALID_ENTITY_ID;
    }
    if (!find_spatial_parent_link(kernel->spatial_links, kernel->spatial_link_count,
                                  child, out_link))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_SPATIAL_LINK_NOT_FOUND;
    }
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

size_t minisnn_worlds_kernel_spatial_link_count(
    const MiniSNNWorldsKernel *kernel)
{
    return kernel == NULL ? 0U : kernel->spatial_link_count;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_spatial_link_at(
    const MiniSNNWorldsKernel *kernel,
    size_t canonical_index,
    MiniSNNWorldsKernelSpatialLink *out_link)
{
    if (kernel == NULL || out_link == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (canonical_index >= kernel->spatial_link_count)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INDEX_OUT_OF_RANGE;
    }
    *out_link = kernel->spatial_links[canonical_index];
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

size_t minisnn_worlds_kernel_active_occupancy_count(
    const MiniSNNWorldsKernel *kernel)
{
    return kernel == NULL ? 0U : kernel->active_occupancies;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_occupied_entity_at(
    const MiniSNNWorldsKernel *kernel,
    size_t canonical_index,
    MiniSNNWorldsKernelEntityId *out_entity_id,
    MiniSNNWorldsKernelOccupancy *out_occupancy)
{
    size_t index;
    size_t found = 0U;

    if (kernel == NULL || out_entity_id == NULL || out_occupancy == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (canonical_index >= kernel->entities_with_occupancy)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INDEX_OUT_OF_RANGE;
    }
    for (index = 0U; index < kernel->entity_count; ++index)
    {
        const EntityRecord *record = &kernel->entities[index];

        if (record->alive != 0 && record->has_occupancy != 0)
        {
            if (found == canonical_index)
            {
                *out_entity_id = record->entity_id;
                *out_occupancy = record->occupancy;
                return MINISNN_WORLDS_KERNEL_ERROR_NONE;
            }
            ++found;
        }
    }
    return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
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
                         0, zero_transform(), 0, zero_position(), 0, zero_occupancy(),
                         0, zero_spatial_link_endpoints(), out_command_id);
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
                         0, zero_transform(), 0, zero_position(), 0, zero_occupancy(),
                         0, zero_spatial_link_endpoints(), out_command_id);
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_place_entity(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelEntityId target_entity,
    MiniSNNWorldsKernelTransform transform,
    MiniSNNWorldsKernelCommandId *out_command_id)
{
    return queue_command(kernel, target_tick, priority, issuer,
                         MINISNN_WORLDS_KERNEL_COMMAND_PLACE_ENTITY, target_entity,
                         1, transform, 0, zero_position(), 0, zero_occupancy(), 0, zero_spatial_link_endpoints(), out_command_id);
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_remove_entity_from_space(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelEntityId target_entity,
    MiniSNNWorldsKernelCommandId *out_command_id)
{
    return queue_command(kernel, target_tick, priority, issuer,
                         MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_ENTITY_FROM_SPACE,
                         target_entity, 0, zero_transform(), 0, zero_position(), 0, zero_occupancy(), 0, zero_spatial_link_endpoints(), out_command_id);
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_create_spatial_link(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelEntityId parent,
    MiniSNNWorldsKernelEntityId child,
    MiniSNNWorldsKernelCommandId *out_command_id)
{
    MiniSNNWorldsKernelSpatialLinkEndpoints endpoints;

    endpoints.parent = parent;
    endpoints.child = child;
    return queue_command(kernel, target_tick, priority, issuer,
                         MINISNN_WORLDS_KERNEL_COMMAND_CREATE_SPATIAL_LINK, parent,
                         0, zero_transform(), 0, zero_position(), 0, zero_occupancy(),
                         1, endpoints, out_command_id);
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_remove_spatial_link(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelEntityId parent,
    MiniSNNWorldsKernelEntityId child,
    MiniSNNWorldsKernelCommandId *out_command_id)
{
    MiniSNNWorldsKernelSpatialLinkEndpoints endpoints;

    endpoints.parent = parent;
    endpoints.child = child;
    return queue_command(kernel, target_tick, priority, issuer,
                         MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_SPATIAL_LINK, parent,
                         0, zero_transform(), 0, zero_position(), 0, zero_occupancy(),
                         1, endpoints, out_command_id);
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_move_entity(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelEntityId target_entity,
    MiniSNNWorldsKernelScalar delta_x,
    MiniSNNWorldsKernelScalar delta_y,
    MiniSNNWorldsKernelCommandId *out_command_id)
{
    MiniSNNWorldsKernelPosition displacement;

    displacement.x = delta_x;
    displacement.y = delta_y;
    return queue_command(kernel, target_tick, priority, issuer,
                         MINISNN_WORLDS_KERNEL_COMMAND_MOVE_ENTITY,
                         target_entity, 0, zero_transform(), 1, displacement,
                         0, zero_occupancy(), 0, zero_spatial_link_endpoints(), out_command_id);
}
MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_set_occupancy(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelEntityId target_entity,
    MiniSNNWorldsKernelOccupancy occupancy,
    MiniSNNWorldsKernelCommandId *out_command_id)
{
    return queue_command(kernel, target_tick, priority, issuer,
                         MINISNN_WORLDS_KERNEL_COMMAND_SET_OCCUPANCY,
                         target_entity, 0, zero_transform(), 0, zero_position(), 1, occupancy,
                         0, zero_spatial_link_endpoints(), out_command_id);
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_clear_occupancy(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelEntityId target_entity,
    MiniSNNWorldsKernelCommandId *out_command_id)
{
    return queue_command(kernel, target_tick, priority, issuer,
                         MINISNN_WORLDS_KERNEL_COMMAND_CLEAR_OCCUPANCY,
                         target_entity, 0, zero_transform(), 0, zero_position(), 0, zero_occupancy(),
                         0, zero_spatial_link_endpoints(), out_command_id);
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
    /* Entity IDs are committed monotonically and records are never reordered. */
    if (canonical_index >= kernel->entity_count)
    {
        return NULL;
    }
    return &kernel->entities[canonical_index];
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

static uint64_t compute_state_hash_v1(const MiniSNNWorldsKernel *kernel)
{
    uint64_t hash = MINISNN_WORLDS_KERNEL_FNV1A_OFFSET;
    size_t index;

    fnv1a_append_literal(&hash, "MSWK_STATE_V1");
    fnv1a_append_u32(&hash, MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V1);
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


static int space_bounds_equal(
    MiniSNNWorldsKernelSpaceBounds left,
    MiniSNNWorldsKernelSpaceBounds right)
{
    return left.min_x == right.min_x && left.min_y == right.min_y &&
           left.max_x == right.max_x && left.max_y == right.max_y;
}

static int state_is_k1c_hash_compatible(const MiniSNNWorldsKernel *kernel)
{
    size_t index;

    if (kernel->spatial_link_count != 0U ||
        kernel->total_spatial_links_created != UINT64_C(0) ||
        kernel->total_spatial_links_removed != UINT64_C(0) ||
        kernel->total_spatial_link_commands_processed != UINT64_C(0))
    {
        return 0;
    }
    for (index = 0U; index < kernel->pending_command_count; ++index)
    {
        const MiniSNNWorldsKernelCommandInfo *command = &kernel->pending_commands[index];

        if (command->type == MINISNN_WORLDS_KERNEL_COMMAND_CREATE_SPATIAL_LINK ||
            command->type == MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_SPATIAL_LINK ||
            command->has_spatial_link_endpoints)
        {
            return 0;
        }
    }
    for (index = 0U; index < kernel->last_tick_event_count; ++index)
    {
        const MiniSNNWorldsKernelEvent *event = &kernel->last_tick_events[index];

        if (event->type == MINISNN_WORLDS_KERNEL_EVENT_SPATIAL_LINK_CREATED ||
            event->type == MINISNN_WORLDS_KERNEL_EVENT_SPATIAL_LINK_REMOVED ||
            event->has_spatial_link ||
            event->affected_entity.value != UINT64_C(0))
        {
            return 0;
        }
    }
    return 1;
}
static int state_is_k1b1_hash_compatible(const MiniSNNWorldsKernel *kernel)
{
    size_t index;

    if (!state_is_k1c_hash_compatible(kernel) ||
        kernel->total_movement_commands_processed != UINT64_C(0) ||
        kernel->total_entities_moved != UINT64_C(0) ||
        kernel->total_movement_overflows_rejected != UINT64_C(0))
    {
        return 0;
    }
    for (index = 0U; index < kernel->pending_command_count; ++index)
    {
        const MiniSNNWorldsKernelCommandInfo *command = &kernel->pending_commands[index];

        if (command->type == MINISNN_WORLDS_KERNEL_COMMAND_MOVE_ENTITY ||
            command->has_displacement)
        {
            return 0;
        }
    }
    for (index = 0U; index < kernel->last_tick_event_count; ++index)
    {
        const MiniSNNWorldsKernelEvent *event = &kernel->last_tick_events[index];

        if (event->type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED ||
            event->has_previous_transform)
        {
            return 0;
        }
    }
    return 1;
}
static int state_is_k1a_hash_compatible(const MiniSNNWorldsKernel *kernel)
{
    size_t index;

    if (!state_is_k1b1_hash_compatible(kernel) ||
        kernel->entities_with_occupancy != 0U || kernel->active_occupancies != 0U ||
        kernel->blocking_occupancies != 0U || kernel->total_occupancies_set != 0U ||
        kernel->total_occupancies_cleared != 0U ||
        kernel->total_occupancy_conflicts_rejected != 0U)
    {
        return 0;
    }
    for (index = 0U; index < kernel->entity_count; ++index)
    {
        if (kernel->entities[index].has_occupancy != 0)
        {
            return 0;
        }
    }
    for (index = 0U; index < kernel->pending_command_count; ++index)
    {
        const MiniSNNWorldsKernelCommandInfo *command = &kernel->pending_commands[index];

        if (command->type == MINISNN_WORLDS_KERNEL_COMMAND_SET_OCCUPANCY ||
            command->type == MINISNN_WORLDS_KERNEL_COMMAND_CLEAR_OCCUPANCY ||
            command->has_occupancy)
        {
            return 0;
        }
    }
    for (index = 0U; index < kernel->last_tick_event_count; ++index)
    {
        const MiniSNNWorldsKernelEvent *event = &kernel->last_tick_events[index];

        if (event->type == MINISNN_WORLDS_KERNEL_EVENT_OCCUPANCY_SET ||
            event->type == MINISNN_WORLDS_KERNEL_EVENT_OCCUPANCY_CLEARED ||
            event->has_occupancy || event->related_entity.value != UINT64_C(0))
        {
            return 0;
        }
    }
    return 1;
}

static int state_is_k0_hash_compatible(const MiniSNNWorldsKernel *kernel)
{
    size_t index;

    if (!state_is_k1a_hash_compatible(kernel) ||
        !space_bounds_equal(kernel->space_bounds, default_space_bounds()) ||
        kernel->placed_entity_count != 0U || kernel->total_entities_placed != 0U ||
        kernel->total_entities_removed_from_space != 0U)
    {
        return 0;
    }
    for (index = 0U; index < kernel->entity_count; ++index)
    {
        if (kernel->entities[index].has_transform != 0)
        {
            return 0;
        }
    }
    for (index = 0U; index < kernel->pending_command_count; ++index)
    {
        const MiniSNNWorldsKernelCommandInfo *command = &kernel->pending_commands[index];

        if (command->type == MINISNN_WORLDS_KERNEL_COMMAND_PLACE_ENTITY ||
            command->type == MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_ENTITY_FROM_SPACE ||
            command->has_transform)
        {
            return 0;
        }
    }
    for (index = 0U; index < kernel->last_tick_event_count; ++index)
    {
        const MiniSNNWorldsKernelEvent *event = &kernel->last_tick_events[index];

        if (event->type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_PLACED ||
            event->type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_REMOVED_FROM_SPACE ||
            event->has_transform)
        {
            return 0;
        }
    }
    return 1;
}

static uint64_t scalar_hash_encoding(MiniSNNWorldsKernelScalar value)
{
    return ((uint64_t)value) ^ UINT64_C(0x8000000000000000);
}

static void fnv1a_append_occupancy(
    uint64_t *in_out_hash,
    MiniSNNWorldsKernelOccupancy occupancy)
{
    fnv1a_append_u64(in_out_hash, scalar_hash_encoding(occupancy.half_extent_x));
    fnv1a_append_u64(in_out_hash, scalar_hash_encoding(occupancy.half_extent_y));
    fnv1a_append_u32(in_out_hash, occupancy.category_bits);
    fnv1a_append_u32(in_out_hash, occupancy.blocking_mask);
}

static void fnv1a_append_spatial_link(
    uint64_t *in_out_hash,
    MiniSNNWorldsKernelSpatialLink link)
{
    fnv1a_append_u64(in_out_hash, link.parent.value);
    fnv1a_append_u64(in_out_hash, link.child.value);
    fnv1a_append_u64(in_out_hash, scalar_hash_encoding(link.offset_x));
    fnv1a_append_u64(in_out_hash, scalar_hash_encoding(link.offset_y));
}
static uint64_t compute_state_hash_v2(const MiniSNNWorldsKernel *kernel)
{
    uint64_t hash = compute_state_hash_v1(kernel);
    size_t index;

    fnv1a_append_literal(&hash, "MSWK_STATE_V2");
    fnv1a_append_u32(&hash, MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V2);
    fnv1a_append_u64(&hash, (uint64_t)MINISNN_WORLDS_KERNEL_SCALAR_SCALE);
    fnv1a_append_u64(&hash, scalar_hash_encoding(kernel->space_bounds.min_x));
    fnv1a_append_u64(&hash, scalar_hash_encoding(kernel->space_bounds.min_y));
    fnv1a_append_u64(&hash, scalar_hash_encoding(kernel->space_bounds.max_x));
    fnv1a_append_u64(&hash, scalar_hash_encoding(kernel->space_bounds.max_y));
    fnv1a_append_u64(&hash, (uint64_t)kernel->placed_entity_count);
    for (index = 0U; index < kernel->entity_count; ++index)
    {
        const EntityRecord *record = canonical_entity_at(kernel, index);

        fnv1a_append_u64(&hash, record->entity_id.value);
        fnv1a_append_byte(&hash, record->has_transform != 0 ? UINT8_C(1) : UINT8_C(0));
        if (record->has_transform != 0)
        {
            fnv1a_append_u64(&hash, scalar_hash_encoding(record->transform.position.x));
            fnv1a_append_u64(&hash, scalar_hash_encoding(record->transform.position.y));
            fnv1a_append_u32(&hash, record->transform.orientation);
        }
    }
    for (index = 0U; index < kernel->pending_command_count; ++index)
    {
        const MiniSNNWorldsKernelCommandInfo *command =
            canonical_pending_command_at(kernel, index);

        fnv1a_append_u64(&hash, command->command_id.value);
        fnv1a_append_byte(&hash, command->has_transform ? UINT8_C(1) : UINT8_C(0));
        if (command->has_transform)
        {
            fnv1a_append_u64(&hash, scalar_hash_encoding(command->transform.position.x));
            fnv1a_append_u64(&hash, scalar_hash_encoding(command->transform.position.y));
            fnv1a_append_u32(&hash, command->transform.orientation);
        }
    }
    for (index = 0U; index < kernel->last_tick_event_count; ++index)
    {
        const MiniSNNWorldsKernelEvent *event = &kernel->last_tick_events[index];

        fnv1a_append_u64(&hash, event->event_id.value);
        fnv1a_append_byte(&hash, event->has_transform ? UINT8_C(1) : UINT8_C(0));
        if (event->has_transform)
        {
            fnv1a_append_u64(&hash, scalar_hash_encoding(event->transform.position.x));
            fnv1a_append_u64(&hash, scalar_hash_encoding(event->transform.position.y));
            fnv1a_append_u32(&hash, event->transform.orientation);
        }
    }
    fnv1a_append_u64(&hash, kernel->total_entities_placed);
    fnv1a_append_u64(&hash, kernel->total_entities_removed_from_space);
    return hash;
}

static uint64_t compute_state_hash_v3(const MiniSNNWorldsKernel *kernel)
{
    uint64_t hash = compute_state_hash_v2(kernel);
    size_t index;

    fnv1a_append_literal(&hash, "MSWK_STATE_V3");
    fnv1a_append_u32(&hash, MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V3);
    fnv1a_append_u64(&hash, (uint64_t)kernel->entities_with_occupancy);
    fnv1a_append_u64(&hash, (uint64_t)kernel->active_occupancies);
    fnv1a_append_u64(&hash, (uint64_t)kernel->blocking_occupancies);
    for (index = 0U; index < kernel->entity_count; ++index)
    {
        const EntityRecord *record = canonical_entity_at(kernel, index);

        fnv1a_append_u64(&hash, record->entity_id.value);
        fnv1a_append_byte(&hash, record->has_occupancy != 0 ? UINT8_C(1) : UINT8_C(0));
        if (record->has_occupancy != 0)
        {
            fnv1a_append_occupancy(&hash, record->occupancy);
        }
    }
    for (index = 0U; index < kernel->pending_command_count; ++index)
    {
        const MiniSNNWorldsKernelCommandInfo *command =
            canonical_pending_command_at(kernel, index);

        fnv1a_append_u64(&hash, command->command_id.value);
        fnv1a_append_byte(&hash, command->has_occupancy ? UINT8_C(1) : UINT8_C(0));
        if (command->has_occupancy)
        {
            fnv1a_append_occupancy(&hash, command->occupancy);
        }
    }
    for (index = 0U; index < kernel->last_tick_event_count; ++index)
    {
        const MiniSNNWorldsKernelEvent *event = &kernel->last_tick_events[index];

        fnv1a_append_u64(&hash, event->event_id.value);
        fnv1a_append_u64(&hash, event->related_entity.value);
        fnv1a_append_byte(&hash, event->has_occupancy ? UINT8_C(1) : UINT8_C(0));
        if (event->has_occupancy)
        {
            fnv1a_append_occupancy(&hash, event->occupancy);
        }
    }
    fnv1a_append_u64(&hash, kernel->total_occupancies_set);
    fnv1a_append_u64(&hash, kernel->total_occupancies_cleared);
    fnv1a_append_u64(&hash, kernel->total_occupancy_conflicts_rejected);
    return hash;
}

static uint64_t compute_state_hash_v4(const MiniSNNWorldsKernel *kernel)
{
    uint64_t hash = compute_state_hash_v3(kernel);
    size_t index;

    fnv1a_append_literal(&hash, "MSWK_STATE_V4");
    fnv1a_append_u32(&hash, MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V4);
    for (index = 0U; index < kernel->pending_command_count; ++index)
    {
        const MiniSNNWorldsKernelCommandInfo *command =
            canonical_pending_command_at(kernel, index);

        fnv1a_append_u64(&hash, command->command_id.value);
        fnv1a_append_byte(&hash, command->has_displacement ? UINT8_C(1) : UINT8_C(0));
        if (command->has_displacement)
        {
            fnv1a_append_u64(&hash, scalar_hash_encoding(command->displacement.x));
            fnv1a_append_u64(&hash, scalar_hash_encoding(command->displacement.y));
        }
    }
    for (index = 0U; index < kernel->last_tick_event_count; ++index)
    {
        const MiniSNNWorldsKernelEvent *event = &kernel->last_tick_events[index];

        fnv1a_append_u64(&hash, event->event_id.value);
        fnv1a_append_byte(&hash,
                          event->has_previous_transform ? UINT8_C(1) : UINT8_C(0));
        if (event->has_previous_transform)
        {
            fnv1a_append_u64(&hash,
                              scalar_hash_encoding(event->previous_transform.position.x));
            fnv1a_append_u64(&hash,
                              scalar_hash_encoding(event->previous_transform.position.y));
            fnv1a_append_u32(&hash, event->previous_transform.orientation);
        }
    }
    fnv1a_append_u64(&hash, kernel->total_movement_commands_processed);
    fnv1a_append_u64(&hash, kernel->total_entities_moved);
    fnv1a_append_u64(&hash, kernel->total_movement_overflows_rejected);
    return hash;
}
static uint64_t compute_state_hash_v5(const MiniSNNWorldsKernel *kernel)
{
    uint64_t hash = compute_state_hash_v4(kernel);
    size_t index;

    fnv1a_append_literal(&hash, "MSWK_STATE_V5");
    fnv1a_append_u32(&hash, MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V5);
    fnv1a_append_u64(&hash, (uint64_t)kernel->spatial_link_count);
    for (index = 0U; index < kernel->spatial_link_count; ++index)
    {
        fnv1a_append_spatial_link(&hash, kernel->spatial_links[index]);
    }
    for (index = 0U; index < kernel->pending_command_count; ++index)
    {
        const MiniSNNWorldsKernelCommandInfo *command =
            canonical_pending_command_at(kernel, index);

        fnv1a_append_u64(&hash, command->command_id.value);
        fnv1a_append_byte(&hash,
                          command->has_spatial_link_endpoints ? UINT8_C(1) : UINT8_C(0));
        fnv1a_append_u64(&hash, command->spatial_link_endpoints.parent.value);
        fnv1a_append_u64(&hash, command->spatial_link_endpoints.child.value);
    }
    for (index = 0U; index < kernel->last_tick_event_count; ++index)
    {
        const MiniSNNWorldsKernelEvent *event = &kernel->last_tick_events[index];

        fnv1a_append_u64(&hash, event->event_id.value);
        fnv1a_append_byte(&hash, event->has_spatial_link ? UINT8_C(1) : UINT8_C(0));
        if (event->has_spatial_link)
        {
            fnv1a_append_spatial_link(&hash, event->spatial_link);
        }
        fnv1a_append_u64(&hash, event->affected_entity.value);
    }
    fnv1a_append_u64(&hash, (uint64_t)kernel->spatial_link_count);
    fnv1a_append_u64(&hash, kernel->total_spatial_links_created);
    fnv1a_append_u64(&hash, kernel->total_spatial_links_removed);
    fnv1a_append_u64(&hash, kernel->total_spatial_link_commands_processed);
    return hash;
}
static uint32_t current_state_hash_version(const MiniSNNWorldsKernel *kernel)
{
    if (state_is_k0_hash_compatible(kernel))
    {
        return MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V1;
    }
    if (state_is_k1a_hash_compatible(kernel))
    {
        return MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V2;
    }
    if (state_is_k1b1_hash_compatible(kernel))
    {
        return MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V3;
    }
    return state_is_k1c_hash_compatible(kernel) ?
        MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V4 :
        MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V5;
}

static uint64_t compute_current_state_hash(const MiniSNNWorldsKernel *kernel)
{
    uint32_t version = current_state_hash_version(kernel);

    if (version == MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V1)
    {
        return compute_state_hash_v1(kernel);
    }
    if (version == MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V2)
    {
        return compute_state_hash_v2(kernel);
    }
    if (version == MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V3)
    {
        return compute_state_hash_v3(kernel);
    }
    if (version == MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V4)
    {
        return compute_state_hash_v4(kernel);
    }
    return compute_state_hash_v5(kernel);
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_state_hash_versioned(
    const MiniSNNWorldsKernel *kernel,
    uint32_t version,
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
    if (version == MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V1)
    {
        if (!state_is_k0_hash_compatible(kernel))
        {
            return MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE;
        }
        *out_hash = compute_state_hash_v1(kernel);
        return MINISNN_WORLDS_KERNEL_ERROR_NONE;
    }
    if (version == MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V2)
    {
        if (!state_is_k1a_hash_compatible(kernel))
        {
            return MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE;
        }
        *out_hash = compute_state_hash_v2(kernel);
        return MINISNN_WORLDS_KERNEL_ERROR_NONE;
    }
    if (version == MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V3)
    {
        if (!state_is_k1b1_hash_compatible(kernel))
        {
            return MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE;
        }
        *out_hash = compute_state_hash_v3(kernel);
        return MINISNN_WORLDS_KERNEL_ERROR_NONE;
    }
    if (version == MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V4)
    {
        if (!state_is_k1c_hash_compatible(kernel))
        {
            return MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE;
        }
        *out_hash = compute_state_hash_v4(kernel);
        return MINISNN_WORLDS_KERNEL_ERROR_NONE;
    }
    if (version != MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V5)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INVALID_ARGUMENT;
    }
    *out_hash = compute_state_hash_v5(kernel);
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}
MiniSNNWorldsKernelError minisnn_worlds_kernel_state_hash(
    const MiniSNNWorldsKernel *kernel,
    uint64_t *out_hash)
{
    if (kernel == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    return minisnn_worlds_kernel_state_hash_versioned(
        kernel, current_state_hash_version(kernel), out_hash);
}

bool minisnn_worlds_kernel_command_batch_active(const MiniSNNWorldsKernel *kernel)
{
    return kernel != NULL && kernel->command_batch_active != 0;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_command_batch_begin(
    MiniSNNWorldsKernel *kernel)
{
    if (kernel == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (kernel->state != MINISNN_WORLDS_KERNEL_STATE_READY ||
        kernel->command_batch_active != 0)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE);
        return kernel->last_error;
    }
    kernel->command_batch_active = 1;
    kernel->command_batch_begin_tick = kernel->tick;
    kernel->command_batch_pending_command_count = kernel->pending_command_count;
    kernel->command_batch_next_command_id = kernel->next_command_id;
    kernel->command_batch_total_commands_submitted = kernel->total_commands_submitted;
    kernel->command_batch_last_error = kernel->last_error;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_command_batch_commit(
    MiniSNNWorldsKernel *kernel)
{
    if (kernel == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (kernel->state != MINISNN_WORLDS_KERNEL_STATE_READY ||
        kernel->command_batch_active == 0)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE);
        return kernel->last_error;
    }
    kernel->command_batch_active = 0;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_command_batch_rollback(
    MiniSNNWorldsKernel *kernel)
{
    if (kernel == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (kernel->state != MINISNN_WORLDS_KERNEL_STATE_READY ||
        kernel->command_batch_active == 0 ||
        kernel->tick != kernel->command_batch_begin_tick)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE);
        return kernel->last_error;
    }
    kernel->pending_command_count = kernel->command_batch_pending_command_count;
    kernel->next_command_id = kernel->command_batch_next_command_id;
    kernel->total_commands_submitted = kernel->command_batch_total_commands_submitted;
    kernel->last_error = kernel->command_batch_last_error;
    kernel->command_batch_active = 0;
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

    error = validate_step_counter_promotion(kernel, &plan);
    if (error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        step_plan_destroy(&plan);
        kernel->state = MINISNN_WORLDS_KERNEL_STATE_READY;
        kernel->last_error = error;
        return error;
    }
#ifdef MINISNN_WORLDS_KERNEL_C3_INVARIANT_TESTING
    if (plan.command_count != 0U &&
        validate_planned_invariants(kernel, &plan) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {

        step_plan_destroy(&plan);
        kernel->state = MINISNN_WORLDS_KERNEL_STATE_READY;
        kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
        return kernel->last_error;
    }
#endif

    if (plan.command_count != 0U)
    {
        free(kernel->entities);
        kernel->entities = plan.planned_entities;
        kernel->entity_capacity = plan.planned_entity_capacity;
        plan.planned_entities = NULL;
        free(kernel->spatial_links);
        kernel->spatial_links = plan.planned_spatial_links;
        kernel->spatial_link_capacity = plan.planned_spatial_link_capacity;
        plan.planned_spatial_links = NULL;
    }
    kernel->entity_count = plan.planned_entity_count;
    kernel->spatial_link_count = plan.planned_spatial_link_count;
    kernel->alive_entity_count = plan.planned_alive_entity_count;
    kernel->placed_entity_count = plan.planned_placed_entity_count;
    kernel->entities_with_occupancy = plan.planned_entities_with_occupancy;
    kernel->active_occupancies = plan.planned_active_occupancies;
    kernel->blocking_occupancies = plan.planned_blocking_occupancies;
    kernel->next_entity_id = plan.next_entity_id;
    kernel->next_event_id = plan.next_event_id;
    kernel->total_entities_created += plan.created_count;
    kernel->total_entities_destroyed += plan.destroyed_count;
    kernel->total_entities_placed += plan.placed_count;
    kernel->total_entities_removed_from_space += plan.removed_from_space_count;
    kernel->total_occupancies_set += plan.occupancies_set_count;
    kernel->total_occupancies_cleared += plan.occupancies_cleared_count;
    kernel->total_occupancy_conflicts_rejected += plan.occupancy_conflicts_rejected_count;
    kernel->total_movement_commands_processed += plan.movement_commands_processed_count;
    kernel->total_entities_moved += plan.moved_count;
    kernel->total_movement_overflows_rejected += plan.movement_overflows_rejected_count;
    kernel->total_spatial_links_created += plan.spatial_links_created_count;
    kernel->total_spatial_links_removed += plan.spatial_links_removed_count;
    kernel->total_spatial_link_commands_processed +=
        plan.spatial_link_commands_processed_count;
    kernel->total_commands_applied += plan.applied_count;
    kernel->total_commands_rejected += plan.rejected_count;
    kernel->total_events_emitted += (uint64_t)plan.next_event_count;
    free(kernel->last_tick_events);
    kernel->last_tick_events = plan.next_events;
    kernel->last_tick_event_count = plan.next_event_count;
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
#ifdef MINISNN_WORLDS_KERNEL_C3_INVARIANT_TESTING
    if (minisnn_worlds_kernel_internal_validate_invariants(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {

        kernel->state = MINISNN_WORLDS_KERNEL_STATE_READY;
        kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
        step_plan_destroy(&plan);
        return kernel->last_error;
    }
#endif
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
    diagnostics.placed_entities = (uint64_t)kernel->placed_entity_count;
    diagnostics.total_entities_placed = kernel->total_entities_placed;
    diagnostics.total_entities_removed_from_space =
        kernel->total_entities_removed_from_space;
    diagnostics.entities_with_occupancy = (uint64_t)kernel->entities_with_occupancy;
    diagnostics.active_occupancies = (uint64_t)kernel->active_occupancies;
    diagnostics.blocking_occupancies = (uint64_t)kernel->blocking_occupancies;
    diagnostics.total_occupancies_set = kernel->total_occupancies_set;
    diagnostics.total_occupancies_cleared = kernel->total_occupancies_cleared;
    diagnostics.total_occupancy_conflicts_rejected =
        kernel->total_occupancy_conflicts_rejected;
    diagnostics.total_movement_commands_processed =
        kernel->total_movement_commands_processed;
    diagnostics.total_entities_moved = kernel->total_entities_moved;
    diagnostics.total_movement_overflows_rejected =
        kernel->total_movement_overflows_rejected;
    diagnostics.active_spatial_links = (uint64_t)kernel->spatial_link_count;
    diagnostics.total_spatial_links_created = kernel->total_spatial_links_created;
    diagnostics.total_spatial_links_removed = kernel->total_spatial_links_removed;
    diagnostics.total_spatial_link_commands_processed =
        kernel->total_spatial_link_commands_processed;
    diagnostics.pending_commands = (uint64_t)kernel->pending_command_count;
    diagnostics.total_commands_submitted = kernel->total_commands_submitted;
    diagnostics.total_commands_applied = kernel->total_commands_applied;
    diagnostics.total_commands_rejected = kernel->total_commands_rejected;
    diagnostics.last_tick_events = (uint64_t)kernel->last_tick_event_count;
    diagnostics.total_events_emitted = kernel->total_events_emitted;
    diagnostics.master_seed = kernel->master_seed;
    diagnostics.random_streams = (uint64_t)kernel->random_stream_count;
    diagnostics.total_random_u32_generated = kernel->total_random_u32_generated;
    diagnostics.state_hash_version = current_state_hash_version(kernel);
    diagnostics.current_state_hash = compute_current_state_hash(kernel);
    diagnostics.prng_version = MINISNN_WORLDS_KERNEL_PRNG_VERSION;
    diagnostics.space_min_x = kernel->space_bounds.min_x;
    diagnostics.space_min_y = kernel->space_bounds.min_y;
    diagnostics.space_max_x = kernel->space_bounds.max_x;
    diagnostics.space_max_y = kernel->space_bounds.max_y;
    diagnostics.scalar_scale = MINISNN_WORLDS_KERNEL_SCALAR_SCALE;
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
    trace.state_hash = compute_current_state_hash(kernel);
    trace.alive_entities = (uint64_t)kernel->alive_entity_count;
    trace.pending_commands = (uint64_t)kernel->pending_command_count;
    trace.last_tick_events = (uint64_t)kernel->last_tick_event_count;
    trace.random_streams = (uint64_t)kernel->random_stream_count;
    trace.total_random_u32_generated = kernel->total_random_u32_generated;
    *out_trace = trace;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

#ifdef MINISNN_WORLDS_KERNEL_TESTING
int minisnn_worlds_kernel_testing_scalar_add(
    MiniSNNWorldsKernelScalar left,
    MiniSNNWorldsKernelScalar right,
    MiniSNNWorldsKernelScalar *out_result)
{
    return scalar_add_checked(left, right, out_result);
}

int minisnn_worlds_kernel_testing_scalar_subtract(
    MiniSNNWorldsKernelScalar left,
    MiniSNNWorldsKernelScalar right,
    MiniSNNWorldsKernelScalar *out_result)
{
    return scalar_subtract_checked(left, right, out_result);
}

void minisnn_worlds_kernel_testing_fail_next_allocation(void)
{
    testing_allocation_fail_after = 0U;
}

void minisnn_worlds_kernel_testing_fail_allocation_after(size_t successful_allocations)
{
    testing_allocation_fail_after = successful_allocations;
}


MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_reserve_entity_capacity(
    MiniSNNWorldsKernel *kernel,
    size_t capacity)
{
    EntityRecord *expanded;

    if (kernel == NULL || capacity < kernel->entity_count)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INVALID_ARGUMENT;
    }
    if (capacity == kernel->entity_capacity)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NONE;
    }
    if (capacity == 0U)
    {
        free(kernel->entities);
        kernel->entities = NULL;
        kernel->entity_capacity = 0U;
        return MINISNN_WORLDS_KERNEL_ERROR_NONE;
    }
    expanded = allocate_expanded_copy(kernel->entities, kernel->entity_count,
                                     capacity, sizeof(*expanded));
    if (expanded == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    free(kernel->entities);
    kernel->entities = expanded;
    kernel->entity_capacity = capacity;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
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

MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_set_observability_counters(
    MiniSNNWorldsKernel *kernel,
    uint64_t total_events_emitted,
    uint64_t total_entities_moved)
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
    kernel->total_events_emitted = total_events_emitted;
    kernel->total_entities_moved = total_entities_moved;
    kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_NONE;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}
MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_set_all_counters(
    MiniSNNWorldsKernel *kernel,
    uint64_t value)
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
    kernel->total_entities_created = value;
    kernel->total_entities_destroyed = value;
    kernel->total_entities_placed = value;
    kernel->total_entities_removed_from_space = value;
    kernel->total_occupancies_set = value;
    kernel->total_occupancies_cleared = value;
    kernel->total_occupancy_conflicts_rejected = value;
    kernel->total_movement_commands_processed = value;
    kernel->total_entities_moved = value;
    kernel->total_movement_overflows_rejected = value;
    kernel->total_spatial_links_created = value;
    kernel->total_spatial_links_removed = value;
    kernel->total_spatial_link_commands_processed = value;
    kernel->total_commands_submitted = value;
    kernel->total_commands_applied = value;
    kernel->total_commands_rejected = value;
    kernel->total_events_emitted = value;
    kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_NONE;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_validate_invariants(
    const MiniSNNWorldsKernel *kernel)
{
    return minisnn_worlds_kernel_internal_validate_invariants(kernel);
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_inject_corruption(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelTestingCorruption corruption)
{
    size_t entity_index;

    if (kernel == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (kernel->state != MINISNN_WORLDS_KERNEL_STATE_READY)
    {
        set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE);
        return kernel->last_error;
    }
    if (corruption == MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_COUNTERS)
    {
        if (kernel->total_spatial_links_created == UINT64_MAX)
        {
            set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_ARGUMENT);
            return kernel->last_error;
        }
        kernel->total_spatial_links_removed = kernel->total_spatial_links_created + UINT64_C(1);
    }
    else
    {
        if (kernel->spatial_link_count == 0U)
        {
            set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_ARGUMENT);
            return kernel->last_error;
        }
        switch (corruption)
        {
            case MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_SPATIAL_LINK_CYCLE:
                if (kernel->spatial_link_count < 2U)
                {
                    set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_ARGUMENT);
                    return kernel->last_error;
                }
                kernel->spatial_links[0U].parent = kernel->spatial_links[1U].child;
                break;
            case MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_CHILD_HAS_TWO_PARENTS:
                if (kernel->spatial_link_count < 2U)
                {
                    set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_ARGUMENT);
                    return kernel->last_error;
                }
                kernel->spatial_links[1U].child = kernel->spatial_links[0U].child;
                break;
            case MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_DUPLICATE_SPATIAL_LINK:
                if (kernel->spatial_link_count < 2U)
                {
                    set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_ARGUMENT);
                    return kernel->last_error;
                }
                kernel->spatial_links[1U] = kernel->spatial_links[0U];
                break;
            case MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_UNSORTED_SPATIAL_LINKS:
            {
                SpatialLinkRecord temporary_link;

                if (kernel->spatial_link_count < 2U)
                {
                    set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_ARGUMENT);
                    return kernel->last_error;
                }
                temporary_link = kernel->spatial_links[0U];
                kernel->spatial_links[0U] = kernel->spatial_links[1U];
                kernel->spatial_links[1U] = temporary_link;
                break;
            }
            case MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_DEAD_LINK_ENDPOINT:
                if (!find_entity_record_index(kernel->entities, kernel->entity_count,
                                              kernel->spatial_links[0U].parent, &entity_index))
                {
                    return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
                }
                kernel->entities[entity_index].alive = 0;
                break;
            case MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_UNPLACED_LINK_ENDPOINT:
                if (!find_entity_record_index(kernel->entities, kernel->entity_count,
                                              kernel->spatial_links[0U].parent, &entity_index))
                {
                    return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
                }
                kernel->entities[entity_index].has_transform = 0;
                break;
            case MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_LINK_OFFSET:
                if (kernel->spatial_links[0U].offset_x < INT64_MAX)
                {
                    ++kernel->spatial_links[0U].offset_x;
                }
                else
                {
                    --kernel->spatial_links[0U].offset_x;
                }
                break;
            case MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_UNKNOWN_LINK_ENDPOINT:
                kernel->spatial_links[0U].child.value = UINT64_MAX;
                break;
            default:
                set_last_error(kernel, MINISNN_WORLDS_KERNEL_ERROR_INVALID_ARGUMENT);
                return kernel->last_error;
        }
    }
    kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_NONE;
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
