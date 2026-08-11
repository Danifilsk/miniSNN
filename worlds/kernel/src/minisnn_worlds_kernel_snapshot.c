#include "minisnn_worlds_kernel_snapshot.h"
#include "minisnn_worlds_kernel_internal.h"

#include <limits.h>
#include <stdlib.h>

#define MINISNN_WORLDS_KERNEL_SNAPSHOT_HEADER_SIZE ((size_t)40U)
#define MINISNN_WORLDS_KERNEL_SNAPSHOT_COUNTER_COUNT ((size_t)18U)

typedef struct
{
    uint8_t *data;
    size_t capacity;
    size_t offset;
    int failed;
} SnapshotWriter;

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

static void writer_reserve(SnapshotWriter *writer, size_t count)
{
    size_t next_offset;

    if (writer == NULL || writer->failed != 0 ||
        !size_add_checked(writer->offset, count, &next_offset) ||
        next_offset > writer->capacity)
    {
        if (writer != NULL)
        {
            writer->failed = 1;
        }
        return;
    }
    writer->offset = next_offset;
}

static void writer_u8(SnapshotWriter *writer, uint8_t value)
{
    size_t offset;

    if (writer == NULL)
    {
        return;
    }
    offset = writer->offset;
    writer_reserve(writer, 1U);
    if (writer->failed == 0 && writer->data != NULL)
    {
        writer->data[offset] = value;
    }
}

static void writer_u16(SnapshotWriter *writer, uint16_t value)
{
    writer_u8(writer, (uint8_t)value);
    writer_u8(writer, (uint8_t)(value >> 8U));
}

static void writer_u32(SnapshotWriter *writer, uint32_t value)
{
    size_t index;

    for (index = 0U; index < 4U; ++index)
    {
        writer_u8(writer, (uint8_t)(value >> (index * 8U)));
    }
}

static void writer_u64(SnapshotWriter *writer, uint64_t value)
{
    size_t index;

    for (index = 0U; index < 8U; ++index)
    {
        writer_u8(writer, (uint8_t)(value >> (index * 8U)));
    }
}

static void writer_i64(SnapshotWriter *writer, int64_t value)
{
    writer_u64(writer, (uint64_t)value);
}

static void writer_bool(SnapshotWriter *writer, int value)
{
    writer_u8(writer, value != 0 ? UINT8_C(1) : UINT8_C(0));
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

static const EntityRecord *canonical_entity_at(
    const MiniSNNWorldsKernel *kernel,
    size_t canonical_index)
{
    size_t candidate_index;

    for (candidate_index = 0U; candidate_index < kernel->entity_count; ++candidate_index)
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

static const SpatialLinkRecord *canonical_spatial_link_at(
    const MiniSNNWorldsKernel *kernel,
    size_t canonical_index)
{
    size_t candidate_index;

    for (candidate_index = 0U; candidate_index < kernel->spatial_link_count;
         ++candidate_index)
    {
        size_t other_index;
        size_t rank = 0U;

        for (other_index = 0U; other_index < kernel->spatial_link_count;
             ++other_index)
        {
            if (spatial_link_compare(&kernel->spatial_links[other_index],
                                     &kernel->spatial_links[candidate_index]) < 0)
            {
                ++rank;
            }
        }
        if (rank == canonical_index)
        {
            return &kernel->spatial_links[candidate_index];
        }
    }
    return NULL;
}

static const MiniSNNWorldsKernelCommandInfo *canonical_command_at(
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

static const RandomStreamRecord *canonical_stream_at(
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
            if (stream_compare(kernel->random_streams[other_index].key,
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

static void writer_transform(
    SnapshotWriter *writer,
    MiniSNNWorldsKernelTransform transform)
{
    writer_i64(writer, transform.position.x);
    writer_i64(writer, transform.position.y);
    writer_u32(writer, transform.orientation);
}

static void writer_occupancy(
    SnapshotWriter *writer,
    MiniSNNWorldsKernelOccupancy occupancy)
{
    writer_i64(writer, occupancy.half_extent_x);
    writer_i64(writer, occupancy.half_extent_y);
    writer_u32(writer, occupancy.category_bits);
    writer_u32(writer, occupancy.blocking_mask);
}

static void writer_link(
    SnapshotWriter *writer,
    MiniSNNWorldsKernelSpatialLink link)
{
    writer_u64(writer, link.parent.value);
    writer_u64(writer, link.child.value);
    writer_i64(writer, link.offset_x);
    writer_i64(writer, link.offset_y);
}

static void writer_command(
    SnapshotWriter *writer,
    const MiniSNNWorldsKernelCommandInfo *command)
{
    writer_u64(writer, command->command_id.value);
    writer_u64(writer, command->target_tick);
    writer_u32(writer, command->priority);
    writer_u64(writer, command->issuer.value);
    writer_u32(writer, (uint32_t)command->type);
    writer_u64(writer, command->target_entity.value);
    writer_bool(writer, command->has_transform);
    writer_bool(writer, command->has_displacement);
    writer_bool(writer, command->has_occupancy);
    writer_bool(writer, command->has_spatial_link_endpoints);
    if (command->has_transform)
    {
        writer_transform(writer, command->transform);
    }
    if (command->has_displacement)
    {
        writer_i64(writer, command->displacement.x);
        writer_i64(writer, command->displacement.y);
    }
    if (command->has_occupancy)
    {
        writer_occupancy(writer, command->occupancy);
    }
    if (command->has_spatial_link_endpoints)
    {
        writer_u64(writer, command->spatial_link_endpoints.parent.value);
        writer_u64(writer, command->spatial_link_endpoints.child.value);
    }
}

static void writer_event(
    SnapshotWriter *writer,
    const MiniSNNWorldsKernelEvent *event)
{
    writer_u64(writer, event->event_id.value);
    writer_u64(writer, event->tick);
    writer_u32(writer, (uint32_t)event->type);
    writer_u64(writer, event->command_id.value);
    writer_u64(writer, event->issuer.value);
    writer_u64(writer, event->subject.value);
    writer_u64(writer, event->related_entity.value);
    writer_u32(writer, (uint32_t)event->rejection);
    writer_bool(writer, event->has_previous_transform);
    writer_bool(writer, event->has_transform);
    writer_bool(writer, event->has_displacement);
    writer_bool(writer, event->has_occupancy);
    writer_bool(writer, event->has_spatial_link);
    writer_u64(writer, event->affected_entity.value);
    if (event->has_previous_transform)
    {
        writer_transform(writer, event->previous_transform);
    }
    if (event->has_transform)
    {
        writer_transform(writer, event->transform);
    }
    if (event->has_displacement)
    {
        writer_i64(writer, event->displacement.x);
        writer_i64(writer, event->displacement.y);
    }
    if (event->has_occupancy)
    {
        writer_occupancy(writer, event->occupancy);
    }
    if (event->has_spatial_link)
    {
        writer_link(writer, event->spatial_link);
    }
}


static int minimum_payload_size_is_representable(
    const MiniSNNWorldsKernel *kernel)
{
    static const size_t fixed_size =
        4U + 8U + 32U + 8U + 4U + 4U + 24U + (10U * 8U) +
        (MINISNN_WORLDS_KERNEL_SNAPSHOT_COUNTER_COUNT * 8U);
    size_t minimum_size = fixed_size;
    size_t section_size;

    if (!size_multiply_checked(kernel->entity_count, 28U, &section_size) ||
        !size_add_checked(minimum_size, section_size, &minimum_size) ||
        !size_multiply_checked(kernel->spatial_link_count, 32U, &section_size) ||
        !size_add_checked(minimum_size, section_size, &minimum_size) ||
        !size_multiply_checked(kernel->pending_command_count, 44U, &section_size) ||
        !size_add_checked(minimum_size, section_size, &minimum_size) ||
        !size_multiply_checked(kernel->last_tick_event_count, 69U, &section_size) ||
        !size_add_checked(minimum_size, section_size, &minimum_size) ||
        !size_multiply_checked(kernel->random_stream_count, 40U, &section_size) ||
        !size_add_checked(minimum_size, section_size, &minimum_size))
    {
        return 0;
    }
    return minimum_size <= SIZE_MAX - MINISNN_WORLDS_KERNEL_SNAPSHOT_HEADER_SIZE;
}
static int encode_payload(
    const MiniSNNWorldsKernel *kernel,
    SnapshotWriter *writer)
{
    size_t index;

    writer_u32(writer, MINISNN_WORLDS_KERNEL_CONFIG_VERSION);
    writer_u64(writer, kernel->master_seed);
    writer_i64(writer, kernel->space_bounds.min_x);
    writer_i64(writer, kernel->space_bounds.min_y);
    writer_i64(writer, kernel->space_bounds.max_x);
    writer_i64(writer, kernel->space_bounds.max_y);
    writer_u64(writer, kernel->tick);
    writer_u32(writer, (uint32_t)kernel->state);
    writer_u32(writer, (uint32_t)kernel->last_error);
    writer_u64(writer, kernel->next_entity_id.value);
    writer_u64(writer, kernel->next_command_id.value);
    writer_u64(writer, kernel->next_event_id.value);

    writer_u64(writer, (uint64_t)kernel->entity_count);
    writer_u64(writer, (uint64_t)kernel->alive_entity_count);
    writer_u64(writer, (uint64_t)kernel->placed_entity_count);
    writer_u64(writer, (uint64_t)kernel->entities_with_occupancy);
    writer_u64(writer, (uint64_t)kernel->active_occupancies);
    writer_u64(writer, (uint64_t)kernel->blocking_occupancies);
    writer_u64(writer, (uint64_t)kernel->spatial_link_count);
    writer_u64(writer, (uint64_t)kernel->pending_command_count);
    writer_u64(writer, (uint64_t)kernel->last_tick_event_count);
    writer_u64(writer, (uint64_t)kernel->random_stream_count);

    writer_u64(writer, kernel->total_entities_created);
    writer_u64(writer, kernel->total_entities_destroyed);
    writer_u64(writer, kernel->total_entities_placed);
    writer_u64(writer, kernel->total_entities_removed_from_space);
    writer_u64(writer, kernel->total_occupancies_set);
    writer_u64(writer, kernel->total_occupancies_cleared);
    writer_u64(writer, kernel->total_occupancy_conflicts_rejected);
    writer_u64(writer, kernel->total_movement_commands_processed);
    writer_u64(writer, kernel->total_entities_moved);
    writer_u64(writer, kernel->total_movement_overflows_rejected);
    writer_u64(writer, kernel->total_spatial_links_created);
    writer_u64(writer, kernel->total_spatial_links_removed);
    writer_u64(writer, kernel->total_spatial_link_commands_processed);
    writer_u64(writer, kernel->total_commands_submitted);
    writer_u64(writer, kernel->total_commands_applied);
    writer_u64(writer, kernel->total_commands_rejected);
    writer_u64(writer, kernel->total_events_emitted);
    writer_u64(writer, kernel->total_random_u32_generated);

    for (index = 0U; index < kernel->entity_count; ++index)
    {
        const EntityRecord *record = canonical_entity_at(kernel, index);

        if (record == NULL)
        {
            return 0;
        }
        writer_u64(writer, record->entity_id.value);
        writer_u64(writer, record->creation_tick);
        writer_u64(writer, record->destruction_tick);
        writer_bool(writer, record->alive);
        writer_bool(writer, record->has_transform);
        writer_bool(writer, record->has_occupancy);
        writer_u8(writer, UINT8_C(0));
        if (record->has_transform)
        {
            writer_transform(writer, record->transform);
        }
        if (record->has_occupancy)
        {
            writer_occupancy(writer, record->occupancy);
        }
    }
    for (index = 0U; index < kernel->spatial_link_count; ++index)
    {
        const SpatialLinkRecord *link = canonical_spatial_link_at(kernel, index);

        if (link == NULL)
        {
            return 0;
        }
        writer_link(writer, *link);
    }
    for (index = 0U; index < kernel->pending_command_count; ++index)
    {
        const MiniSNNWorldsKernelCommandInfo *command =
            canonical_command_at(kernel, index);

        if (command == NULL)
        {
            return 0;
        }
        writer_command(writer, command);
    }
    for (index = 0U; index < kernel->last_tick_event_count; ++index)
    {
        writer_event(writer, &kernel->last_tick_events[index]);
    }
    for (index = 0U; index < kernel->random_stream_count; ++index)
    {
        const RandomStreamRecord *stream = canonical_stream_at(kernel, index);

        if (stream == NULL)
        {
            return 0;
        }
        writer_u64(writer, stream->key.namespace_id);
        writer_u64(writer, stream->key.stream_id);
        writer_u64(writer, stream->state);
        writer_u64(writer, stream->sequence);
        writer_u64(writer, stream->generated_u32_count);
    }
    return writer->failed == 0;
}

static int encode_header(
    SnapshotWriter *writer,
    uint64_t payload_size,
    uint32_t state_hash_version,
    uint64_t state_hash)
{
    static const uint8_t magic[8] = { 'M', 'S', 'W', 'K', 'S', 'N', 'P', '1' };
    size_t index;

    for (index = 0U; index < sizeof(magic); ++index)
    {
        writer_u8(writer, magic[index]);
    }
    writer_u32(writer, MINISNN_WORLDS_KERNEL_SNAPSHOT_FORMAT_VERSION_V1);
    writer_u32(writer, MINISNN_WORLDS_KERNEL_CONFIG_VERSION);
    writer_u32(writer, state_hash_version);
    writer_u16(writer, UINT16_C(0));
    writer_u16(writer, UINT16_C(0));
    writer_u64(writer, payload_size);
    writer_u64(writer, state_hash);
    return writer->failed == 0 && writer->offset == MINISNN_WORLDS_KERNEL_SNAPSHOT_HEADER_SIZE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_snapshot_capture(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelSnapshot **out_snapshot)
{
    SnapshotWriter sizing_writer;
    SnapshotWriter data_writer;
    MiniSNNWorldsKernelDiagnostics diagnostics;
    MiniSNNWorldsKernelSnapshot *snapshot;
    size_t total_size;
    MiniSNNWorldsKernelError error;

    if (out_snapshot == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    *out_snapshot = NULL;
    if (kernel == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (minisnn_worlds_kernel_command_batch_active(kernel))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE;
    }
    error = minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics);
    if (error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return error;
    }

    if (!minimum_payload_size_is_representable(kernel))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_SIZE_OVERFLOW;
    }
    sizing_writer.data = NULL;
    sizing_writer.capacity = SIZE_MAX;
    sizing_writer.offset = 0U;
    sizing_writer.failed = 0;
    if (!encode_payload(kernel, &sizing_writer) ||
        !size_add_checked(MINISNN_WORLDS_KERNEL_SNAPSHOT_HEADER_SIZE,
                          sizing_writer.offset, &total_size))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_SIZE_OVERFLOW;
    }

    snapshot = minisnn_worlds_kernel_internal_allocate(sizeof(*snapshot));
    if (snapshot == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    snapshot->data = minisnn_worlds_kernel_internal_allocate(total_size);
    if (snapshot->data == NULL)
    {
        free(snapshot);
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    snapshot->size = total_size;
    snapshot->format_version = MINISNN_WORLDS_KERNEL_SNAPSHOT_FORMAT_VERSION_V1;

    data_writer.data = snapshot->data;
    data_writer.capacity = snapshot->size;
    data_writer.offset = 0U;
    data_writer.failed = 0;
    if (!encode_header(&data_writer, (uint64_t)sizing_writer.offset,
                       diagnostics.state_hash_version,
                       diagnostics.current_state_hash) ||
        !encode_payload(kernel, &data_writer) || data_writer.offset != snapshot->size)
    {
        free(snapshot->data);
        free(snapshot);
        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }
    *out_snapshot = snapshot;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

void minisnn_worlds_kernel_snapshot_destroy(
    MiniSNNWorldsKernelSnapshot *snapshot)
{
    if (snapshot != NULL)
    {
        free(snapshot->data);
        free(snapshot);
    }
}

uint32_t minisnn_worlds_kernel_snapshot_format_version(
    const MiniSNNWorldsKernelSnapshot *snapshot)
{
    return snapshot == NULL ? UINT32_C(0) : snapshot->format_version;
}

size_t minisnn_worlds_kernel_snapshot_size(
    const MiniSNNWorldsKernelSnapshot *snapshot)
{
    return snapshot == NULL ? 0U : snapshot->size;
}

const uint8_t *minisnn_worlds_kernel_snapshot_data(
    const MiniSNNWorldsKernelSnapshot *snapshot)
{
    return snapshot == NULL ? NULL : snapshot->data;
}

#ifdef MINISNN_WORLDS_KERNEL_TESTING
int minisnn_worlds_kernel_snapshot_testing_size_add(
    size_t left,
    size_t right,
    size_t *out_result)
{
    return size_add_checked(left, right, out_result);
}

int minisnn_worlds_kernel_snapshot_testing_size_multiply(
    size_t left,
    size_t right,
    size_t *out_result)
{
    return size_multiply_checked(left, right, out_result);
}
#endif