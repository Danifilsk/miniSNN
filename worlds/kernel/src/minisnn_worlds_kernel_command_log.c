#include "minisnn_worlds_kernel_command_log.h"
#include "minisnn_worlds_kernel_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define MINISNN_WORLDS_KERNEL_COMMAND_LOG_HEADER_SIZE ((size_t)40U)
#define MINISNN_WORLDS_KERNEL_COMMAND_LOG_RECORD_SIZE ((size_t)136U)

static const uint8_t command_log_magic[8] =
{
    (uint8_t)'M', (uint8_t)'S', (uint8_t)'W', (uint8_t)'K',
    (uint8_t)'L', (uint8_t)'O', (uint8_t)'G', (uint8_t)'1'
};

struct MiniSNNWorldsKernelCommandLog
{
    MiniSNNWorldsKernelCommandLogRecord *records;
    size_t count;
    uint8_t *data;
    size_t size;
    uint64_t digest;
};
struct MiniSNNWorldsKernelReplaySession
{
    const MiniSNNWorldsKernelCommandLog *log;
    size_t cursor;
    uint64_t expected_state_hash;
    int binding_validated;
};

typedef struct
{
    const uint8_t *data;
    size_t size;
    size_t offset;
} CommandLogReader;

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

static void writer_u32(uint8_t *data, size_t *offset, uint32_t value)
{
    size_t index;

    for (index = 0U; index < 4U; ++index)
    {
        data[*offset + index] = (uint8_t)(value >> (index * 8U));
    }
    *offset += 4U;
}

static void writer_u64(uint8_t *data, size_t *offset, uint64_t value)
{
    size_t index;

    for (index = 0U; index < 8U; ++index)
    {
        data[*offset + index] = (uint8_t)(value >> (index * 8U));
    }
    *offset += 8U;
}

static void writer_i64(uint8_t *data, size_t *offset, int64_t value)
{
    writer_u64(data, offset, (uint64_t)value);
}

static int reader_take(CommandLogReader *reader, size_t count, const uint8_t **out_data)
{
    const uint8_t *result;

    if (reader == NULL || out_data == NULL || count > reader->size - reader->offset)
    {
        return 0;
    }
    result = reader->data + reader->offset;
    reader->offset += count;
    *out_data = result;
    return 1;
}

static int reader_u8(CommandLogReader *reader, uint8_t *out_value)
{
    const uint8_t *data;

    if (out_value == NULL || !reader_take(reader, 1U, &data))
    {
        return 0;
    }
    *out_value = data[0];
    return 1;
}

static int reader_u32(CommandLogReader *reader, uint32_t *out_value)
{
    const uint8_t *data;
    uint32_t value = UINT32_C(0);
    size_t index;

    if (out_value == NULL || !reader_take(reader, 4U, &data))
    {
        return 0;
    }
    for (index = 0U; index < 4U; ++index)
    {
        value |= (uint32_t)data[index] << (index * 8U);
    }
    *out_value = value;
    return 1;
}

static int reader_u64(CommandLogReader *reader, uint64_t *out_value)
{
    const uint8_t *data;
    uint64_t value = UINT64_C(0);
    size_t index;

    if (out_value == NULL || !reader_take(reader, 8U, &data))
    {
        return 0;
    }
    for (index = 0U; index < 8U; ++index)
    {
        value |= (uint64_t)data[index] << (index * 8U);
    }
    *out_value = value;
    return 1;
}

static int reader_i64(CommandLogReader *reader, int64_t *out_value)
{
    uint64_t value;

    if (out_value == NULL || !reader_u64(reader, &value))
    {
        return 0;
    }
    *out_value = (int64_t)value;
    return 1;
}

static int command_type_is_valid(MiniSNNWorldsKernelCommandType type)
{
    return type >= MINISNN_WORLDS_KERNEL_COMMAND_CREATE_ENTITY &&
           type <= MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_SPATIAL_LINK;
}

static int transform_is_zero(MiniSNNWorldsKernelTransform transform)
{
    return transform.position.x == MINISNN_WORLDS_KERNEL_SCALAR_ZERO &&
           transform.position.y == MINISNN_WORLDS_KERNEL_SCALAR_ZERO &&
           transform.orientation == UINT32_C(0);
}

static int position_is_zero(MiniSNNWorldsKernelPosition position)
{
    return position.x == MINISNN_WORLDS_KERNEL_SCALAR_ZERO &&
           position.y == MINISNN_WORLDS_KERNEL_SCALAR_ZERO;
}

static int occupancy_is_zero(MiniSNNWorldsKernelOccupancy occupancy)
{
    return occupancy.half_extent_x == MINISNN_WORLDS_KERNEL_SCALAR_ZERO &&
           occupancy.half_extent_y == MINISNN_WORLDS_KERNEL_SCALAR_ZERO &&
           occupancy.category_bits == UINT32_C(0) && occupancy.blocking_mask == UINT32_C(0);
}

static int endpoints_are_zero(MiniSNNWorldsKernelSpatialLinkEndpoints endpoints)
{
    return endpoints.parent.value == UINT64_C(0) && endpoints.child.value == UINT64_C(0);
}

static int record_is_canonical(const MiniSNNWorldsKernelCommandLogRecord *record)
{
    const MiniSNNWorldsKernelCommandInfo *command;
    int needs_transform;
    int needs_displacement;
    int needs_occupancy;
    int needs_endpoints;

    if (record == NULL)
    {
        return 0;
    }
    command = &record->command;
    if (record->submission_tick >= command->target_tick ||
        command->command_id.value == UINT64_C(0) || !command_type_is_valid(command->type))
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
    needs_endpoints = command->type == MINISNN_WORLDS_KERNEL_COMMAND_CREATE_SPATIAL_LINK ||
                      command->type == MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_SPATIAL_LINK;
    if ((command->has_transform != 0) != needs_transform ||
        (command->has_displacement != 0) != needs_displacement ||
        (command->has_occupancy != 0) != needs_occupancy ||
        (command->has_spatial_link_endpoints != 0) != needs_endpoints ||
        (needs_transform != 0 && command->transform.orientation >=
             MINISNN_WORLDS_KERNEL_ORIENTATION_FULL_TURN) ||
        (needs_occupancy != 0 &&
         (command->occupancy.half_extent_x <= MINISNN_WORLDS_KERNEL_SCALAR_ZERO ||
          command->occupancy.half_extent_y <= MINISNN_WORLDS_KERNEL_SCALAR_ZERO)) ||
        (needs_endpoints != 0 &&
         (command->spatial_link_endpoints.parent.value == UINT64_C(0) ||
          command->spatial_link_endpoints.child.value == UINT64_C(0) ||
          command->spatial_link_endpoints.parent.value != command->target_entity.value)) ||
        (needs_transform == 0 && !transform_is_zero(command->transform)) ||
        (needs_displacement == 0 && !position_is_zero(command->displacement)) ||
        (needs_occupancy == 0 && !occupancy_is_zero(command->occupancy)) ||
        (needs_endpoints == 0 && !endpoints_are_zero(command->spatial_link_endpoints)))
    {
        return 0;
    }
    return 1;
}

static int records_are_in_submission_order(
    const MiniSNNWorldsKernelCommandLogRecord *records,
    size_t count)
{
    size_t index;

    if (count != 0U && records == NULL)
    {
        return 0;
    }
    for (index = 0U; index < count; ++index)
    {
        if (!record_is_canonical(&records[index]) ||
            (index != 0U &&
             (records[index - 1U].submission_tick > records[index].submission_tick ||
              records[index - 1U].command.command_id.value >=
                  records[index].command.command_id.value)))
        {
            return 0;
        }
    }
    return 1;
}

static int calculate_encoded_size(size_t count, size_t *out_size)
{
    size_t payload_size;

    return out_size != NULL &&
           size_multiply_checked(count, MINISNN_WORLDS_KERNEL_COMMAND_LOG_RECORD_SIZE,
                                 &payload_size) &&
           size_add_checked(MINISNN_WORLDS_KERNEL_COMMAND_LOG_HEADER_SIZE,
                            payload_size, out_size);
}

static void encode_record(
    uint8_t *data,
    size_t *offset,
    const MiniSNNWorldsKernelCommandLogRecord *record)
{
    const MiniSNNWorldsKernelCommandInfo *command = &record->command;

    writer_u64(data, offset, record->submission_tick);
    writer_u64(data, offset, command->command_id.value);
    writer_u64(data, offset, command->target_tick);
    writer_u32(data, offset, command->priority);
    writer_u32(data, offset, (uint32_t)command->type);
    writer_u64(data, offset, command->issuer.value);
    writer_u64(data, offset, command->target_entity.value);
    data[(*offset)++] = command->has_transform ? UINT8_C(1) : UINT8_C(0);
    data[(*offset)++] = command->has_displacement ? UINT8_C(1) : UINT8_C(0);
    data[(*offset)++] = command->has_occupancy ? UINT8_C(1) : UINT8_C(0);
    data[(*offset)++] = command->has_spatial_link_endpoints ? UINT8_C(1) : UINT8_C(0);
    writer_u32(data, offset, UINT32_C(0));
    writer_i64(data, offset, command->transform.position.x);
    writer_i64(data, offset, command->transform.position.y);
    writer_u32(data, offset, command->transform.orientation);
    writer_u32(data, offset, UINT32_C(0));
    writer_i64(data, offset, command->displacement.x);
    writer_i64(data, offset, command->displacement.y);
    writer_i64(data, offset, command->occupancy.half_extent_x);
    writer_i64(data, offset, command->occupancy.half_extent_y);
    writer_u32(data, offset, command->occupancy.category_bits);
    writer_u32(data, offset, command->occupancy.blocking_mask);
    writer_u64(data, offset, command->spatial_link_endpoints.parent.value);
    writer_u64(data, offset, command->spatial_link_endpoints.child.value);
}

static uint8_t *encode_records(
    const MiniSNNWorldsKernelCommandLogRecord *records,
    size_t count,
    size_t *out_size,
    uint64_t *out_digest)
{
    uint8_t *data;
    size_t size;
    size_t payload_size;
    size_t offset = 0U;
    size_t index;
    uint64_t digest;

    if (out_size == NULL || out_digest == NULL || !records_are_in_submission_order(records, count) ||
        !calculate_encoded_size(count, &size) ||
        !size_multiply_checked(count, MINISNN_WORLDS_KERNEL_COMMAND_LOG_RECORD_SIZE,
                               &payload_size))
    {
        return NULL;
    }
    data = minisnn_worlds_kernel_internal_allocate(size);
    if (data == NULL)
    {
        return NULL;
    }
    memcpy(data + offset, command_log_magic, sizeof(command_log_magic));
    offset += sizeof(command_log_magic);
    writer_u32(data, &offset, MINISNN_WORLDS_KERNEL_COMMAND_LOG_FORMAT_VERSION_V1);
    writer_u32(data, &offset, UINT32_C(0));
    writer_u64(data, &offset, (uint64_t)count);
    writer_u64(data, &offset, (uint64_t)payload_size);
    writer_u64(data, &offset, UINT64_C(0));
    for (index = 0U; index < count; ++index)
    {
        encode_record(data, &offset, &records[index]);
    }
    /* The digest excludes its own eight-byte field and covers canonical bytes. */

    digest = UINT64_C(14695981039346656037);
    for (index = 0U; index < MINISNN_WORLDS_KERNEL_COMMAND_LOG_HEADER_SIZE - 8U; ++index)
    {
        digest ^= data[index];
        digest *= UINT64_C(1099511628211);
    }
    for (index = MINISNN_WORLDS_KERNEL_COMMAND_LOG_HEADER_SIZE; index < size; ++index)
    {
        digest ^= data[index];
        digest *= UINT64_C(1099511628211);
    }
    offset = MINISNN_WORLDS_KERNEL_COMMAND_LOG_HEADER_SIZE - 8U;
    writer_u64(data, &offset, digest);
    *out_size = size;
    *out_digest = digest;
    return data;
}

static int reader_record(CommandLogReader *reader, MiniSNNWorldsKernelCommandLogRecord *out_record)
{
    uint32_t type;
    uint8_t has_transform;
    uint8_t has_displacement;
    uint8_t has_occupancy;
    uint8_t has_endpoints;
    uint32_t reserved;

    if (reader == NULL || out_record == NULL ||
        !reader_u64(reader, &out_record->submission_tick) ||
        !reader_u64(reader, &out_record->command.command_id.value) ||
        !reader_u64(reader, &out_record->command.target_tick) ||
        !reader_u32(reader, &out_record->command.priority) ||
        !reader_u32(reader, &type) ||
        !reader_u64(reader, &out_record->command.issuer.value) ||
        !reader_u64(reader, &out_record->command.target_entity.value) ||
        !reader_u8(reader, &has_transform) || !reader_u8(reader, &has_displacement) ||
        !reader_u8(reader, &has_occupancy) || !reader_u8(reader, &has_endpoints) ||
        !reader_u32(reader, &reserved) || reserved != UINT32_C(0) ||
        has_transform > UINT8_C(1) || has_displacement > UINT8_C(1) ||
        has_occupancy > UINT8_C(1) || has_endpoints > UINT8_C(1) ||
        !reader_i64(reader, &out_record->command.transform.position.x) ||
        !reader_i64(reader, &out_record->command.transform.position.y) ||
        !reader_u32(reader, &out_record->command.transform.orientation) ||
        !reader_u32(reader, &reserved) || reserved != UINT32_C(0) ||
        !reader_i64(reader, &out_record->command.displacement.x) ||
        !reader_i64(reader, &out_record->command.displacement.y) ||
        !reader_i64(reader, &out_record->command.occupancy.half_extent_x) ||
        !reader_i64(reader, &out_record->command.occupancy.half_extent_y) ||
        !reader_u32(reader, &out_record->command.occupancy.category_bits) ||
        !reader_u32(reader, &out_record->command.occupancy.blocking_mask) ||
        !reader_u64(reader, &out_record->command.spatial_link_endpoints.parent.value) ||
        !reader_u64(reader, &out_record->command.spatial_link_endpoints.child.value))
    {
        return 0;
    }
    out_record->command.type = (MiniSNNWorldsKernelCommandType)type;
    out_record->command.has_transform = has_transform != UINT8_C(0);
    out_record->command.has_displacement = has_displacement != UINT8_C(0);
    out_record->command.has_occupancy = has_occupancy != UINT8_C(0);
    out_record->command.has_spatial_link_endpoints = has_endpoints != UINT8_C(0);
    return 1;
}

static MiniSNNWorldsKernelError dispatch_command(
    MiniSNNWorldsKernel *kernel,
    const MiniSNNWorldsKernelCommandInfo *command,
    MiniSNNWorldsKernelCommandId *out_id)
{
    switch (command->type)
    {
        case MINISNN_WORLDS_KERNEL_COMMAND_CREATE_ENTITY:
            return minisnn_worlds_kernel_queue_create_entity(kernel, command->target_tick,
                command->priority, command->issuer, out_id);
        case MINISNN_WORLDS_KERNEL_COMMAND_DESTROY_ENTITY:
            return minisnn_worlds_kernel_queue_destroy_entity(kernel, command->target_tick,
                command->priority, command->issuer, command->target_entity, out_id);
        case MINISNN_WORLDS_KERNEL_COMMAND_PLACE_ENTITY:
            return minisnn_worlds_kernel_queue_place_entity(kernel, command->target_tick,
                command->priority, command->issuer, command->target_entity,
                command->transform, out_id);
        case MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_ENTITY_FROM_SPACE:
            return minisnn_worlds_kernel_queue_remove_entity_from_space(kernel,
                command->target_tick, command->priority, command->issuer,
                command->target_entity, out_id);
        case MINISNN_WORLDS_KERNEL_COMMAND_SET_OCCUPANCY:
            return minisnn_worlds_kernel_queue_set_occupancy(kernel, command->target_tick,
                command->priority, command->issuer, command->target_entity,
                command->occupancy, out_id);
        case MINISNN_WORLDS_KERNEL_COMMAND_CLEAR_OCCUPANCY:
            return minisnn_worlds_kernel_queue_clear_occupancy(kernel, command->target_tick,
                command->priority, command->issuer, command->target_entity, out_id);
        case MINISNN_WORLDS_KERNEL_COMMAND_MOVE_ENTITY:
            return minisnn_worlds_kernel_queue_move_entity(kernel, command->target_tick,
                command->priority, command->issuer, command->target_entity,
                command->displacement.x, command->displacement.y, out_id);
        case MINISNN_WORLDS_KERNEL_COMMAND_CREATE_SPATIAL_LINK:
            return minisnn_worlds_kernel_queue_create_spatial_link(kernel,
                command->target_tick, command->priority, command->issuer,
                command->spatial_link_endpoints.parent,
                command->spatial_link_endpoints.child, out_id);
        case MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_SPATIAL_LINK:
            return minisnn_worlds_kernel_queue_remove_spatial_link(kernel,
                command->target_tick, command->priority, command->issuer,
                command->spatial_link_endpoints.parent,
                command->spatial_link_endpoints.child, out_id);
        default:
            return MINISNN_WORLDS_KERNEL_ERROR_REPLAY_DIVERGENCE;
    }
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_command_log_create(
    MiniSNNWorldsKernelCommandLog **out_log)
{
    MiniSNNWorldsKernelCommandLog *log;

    if (out_log == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    *out_log = NULL;
    log = minisnn_worlds_kernel_internal_allocate(sizeof(*log));
    if (log == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    memset(log, 0, sizeof(*log));
    log->data = encode_records(NULL, 0U, &log->size, &log->digest);
    if (log->data == NULL)
    {
        free(log);
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    *out_log = log;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

void minisnn_worlds_kernel_command_log_destroy(MiniSNNWorldsKernelCommandLog *log)
{
    if (log != NULL)
    {
        free(log->data);
        free(log->records);
        free(log);
    }
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_command_log_append(
    MiniSNNWorldsKernelCommandLog *log,
    const MiniSNNWorldsKernelCommandLogRecord *record)
{
    MiniSNNWorldsKernelCommandLogRecord *records;
    uint8_t *data;
    size_t count;
    size_t size;
    uint64_t digest;

    if (log == NULL || record == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (!record_is_canonical(record) || log->count == SIZE_MAX ||
        (log->count != 0U &&
         (log->records[log->count - 1U].submission_tick > record->submission_tick ||
          log->records[log->count - 1U].command.command_id.value >=
              record->command.command_id.value)))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INVALID_COMMAND;
    }
    count = log->count + 1U;
    if (count > SIZE_MAX / sizeof(*records))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    records = minisnn_worlds_kernel_internal_allocate(count * sizeof(*records));
    if (records == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    if (log->count != 0U)
    {
        memcpy(records, log->records, log->count * sizeof(*records));
    }
    records[log->count] = *record;
    data = encode_records(records, count, &size, &digest);
    if (data == NULL)
    {
        free(records);
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    free(log->records);
    free(log->data);
    log->records = records;
    log->count = count;
    log->data = data;
    log->size = size;
    log->digest = digest;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_command_log_capture_submission(
    MiniSNNWorldsKernelCommandLog *log,
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick submission_tick,
    MiniSNNWorldsKernelCommandId command_id)
{
    MiniSNNWorldsKernelCommandLogRecord record;
    size_t index;

    if (log == NULL || kernel == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (minisnn_worlds_kernel_command_batch_active(kernel))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE;
    }
    if (kernel->tick != submission_tick || command_id.value == UINT64_C(0))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INVALID_TICK;
    }
    for (index = 0U; index < kernel->pending_command_count; ++index)
    {
        if (kernel->pending_commands[index].command_id.value == command_id.value)
        {
            record.submission_tick = submission_tick;
            record.command = kernel->pending_commands[index];
            return minisnn_worlds_kernel_command_log_append(log, &record);
        }
    }
    return MINISNN_WORLDS_KERNEL_ERROR_INVALID_COMMAND;
}

size_t minisnn_worlds_kernel_command_log_count(const MiniSNNWorldsKernelCommandLog *log)
{
    return log == NULL ? 0U : log->count;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_command_log_record_at(
    const MiniSNNWorldsKernelCommandLog *log,
    size_t index,
    MiniSNNWorldsKernelCommandLogRecord *out_record)
{
    if (log == NULL || out_record == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (index >= log->count)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INDEX_OUT_OF_RANGE;
    }
    *out_record = log->records[index];
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

const uint8_t *minisnn_worlds_kernel_command_log_data(const MiniSNNWorldsKernelCommandLog *log)
{
    return log == NULL ? NULL : log->data;
}

size_t minisnn_worlds_kernel_command_log_size(const MiniSNNWorldsKernelCommandLog *log)
{
    return log == NULL ? 0U : log->size;
}

uint64_t minisnn_worlds_kernel_command_log_digest(const MiniSNNWorldsKernelCommandLog *log)
{
    return log == NULL ? UINT64_C(0) : log->digest;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_command_log_from_bytes(
    const uint8_t *data,
    size_t size,
    MiniSNNWorldsKernelCommandLog **out_log)
{
    CommandLogReader reader;
    const uint8_t *magic;
    uint32_t version;
    uint32_t reserved;
    uint64_t count_value;
    uint64_t payload_value;
    uint64_t stored_digest;
    size_t count;
    size_t payload_size;
    size_t expected_size;
    MiniSNNWorldsKernelCommandLogRecord *records = NULL;
    MiniSNNWorldsKernelCommandLog *log = NULL;
    uint8_t *canonical = NULL;
    size_t canonical_size;
    uint64_t canonical_digest;
    size_t index;

    if (out_log == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    *out_log = NULL;
    if (data == NULL || size == 0U)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    reader.data = data;
    reader.size = size;
    reader.offset = 0U;
    if (!reader_take(&reader, sizeof(command_log_magic), &magic) ||
        memcmp(magic, command_log_magic, sizeof(command_log_magic)) != 0 ||
        !reader_u32(&reader, &version) || !reader_u32(&reader, &reserved) ||
        !reader_u64(&reader, &count_value) || !reader_u64(&reader, &payload_value) ||
        !reader_u64(&reader, &stored_digest) || reserved != UINT32_C(0))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_COMMAND_LOG_INVALID_FORMAT;
    }
    if (version != MINISNN_WORLDS_KERNEL_COMMAND_LOG_FORMAT_VERSION_V1)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_COMMAND_LOG_UNSUPPORTED_VERSION;
    }
    if (count_value > (uint64_t)SIZE_MAX || payload_value > (uint64_t)SIZE_MAX ||
        !size_multiply_checked((size_t)count_value,
                               MINISNN_WORLDS_KERNEL_COMMAND_LOG_RECORD_SIZE,
                               &payload_size) || payload_size != (size_t)payload_value ||
        !size_add_checked(MINISNN_WORLDS_KERNEL_COMMAND_LOG_HEADER_SIZE, payload_size,
                          &expected_size) || expected_size != size)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_COMMAND_LOG_INVALID_FORMAT;
    }
    count = (size_t)count_value;
    if (count > SIZE_MAX / sizeof(*records))
    {
        return MINISNN_WORLDS_KERNEL_ERROR_COMMAND_LOG_INVALID_FORMAT;
    }
    if (count != 0U)
    {
        records = minisnn_worlds_kernel_internal_allocate(count * sizeof(*records));
        if (records == NULL)
        {
            return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
        }
        memset(records, 0, count * sizeof(*records));
    }
    for (index = 0U; index < count; ++index)
    {
        if (!reader_record(&reader, &records[index]))
        {
            free(records);
            return MINISNN_WORLDS_KERNEL_ERROR_COMMAND_LOG_INVALID_FORMAT;
        }
    }
    if (reader.offset != size || !records_are_in_submission_order(records, count))
    {
        free(records);
        return MINISNN_WORLDS_KERNEL_ERROR_COMMAND_LOG_INVALID_FORMAT;
    }
    canonical = encode_records(records, count, &canonical_size, &canonical_digest);
    if (canonical == NULL)
    {
        free(records);
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    if (canonical_size != size || canonical_digest != stored_digest ||
        memcmp(canonical, data, size) != 0)
    {
        free(canonical);
        free(records);
        return MINISNN_WORLDS_KERNEL_ERROR_COMMAND_LOG_INVALID_FORMAT;
    }
    log = minisnn_worlds_kernel_internal_allocate(sizeof(*log));
    if (log == NULL)
    {
        free(canonical);
        free(records);
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    memset(log, 0, sizeof(*log));
    log->records = records;
    log->count = count;
    log->data = canonical;
    log->size = canonical_size;
    log->digest = canonical_digest;
    *out_log = log;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_command_log_replay_next(
    const MiniSNNWorldsKernelCommandLog *log,
    size_t index,
    MiniSNNWorldsKernel *kernel)
{
    const MiniSNNWorldsKernelCommandLogRecord *record;
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelError error;

    if (log == NULL || kernel == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (index >= log->count)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INDEX_OUT_OF_RANGE;
    }
    record = &log->records[index];
    if (kernel->state != MINISNN_WORLDS_KERNEL_STATE_READY ||
        kernel->tick != record->submission_tick ||
        kernel->next_command_id.value != record->command.command_id.value)
    {
        kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_REPLAY_DIVERGENCE;
        return kernel->last_error;
    }
    error = dispatch_command(kernel, &record->command, &command_id);
    if (error != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        command_id.value != record->command.command_id.value)
    {
        if (error == MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_REPLAY_DIVERGENCE;
            return kernel->last_error;
        }
        return error;
    }
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int replay_session_cursor_matches_kernel(
    const MiniSNNWorldsKernelReplaySession *session,
    const MiniSNNWorldsKernel *kernel)
{
    const MiniSNNWorldsKernelCommandLogRecord *record;

    if (session->cursor == session->log->count)
    {
        if (session->log->count == 0U)
        {
            return 1;
        }
        record = &session->log->records[session->log->count - 1U];
        return record->command.command_id.value != UINT64_MAX &&
               kernel->next_command_id.value == record->command.command_id.value + 1U;
    }
    record = &session->log->records[session->cursor];
    return kernel->tick == record->submission_tick &&
           kernel->next_command_id.value == record->command.command_id.value;
}MiniSNNWorldsKernelError minisnn_worlds_kernel_replay_session_create(
    const MiniSNNWorldsKernelCommandLog *log,
    size_t initial_cursor,
    uint64_t expected_state_hash,
    MiniSNNWorldsKernelReplaySession **out_session)
{
    MiniSNNWorldsKernelReplaySession *session;

    if (out_session == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    *out_session = NULL;
    if (log == NULL || initial_cursor > log->count)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INVALID_ARGUMENT;
    }
    session = minisnn_worlds_kernel_internal_allocate(sizeof(*session));
    if (session == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    session->log = log;
    session->cursor = initial_cursor;
    session->expected_state_hash = expected_state_hash;
    session->binding_validated = 0;
    *out_session = session;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

void minisnn_worlds_kernel_replay_session_destroy(
    MiniSNNWorldsKernelReplaySession *session)
{
    free(session);
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_replay_session_validate(
    MiniSNNWorldsKernelReplaySession *session,
    MiniSNNWorldsKernel *kernel)
{
    uint64_t actual_hash;
    MiniSNNWorldsKernelError error;

    if (session == NULL || kernel == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (session->cursor > session->log->count)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INVALID_ARGUMENT;
    }
    error = minisnn_worlds_kernel_state_hash(kernel, &actual_hash);
    if (error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return error;
    }
    if (actual_hash != session->expected_state_hash ||
        !replay_session_cursor_matches_kernel(session, kernel))
    {
        kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_REPLAY_DIVERGENCE;
        return kernel->last_error;
    }
    session->binding_validated = 1;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_replay_session_replay_next(
    MiniSNNWorldsKernelReplaySession *session,
    MiniSNNWorldsKernel *kernel)
{
    MiniSNNWorldsKernelError error;

    if (session == NULL || kernel == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (!session->binding_validated)
    {
        error = minisnn_worlds_kernel_replay_session_validate(session, kernel);
        if (error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return error;
        }
    }
    error = minisnn_worlds_kernel_command_log_replay_next(session->log,
                                                           session->cursor,
                                                           kernel);
    if (error == MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        ++session->cursor;
    }
    return error;
}

size_t minisnn_worlds_kernel_replay_session_cursor(
    const MiniSNNWorldsKernelReplaySession *session)
{
    return session == NULL ? 0U : session->cursor;
}