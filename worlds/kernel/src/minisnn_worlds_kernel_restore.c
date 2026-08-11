#include "minisnn_worlds_kernel_snapshot.h"
#include "minisnn_worlds_kernel_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define MINISNN_WORLDS_KERNEL_SNAPSHOT_HEADER_SIZE ((size_t)40U)

/* Snapshot V1 was defined with these constants implicit in the payload. */
_Static_assert(MINISNN_WORLDS_KERNEL_PRNG_VERSION ==
                   MINISNN_WORLDS_KERNEL_SNAPSHOT_V1_PRNG_VERSION,
               "Snapshot V1 requires PRNG version 1");
_Static_assert(MINISNN_WORLDS_KERNEL_SCALAR_SCALE ==
                   MINISNN_WORLDS_KERNEL_SNAPSHOT_V1_SCALAR_SCALE,
               "Snapshot V1 requires fixed-point scale 1000");

typedef struct
{
    const uint8_t *data;
    size_t size;
    size_t offset;
    int failed;
} SnapshotReader;

typedef struct
{
    uint32_t state_hash_version;
    uint64_t state_hash;
} SnapshotHeader;

typedef struct
{
    size_t entity_count;
    size_t alive_entity_count;
    size_t placed_entity_count;
    size_t entities_with_occupancy;
    size_t active_occupancies;
    size_t blocking_occupancies;
    size_t spatial_link_count;
    size_t pending_command_count;
    size_t last_tick_event_count;
    size_t random_stream_count;
} SnapshotCounts;

static int size_add_checked(size_t left, size_t right, size_t *out_result)
{
    if (out_result == NULL || right > SIZE_MAX - left)
    {
        return 0;
    }
    *out_result = left + right;
    return 1;
}

static int size_multiply_checked(size_t left, size_t right, size_t *out_result)
{
    if (out_result == NULL || (left != 0U && right > SIZE_MAX / left))
    {
        return 0;
    }
    *out_result = left * right;
    return 1;
}

static int reader_reserve(SnapshotReader *reader, size_t count)
{
    if (reader == NULL || reader->failed != 0 || count > reader->size - reader->offset)
    {
        if (reader != NULL)
        {
            reader->failed = 1;
        }
        return 0;
    }
    reader->offset += count;
    return 1;
}

static int reader_u8(SnapshotReader *reader, uint8_t *out_value)
{
    size_t offset;

    if (reader == NULL || out_value == NULL)
    {
        return 0;
    }
    offset = reader->offset;
    if (!reader_reserve(reader, 1U))
    {
        return 0;
    }
    *out_value = reader->data[offset];
    return 1;
}

static int reader_u16(SnapshotReader *reader, uint16_t *out_value)
{
    uint16_t value = UINT16_C(0);
    size_t index;

    if (out_value == NULL)
    {
        return 0;
    }
    for (index = 0U; index < 2U; ++index)
    {
        uint8_t byte;

        if (!reader_u8(reader, &byte))
        {
            return 0;
        }
        value |= (uint16_t)((uint16_t)byte << (index * 8U));
    }
    *out_value = value;
    return 1;
}

static int reader_u32(SnapshotReader *reader, uint32_t *out_value)
{
    uint32_t value = UINT32_C(0);
    size_t index;

    if (out_value == NULL)
    {
        return 0;
    }
    for (index = 0U; index < 4U; ++index)
    {
        uint8_t byte;

        if (!reader_u8(reader, &byte))
        {
            return 0;
        }
        value |= (uint32_t)((uint32_t)byte << (index * 8U));
    }
    *out_value = value;
    return 1;
}

static int reader_u64(SnapshotReader *reader, uint64_t *out_value)
{
    uint64_t value = UINT64_C(0);
    size_t index;

    if (out_value == NULL)
    {
        return 0;
    }
    for (index = 0U; index < 8U; ++index)
    {
        uint8_t byte;

        if (!reader_u8(reader, &byte))
        {
            return 0;
        }
        value |= (uint64_t)byte << (index * 8U);
    }
    *out_value = value;
    return 1;
}

static int reader_i64(SnapshotReader *reader, int64_t *out_value)
{
    uint64_t value;

    if (out_value == NULL || !reader_u64(reader, &value))
    {
        return 0;
    }
    *out_value = (int64_t)value;
    return 1;
}

static int reader_bool(SnapshotReader *reader, int *out_value)
{
    uint8_t value;

    if (out_value == NULL || !reader_u8(reader, &value) || value > UINT8_C(1))
    {
        if (reader != NULL)
        {
            reader->failed = 1;
        }
        return 0;
    }
    *out_value = value == UINT8_C(1) ? 1 : 0;
    return 1;
}

static int u64_to_size(uint64_t value, size_t *out_value)
{
    if (out_value == NULL || value > (uint64_t)SIZE_MAX)
    {
        return 0;
    }
    *out_value = (size_t)value;
    return 1;
}

static int minimum_sections_fit(const SnapshotCounts *counts, size_t remaining)
{
    size_t required = 0U;
    size_t section_size;

    if (counts == NULL ||
        !size_multiply_checked(counts->entity_count, 28U, &section_size) ||
        !size_add_checked(required, section_size, &required) ||
        !size_multiply_checked(counts->spatial_link_count, 32U, &section_size) ||
        !size_add_checked(required, section_size, &required) ||
        !size_multiply_checked(counts->pending_command_count, 44U, &section_size) ||
        !size_add_checked(required, section_size, &required) ||
        !size_multiply_checked(counts->last_tick_event_count, 69U, &section_size) ||
        !size_add_checked(required, section_size, &required) ||
        !size_multiply_checked(counts->random_stream_count, 40U, &section_size) ||
        !size_add_checked(required, section_size, &required))
    {
        return 0;
    }
    return required <= remaining;
}

static int snapshot_state_is_valid(uint32_t value)
{
    return value == (uint32_t)MINISNN_WORLDS_KERNEL_STATE_READY;
}

static int snapshot_error_is_valid(uint32_t value)
{
    return value <= (uint32_t)MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_STATE_HASH_MISMATCH;
}

static int snapshot_command_type_is_valid(uint32_t value)
{
    return value >= (uint32_t)MINISNN_WORLDS_KERNEL_COMMAND_CREATE_ENTITY &&
           value <= (uint32_t)MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_SPATIAL_LINK;
}

static int snapshot_event_type_is_valid(uint32_t value)
{
    return value >= (uint32_t)MINISNN_WORLDS_KERNEL_EVENT_ENTITY_CREATED &&
           value <= (uint32_t)MINISNN_WORLDS_KERNEL_EVENT_SPATIAL_LINK_REMOVED;
}

static int snapshot_rejection_is_valid(uint32_t value)
{
    return value <= (uint32_t)MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_HAS_SPATIAL_LINKS;
}
static int snapshot_bounds_are_valid(MiniSNNWorldsKernelSpaceBounds bounds)
{
    return bounds.min_x < bounds.max_x && bounds.min_y < bounds.max_y;
}

static int snapshot_transform_is_valid(
    MiniSNNWorldsKernelTransform transform,
    MiniSNNWorldsKernelSpaceBounds bounds)
{
    return transform.orientation < MINISNN_WORLDS_KERNEL_ORIENTATION_FULL_TURN &&
           transform.position.x >= bounds.min_x && transform.position.x <= bounds.max_x &&
           transform.position.y >= bounds.min_y && transform.position.y <= bounds.max_y;
}

static int snapshot_add_scalar(
    MiniSNNWorldsKernelScalar left,
    MiniSNNWorldsKernelScalar right,
    MiniSNNWorldsKernelScalar *out_value)
{
    if (out_value == NULL ||
        (right > 0 && left > INT64_MAX - right) ||
        (right < 0 && left < INT64_MIN - right))
    {
        return 0;
    }
    *out_value = left + right;
    return 1;
}

static int snapshot_subtract_scalar(
    MiniSNNWorldsKernelScalar left,
    MiniSNNWorldsKernelScalar right,
    MiniSNNWorldsKernelScalar *out_value)
{
    if (right == INT64_MIN)
    {
        if (left >= 0)
        {
            return 0;
        }
        *out_value = left - right;
        return 1;
    }
    return snapshot_add_scalar(left, -right, out_value);
}

static int snapshot_occupancy_is_valid(
    MiniSNNWorldsKernelOccupancy occupancy,
    MiniSNNWorldsKernelSpaceBounds bounds)
{
    MiniSNNWorldsKernelScalar min_center_x;
    MiniSNNWorldsKernelScalar max_center_x;
    MiniSNNWorldsKernelScalar min_center_y;
    MiniSNNWorldsKernelScalar max_center_y;

    return occupancy.half_extent_x > MINISNN_WORLDS_KERNEL_SCALAR_ZERO &&
           occupancy.half_extent_y > MINISNN_WORLDS_KERNEL_SCALAR_ZERO &&
           occupancy.category_bits != UINT32_C(0) &&
           snapshot_add_scalar(bounds.min_x, occupancy.half_extent_x, &min_center_x) &&
           snapshot_subtract_scalar(bounds.max_x, occupancy.half_extent_x, &max_center_x) &&
           snapshot_add_scalar(bounds.min_y, occupancy.half_extent_y, &min_center_y) &&
           snapshot_subtract_scalar(bounds.max_y, occupancy.half_extent_y, &max_center_y) &&
           min_center_x <= max_center_x && min_center_y <= max_center_y;
}

static int reader_transform(
    SnapshotReader *reader,
    MiniSNNWorldsKernelTransform *out_transform)
{
    return out_transform != NULL &&
           reader_i64(reader, &out_transform->position.x) &&
           reader_i64(reader, &out_transform->position.y) &&
           reader_u32(reader, &out_transform->orientation);
}

static int reader_occupancy(
    SnapshotReader *reader,
    MiniSNNWorldsKernelOccupancy *out_occupancy)
{
    return out_occupancy != NULL &&
           reader_i64(reader, &out_occupancy->half_extent_x) &&
           reader_i64(reader, &out_occupancy->half_extent_y) &&
           reader_u32(reader, &out_occupancy->category_bits) &&
           reader_u32(reader, &out_occupancy->blocking_mask);
}

static int reader_link(
    SnapshotReader *reader,
    MiniSNNWorldsKernelSpatialLink *out_link)
{
    return out_link != NULL &&
           reader_u64(reader, &out_link->parent.value) &&
           reader_u64(reader, &out_link->child.value) &&
           reader_i64(reader, &out_link->offset_x) &&
           reader_i64(reader, &out_link->offset_y);
}

static int reader_command(
    SnapshotReader *reader,
    MiniSNNWorldsKernelCommandInfo *out_command)
{
    uint32_t type;
    int has_transform;
    int has_displacement;
    int has_occupancy;
    int has_link_endpoints;

    if (reader == NULL || out_command == NULL ||
        !reader_u64(reader, &out_command->command_id.value) ||
        !reader_u64(reader, &out_command->target_tick) ||
        !reader_u32(reader, &out_command->priority) ||
        !reader_u64(reader, &out_command->issuer.value) ||
        !reader_u32(reader, &type) ||
        !reader_u64(reader, &out_command->target_entity.value) ||
        !reader_bool(reader, &has_transform) ||
        !reader_bool(reader, &has_displacement) ||
        !reader_bool(reader, &has_occupancy) ||
        !reader_bool(reader, &has_link_endpoints))
    {
        return 0;
    }
    out_command->type = (MiniSNNWorldsKernelCommandType)type;
    out_command->has_transform = has_transform != 0;
    out_command->has_displacement = has_displacement != 0;
    out_command->has_occupancy = has_occupancy != 0;
    out_command->has_spatial_link_endpoints = has_link_endpoints != 0;
    if ((out_command->has_transform && !reader_transform(reader, &out_command->transform)) ||
        (out_command->has_displacement &&
         (!reader_i64(reader, &out_command->displacement.x) ||
          !reader_i64(reader, &out_command->displacement.y))) ||
        (out_command->has_occupancy && !reader_occupancy(reader, &out_command->occupancy)) ||
        (out_command->has_spatial_link_endpoints &&
         (!reader_u64(reader, &out_command->spatial_link_endpoints.parent.value) ||
          !reader_u64(reader, &out_command->spatial_link_endpoints.child.value))))
    {
        return 0;
    }
    return 1;
}

static int reader_event(
    SnapshotReader *reader,
    MiniSNNWorldsKernelEvent *out_event)
{
    uint32_t type;
    uint32_t rejection;
    int has_previous_transform;
    int has_transform;
    int has_displacement;
    int has_occupancy;
    int has_spatial_link;

    if (reader == NULL || out_event == NULL ||
        !reader_u64(reader, &out_event->event_id.value) ||
        !reader_u64(reader, &out_event->tick) ||
        !reader_u32(reader, &type) ||
        !reader_u64(reader, &out_event->command_id.value) ||
        !reader_u64(reader, &out_event->issuer.value) ||
        !reader_u64(reader, &out_event->subject.value) ||
        !reader_u64(reader, &out_event->related_entity.value) ||
        !reader_u32(reader, &rejection) ||
        !reader_bool(reader, &has_previous_transform) ||
        !reader_bool(reader, &has_transform) ||
        !reader_bool(reader, &has_displacement) ||
        !reader_bool(reader, &has_occupancy) ||
        !reader_bool(reader, &has_spatial_link) ||
        !reader_u64(reader, &out_event->affected_entity.value))
    {
        return 0;
    }
    out_event->type = (MiniSNNWorldsKernelEventType)type;
    out_event->rejection = (MiniSNNWorldsKernelCommandRejection)rejection;
    out_event->has_previous_transform = has_previous_transform != 0;
    out_event->has_transform = has_transform != 0;
    out_event->has_displacement = has_displacement != 0;
    out_event->has_occupancy = has_occupancy != 0;
    out_event->has_spatial_link = has_spatial_link != 0;
    if ((out_event->has_previous_transform &&
         !reader_transform(reader, &out_event->previous_transform)) ||
        (out_event->has_transform && !reader_transform(reader, &out_event->transform)) ||
        (out_event->has_displacement &&
         (!reader_i64(reader, &out_event->displacement.x) ||
          !reader_i64(reader, &out_event->displacement.y))) ||
        (out_event->has_occupancy && !reader_occupancy(reader, &out_event->occupancy)) ||
        (out_event->has_spatial_link && !reader_link(reader, &out_event->spatial_link)))
    {
        return 0;
    }
    return 1;
}
static int command_compare(
    const MiniSNNWorldsKernelCommandInfo *left,
    const MiniSNNWorldsKernelCommandInfo *right)
{
    if (left->target_tick != right->target_tick)
    {
        return left->target_tick < right->target_tick ? -1 : 1;
    }
    if (left->priority != right->priority)
    {
        return left->priority < right->priority ? -1 : 1;
    }
    if (left->issuer.value != right->issuer.value)
    {
        return left->issuer.value < right->issuer.value ? -1 : 1;
    }
    if (left->command_id.value != right->command_id.value)
    {
        return left->command_id.value < right->command_id.value ? -1 : 1;
    }
    return 0;
}

static int spatial_link_compare(
    const SpatialLinkRecord *left,
    const SpatialLinkRecord *right)
{
    if (left->parent.value != right->parent.value)
    {
        return left->parent.value < right->parent.value ? -1 : 1;
    }
    if (left->child.value != right->child.value)
    {
        return left->child.value < right->child.value ? -1 : 1;
    }
    return 0;
}

static int stream_compare(
    MiniSNNWorldsKernelRandomStreamKey left,
    MiniSNNWorldsKernelRandomStreamKey right)
{
    if (left.namespace_id != right.namespace_id)
    {
        return left.namespace_id < right.namespace_id ? -1 : 1;
    }
    if (left.stream_id != right.stream_id)
    {
        return left.stream_id < right.stream_id ? -1 : 1;
    }
    return 0;
}

static int command_payload_is_valid(
    const MiniSNNWorldsKernelCommandInfo *command,
    MiniSNNWorldsTick current_tick,
    MiniSNNWorldsKernelSpaceBounds bounds)
{
    int needs_transform;
    int needs_displacement;
    int needs_occupancy;
    int needs_link;

    if (command == NULL || command->command_id.value == UINT64_C(0) ||
        command->target_tick <= current_tick ||
        !snapshot_command_type_is_valid((uint32_t)command->type))
    {
        return 0;
    }
    if ((command->type == MINISNN_WORLDS_KERNEL_COMMAND_CREATE_ENTITY &&
         command->target_entity.value != UINT64_C(0)) ||
        (command->type != MINISNN_WORLDS_KERNEL_COMMAND_CREATE_ENTITY &&
         command->target_entity.value == UINT64_C(0)))
    {
        return 0;
    }
    needs_transform = command->type == MINISNN_WORLDS_KERNEL_COMMAND_PLACE_ENTITY;
    needs_displacement = command->type == MINISNN_WORLDS_KERNEL_COMMAND_MOVE_ENTITY;
    needs_occupancy = command->type == MINISNN_WORLDS_KERNEL_COMMAND_SET_OCCUPANCY;
    needs_link = command->type == MINISNN_WORLDS_KERNEL_COMMAND_CREATE_SPATIAL_LINK ||
                 command->type == MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_SPATIAL_LINK;
    if ((command->has_transform != 0) != needs_transform ||
        (command->has_displacement != 0) != needs_displacement ||
        (command->has_occupancy != 0) != needs_occupancy ||
        (command->has_spatial_link_endpoints != 0) != needs_link)
    {
        return 0;
    }
    if (needs_transform != 0 && !snapshot_transform_is_valid(command->transform, bounds))
    {
        return 0;
    }
    if (needs_occupancy != 0 && !snapshot_occupancy_is_valid(command->occupancy, bounds))
    {
        return 0;
    }
    if (needs_link != 0 &&
        (command->spatial_link_endpoints.parent.value == UINT64_C(0) ||
         command->spatial_link_endpoints.child.value == UINT64_C(0) ||
         command->spatial_link_endpoints.parent.value != command->target_entity.value))
    {
        return 0;
    }
    return 1;
}

static int allocate_restore_array(void **out_data, size_t count, size_t element_size)
{
    size_t byte_count;
    void *data;

    if (out_data == NULL || element_size == 0U ||
        !size_multiply_checked(count, element_size, &byte_count))
    {
        return 0;
    }
    *out_data = NULL;
    if (count == 0U)
    {
        return 1;
    }
    data = minisnn_worlds_kernel_internal_allocate(byte_count);
    if (data == NULL)
    {
        return 0;
    }
    memset(data, 0, byte_count);
    *out_data = data;
    return 1;
}

static MiniSNNWorldsKernelError snapshot_read_header(
    const uint8_t *data,
    size_t size,
    SnapshotReader *out_reader,
    SnapshotHeader *out_header)
{
    static const uint8_t magic[8] = { 'M', 'S', 'W', 'K', 'S', 'N', 'P', '1' };
    SnapshotReader reader;
    uint32_t format_version;
    uint32_t config_version;
    uint16_t reserved_first;
    uint16_t reserved_second;
    uint64_t payload_size;

    if (data == NULL || out_reader == NULL || out_header == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (size < MINISNN_WORLDS_KERNEL_SNAPSHOT_HEADER_SIZE ||
        memcmp(data, magic, sizeof(magic)) != 0)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT;
    }
    reader.data = data;
    reader.size = size;
    reader.offset = sizeof(magic);
    reader.failed = 0;
    if (!reader_u32(&reader, &format_version))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT;
    }
    if (format_version != MINISNN_WORLDS_KERNEL_SNAPSHOT_FORMAT_VERSION_V1)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_UNSUPPORTED_VERSION;
    }
    if (!reader_u32(&reader, &config_version) ||
        !reader_u32(&reader, &out_header->state_hash_version) ||
        !reader_u16(&reader, &reserved_first) ||
        !reader_u16(&reader, &reserved_second) ||
        !reader_u64(&reader, &payload_size) ||
        !reader_u64(&reader, &out_header->state_hash) ||
        reader.offset != MINISNN_WORLDS_KERNEL_SNAPSHOT_HEADER_SIZE ||
        config_version != MINISNN_WORLDS_KERNEL_CONFIG_VERSION ||
        out_header->state_hash_version < MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V1 ||
        out_header->state_hash_version > MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V5 ||
        reserved_first != UINT16_C(0) || reserved_second != UINT16_C(0) ||
        payload_size > (uint64_t)(SIZE_MAX - MINISNN_WORLDS_KERNEL_SNAPSHOT_HEADER_SIZE) ||
        size != MINISNN_WORLDS_KERNEL_SNAPSHOT_HEADER_SIZE + (size_t)payload_size)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT;
    }
    *out_reader = reader;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static void restore_candidate_destroy(MiniSNNWorldsKernel *kernel)
{
    minisnn_worlds_kernel_destroy(kernel);
}
static MiniSNNWorldsKernelError restore_from_v1_bytes(
    const uint8_t *data,
    size_t size,
    MiniSNNWorldsKernel **out_kernel)
{
    SnapshotReader reader;
    SnapshotHeader header;
    SnapshotCounts counts;
    MiniSNNWorldsKernel *candidate = NULL;
    uint32_t config_version;
    uint32_t state;
    uint32_t last_error;
    uint64_t count_value;
    uint64_t computed_hash;
    uint64_t maximum_command_id = UINT64_C(0);
    uint64_t maximum_event_id = UINT64_C(0);
    uint64_t stream_draw_count = UINT64_C(0);
    size_t index;
    size_t other_index;
    MiniSNNWorldsKernelSnapshot *canonical = NULL;
    MiniSNNWorldsKernelError error;

    if (out_kernel == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    *out_kernel = NULL;
    error = snapshot_read_header(data, size, &reader, &header);
    if (error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return error;
    }
    candidate = minisnn_worlds_kernel_internal_allocate(sizeof(*candidate));
    if (candidate == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    memset(candidate, 0, sizeof(*candidate));
    if (!reader_u32(&reader, &config_version) ||
        !reader_u64(&reader, &candidate->master_seed) ||
        !reader_i64(&reader, &candidate->space_bounds.min_x) ||
        !reader_i64(&reader, &candidate->space_bounds.min_y) ||
        !reader_i64(&reader, &candidate->space_bounds.max_x) ||
        !reader_i64(&reader, &candidate->space_bounds.max_y) ||
        !reader_u64(&reader, &candidate->tick) || !reader_u32(&reader, &state) ||
        !reader_u32(&reader, &last_error) ||
        !reader_u64(&reader, &candidate->next_entity_id.value) ||
        !reader_u64(&reader, &candidate->next_command_id.value) ||
        !reader_u64(&reader, &candidate->next_event_id.value) ||
        config_version != MINISNN_WORLDS_KERNEL_CONFIG_VERSION ||
        !snapshot_state_is_valid(state) || !snapshot_error_is_valid(last_error) ||
        !snapshot_bounds_are_valid(candidate->space_bounds))
    {
        restore_candidate_destroy(candidate);
        return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT;
    }
    candidate->state = (MiniSNNWorldsKernelState)state;
    candidate->last_error = (MiniSNNWorldsKernelError)last_error;
    if (!reader_u64(&reader, &count_value) || !u64_to_size(count_value, &counts.entity_count) ||
        !reader_u64(&reader, &count_value) || !u64_to_size(count_value, &counts.alive_entity_count) ||
        !reader_u64(&reader, &count_value) || !u64_to_size(count_value, &counts.placed_entity_count) ||
        !reader_u64(&reader, &count_value) || !u64_to_size(count_value, &counts.entities_with_occupancy) ||
        !reader_u64(&reader, &count_value) || !u64_to_size(count_value, &counts.active_occupancies) ||
        !reader_u64(&reader, &count_value) || !u64_to_size(count_value, &counts.blocking_occupancies) ||
        !reader_u64(&reader, &count_value) || !u64_to_size(count_value, &counts.spatial_link_count) ||
        !reader_u64(&reader, &count_value) || !u64_to_size(count_value, &counts.pending_command_count) ||
        !reader_u64(&reader, &count_value) || !u64_to_size(count_value, &counts.last_tick_event_count) ||
        !reader_u64(&reader, &count_value) || !u64_to_size(count_value, &counts.random_stream_count) ||
        !minimum_sections_fit(&counts, reader.size - reader.offset))
    {
        restore_candidate_destroy(candidate);
        return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT;
    }
    candidate->entity_count = counts.entity_count;
    candidate->entity_capacity = counts.entity_count;
    candidate->alive_entity_count = counts.alive_entity_count;
    candidate->placed_entity_count = counts.placed_entity_count;
    candidate->entities_with_occupancy = counts.entities_with_occupancy;
    candidate->active_occupancies = counts.active_occupancies;
    candidate->blocking_occupancies = counts.blocking_occupancies;
    candidate->spatial_link_count = counts.spatial_link_count;
    candidate->spatial_link_capacity = counts.spatial_link_count;
    candidate->pending_command_count = counts.pending_command_count;
    candidate->pending_command_capacity = counts.pending_command_count;
    candidate->last_tick_event_count = counts.last_tick_event_count;
    candidate->random_stream_count = counts.random_stream_count;
    candidate->random_stream_capacity = counts.random_stream_count;
    if (!reader_u64(&reader, &candidate->total_entities_created) ||
        !reader_u64(&reader, &candidate->total_entities_destroyed) ||
        !reader_u64(&reader, &candidate->total_entities_placed) ||
        !reader_u64(&reader, &candidate->total_entities_removed_from_space) ||
        !reader_u64(&reader, &candidate->total_occupancies_set) ||
        !reader_u64(&reader, &candidate->total_occupancies_cleared) ||
        !reader_u64(&reader, &candidate->total_occupancy_conflicts_rejected) ||
        !reader_u64(&reader, &candidate->total_movement_commands_processed) ||
        !reader_u64(&reader, &candidate->total_entities_moved) ||
        !reader_u64(&reader, &candidate->total_movement_overflows_rejected) ||
        !reader_u64(&reader, &candidate->total_spatial_links_created) ||
        !reader_u64(&reader, &candidate->total_spatial_links_removed) ||
        !reader_u64(&reader, &candidate->total_spatial_link_commands_processed) ||
        !reader_u64(&reader, &candidate->total_commands_submitted) ||
        !reader_u64(&reader, &candidate->total_commands_applied) ||
        !reader_u64(&reader, &candidate->total_commands_rejected) ||
        !reader_u64(&reader, &candidate->total_events_emitted) ||
        !reader_u64(&reader, &candidate->total_random_u32_generated) ||
        !allocate_restore_array((void **)&candidate->entities, candidate->entity_count,
                                sizeof(*candidate->entities)) ||
        !allocate_restore_array((void **)&candidate->spatial_links,
                                candidate->spatial_link_count,
                                sizeof(*candidate->spatial_links)) ||
        !allocate_restore_array((void **)&candidate->pending_commands,
                                candidate->pending_command_count,
                                sizeof(*candidate->pending_commands)) ||
        !allocate_restore_array((void **)&candidate->last_tick_events,
                                candidate->last_tick_event_count,
                                sizeof(*candidate->last_tick_events)) ||
        !allocate_restore_array((void **)&candidate->random_streams,
                                candidate->random_stream_count,
                                sizeof(*candidate->random_streams)))
    {
        restore_candidate_destroy(candidate);
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    for (index = 0U; index < candidate->entity_count; ++index)
    {
        EntityRecord *record = &candidate->entities[index];
        uint8_t reserved;

        if (!reader_u64(&reader, &record->entity_id.value) ||
            !reader_u64(&reader, &record->creation_tick) ||
            !reader_u64(&reader, &record->destruction_tick) ||
            !reader_bool(&reader, &record->alive) ||
            !reader_bool(&reader, &record->has_transform) ||
            !reader_bool(&reader, &record->has_occupancy) ||
            !reader_u8(&reader, &reserved) || reserved != UINT8_C(0) ||
            (record->has_transform && !reader_transform(&reader, &record->transform)) ||
            (record->has_occupancy && !reader_occupancy(&reader, &record->occupancy)) ||
            record->entity_id.value == UINT64_C(0) ||
            (index != 0U && candidate->entities[index - 1U].entity_id.value >=
                               record->entity_id.value) ||
            record->creation_tick == MINISNN_WORLDS_TICK_INITIAL ||
            record->creation_tick > candidate->tick ||
            (record->alive && record->destruction_tick != MINISNN_WORLDS_TICK_INITIAL) ||
            (!record->alive &&
             (record->destruction_tick == MINISNN_WORLDS_TICK_INITIAL ||
              record->destruction_tick < record->creation_tick ||
              record->destruction_tick > candidate->tick || record->has_transform ||
              record->has_occupancy)) ||
            (record->has_transform &&
             !snapshot_transform_is_valid(record->transform, candidate->space_bounds)) ||
            (record->has_occupancy &&
             !snapshot_occupancy_is_valid(record->occupancy, candidate->space_bounds)))
        {
            restore_candidate_destroy(candidate);
            return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT;
        }
    }
    for (index = 0U; index < candidate->spatial_link_count; ++index)
    {
        if (!reader_link(&reader, &candidate->spatial_links[index]) ||
            candidate->spatial_links[index].parent.value == UINT64_C(0) ||
            candidate->spatial_links[index].child.value == UINT64_C(0) ||
            (index != 0U &&
             spatial_link_compare(&candidate->spatial_links[index - 1U],
                                  &candidate->spatial_links[index]) >= 0))
        {
            restore_candidate_destroy(candidate);
            return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT;
        }
    }
    for (index = 0U; index < candidate->pending_command_count; ++index)
    {
        if (!reader_command(&reader, &candidate->pending_commands[index]) ||
            !command_payload_is_valid(&candidate->pending_commands[index], candidate->tick,
                                      candidate->space_bounds) ||
            (index != 0U && command_compare(&candidate->pending_commands[index - 1U],
                                             &candidate->pending_commands[index]) >= 0))
        {
            restore_candidate_destroy(candidate);
            return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT;
        }
        for (other_index = 0U; other_index < index; ++other_index)
        {
            if (candidate->pending_commands[other_index].command_id.value ==
                candidate->pending_commands[index].command_id.value)
            {
                restore_candidate_destroy(candidate);
                return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT;
            }
        }
        maximum_command_id = candidate->pending_commands[index].command_id.value >
                                     maximum_command_id
                                 ? candidate->pending_commands[index].command_id.value
                                 : maximum_command_id;
    }
    for (index = 0U; index < candidate->last_tick_event_count; ++index)
    {
        MiniSNNWorldsKernelEvent *event = &candidate->last_tick_events[index];

        if (!reader_event(&reader, event) || event->event_id.value == UINT64_C(0) ||
            event->command_id.value == UINT64_C(0) || event->tick != candidate->tick ||
            !snapshot_event_type_is_valid((uint32_t)event->type) ||
            !snapshot_rejection_is_valid((uint32_t)event->rejection) ||
            (event->type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED &&
             event->rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_NONE) ||
            (event->type != MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED &&
             event->rejection != MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_NONE) ||
            (event->has_previous_transform &&
             !snapshot_transform_is_valid(event->previous_transform, candidate->space_bounds)) ||
            (event->has_transform &&
             !snapshot_transform_is_valid(event->transform, candidate->space_bounds)) ||
            (event->has_occupancy &&
             !snapshot_occupancy_is_valid(event->occupancy, candidate->space_bounds)) ||
            (event->has_spatial_link &&
             (event->spatial_link.parent.value == UINT64_C(0) ||
              event->spatial_link.child.value == UINT64_C(0))) ||
            (index != 0U && event->event_id.value !=
                               candidate->last_tick_events[index - 1U].event_id.value + UINT64_C(1)))
        {
            restore_candidate_destroy(candidate);
            return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT;
        }
        maximum_event_id = event->event_id.value;
    }
    for (index = 0U; index < candidate->random_stream_count; ++index)
    {
        RandomStreamRecord *stream = &candidate->random_streams[index];

        if (!reader_u64(&reader, &stream->key.namespace_id) ||
            !reader_u64(&reader, &stream->key.stream_id) ||
            !reader_u64(&reader, &stream->state) ||
            !reader_u64(&reader, &stream->sequence) ||
            !reader_u64(&reader, &stream->generated_u32_count) ||
            (stream->key.namespace_id == UINT64_C(0) &&
             stream->key.stream_id == UINT64_C(0)) ||
            (index != 0U &&
             stream_compare(candidate->random_streams[index - 1U].key, stream->key) >= 0))
        {
            restore_candidate_destroy(candidate);
            return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT;
        }
        if (UINT64_MAX - stream_draw_count < stream->generated_u32_count)
        {
            restore_candidate_destroy(candidate);
            return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT;
        }
        stream_draw_count += stream->generated_u32_count;
    }
    if (reader.failed != 0 || reader.offset != reader.size ||
        candidate->total_entities_created < candidate->total_entities_destroyed ||
        candidate->total_entities_created != (uint64_t)candidate->entity_count ||
        candidate->total_entities_created - candidate->total_entities_destroyed !=
            (uint64_t)candidate->alive_entity_count ||
        candidate->total_spatial_links_created < candidate->total_spatial_links_removed ||
        candidate->total_spatial_links_created - candidate->total_spatial_links_removed !=
            (uint64_t)candidate->spatial_link_count ||
        candidate->total_commands_submitted < (uint64_t)candidate->pending_command_count ||
        candidate->total_commands_applied > UINT64_MAX - candidate->total_commands_rejected ||
        candidate->total_commands_submitted - (uint64_t)candidate->pending_command_count !=
            candidate->total_commands_applied + candidate->total_commands_rejected ||
        candidate->total_events_emitted < (uint64_t)candidate->last_tick_event_count ||
        candidate->total_entities_created >= UINT64_MAX ||
        candidate->next_entity_id.value != candidate->total_entities_created + UINT64_C(1) ||
        candidate->total_commands_submitted >= UINT64_MAX ||
        candidate->next_command_id.value != candidate->total_commands_submitted + UINT64_C(1) ||
        candidate->total_events_emitted >= UINT64_MAX ||
        candidate->next_event_id.value != candidate->total_events_emitted + UINT64_C(1) ||
        candidate->next_command_id.value <= maximum_command_id ||
        candidate->next_event_id.value <= maximum_event_id ||
        candidate->total_random_u32_generated != stream_draw_count ||
        minisnn_worlds_kernel_internal_validate_invariants(candidate) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        restore_candidate_destroy(candidate);
        return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT;
    }
    error = minisnn_worlds_kernel_state_hash_versioned(candidate,
                                                        header.state_hash_version,
                                                        &computed_hash);
    if (error != MINISNN_WORLDS_KERNEL_ERROR_NONE || computed_hash != header.state_hash)
    {
        restore_candidate_destroy(candidate);
        return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_STATE_HASH_MISMATCH;
    }
    error = minisnn_worlds_kernel_snapshot_capture(candidate, &canonical);
    if (error != MINISNN_WORLDS_KERNEL_ERROR_NONE || canonical == NULL ||
        canonical->size != size || memcmp(canonical->data, data, size) != 0)
    {
        minisnn_worlds_kernel_snapshot_destroy(canonical);
        restore_candidate_destroy(candidate);
        return error == MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION ? error :
            MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT;
    }
    minisnn_worlds_kernel_snapshot_destroy(canonical);
    *out_kernel = candidate;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_snapshot_from_bytes(
    const uint8_t *data,
    size_t size,
    MiniSNNWorldsKernelSnapshot **out_snapshot)
{
    MiniSNNWorldsKernel *candidate = NULL;
    MiniSNNWorldsKernelSnapshot *snapshot;
    MiniSNNWorldsKernelError error;

    if (out_snapshot == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    *out_snapshot = NULL;
    if (data == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    error = restore_from_v1_bytes(data, size, &candidate);
    if (error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return error;
    }
    snapshot = minisnn_worlds_kernel_internal_allocate(sizeof(*snapshot));
    if (snapshot == NULL)
    {
        restore_candidate_destroy(candidate);
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    snapshot->data = minisnn_worlds_kernel_internal_allocate(size);
    if (snapshot->data == NULL)
    {
        free(snapshot);
        restore_candidate_destroy(candidate);
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    memcpy(snapshot->data, data, size);
    snapshot->size = size;
    snapshot->format_version = MINISNN_WORLDS_KERNEL_SNAPSHOT_FORMAT_VERSION_V1;
    restore_candidate_destroy(candidate);
    *out_snapshot = snapshot;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_create_from_snapshot(
    const MiniSNNWorldsKernelSnapshot *snapshot,
    MiniSNNWorldsKernel **out_kernel)
{
    if (out_kernel == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    *out_kernel = NULL;
    if (snapshot == NULL || snapshot->data == NULL || snapshot->size == 0U ||
        snapshot->format_version != MINISNN_WORLDS_KERNEL_SNAPSHOT_FORMAT_VERSION_V1)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    return restore_from_v1_bytes(snapshot->data, snapshot->size, out_kernel);
}
