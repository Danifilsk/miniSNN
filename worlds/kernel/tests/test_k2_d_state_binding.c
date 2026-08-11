#define main k2_c_command_log_fixture_main
#include "test_k2_c_command_log.c"
#undef main

static int capture_hash(const MiniSNNWorldsKernel *kernel, uint64_t *out_hash)
{
    return minisnn_worlds_kernel_state_hash(kernel, out_hash) ==
           MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int test_initial_binding_and_cursor(void)
{
    MiniSNNWorldsKernel *source = create_kernel();
    MiniSNNWorldsKernel *replay = create_kernel();
    MiniSNNWorldsKernelCommandLog *log = NULL;
    MiniSNNWorldsKernelReplaySession *session = NULL;
    MiniSNNWorldsKernelCommandId command;
    uint64_t initial_hash;

    REQUIRE(source != NULL && replay != NULL);
    REQUIRE(minisnn_worlds_kernel_command_log_create(&log) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(capture_hash(source, &initial_hash));
    REQUIRE(minisnn_worlds_kernel_queue_create_entity(source, 1U, 0U, id(0U), &command) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(capture_submission(log, source, command));
    REQUIRE(minisnn_worlds_kernel_replay_session_create(log, 0U, initial_hash, &session) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_replay_session_replay_next(session, replay) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_replay_session_cursor(session) == 1U);
    REQUIRE(step_kernel(source));
    REQUIRE(step_kernel(replay));
    REQUIRE(snapshots_equal(source, replay));
    minisnn_worlds_kernel_replay_session_destroy(session);
    session = NULL;
    REQUIRE(minisnn_worlds_kernel_replay_session_create(
                log, minisnn_worlds_kernel_command_log_count(log) + 1U, initial_hash,
                &session) == MINISNN_WORLDS_KERNEL_ERROR_INVALID_ARGUMENT);
    REQUIRE(session == NULL);
    minisnn_worlds_kernel_command_log_destroy(log);
    minisnn_worlds_kernel_destroy(replay);
    minisnn_worlds_kernel_destroy(source);
    return 0;
}

static int test_snapshot_binding(void)
{
    MiniSNNWorldsKernel *source = create_kernel();
    MiniSNNWorldsKernel *restored = NULL;
    MiniSNNWorldsKernelCommandLog *log = NULL;
    MiniSNNWorldsKernelSnapshot *snapshot = NULL;
    MiniSNNWorldsKernelReplaySession *session = NULL;
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelCommandLogRecord future_record;
    uint64_t checkpoint_hash;

    REQUIRE(source != NULL);
    REQUIRE(minisnn_worlds_kernel_command_log_create(&log) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_entity(source, 1U, 0U, id(0U), &command) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(capture_submission(log, source, command));
    REQUIRE(step_kernel(source));
    REQUIRE(capture_hash(source, &checkpoint_hash));
    memset(&future_record, 0, sizeof(future_record));
    future_record.submission_tick = 1U;
    future_record.command.command_id = (MiniSNNWorldsKernelCommandId){ UINT64_C(2) };
    future_record.command.target_tick = 2U;
    future_record.command.type = MINISNN_WORLDS_KERNEL_COMMAND_CREATE_ENTITY;
    REQUIRE(minisnn_worlds_kernel_command_log_append(log, &future_record) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_snapshot_capture(source, &snapshot) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_create_from_snapshot(snapshot, &restored) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_replay_session_create(log, 1U, checkpoint_hash, &session) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_replay_session_validate(session, restored) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    minisnn_worlds_kernel_replay_session_destroy(session);
    session = NULL;
    REQUIRE(minisnn_worlds_kernel_replay_session_create(log, 0U, checkpoint_hash, &session) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_replay_session_validate(session, restored) ==
            MINISNN_WORLDS_KERNEL_ERROR_REPLAY_DIVERGENCE);
    minisnn_worlds_kernel_replay_session_destroy(session);
    session = NULL;
    REQUIRE(minisnn_worlds_kernel_replay_session_create(log, 2U, checkpoint_hash, &session) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_replay_session_validate(session, restored) ==
            MINISNN_WORLDS_KERNEL_ERROR_REPLAY_DIVERGENCE);
    minisnn_worlds_kernel_replay_session_destroy(session);
    minisnn_worlds_kernel_destroy(restored);
    minisnn_worlds_kernel_snapshot_destroy(snapshot);
    minisnn_worlds_kernel_command_log_destroy(log);
    minisnn_worlds_kernel_destroy(source);
    return 0;
}

static int test_allocation_failure(void)
{
    MiniSNNWorldsKernelCommandLog *log = NULL;
    MiniSNNWorldsKernelReplaySession *session = NULL;

    REQUIRE(minisnn_worlds_kernel_command_log_create(&log) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    minisnn_worlds_kernel_testing_fail_next_allocation();
    REQUIRE(minisnn_worlds_kernel_replay_session_create(log, 0U, 0U, &session) ==
            MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
    REQUIRE(session == NULL);
    minisnn_worlds_kernel_testing_fail_allocation_after(SIZE_MAX);
    minisnn_worlds_kernel_command_log_destroy(log);
    return 0;
}

int main(void)
{
    if (test_initial_binding_and_cursor() != 0 ||
        test_snapshot_binding() != 0 ||
        test_allocation_failure() != 0)
    {
        return 1;
    }
    puts("K2-D state-binding and replay cursor validation OK");
    return 0;
}