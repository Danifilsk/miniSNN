#include "minisnn_worlds_kernel.h"
#include "k2_command_log_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static int create_directory(const char *path)
{
#ifdef _WIN32
    return _mkdir(path) == 0;
#else
    return mkdir(path, 0700) == 0;
#endif
}

static int remove_directory(const char *path)
{
#ifdef _WIN32
    return _rmdir(path) == 0;
#else
    return rmdir(path) == 0;
#endif
}

static int file_exists(const char *path)
{
    FILE *file = fopen(path, "rb");

    if (file == NULL)
    {
        return 0;
    }
    return fclose(file) == 0;
}

#define REQUIRE(condition) do { if (!(condition)) { \
    fprintf(stderr, "requirement failed: %s (%s:%d)\n", #condition, __FILE__, __LINE__); \
    return 1; } } while (0)

static MiniSNNWorldsKernelEntityId id(uint64_t value)
{
    MiniSNNWorldsKernelEntityId result;
    result.value = value;
    return result;
}

static MiniSNNWorldsKernelTransform transform(int64_t x, int64_t y)
{
    MiniSNNWorldsKernelTransform result;
    result.position.x = x;
    result.position.y = y;
    result.orientation = UINT32_C(0);
    return result;
}

static MiniSNNWorldsKernelOccupancy occupancy(int64_t half_extent)
{
    MiniSNNWorldsKernelOccupancy result;
    result.half_extent_x = half_extent;
    result.half_extent_y = half_extent;
    result.category_bits = UINT32_C(1);
    result.blocking_mask = UINT32_C(1);
    return result;
}

static int capture_submission(
    MiniSNNWorldsKernelCommandLog *log,
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelCommandId command)
{
    return minisnn_worlds_kernel_command_log_capture_submission(
               log, kernel, minisnn_worlds_kernel_tick(kernel), command) ==
           MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int queue_create(MiniSNNWorldsKernelCommandLog *log, MiniSNNWorldsKernel *kernel,
                        MiniSNNWorldsTick target_tick, uint32_t priority)
{
    MiniSNNWorldsKernelCommandId command;
    return minisnn_worlds_kernel_queue_create_entity(kernel, target_tick, priority, id(0U),
                                                      &command) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           capture_submission(log, kernel, command);
}

static int queue_place(MiniSNNWorldsKernelCommandLog *log, MiniSNNWorldsKernel *kernel,
                       MiniSNNWorldsKernelEntityId entity, MiniSNNWorldsTick target_tick,
                       uint32_t priority, int64_t x, int64_t y)
{
    MiniSNNWorldsKernelCommandId command;
    return minisnn_worlds_kernel_queue_place_entity(kernel, target_tick, priority, id(0U),
                                                     entity, transform(x, y), &command) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           capture_submission(log, kernel, command);
}

static int queue_set_occupancy(MiniSNNWorldsKernelCommandLog *log,
                               MiniSNNWorldsKernel *kernel,
                               MiniSNNWorldsKernelEntityId entity,
                               MiniSNNWorldsTick target_tick,
                               uint32_t priority,
                               int64_t half_extent)
{
    MiniSNNWorldsKernelCommandId command;
    return minisnn_worlds_kernel_queue_set_occupancy(kernel, target_tick, priority, id(0U),
                                                      entity, occupancy(half_extent), &command) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           capture_submission(log, kernel, command);
}

static int queue_link(MiniSNNWorldsKernelCommandLog *log, MiniSNNWorldsKernel *kernel,
                      MiniSNNWorldsTick target_tick, uint32_t priority)
{
    MiniSNNWorldsKernelCommandId command;
    return minisnn_worlds_kernel_queue_create_spatial_link(kernel, target_tick, priority,
                                                            id(0U), id(1U), id(2U), &command) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           capture_submission(log, kernel, command);
}

static int queue_move(MiniSNNWorldsKernelCommandLog *log, MiniSNNWorldsKernel *kernel,
                      MiniSNNWorldsTick target_tick, uint32_t priority)
{
    MiniSNNWorldsKernelCommandId command;
    return minisnn_worlds_kernel_queue_move_entity(kernel, target_tick, priority, id(0U),
                                                    id(1U), 10, 0, &command) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           capture_submission(log, kernel, command);
}

static int queue_destroy_first(MiniSNNWorldsKernelCommandLog *log,
                               MiniSNNWorldsKernel *kernel,
                               MiniSNNWorldsTick target_tick,
                               uint32_t priority)
{
    MiniSNNWorldsKernelCommandId command;
    return minisnn_worlds_kernel_queue_destroy_entity(kernel, target_tick, priority, id(0U),
                                                       id(1U), &command) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           capture_submission(log, kernel, command);
}

static int queue_clear_first(MiniSNNWorldsKernelCommandLog *log,
                             MiniSNNWorldsKernel *kernel,
                             MiniSNNWorldsTick target_tick,
                             uint32_t priority)
{
    MiniSNNWorldsKernelCommandId command;
    return minisnn_worlds_kernel_queue_clear_occupancy(kernel, target_tick, priority, id(0U),
                                                        id(1U), &command) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           capture_submission(log, kernel, command);
}

static int queue_remove_link(MiniSNNWorldsKernelCommandLog *log,
                             MiniSNNWorldsKernel *kernel,
                             MiniSNNWorldsTick target_tick)
{
    MiniSNNWorldsKernelCommandId command;
    return minisnn_worlds_kernel_queue_remove_spatial_link(kernel, target_tick, 0U, id(0U),
                                                            id(1U), id(2U), &command) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           capture_submission(log, kernel, command);
}

static int queue_remove_destroy_third(MiniSNNWorldsKernelCommandLog *log,
                                      MiniSNNWorldsKernel *kernel,
                                      MiniSNNWorldsTick target_tick)
{
    MiniSNNWorldsKernelCommandId first;
    MiniSNNWorldsKernelCommandId second;
    return minisnn_worlds_kernel_queue_remove_entity_from_space(kernel, target_tick, 1U,
                                                                  id(0U), id(3U), &first) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           capture_submission(log, kernel, first) &&
           minisnn_worlds_kernel_queue_destroy_entity(kernel, target_tick, 2U, id(0U), id(3U),
                                                       &second) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           capture_submission(log, kernel, second);
}

static int draw_random(MiniSNNWorldsKernel *kernel)
{
    MiniSNNWorldsKernelRandomStreamKey key = { UINT64_C(33), UINT64_C(9) };
    uint32_t value;

    return minisnn_worlds_kernel_random_u32(kernel, key, &value) ==
           MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int step_kernel(MiniSNNWorldsKernel *kernel)
{
    return draw_random(kernel) &&
           minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int submit_workload_for_tick(MiniSNNWorldsKernelCommandLog *log,
                                    MiniSNNWorldsKernel *kernel)
{
    switch (minisnn_worlds_kernel_tick(kernel))
    {
        case 0U:
            return queue_create(log, kernel, 1U, 2U) &&
                   queue_create(log, kernel, 1U, 0U) &&
                   queue_create(log, kernel, 1U, 1U);
        case 1U:
            return queue_place(log, kernel, id(1U), 2U, 2U, 0, 0) &&
                   queue_place(log, kernel, id(2U), 2U, 0U, 10, 0) &&
                   queue_place(log, kernel, id(3U), 2U, 1U, 80, 0);
        case 2U:
            return queue_set_occupancy(log, kernel, id(1U), 3U, 2U, 10) &&
                   queue_set_occupancy(log, kernel, id(2U), 3U, 0U, 10) &&
                   queue_link(log, kernel, 3U, 1U);
        case 3U:
            return queue_move(log, kernel, 4U, 2U) &&
                   queue_destroy_first(log, kernel, 4U, 0U) &&
                   queue_clear_first(log, kernel, 4U, 1U);
        case 4U:
            return queue_remove_link(log, kernel, 5U) &&
                   queue_remove_destroy_third(log, kernel, 5U);
        default:
            return 1;
    }
}

static MiniSNNWorldsKernel *create_kernel(void)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;

    config.master_seed = UINT64_C(0x4B32435F44454D4F);
    return minisnn_worlds_kernel_create(&config, &error);
}

static int snapshots_equal(MiniSNNWorldsKernel *left, MiniSNNWorldsKernel *right)
{
    MiniSNNWorldsKernelSnapshot *left_snapshot = NULL;
    MiniSNNWorldsKernelSnapshot *right_snapshot = NULL;
    uint64_t left_hash;
    uint64_t right_hash;
    int equal;

    if (minisnn_worlds_kernel_snapshot_capture(left, &left_snapshot) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_snapshot_capture(right, &right_snapshot) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(left, &left_hash) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(right, &right_hash) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_snapshot_destroy(right_snapshot);
        minisnn_worlds_kernel_snapshot_destroy(left_snapshot);
        return 0;
    }
    equal = left_hash == right_hash &&
            minisnn_worlds_kernel_snapshot_size(left_snapshot) ==
                minisnn_worlds_kernel_snapshot_size(right_snapshot) &&
            memcmp(minisnn_worlds_kernel_snapshot_data(left_snapshot),
                   minisnn_worlds_kernel_snapshot_data(right_snapshot),
                   minisnn_worlds_kernel_snapshot_size(left_snapshot)) == 0;
    minisnn_worlds_kernel_snapshot_destroy(right_snapshot);
    minisnn_worlds_kernel_snapshot_destroy(left_snapshot);
    return equal;
}

static int replay_submissions_for_tick(const MiniSNNWorldsKernelCommandLog *log,
                                       MiniSNNWorldsKernel *kernel,
                                       size_t *in_out_cursor)
{
    MiniSNNWorldsKernelCommandLogRecord record;

    while (*in_out_cursor < minisnn_worlds_kernel_command_log_count(log))
    {
        if (minisnn_worlds_kernel_command_log_record_at(log, *in_out_cursor, &record) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return 0;
        }
        if (record.submission_tick != minisnn_worlds_kernel_tick(kernel))
        {
            return 1;
        }
        if (minisnn_worlds_kernel_command_log_replay_next(log, *in_out_cursor, kernel) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return 0;
        }
        ++*in_out_cursor;
    }
    return 1;
}

static int test_replay_and_snapshot_checkpoint(void)
{
    MiniSNNWorldsKernel *original = create_kernel();
    MiniSNNWorldsKernel *replayed = create_kernel();
    MiniSNNWorldsKernel *restored = NULL;
    MiniSNNWorldsKernelCommandLog *log = NULL;
    MiniSNNWorldsKernelSnapshot *checkpoint = NULL;
    MiniSNNWorldsKernelSnapshot *loaded_checkpoint = NULL;
    size_t cursor = 0U;
    size_t checkpoint_cursor = 0U;
    size_t tick;
    static const char log_path[] = "k2_c_command_log_roundtrip.bin";
    static const char replace_failure_path[] = "k2_c_replace_failure";
    static const char replace_failure_temp_path[] = "k2_c_replace_failure.tmp";

    REQUIRE(original != NULL && replayed != NULL);
    REQUIRE(minisnn_worlds_kernel_command_log_create(&log) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    for (tick = 0U; tick < 5U; ++tick)
    {
        REQUIRE(submit_workload_for_tick(log, original));
        REQUIRE(step_kernel(original));
        if (tick == 2U)
        {
            checkpoint_cursor = minisnn_worlds_kernel_command_log_count(log);
            REQUIRE(minisnn_worlds_kernel_snapshot_capture(original, &checkpoint) ==
                    MINISNN_WORLDS_KERNEL_ERROR_NONE);
        }
    }
    REQUIRE(minisnn_worlds_kernel_command_log_count(log) == 15U);
    REQUIRE(minisnn_worlds_kernel_command_log_save_file(log_path, log) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    {
        MiniSNNWorldsKernelCommandLog *loaded_log = NULL;
        REQUIRE(minisnn_worlds_kernel_command_log_load_file(log_path, &loaded_log) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
        REQUIRE(minisnn_worlds_kernel_command_log_size(loaded_log) ==
                minisnn_worlds_kernel_command_log_size(log));
        REQUIRE(memcmp(minisnn_worlds_kernel_command_log_data(loaded_log),
                       minisnn_worlds_kernel_command_log_data(log),
                       minisnn_worlds_kernel_command_log_size(log)) == 0);
        minisnn_worlds_kernel_command_log_destroy(loaded_log);
    }
    REQUIRE(remove(log_path) == 0);
    REQUIRE(create_directory(replace_failure_path));
    REQUIRE(minisnn_worlds_kernel_command_log_save_file(replace_failure_path, log) ==
            MINISNN_WORLDS_KERNEL_ERROR_INTERNAL);
    REQUIRE(!file_exists(replace_failure_temp_path));
    REQUIRE(remove_directory(replace_failure_path));

    for (tick = 0U; tick < 5U; ++tick)
    {
        REQUIRE(replay_submissions_for_tick(log, replayed, &cursor));
        REQUIRE(step_kernel(replayed));
    }
    REQUIRE(cursor == minisnn_worlds_kernel_command_log_count(log));
    REQUIRE(snapshots_equal(original, replayed));

    REQUIRE(minisnn_worlds_kernel_snapshot_from_bytes(
                minisnn_worlds_kernel_snapshot_data(checkpoint),
                minisnn_worlds_kernel_snapshot_size(checkpoint), &loaded_checkpoint) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_create_from_snapshot(loaded_checkpoint, &restored) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    cursor = checkpoint_cursor;
    for (tick = 3U; tick < 5U; ++tick)
    {
        REQUIRE(replay_submissions_for_tick(log, restored, &cursor));
        REQUIRE(step_kernel(restored));
    }
    REQUIRE(cursor == minisnn_worlds_kernel_command_log_count(log));
    REQUIRE(snapshots_equal(original, restored));

    minisnn_worlds_kernel_destroy(restored);
    minisnn_worlds_kernel_snapshot_destroy(loaded_checkpoint);
    minisnn_worlds_kernel_snapshot_destroy(checkpoint);
    minisnn_worlds_kernel_command_log_destroy(log);
    minisnn_worlds_kernel_destroy(replayed);
    minisnn_worlds_kernel_destroy(original);
    return 0;
}

static int test_format_divergence_and_failure_atomicity(void)
{
    MiniSNNWorldsKernelCommandLog *log = NULL;
    MiniSNNWorldsKernelCommandLog *imported = NULL;
    MiniSNNWorldsKernelCommandLog *failed_log = NULL;
    MiniSNNWorldsKernelCommandLogRecord record;
    MiniSNNWorldsKernelCommandLogRecord second;
    MiniSNNWorldsKernel *kernel = create_kernel();
    uint8_t *bytes;
    size_t size;
    size_t count_before;
    size_t index;

    REQUIRE(kernel != NULL);
    for (index = 0U; index < 2U; ++index)
    {
        minisnn_worlds_kernel_testing_fail_allocation_after(index);
        REQUIRE(minisnn_worlds_kernel_command_log_create(&failed_log) ==
                MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
        REQUIRE(failed_log == NULL);
    }
    minisnn_worlds_kernel_testing_fail_allocation_after(SIZE_MAX);
    memset(&record, 0, sizeof(record));
    record.submission_tick = 0U;
    record.command.command_id.value = UINT64_C(1);
    record.command.target_tick = 1U;
    record.command.type = MINISNN_WORLDS_KERNEL_COMMAND_CREATE_ENTITY;
    REQUIRE(minisnn_worlds_kernel_command_log_create(&log) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_command_log_append(log, &record) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    size = minisnn_worlds_kernel_command_log_size(log);
    bytes = malloc(size);
    REQUIRE(bytes != NULL);
    memcpy(bytes, minisnn_worlds_kernel_command_log_data(log), size);
    bytes[0] ^= UINT8_C(1);
    REQUIRE(minisnn_worlds_kernel_command_log_from_bytes(bytes, size, &imported) ==
            MINISNN_WORLDS_KERNEL_ERROR_COMMAND_LOG_INVALID_FORMAT);
    REQUIRE(imported == NULL);
    bytes[0] ^= UINT8_C(1);
    bytes[8] = UINT8_C(2);
    REQUIRE(minisnn_worlds_kernel_command_log_from_bytes(bytes, size, &imported) ==
            MINISNN_WORLDS_KERNEL_ERROR_COMMAND_LOG_UNSUPPORTED_VERSION);
    REQUIRE(imported == NULL);
    memcpy(bytes, minisnn_worlds_kernel_command_log_data(log), size);
    REQUIRE(minisnn_worlds_kernel_command_log_from_bytes(bytes, size - 1U, &imported) ==
            MINISNN_WORLDS_KERNEL_ERROR_COMMAND_LOG_INVALID_FORMAT);
    REQUIRE(imported == NULL);
    memcpy(bytes, minisnn_worlds_kernel_command_log_data(log), size);
    for (index = 0U; index < 3U; ++index)
    {
        minisnn_worlds_kernel_testing_fail_allocation_after(index);
        REQUIRE(minisnn_worlds_kernel_command_log_from_bytes(bytes, size, &imported) ==
                MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
        REQUIRE(imported == NULL);
    }
    minisnn_worlds_kernel_testing_fail_allocation_after(SIZE_MAX);

    second = record;
    second.command.command_id.value = UINT64_C(2);
    second.command.target_tick = 2U;
    second.submission_tick = 1U;
    count_before = minisnn_worlds_kernel_command_log_count(log);
    for (index = 0U; index < 2U; ++index)
    {
        minisnn_worlds_kernel_testing_fail_allocation_after(index);
        REQUIRE(minisnn_worlds_kernel_command_log_append(log, &second) ==
                MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
        REQUIRE(minisnn_worlds_kernel_command_log_count(log) == count_before);
    }
    minisnn_worlds_kernel_testing_fail_allocation_after(SIZE_MAX);
    REQUIRE(minisnn_worlds_kernel_command_log_append(log, &second) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_command_log_replay_next(log, 0U, kernel) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_command_log_replay_next(log, 1U, kernel) ==
            MINISNN_WORLDS_KERNEL_ERROR_REPLAY_DIVERGENCE);
    REQUIRE(minisnn_worlds_kernel_last_error(kernel) ==
            MINISNN_WORLDS_KERNEL_ERROR_REPLAY_DIVERGENCE);

    free(bytes);
    minisnn_worlds_kernel_destroy(kernel);
    minisnn_worlds_kernel_command_log_destroy(log);
    return 0;
}

int main(void)
{
    if (test_replay_and_snapshot_checkpoint() != 0 ||
        test_format_divergence_and_failure_atomicity() != 0)
    {
        return 1;
    }
    puts("K2-C command log, replay and snapshot continuation OK");
    return 0;
}