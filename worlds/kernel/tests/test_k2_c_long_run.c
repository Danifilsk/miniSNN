#define main k2_c_command_log_fixture_main
#include "test_k2_c_command_log.c"
#undef main

#define LONG_RUN_TICKS 1000U
#define CHECKPOINT_COUNT 3U

static int stored_snapshots_equal(
    const MiniSNNWorldsKernelSnapshot *left,
    const MiniSNNWorldsKernelSnapshot *right)
{
    return left != NULL && right != NULL &&
           minisnn_worlds_kernel_snapshot_size(left) ==
               minisnn_worlds_kernel_snapshot_size(right) &&
           memcmp(minisnn_worlds_kernel_snapshot_data(left),
                  minisnn_worlds_kernel_snapshot_data(right),
                  minisnn_worlds_kernel_snapshot_size(left)) == 0;
}

static int capture_checkpoint(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelSnapshot **out_snapshot)
{
    minisnn_worlds_kernel_snapshot_destroy(*out_snapshot);
    *out_snapshot = NULL;
    return minisnn_worlds_kernel_snapshot_capture(kernel, out_snapshot) ==
           MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int compare_checkpoint(
    MiniSNNWorldsKernel *kernel,
    const MiniSNNWorldsKernelSnapshot *expected)
{
    MiniSNNWorldsKernelSnapshot *actual = NULL;
    int equal;

    if (minisnn_worlds_kernel_snapshot_capture(kernel, &actual) !=
        MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    equal = stored_snapshots_equal(expected, actual);
    minisnn_worlds_kernel_snapshot_destroy(actual);
    return equal;
}

int main(void)
{
    static const size_t checkpoint_ticks[CHECKPOINT_COUNT] = { 100U, 500U, 1000U };
    MiniSNNWorldsKernel *original = create_kernel();
    MiniSNNWorldsKernel *replayed = create_kernel();
    MiniSNNWorldsKernel *restored = NULL;
    MiniSNNWorldsKernelCommandLog *log = NULL;
    MiniSNNWorldsKernelSnapshot *checkpoints[CHECKPOINT_COUNT] = { NULL, NULL, NULL };
    MiniSNNWorldsKernelSnapshot *checkpoint_snapshot = NULL;
    MiniSNNWorldsKernelSnapshot *checkpoint_copy = NULL;
    size_t checkpoint_cursor = 0U;
    size_t cursor = 0U;
    size_t tick;
    size_t checkpoint_index;

    REQUIRE(original != NULL && replayed != NULL);
    REQUIRE(minisnn_worlds_kernel_command_log_create(&log) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);

    for (tick = 0U; tick < LONG_RUN_TICKS; ++tick)
    {
        if (tick == 0U)
        {
            REQUIRE(queue_create(log, original, 1U, 0U));
        }
        else if (tick == 1U)
        {
            REQUIRE(queue_place(log, original, id(1U), 2U, 0U, 0, 0));
        }
        else
        {
            REQUIRE(queue_move(log, original, tick + 1U, 0U));
        }
        REQUIRE(step_kernel(original));
        for (checkpoint_index = 0U; checkpoint_index < CHECKPOINT_COUNT;
             ++checkpoint_index)
        {
            if (minisnn_worlds_kernel_tick(original) == checkpoint_ticks[checkpoint_index])
            {
                REQUIRE(capture_checkpoint(original, &checkpoints[checkpoint_index]));
            }
        }
        if (minisnn_worlds_kernel_tick(original) == 500U)
        {
            checkpoint_cursor = minisnn_worlds_kernel_command_log_count(log);
            REQUIRE(capture_checkpoint(original, &checkpoint_snapshot));
        }
    }
    REQUIRE(minisnn_worlds_kernel_command_log_count(log) == LONG_RUN_TICKS);
    REQUIRE(checkpoint_cursor == 500U);
    REQUIRE(checkpoint_snapshot != NULL);

    for (tick = 0U; tick < LONG_RUN_TICKS; ++tick)
    {
        REQUIRE(replay_submissions_for_tick(log, replayed, &cursor));
        REQUIRE(step_kernel(replayed));
        for (checkpoint_index = 0U; checkpoint_index < CHECKPOINT_COUNT;
             ++checkpoint_index)
        {
            if (minisnn_worlds_kernel_tick(replayed) == checkpoint_ticks[checkpoint_index])
            {
                REQUIRE(compare_checkpoint(replayed, checkpoints[checkpoint_index]));
            }
        }
    }
    REQUIRE(cursor == minisnn_worlds_kernel_command_log_count(log));
    REQUIRE(snapshots_equal(original, replayed));

    REQUIRE(minisnn_worlds_kernel_snapshot_from_bytes(
                minisnn_worlds_kernel_snapshot_data(checkpoint_snapshot),
                minisnn_worlds_kernel_snapshot_size(checkpoint_snapshot),
                &checkpoint_copy) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_create_from_snapshot(checkpoint_copy, &restored) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    cursor = checkpoint_cursor;
    for (tick = 500U; tick < LONG_RUN_TICKS; ++tick)
    {
        REQUIRE(replay_submissions_for_tick(log, restored, &cursor));
        REQUIRE(step_kernel(restored));
        for (checkpoint_index = 0U; checkpoint_index < CHECKPOINT_COUNT;
             ++checkpoint_index)
        {
            if (minisnn_worlds_kernel_tick(restored) == checkpoint_ticks[checkpoint_index])
            {
                REQUIRE(compare_checkpoint(restored, checkpoints[checkpoint_index]));
            }
        }
    }
    REQUIRE(cursor == minisnn_worlds_kernel_command_log_count(log));
    REQUIRE(snapshots_equal(original, restored));

    minisnn_worlds_kernel_destroy(restored);
    minisnn_worlds_kernel_snapshot_destroy(checkpoint_copy);
    minisnn_worlds_kernel_snapshot_destroy(checkpoint_snapshot);
    for (checkpoint_index = 0U; checkpoint_index < CHECKPOINT_COUNT; ++checkpoint_index)
    {
        minisnn_worlds_kernel_snapshot_destroy(checkpoints[checkpoint_index]);
    }
    minisnn_worlds_kernel_command_log_destroy(log);
    minisnn_worlds_kernel_destroy(replayed);
    minisnn_worlds_kernel_destroy(original);
    puts("K2-C 1000-tick replay, checkpoint and restore continuation OK");
    return 0;
}