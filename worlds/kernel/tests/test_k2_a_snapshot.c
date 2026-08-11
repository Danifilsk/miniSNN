#include "minisnn_worlds_kernel.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(condition) \
    do { if (!(condition)) { \
        fprintf(stderr, "requirement failed: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
        exit(1); \
    } } while (0)

#define SNAPSHOT_HEADER_SIZE ((size_t)40U)

typedef struct
{
    const uint8_t *data;
    size_t size;
    size_t offset;
} SnapshotReader;

static MiniSNNWorldsKernelEntityId id(uint64_t value)
{
    MiniSNNWorldsKernelEntityId result;
    result.value = value;
    return result;
}

static MiniSNNWorldsKernelTransform transform(int64_t x, int64_t y, uint32_t orientation)
{
    MiniSNNWorldsKernelTransform result;
    result.position.x = x;
    result.position.y = y;
    result.orientation = orientation;
    return result;
}

static MiniSNNWorldsKernelOccupancy occupancy(void)
{
    MiniSNNWorldsKernelOccupancy result;
    result.half_extent_x = INT64_C(5);
    result.half_extent_y = INT64_C(7);
    result.category_bits = UINT32_C(3);
    result.blocking_mask = UINT32_C(1);
    return result;
}

static uint8_t reader_u8(SnapshotReader *reader)
{
    REQUIRE(reader != NULL);
    REQUIRE(reader->offset < reader->size);
    return reader->data[reader->offset++];
}

static uint32_t reader_u32(SnapshotReader *reader)
{
    uint32_t value = 0U;
    size_t index;

    REQUIRE(reader != NULL);
    REQUIRE(reader->offset <= reader->size && reader->size - reader->offset >= 4U);
    for (index = 0U; index < 4U; ++index)
    {
        value |= (uint32_t)reader->data[reader->offset + index] << (index * 8U);
    }
    reader->offset += 4U;
    return value;
}

static uint64_t reader_u64(SnapshotReader *reader)
{
    uint64_t value = UINT64_C(0);
    size_t index;

    REQUIRE(reader != NULL);
    REQUIRE(reader->offset <= reader->size && reader->size - reader->offset >= 8U);
    for (index = 0U; index < 8U; ++index)
    {
        value |= (uint64_t)reader->data[reader->offset + index] << (index * 8U);
    }
    reader->offset += 8U;
    return value;
}

static int reader_bool(SnapshotReader *reader)
{
    uint8_t value = reader_u8(reader);

    REQUIRE(value <= UINT8_C(1));
    return value != UINT8_C(0);
}

static void reader_skip_transform(SnapshotReader *reader)
{
    (void)reader_u64(reader);
    (void)reader_u64(reader);
    (void)reader_u32(reader);
}

static void reader_skip_occupancy(SnapshotReader *reader)
{
    (void)reader_u64(reader);
    (void)reader_u64(reader);
    (void)reader_u32(reader);
    (void)reader_u32(reader);
}

static void reader_skip_link(SnapshotReader *reader)
{
    (void)reader_u64(reader);
    (void)reader_u64(reader);
    (void)reader_u64(reader);
    (void)reader_u64(reader);
}

static uint64_t fnv1a(const uint8_t *data, size_t size)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t index;

    for (index = 0U; index < size; ++index)
    {
        hash ^= data[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int diagnostics_equal(
    const MiniSNNWorldsKernelDiagnostics *left,
    const MiniSNNWorldsKernelDiagnostics *right)
{
    return left != NULL && right != NULL &&
           left->completed_ticks == right->completed_ticks &&
           left->state == right->state && left->last_error == right->last_error &&
           left->alive_entities == right->alive_entities &&
           left->total_entities_created == right->total_entities_created &&
           left->total_entities_destroyed == right->total_entities_destroyed &&
           left->placed_entities == right->placed_entities &&
           left->total_entities_placed == right->total_entities_placed &&
           left->total_entities_removed_from_space == right->total_entities_removed_from_space &&
           left->entities_with_occupancy == right->entities_with_occupancy &&
           left->active_occupancies == right->active_occupancies &&
           left->blocking_occupancies == right->blocking_occupancies &&
           left->total_occupancies_set == right->total_occupancies_set &&
           left->total_occupancies_cleared == right->total_occupancies_cleared &&
           left->total_occupancy_conflicts_rejected == right->total_occupancy_conflicts_rejected &&
           left->total_movement_commands_processed == right->total_movement_commands_processed &&
           left->total_entities_moved == right->total_entities_moved &&
           left->total_movement_overflows_rejected == right->total_movement_overflows_rejected &&
           left->active_spatial_links == right->active_spatial_links &&
           left->total_spatial_links_created == right->total_spatial_links_created &&
           left->total_spatial_links_removed == right->total_spatial_links_removed &&
           left->total_spatial_link_commands_processed == right->total_spatial_link_commands_processed &&
           left->pending_commands == right->pending_commands &&
           left->total_commands_submitted == right->total_commands_submitted &&
           left->total_commands_applied == right->total_commands_applied &&
           left->total_commands_rejected == right->total_commands_rejected &&
           left->last_tick_events == right->last_tick_events &&
           left->total_events_emitted == right->total_events_emitted &&
           left->master_seed == right->master_seed &&
           left->random_streams == right->random_streams &&
           left->total_random_u32_generated == right->total_random_u32_generated &&
           left->current_state_hash == right->current_state_hash &&
           left->state_hash_version == right->state_hash_version &&
           left->prng_version == right->prng_version &&
           left->space_min_x == right->space_min_x && left->space_min_y == right->space_min_y &&
           left->space_max_x == right->space_max_x && left->space_max_y == right->space_max_y &&
           left->scalar_scale == right->scalar_scale;
}
static void step_ok(MiniSNNWorldsKernel *kernel)
{
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
}

static void queue_entities(MiniSNNWorldsKernel *kernel, size_t count)
{
    MiniSNNWorldsKernelCommandId command_id;
    size_t index;

    for (index = 0U; index < count; ++index)
    {
        REQUIRE(minisnn_worlds_kernel_queue_create_entity(
                    kernel, UINT64_C(1), (uint32_t)index, id(UINT64_C(0)),
                    &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    step_ok(kernel);
}

static MiniSNNWorldsKernel *build_full_state(void)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelEvent event;
    MiniSNNWorldsKernel *kernel;
    uint32_t random_value;
    size_t event_index;
    int saw_move = 0;
    int saw_rejection = 0;

    config.master_seed = UINT64_C(0x4B32415F534E4150);
    kernel = minisnn_worlds_kernel_create(&config, &error);
    REQUIRE(kernel != NULL);
    REQUIRE(error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    queue_entities(kernel, 4U);

    REQUIRE(minisnn_worlds_kernel_queue_place_entity(
                kernel, UINT64_C(2), UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)),
                transform(INT64_C(100), INT64_C(20), UINT32_C(3)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_place_entity(
                kernel, UINT64_C(2), UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(2)),
                transform(INT64_C(-30), INT64_C(40), UINT32_C(7)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step_ok(kernel);

    REQUIRE(minisnn_worlds_kernel_queue_set_occupancy(
                kernel, UINT64_C(3), UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)),
                occupancy(), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step_ok(kernel);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, UINT64_C(4), UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)),
                id(UINT64_C(2)), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step_ok(kernel);

    REQUIRE(minisnn_worlds_kernel_queue_destroy_entity(
                kernel, UINT64_C(5), UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(3)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step_ok(kernel);

    /* One causal movement and one rejection remain observable in the snapshot. */
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, UINT64_C(6), UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)),
                INT64_C(3), INT64_C(-2), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, UINT64_C(6), UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(1)),
                id(UINT64_C(2)), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step_ok(kernel);
    REQUIRE(minisnn_worlds_kernel_last_tick_event_count(kernel) >= 2U);
    for (event_index = 0U;
         event_index < minisnn_worlds_kernel_last_tick_event_count(kernel);
         ++event_index)
    {
        REQUIRE(minisnn_worlds_kernel_last_tick_event_at(kernel, event_index, &event) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
        saw_move |= event.type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED;
        saw_rejection |= event.type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED;
    }
    REQUIRE(saw_move != 0);
    REQUIRE(saw_rejection != 0);

    REQUIRE(minisnn_worlds_kernel_random_u32(
                kernel, (MiniSNNWorldsKernelRandomStreamKey){ UINT64_C(1), UINT64_C(9) },
                &random_value) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_random_u32(
                kernel, (MiniSNNWorldsKernelRandomStreamKey){ UINT64_C(2), UINT64_C(1) },
                &random_value) == MINISNN_WORLDS_KERNEL_ERROR_NONE);

    /* Keep one pending payload for every K0/K1 command type beyond the current tick. */
    REQUIRE(minisnn_worlds_kernel_queue_create_entity(
                kernel, UINT64_C(7), UINT32_C(2), id(UINT64_C(0)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_destroy_entity(
                kernel, UINT64_C(8), UINT32_C(1), id(UINT64_C(1)), id(UINT64_C(4)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_place_entity(
                kernel, UINT64_C(9), UINT32_C(3), id(UINT64_C(2)), id(UINT64_C(4)),
                transform(INT64_C(9), INT64_C(-4), UINT32_C(11)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_remove_entity_from_space(
                kernel, UINT64_C(10), UINT32_C(0), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_set_occupancy(
                kernel, UINT64_C(11), UINT32_C(1), id(UINT64_C(2)), id(UINT64_C(2)),
                occupancy(), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_clear_occupancy(
                kernel, UINT64_C(12), UINT32_C(2), id(UINT64_C(1)), id(UINT64_C(1)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, UINT64_C(13), UINT32_C(3), id(UINT64_C(2)), id(UINT64_C(1)),
                INT64_C(11), INT64_C(-9), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, UINT64_C(14), UINT32_C(1), id(UINT64_C(1)), id(UINT64_C(1)),
                id(UINT64_C(2)), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_remove_spatial_link(
                kernel, UINT64_C(15), UINT32_C(0), id(UINT64_C(2)), id(UINT64_C(1)),
                id(UINT64_C(2)), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    return kernel;
}


static uint32_t read_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
}

static uint64_t read_u64_le(const uint8_t *data)
{
    uint64_t value = UINT64_C(0);
    size_t index;

    for (index = 0U; index < 8U; ++index)
    {
        value |= (uint64_t)data[index] << (index * 8U);
    }
    return value;
}

static int snapshot_layout_is_valid(const uint8_t *data, size_t size)
{
    size_t payload_size;
    size_t payload_offset = SNAPSHOT_HEADER_SIZE;
    size_t count_offset;
    uint64_t entity_count;
    uint64_t link_count;
    uint64_t command_count;
    uint64_t event_count;
    uint64_t stream_count;
    uint64_t minimum;

    if (data == NULL || size < SNAPSHOT_HEADER_SIZE ||
        memcmp(data, "MSWKSNP1", 8U) != 0 ||
        read_u32_le(data + 8U) != MINISNN_WORLDS_KERNEL_SNAPSHOT_FORMAT_VERSION_V1 ||
        read_u32_le(data + 12U) != MINISNN_WORLDS_KERNEL_CONFIG_VERSION ||
        read_u32_le(data + 16U) < MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V1 ||
        read_u32_le(data + 16U) > MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V5 ||
        read_u32_le(data + 20U) != 0U)
    {
        return 0;
    }
    payload_size = (size_t)read_u64_le(data + 24U);
    if ((uint64_t)payload_size != read_u64_le(data + 24U) ||
        payload_size != size - payload_offset)
    {
        return 0;
    }
    /* Config, bounds, tick/state/error, IDs, ten counts and eighteen counters. */
    count_offset = payload_offset + 4U + 8U + 32U + 8U + 4U + 4U + 24U;
    if (count_offset > size || size - count_offset < 80U + 144U)
    {
        return 0;
    }
    entity_count = read_u64_le(data + count_offset);
    link_count = read_u64_le(data + count_offset + 48U);
    command_count = read_u64_le(data + count_offset + 56U);
    event_count = read_u64_le(data + count_offset + 64U);
    stream_count = read_u64_le(data + count_offset + 72U);
    minimum = UINT64_C(308);
    if (entity_count > (UINT64_MAX - minimum) / UINT64_C(28))
    {
        return 0;
    }
    minimum += entity_count * UINT64_C(28);
    if (link_count > (UINT64_MAX - minimum) / UINT64_C(32))
    {
        return 0;
    }
    minimum += link_count * UINT64_C(32);
    if (command_count > (UINT64_MAX - minimum) / UINT64_C(44))
    {
        return 0;
    }
    minimum += command_count * UINT64_C(44);
    if (event_count > (UINT64_MAX - minimum) / UINT64_C(69))
    {
        return 0;
    }
    minimum += event_count * UINT64_C(69);
    if (stream_count > (UINT64_MAX - minimum) / UINT64_C(40))
    {
        return 0;
    }
    minimum += stream_count * UINT64_C(40);
    return minimum <= payload_size;
}
static void validate_header(const MiniSNNWorldsKernelSnapshot *snapshot)
{
    SnapshotReader reader;
    const uint8_t *data = minisnn_worlds_kernel_snapshot_data(snapshot);
    size_t index;
    uint64_t payload_size;

    REQUIRE(data != NULL);
    REQUIRE(minisnn_worlds_kernel_snapshot_size(snapshot) >= SNAPSHOT_HEADER_SIZE);
    REQUIRE(memcmp(data, "MSWKSNP1", 8U) == 0);
    reader.data = data;
    reader.size = minisnn_worlds_kernel_snapshot_size(snapshot);
    reader.offset = 8U;
    REQUIRE(reader_u32(&reader) == MINISNN_WORLDS_KERNEL_SNAPSHOT_FORMAT_VERSION_V1);
    REQUIRE(reader_u32(&reader) == MINISNN_WORLDS_KERNEL_CONFIG_VERSION);
    REQUIRE(reader_u32(&reader) >= MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V1);
    REQUIRE(reader_u32(&reader) == 0U);
    payload_size = reader_u64(&reader);
    REQUIRE(reader_u64(&reader) != UINT64_C(0));
    REQUIRE(reader.offset == SNAPSHOT_HEADER_SIZE);
    REQUIRE(payload_size == (uint64_t)(reader.size - reader.offset));
    for (index = 0U; index < 8U; ++index)
    {
        REQUIRE(data[index] == (uint8_t)"MSWKSNP1"[index]);
    }
}

static void validate_complete_payload(const MiniSNNWorldsKernelSnapshot *snapshot)
{
    SnapshotReader reader;
    uint64_t entity_count;
    uint64_t link_count;
    uint64_t command_count;
    uint64_t event_count;
    uint64_t stream_count;
    uint64_t previous_command_id = UINT64_C(0);
    uint64_t previous_target_tick = UINT64_C(0);
    uint64_t previous_issuer = UINT64_C(0);
    uint32_t previous_priority = UINT32_C(0);
    uint32_t command_types = UINT32_C(0);
    uint64_t previous_namespace = UINT64_C(0);
    uint64_t previous_stream = UINT64_C(0);
    uint64_t previous_entity = UINT64_C(0);
    size_t index;
    int previous_command_valid = 0;
    int previous_stream_valid = 0;
    int saw_move = 0;
    int saw_rejection = 0;

    reader.data = minisnn_worlds_kernel_snapshot_data(snapshot);
    reader.size = minisnn_worlds_kernel_snapshot_size(snapshot);
    reader.offset = SNAPSHOT_HEADER_SIZE;
    REQUIRE(reader_u32(&reader) == MINISNN_WORLDS_KERNEL_CONFIG_VERSION);
    REQUIRE(reader_u64(&reader) == UINT64_C(0x4B32415F534E4150));
    (void)reader_u64(&reader);
    (void)reader_u64(&reader);
    (void)reader_u64(&reader);
    (void)reader_u64(&reader);
    REQUIRE(reader_u64(&reader) == UINT64_C(6));
    REQUIRE(reader_u32(&reader) == (uint32_t)MINISNN_WORLDS_KERNEL_STATE_READY);
    REQUIRE(reader_u32(&reader) == (uint32_t)MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(reader_u64(&reader) == UINT64_C(5));
    (void)reader_u64(&reader);
    (void)reader_u64(&reader);

    entity_count = reader_u64(&reader);
    REQUIRE(reader_u64(&reader) == UINT64_C(3));
    REQUIRE(reader_u64(&reader) == UINT64_C(2));
    REQUIRE(reader_u64(&reader) == UINT64_C(1));
    REQUIRE(reader_u64(&reader) == UINT64_C(1));
    REQUIRE(reader_u64(&reader) == UINT64_C(1));
    link_count = reader_u64(&reader);
    command_count = reader_u64(&reader);
    event_count = reader_u64(&reader);
    stream_count = reader_u64(&reader);
    REQUIRE(entity_count == UINT64_C(4));
    REQUIRE(link_count == UINT64_C(1));
    REQUIRE(command_count == UINT64_C(9));
    REQUIRE(event_count >= UINT64_C(2));
    REQUIRE(stream_count == UINT64_C(2));

    for (index = 0U; index < 18U; ++index)
    {
        (void)reader_u64(&reader);
    }
    for (index = 0U; index < (size_t)entity_count; ++index)
    {
        uint64_t entity_id = reader_u64(&reader);
        int has_transform;
        int has_occupancy;

        REQUIRE(index == 0U || entity_id > previous_entity);
        previous_entity = entity_id;
        (void)reader_u64(&reader);
        (void)reader_u64(&reader);
        (void)reader_bool(&reader);
        has_transform = reader_bool(&reader);
        has_occupancy = reader_bool(&reader);
        REQUIRE(reader_u8(&reader) == UINT8_C(0));
        if (has_transform)
        {
            reader_skip_transform(&reader);
        }
        if (has_occupancy)
        {
            reader_skip_occupancy(&reader);
        }
    }
    for (index = 0U; index < (size_t)link_count; ++index)
    {
        REQUIRE(reader_u64(&reader) == UINT64_C(1));
        REQUIRE(reader_u64(&reader) == UINT64_C(2));
        (void)reader_u64(&reader);
        (void)reader_u64(&reader);
    }
    for (index = 0U; index < (size_t)command_count; ++index)
    {
        uint64_t command_id = reader_u64(&reader);
        uint64_t target_tick = reader_u64(&reader);
        uint32_t priority = reader_u32(&reader);
        uint64_t issuer = reader_u64(&reader);
        uint32_t type = reader_u32(&reader);
        int has_transform;
        int has_displacement;
        int has_occupancy;
        int has_link;

        if (previous_command_valid != 0)
        {
            REQUIRE(target_tick > previous_target_tick ||
                    (target_tick == previous_target_tick && priority > previous_priority) ||
                    (target_tick == previous_target_tick && priority == previous_priority &&
                     issuer > previous_issuer) ||
                    (target_tick == previous_target_tick && priority == previous_priority &&
                     issuer == previous_issuer && command_id > previous_command_id));
        }
        previous_command_valid = 1;
        previous_target_tick = target_tick;
        previous_priority = priority;
        previous_issuer = issuer;
        previous_command_id = command_id;
        REQUIRE(type >= (uint32_t)MINISNN_WORLDS_KERNEL_COMMAND_CREATE_ENTITY);
        REQUIRE(type <= (uint32_t)MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_SPATIAL_LINK);
        command_types |= UINT32_C(1) << (type - UINT32_C(1));
        (void)reader_u64(&reader);
        has_transform = reader_bool(&reader);
        has_displacement = reader_bool(&reader);
        has_occupancy = reader_bool(&reader);
        has_link = reader_bool(&reader);
        if (has_transform)
        {
            reader_skip_transform(&reader);
        }
        if (has_displacement)
        {
            (void)reader_u64(&reader);
            (void)reader_u64(&reader);
        }
        if (has_occupancy)
        {
            reader_skip_occupancy(&reader);
        }
        if (has_link)
        {
            (void)reader_u64(&reader);
            (void)reader_u64(&reader);
        }
    }
    REQUIRE(command_types == UINT32_C(0x1ff));
    for (index = 0U; index < (size_t)event_count; ++index)
    {
        uint32_t type;
        int has_previous_transform;
        int has_transform;
        int has_displacement;
        int has_occupancy;
        int has_link;

        (void)reader_u64(&reader);
        REQUIRE(reader_u64(&reader) == UINT64_C(6));
        type = reader_u32(&reader);
        (void)reader_u64(&reader);
        (void)reader_u64(&reader);
        (void)reader_u64(&reader);
        (void)reader_u64(&reader);
        (void)reader_u32(&reader);
        has_previous_transform = reader_bool(&reader);
        has_transform = reader_bool(&reader);
        has_displacement = reader_bool(&reader);
        has_occupancy = reader_bool(&reader);
        has_link = reader_bool(&reader);
        (void)reader_u64(&reader);
        if (has_previous_transform)
        {
            reader_skip_transform(&reader);
        }
        if (has_transform)
        {
            reader_skip_transform(&reader);
        }
        if (has_displacement)
        {
            (void)reader_u64(&reader);
            (void)reader_u64(&reader);
        }
        if (has_occupancy)
        {
            reader_skip_occupancy(&reader);
        }
        if (has_link)
        {
            reader_skip_link(&reader);
        }
        saw_move |= type == (uint32_t)MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED;
        saw_rejection |= type == (uint32_t)MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED;
    }
    REQUIRE(saw_move != 0);
    REQUIRE(saw_rejection != 0);
    for (index = 0U; index < (size_t)stream_count; ++index)
    {
        uint64_t namespace_id = reader_u64(&reader);
        uint64_t stream_id = reader_u64(&reader);

        if (previous_stream_valid != 0)
        {
            REQUIRE(namespace_id > previous_namespace ||
                    (namespace_id == previous_namespace && stream_id > previous_stream));
        }
        previous_stream_valid = 1;
        previous_namespace = namespace_id;
        previous_stream = stream_id;
        (void)reader_u64(&reader);
        (void)reader_u64(&reader);
        REQUIRE(reader_u64(&reader) == UINT64_C(1));
    }
    REQUIRE(reader.offset == reader.size);
}
static void test_empty_and_atomic_capture(void)
{
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel = minisnn_worlds_kernel_create(NULL, &error);
    MiniSNNWorldsKernelSnapshot *first = NULL;
    MiniSNNWorldsKernelSnapshot *second = NULL;
    MiniSNNWorldsKernelDiagnostics before;
    MiniSNNWorldsKernelDiagnostics after;
    uint8_t *corrupt_data;

    REQUIRE(kernel != NULL);
    REQUIRE(error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_snapshot_capture(NULL, &first) ==
            MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT);
    REQUIRE(first == NULL);
    REQUIRE(minisnn_worlds_kernel_snapshot_capture(kernel, NULL) ==
            MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT);
    REQUIRE(minisnn_worlds_kernel_get_diagnostics(kernel, &before) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_snapshot_capture(kernel, &first) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_snapshot_capture(kernel, &second) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    validate_header(first);
    REQUIRE(minisnn_worlds_kernel_snapshot_format_version(first) ==
            MINISNN_WORLDS_KERNEL_SNAPSHOT_FORMAT_VERSION_V1);
    REQUIRE(minisnn_worlds_kernel_snapshot_size(first) ==
            minisnn_worlds_kernel_snapshot_size(second));
    REQUIRE(memcmp(minisnn_worlds_kernel_snapshot_data(first),
                   minisnn_worlds_kernel_snapshot_data(second),
                   minisnn_worlds_kernel_snapshot_size(first)) == 0);
    corrupt_data = malloc(minisnn_worlds_kernel_snapshot_size(first));
    REQUIRE(corrupt_data != NULL);
    memcpy(corrupt_data, minisnn_worlds_kernel_snapshot_data(first),
           minisnn_worlds_kernel_snapshot_size(first));
    corrupt_data[0] ^= UINT8_C(1);
    REQUIRE(!snapshot_layout_is_valid(corrupt_data,
                                      minisnn_worlds_kernel_snapshot_size(first)));
    corrupt_data[0] ^= UINT8_C(1);
    corrupt_data[8] = UINT8_C(2);
    REQUIRE(!snapshot_layout_is_valid(corrupt_data,
                                      minisnn_worlds_kernel_snapshot_size(first)));
    corrupt_data[8] = UINT8_C(1);
    memset(corrupt_data + 124U, 0xff, 8U);
    REQUIRE(!snapshot_layout_is_valid(corrupt_data,
                                      minisnn_worlds_kernel_snapshot_size(first)));
    REQUIRE(!snapshot_layout_is_valid(minisnn_worlds_kernel_snapshot_data(first),
                                      minisnn_worlds_kernel_snapshot_size(first) - 1U));
    free(corrupt_data);
    REQUIRE(minisnn_worlds_kernel_get_diagnostics(kernel, &after) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(diagnostics_equal(&before, &after) != 0);
    minisnn_worlds_kernel_snapshot_destroy(second);
    minisnn_worlds_kernel_snapshot_destroy(first);
    minisnn_worlds_kernel_destroy(kernel);
}

static void test_complete_state_and_failure_atomicity(void)
{
    MiniSNNWorldsKernel *kernel = build_full_state();
    MiniSNNWorldsKernelSnapshot *snapshot = NULL;
    MiniSNNWorldsKernelSnapshot *failed = NULL;
    MiniSNNWorldsKernelDiagnostics before;
    MiniSNNWorldsKernelDiagnostics after;
    uint8_t *saved_data;
    size_t size;

    REQUIRE(minisnn_worlds_kernel_get_diagnostics(kernel, &before) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(before.pending_commands == UINT64_C(9));
    REQUIRE(before.active_spatial_links == UINT64_C(1));
    REQUIRE(before.random_streams == UINT64_C(2));
    REQUIRE(before.total_entities_destroyed == UINT64_C(1));
    REQUIRE(minisnn_worlds_kernel_snapshot_capture(kernel, &snapshot) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    validate_header(snapshot);
    validate_complete_payload(snapshot);
    size = minisnn_worlds_kernel_snapshot_size(snapshot);
    saved_data = malloc(size);
    REQUIRE(saved_data != NULL);
    memcpy(saved_data, minisnn_worlds_kernel_snapshot_data(snapshot), size);

    minisnn_worlds_kernel_testing_fail_next_allocation();
    REQUIRE(minisnn_worlds_kernel_snapshot_capture(kernel, &failed) ==
            MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
    REQUIRE(failed == NULL);
    minisnn_worlds_kernel_testing_fail_allocation_after(1U);
    REQUIRE(minisnn_worlds_kernel_snapshot_capture(kernel, &failed) ==
            MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
    REQUIRE(failed == NULL);

    REQUIRE(minisnn_worlds_kernel_get_diagnostics(kernel, &after) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(diagnostics_equal(&before, &after) != 0);

    minisnn_worlds_kernel_destroy(kernel);
    REQUIRE(memcmp(saved_data, minisnn_worlds_kernel_snapshot_data(snapshot), size) == 0);
    free(saved_data);
    minisnn_worlds_kernel_snapshot_destroy(snapshot);
}


static void test_capacity_independent_canonicalization(void)
{
    MiniSNNWorldsKernel *left = build_full_state();
    MiniSNNWorldsKernel *right = build_full_state();
    MiniSNNWorldsKernelSnapshot *left_snapshot = NULL;
    MiniSNNWorldsKernelSnapshot *right_snapshot = NULL;

    REQUIRE(minisnn_worlds_kernel_testing_reserve_entity_capacity(left, 16U) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_testing_reserve_entity_capacity(left, 32U) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_snapshot_capture(left, &left_snapshot) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_snapshot_capture(right, &right_snapshot) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_snapshot_size(left_snapshot) ==
            minisnn_worlds_kernel_snapshot_size(right_snapshot));
    REQUIRE(memcmp(minisnn_worlds_kernel_snapshot_data(left_snapshot),
                   minisnn_worlds_kernel_snapshot_data(right_snapshot),
                   minisnn_worlds_kernel_snapshot_size(left_snapshot)) == 0);
    minisnn_worlds_kernel_snapshot_destroy(right_snapshot);
    minisnn_worlds_kernel_snapshot_destroy(left_snapshot);
    minisnn_worlds_kernel_destroy(right);
    minisnn_worlds_kernel_destroy(left);
}
static void test_checked_size_arithmetic(void)
{
    size_t value;

    REQUIRE(minisnn_worlds_kernel_snapshot_testing_size_add(SIZE_MAX, 1U, &value) == 0);
    REQUIRE(minisnn_worlds_kernel_snapshot_testing_size_multiply(SIZE_MAX, 2U, &value) == 0);
    REQUIRE(minisnn_worlds_kernel_snapshot_testing_size_add(4U, 5U, &value) != 0);
    REQUIRE(value == 9U);
}

int main(void)
{
    MiniSNNWorldsKernel *golden_kernel;
    MiniSNNWorldsKernelSnapshot *golden_snapshot = NULL;
    uint64_t golden_state_hash;

    test_empty_and_atomic_capture();
    test_complete_state_and_failure_atomicity();
    test_capacity_independent_canonicalization();
    test_checked_size_arithmetic();

    golden_kernel = build_full_state();
    REQUIRE(minisnn_worlds_kernel_snapshot_capture(golden_kernel, &golden_snapshot) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_state_hash(golden_kernel, &golden_state_hash) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    printf("K2-A snapshot OK: format=%" PRIu32 " size=%zu digest=0x%016" PRIX64 " state_hash=0x%016" PRIX64 "\n",
           minisnn_worlds_kernel_snapshot_format_version(golden_snapshot),
           minisnn_worlds_kernel_snapshot_size(golden_snapshot),
           fnv1a(minisnn_worlds_kernel_snapshot_data(golden_snapshot),
                 minisnn_worlds_kernel_snapshot_size(golden_snapshot)),
           golden_state_hash);
    minisnn_worlds_kernel_snapshot_destroy(golden_snapshot);
    minisnn_worlds_kernel_destroy(golden_kernel);
    return 0;
}