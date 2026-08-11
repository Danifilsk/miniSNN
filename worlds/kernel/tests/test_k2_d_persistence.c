#include "k2_snapshot_file.h"
#define main k2_c_command_log_fixture_main
#include "test_k2_c_command_log.c"
#undef main

#define K2D_TICKS 1000U
#define K2D_CHECKPOINT_COUNT 6U

static int snapshots_equal_bytes(const MiniSNNWorldsKernelSnapshot *left,
                                 const MiniSNNWorldsKernelSnapshot *right)
{
    return left != NULL && right != NULL &&
           minisnn_worlds_kernel_snapshot_size(left) ==
               minisnn_worlds_kernel_snapshot_size(right) &&
           memcmp(minisnn_worlds_kernel_snapshot_data(left),
                  minisnn_worlds_kernel_snapshot_data(right),
                  minisnn_worlds_kernel_snapshot_size(left)) == 0;
}

static int capture_snapshot_and_hash(MiniSNNWorldsKernel *kernel,
                                     MiniSNNWorldsKernelSnapshot **out_snapshot,
                                     uint64_t *out_hash)
{
    minisnn_worlds_kernel_snapshot_destroy(*out_snapshot);
    *out_snapshot = NULL;
    return minisnn_worlds_kernel_snapshot_capture(kernel, out_snapshot) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           minisnn_worlds_kernel_state_hash(kernel, out_hash) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int replay_one_tick(MiniSNNWorldsKernelReplaySession *session,
                           const MiniSNNWorldsKernelCommandLog *log,
                           MiniSNNWorldsKernel *kernel)
{
    MiniSNNWorldsKernelCommandLogRecord record;
    size_t cursor = minisnn_worlds_kernel_replay_session_cursor(session);

    while (cursor < minisnn_worlds_kernel_command_log_count(log))
    {
        if (minisnn_worlds_kernel_command_log_record_at(log, cursor, &record) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
            record.submission_tick != minisnn_worlds_kernel_tick(kernel))
        {
            break;
        }
        if (minisnn_worlds_kernel_replay_session_replay_next(session, kernel) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return 0;
        }
        cursor = minisnn_worlds_kernel_replay_session_cursor(session);
    }
    return step_kernel(kernel);
}

static int queue_overflow_move(MiniSNNWorldsKernelCommandLog *log,
                               MiniSNNWorldsKernel *kernel)
{
    MiniSNNWorldsKernelCommandId command;

    return minisnn_worlds_kernel_queue_move_entity(
               kernel, minisnn_worlds_kernel_tick(kernel) + 1U, 3U, id(0U), id(1U),
               INT64_MAX, 0, &command) == MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           capture_submission(log, kernel, command);
}

static int source_submit_one(MiniSNNWorldsKernelCommandLog *log,
                             MiniSNNWorldsKernel *kernel)
{
    MiniSNNWorldsTick tick = minisnn_worlds_kernel_tick(kernel);

    if (tick <= 4U)
    {
        return (tick != 4U || queue_overflow_move(log, kernel)) &&
               submit_workload_for_tick(log, kernel);
    }
    return queue_move(log, kernel, tick + 1U, 0U);
}

static int checkpoint_index_for_tick(size_t tick)
{
    static const size_t ticks[K2D_CHECKPOINT_COUNT] =
        { 0U, 100U, 250U, 500U, 750U, 1000U };
    size_t index;

    for (index = 0U; index < K2D_CHECKPOINT_COUNT; ++index)
    {
        if (ticks[index] == tick)
        {
            return (int)index;
        }
    }
    return -1;
}

static int compare_final(MiniSNNWorldsKernel *expected, MiniSNNWorldsKernel *actual)
{
    MiniSNNWorldsKernelSnapshot *expected_snapshot = NULL;
    MiniSNNWorldsKernelSnapshot *actual_snapshot = NULL;
    MiniSNNWorldsKernelDiagnostics expected_diagnostics;
    MiniSNNWorldsKernelDiagnostics actual_diagnostics;
    size_t random_index;
    int random_equal = 1;
    int equal;

    if (minisnn_worlds_kernel_snapshot_capture(expected, &expected_snapshot) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_snapshot_capture(actual, &actual_snapshot) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_get_diagnostics(expected, &expected_diagnostics) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_get_diagnostics(actual, &actual_diagnostics) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_snapshot_destroy(actual_snapshot);
        minisnn_worlds_kernel_snapshot_destroy(expected_snapshot);
        return 0;
    }
    if (minisnn_worlds_kernel_random_stream_count(expected) !=
        minisnn_worlds_kernel_random_stream_count(actual))
    {
        random_equal = 0;
    }
    for (random_index = 0U;
         random_equal && random_index < minisnn_worlds_kernel_random_stream_count(expected);
         ++random_index)
    {
        MiniSNNWorldsKernelRandomStreamInfo expected_stream;
        MiniSNNWorldsKernelRandomStreamInfo actual_stream;

        if (minisnn_worlds_kernel_random_stream_at(expected, random_index, &expected_stream) !=
                MINISNN_WORLDS_KERNEL_ERROR_NONE ||
            minisnn_worlds_kernel_random_stream_at(actual, random_index, &actual_stream) !=
                MINISNN_WORLDS_KERNEL_ERROR_NONE ||
            expected_stream.key.namespace_id != actual_stream.key.namespace_id ||
            expected_stream.key.stream_id != actual_stream.key.stream_id ||
            expected_stream.state != actual_stream.state ||
            expected_stream.sequence != actual_stream.sequence ||
            expected_stream.generated_u32_count != actual_stream.generated_u32_count)
        {
            random_equal = 0;
        }
    }
    equal = snapshots_equal_bytes(expected_snapshot, actual_snapshot) &&
            random_equal &&
            expected_diagnostics.completed_ticks == actual_diagnostics.completed_ticks &&
            expected_diagnostics.total_commands_submitted ==
                actual_diagnostics.total_commands_submitted &&
            expected_diagnostics.total_commands_applied ==
                actual_diagnostics.total_commands_applied &&
            expected_diagnostics.total_events_emitted ==
                actual_diagnostics.total_events_emitted &&
            expected_diagnostics.total_random_u32_generated ==
                actual_diagnostics.total_random_u32_generated;
    minisnn_worlds_kernel_snapshot_destroy(actual_snapshot);
    minisnn_worlds_kernel_snapshot_destroy(expected_snapshot);
    return equal;
}

int main(void)
{
    MiniSNNWorldsKernel *source = create_kernel();
    MiniSNNWorldsKernel *full_replay = create_kernel();
    MiniSNNWorldsKernel *multi_restore = NULL;
    MiniSNNWorldsKernelCommandLog *log = NULL;
    MiniSNNWorldsKernelReplaySession *session = NULL;
    MiniSNNWorldsKernelSnapshot *checkpoints[K2D_CHECKPOINT_COUNT] =
        { NULL, NULL, NULL, NULL, NULL, NULL };
    uint64_t checkpoint_hashes[K2D_CHECKPOINT_COUNT] = { 0U };
    size_t checkpoint_cursors[K2D_CHECKPOINT_COUNT] = { 0U };
    uint64_t initial_hash;
    size_t tick;
    size_t index;
    static const char checkpoint_path[] = "k2_d_multi_checkpoint.bin";

    REQUIRE(source != NULL && full_replay != NULL);
    REQUIRE(minisnn_worlds_kernel_command_log_create(&log) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_state_hash(source, &initial_hash) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(capture_snapshot_and_hash(source, &checkpoints[0], &checkpoint_hashes[0]));
    checkpoint_cursors[0] = 0U;

    for (tick = 0U; tick < K2D_TICKS; ++tick)
    {
        int checkpoint_index;

        REQUIRE(source_submit_one(log, source));
        REQUIRE(step_kernel(source));
        checkpoint_index = checkpoint_index_for_tick((size_t)minisnn_worlds_kernel_tick(source));
        if (checkpoint_index >= 0)
        {
            REQUIRE(capture_snapshot_and_hash(source, &checkpoints[checkpoint_index],
                                               &checkpoint_hashes[checkpoint_index]));
            checkpoint_cursors[checkpoint_index] =
                minisnn_worlds_kernel_command_log_count(log);
        }
    }
    REQUIRE(minisnn_worlds_kernel_command_log_count(log) >= K2D_TICKS);

    REQUIRE(minisnn_worlds_kernel_replay_session_create(log, 0U, initial_hash, &session) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    for (tick = 0U; tick < K2D_TICKS; ++tick)
    {
        int checkpoint_index;

        REQUIRE(replay_one_tick(session, log, full_replay));
        checkpoint_index = checkpoint_index_for_tick((size_t)minisnn_worlds_kernel_tick(full_replay));
        if (checkpoint_index >= 0)
        {
            MiniSNNWorldsKernelSnapshot *actual = NULL;
            REQUIRE(minisnn_worlds_kernel_snapshot_capture(full_replay, &actual) ==
                    MINISNN_WORLDS_KERNEL_ERROR_NONE);
            REQUIRE(snapshots_equal_bytes(checkpoints[checkpoint_index], actual));
            minisnn_worlds_kernel_snapshot_destroy(actual);
        }
    }
    REQUIRE(minisnn_worlds_kernel_replay_session_cursor(session) ==
            minisnn_worlds_kernel_command_log_count(log));
    REQUIRE(compare_final(source, full_replay));
    minisnn_worlds_kernel_replay_session_destroy(session);
    session = NULL;

    for (index = 0U; index < K2D_CHECKPOINT_COUNT; ++index)
    {
        MiniSNNWorldsKernel *restored = NULL;

        REQUIRE(minisnn_worlds_kernel_create_from_snapshot(checkpoints[index], &restored) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
        REQUIRE(minisnn_worlds_kernel_replay_session_create(
                    log, checkpoint_cursors[index], checkpoint_hashes[index], &session) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
        REQUIRE(minisnn_worlds_kernel_replay_session_validate(session, restored) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
        while (minisnn_worlds_kernel_tick(restored) < K2D_TICKS)
        {
            REQUIRE(replay_one_tick(session, log, restored));
        }
        REQUIRE(compare_final(source, restored));
        minisnn_worlds_kernel_replay_session_destroy(session);
        session = NULL;
        minisnn_worlds_kernel_destroy(restored);
    }

    REQUIRE(minisnn_worlds_kernel_create_from_snapshot(checkpoints[0], &multi_restore) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_replay_session_create(log, 0U, checkpoint_hashes[0], &session) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    while (minisnn_worlds_kernel_tick(multi_restore) < K2D_TICKS)
    {
        REQUIRE(replay_one_tick(session, log, multi_restore));
        if (minisnn_worlds_kernel_tick(multi_restore) == 250U ||
            minisnn_worlds_kernel_tick(multi_restore) == 500U ||
            minisnn_worlds_kernel_tick(multi_restore) == 750U)
        {
            MiniSNNWorldsKernelSnapshot *saved = NULL;
            MiniSNNWorldsKernelSnapshot *loaded = NULL;
            MiniSNNWorldsKernel *replacement = NULL;
            uint64_t saved_hash;
            size_t cursor = minisnn_worlds_kernel_replay_session_cursor(session);

            REQUIRE(capture_snapshot_and_hash(multi_restore, &saved, &saved_hash));
            REQUIRE(minisnn_worlds_kernel_snapshot_save_file(checkpoint_path, saved) ==
                    MINISNN_WORLDS_KERNEL_ERROR_NONE);
            REQUIRE(minisnn_worlds_kernel_snapshot_load_file(checkpoint_path, &loaded) ==
                    MINISNN_WORLDS_KERNEL_ERROR_NONE);
            REQUIRE(minisnn_worlds_kernel_create_from_snapshot(loaded, &replacement) ==
                    MINISNN_WORLDS_KERNEL_ERROR_NONE);
            minisnn_worlds_kernel_replay_session_destroy(session);
            minisnn_worlds_kernel_destroy(multi_restore);
            minisnn_worlds_kernel_snapshot_destroy(loaded);
            minisnn_worlds_kernel_snapshot_destroy(saved);
            multi_restore = replacement;
            session = NULL;
            REQUIRE(minisnn_worlds_kernel_replay_session_create(log, cursor, saved_hash, &session) ==
                    MINISNN_WORLDS_KERNEL_ERROR_NONE);
            REQUIRE(minisnn_worlds_kernel_replay_session_validate(session, multi_restore) ==
                    MINISNN_WORLDS_KERNEL_ERROR_NONE);
        }
    }
    REQUIRE(compare_final(source, multi_restore));
    REQUIRE(remove(checkpoint_path) == 0);

    minisnn_worlds_kernel_replay_session_destroy(session);
    minisnn_worlds_kernel_destroy(multi_restore);
    for (index = 0U; index < K2D_CHECKPOINT_COUNT; ++index)
    {
        minisnn_worlds_kernel_snapshot_destroy(checkpoints[index]);
    }
    minisnn_worlds_kernel_command_log_destroy(log);
    minisnn_worlds_kernel_destroy(full_replay);
    minisnn_worlds_kernel_destroy(source);
    puts("K2-D 1000-tick multi-checkpoint persistence and replay OK");
    return 0;
}