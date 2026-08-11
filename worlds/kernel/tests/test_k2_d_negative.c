#define main k2_c_command_log_fixture_main
#include "test_k2_c_command_log.c"
#undef main

static int divergence_is_atomic(MiniSNNWorldsKernelReplaySession *session,
                                MiniSNNWorldsKernel *kernel)
{
    uint64_t before_hash;
    uint64_t after_hash;
    size_t before_pending;
    MiniSNNWorldsKernelDiagnostics before;
    MiniSNNWorldsKernelDiagnostics after;

    if (minisnn_worlds_kernel_state_hash(kernel, &before_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_get_diagnostics(kernel, &before) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    before_pending = minisnn_worlds_kernel_pending_command_count(kernel);
    if (minisnn_worlds_kernel_replay_session_validate(session, kernel) !=
        MINISNN_WORLDS_KERNEL_ERROR_REPLAY_DIVERGENCE)
    {
        return 0;
    }
    if (minisnn_worlds_kernel_state_hash(kernel, &after_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_get_diagnostics(kernel, &after) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    return before_hash == after_hash &&
           before_pending == minisnn_worlds_kernel_pending_command_count(kernel) &&
           before.completed_ticks == after.completed_ticks &&
           before.total_commands_submitted == after.total_commands_submitted &&
           before.total_commands_applied == after.total_commands_applied &&
           before.total_commands_rejected == after.total_commands_rejected &&
           before.total_events_emitted == after.total_events_emitted &&
           before.total_random_u32_generated == after.total_random_u32_generated &&
           minisnn_worlds_kernel_last_error(kernel) ==
               MINISNN_WORLDS_KERNEL_ERROR_REPLAY_DIVERGENCE;
}

static int make_single_record_log(MiniSNNWorldsKernel **out_source,
                                  MiniSNNWorldsKernelCommandLog **out_log,
                                  uint64_t *out_hash)
{
    MiniSNNWorldsKernelCommandId command;

    *out_source = create_kernel();
    *out_log = NULL;
    if (*out_source == NULL ||
        minisnn_worlds_kernel_command_log_create(out_log) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(*out_source, out_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_create_entity(*out_source, 1U, 0U, id(0U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !capture_submission(*out_log, *out_source, command))
    {
        minisnn_worlds_kernel_command_log_destroy(*out_log);
        minisnn_worlds_kernel_destroy(*out_source);
        *out_log = NULL;
        *out_source = NULL;
        return 0;
    }
    return 1;
}

static int test_wrong_fresh_states(void)
{
    MiniSNNWorldsKernel *source = NULL;
    MiniSNNWorldsKernel *wrong = NULL;
    MiniSNNWorldsKernelCommandLog *log = NULL;
    MiniSNNWorldsKernelReplaySession *session = NULL;
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    uint64_t initial_hash;

    REQUIRE(make_single_record_log(&source, &log, &initial_hash));
    config.master_seed = UINT64_C(0xFEEDBEEF);
    wrong = minisnn_worlds_kernel_create(&config, NULL);
    REQUIRE(wrong != NULL);
    REQUIRE(minisnn_worlds_kernel_replay_session_create(log, 0U, initial_hash, &session) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(divergence_is_atomic(session, wrong));
    minisnn_worlds_kernel_replay_session_destroy(session);
    minisnn_worlds_kernel_destroy(wrong);

    config = minisnn_worlds_kernel_config_default();
    config.master_seed = UINT64_C(0x4B32435F44454D4F);
    config.space_bounds.max_x -= 1;
    wrong = minisnn_worlds_kernel_create(&config, NULL);
    REQUIRE(wrong != NULL);
    REQUIRE(minisnn_worlds_kernel_replay_session_create(log, 0U, initial_hash, &session) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(divergence_is_atomic(session, wrong));
    minisnn_worlds_kernel_replay_session_destroy(session);
    minisnn_worlds_kernel_destroy(wrong);

    wrong = create_kernel();
    REQUIRE(wrong != NULL);
    REQUIRE(minisnn_worlds_kernel_replay_session_create(log, 0U, initial_hash + 1U, &session) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(divergence_is_atomic(session, wrong));
    minisnn_worlds_kernel_replay_session_destroy(session);
    minisnn_worlds_kernel_destroy(wrong);

    minisnn_worlds_kernel_command_log_destroy(log);
    minisnn_worlds_kernel_destroy(source);
    return 0;
}

static int test_wrong_checkpoint_tick_ids_and_snapshot(void)
{
    MiniSNNWorldsKernel *source = NULL;
    MiniSNNWorldsKernel *wrong = NULL;
    MiniSNNWorldsKernel *restored = NULL;
    MiniSNNWorldsKernelCommandLog *log = NULL;
    MiniSNNWorldsKernelReplaySession *session = NULL;
    MiniSNNWorldsKernelSnapshot *checkpoint = NULL;
    MiniSNNWorldsKernelSnapshot *wrong_snapshot = NULL;
    uint64_t initial_hash;
    uint64_t checkpoint_hash;

    REQUIRE(make_single_record_log(&source, &log, &initial_hash));
    REQUIRE(step_kernel(source));
    REQUIRE(minisnn_worlds_kernel_state_hash(source, &checkpoint_hash) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_snapshot_capture(source, &checkpoint) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_create_from_snapshot(checkpoint, &restored) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_replay_session_create(log, 1U, checkpoint_hash, &session) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_replay_session_validate(session, restored) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    minisnn_worlds_kernel_replay_session_destroy(session);
    session = NULL;
    minisnn_worlds_kernel_destroy(restored);
    restored = NULL;

    wrong = create_kernel();
    REQUIRE(wrong != NULL);
    REQUIRE(step_kernel(wrong));
    REQUIRE(minisnn_worlds_kernel_replay_session_create(log, 0U, initial_hash, &session) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(divergence_is_atomic(session, wrong));
    minisnn_worlds_kernel_replay_session_destroy(session);
    session = NULL;
    minisnn_worlds_kernel_destroy(wrong);
    wrong = create_kernel();
    REQUIRE(wrong != NULL);
    REQUIRE(minisnn_worlds_kernel_testing_set_next_command_id(wrong, (MiniSNNWorldsKernelCommandId){ UINT64_C(2) }) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_replay_session_create(log, 0U, initial_hash, &session) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(divergence_is_atomic(session, wrong));
    minisnn_worlds_kernel_replay_session_destroy(session);
    session = NULL;
    minisnn_worlds_kernel_destroy(wrong);

    REQUIRE(minisnn_worlds_kernel_create_from_snapshot(checkpoint, &wrong) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_random_u32(
                wrong, (MiniSNNWorldsKernelRandomStreamKey){ 7U, 11U }, &(uint32_t){ 0U }) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_snapshot_capture(wrong, &wrong_snapshot) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    minisnn_worlds_kernel_destroy(wrong);
    wrong = NULL;
    REQUIRE(minisnn_worlds_kernel_create_from_snapshot(wrong_snapshot, &restored) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_replay_session_create(log, 1U, checkpoint_hash, &session) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(divergence_is_atomic(session, restored));
    minisnn_worlds_kernel_replay_session_destroy(session);
    minisnn_worlds_kernel_destroy(restored);

    minisnn_worlds_kernel_snapshot_destroy(wrong_snapshot);
    minisnn_worlds_kernel_snapshot_destroy(checkpoint);
    minisnn_worlds_kernel_command_log_destroy(log);
    minisnn_worlds_kernel_destroy(source);
    return 0;
}

static int test_malformed_record_and_cursor(void)
{
    MiniSNNWorldsKernel *source = NULL;
    MiniSNNWorldsKernelCommandLog *log = NULL;
    MiniSNNWorldsKernelCommandLog *imported = NULL;
    MiniSNNWorldsKernelReplaySession *session = NULL;
    uint64_t initial_hash;
    uint8_t *bytes;
    size_t size;

    REQUIRE(make_single_record_log(&source, &log, &initial_hash));
    REQUIRE(minisnn_worlds_kernel_replay_session_create(log, 2U, initial_hash, &session) ==
            MINISNN_WORLDS_KERNEL_ERROR_INVALID_ARGUMENT);
    REQUIRE(session == NULL);
    size = minisnn_worlds_kernel_command_log_size(log);
    bytes = malloc(size);
    REQUIRE(bytes != NULL);
    memcpy(bytes, minisnn_worlds_kernel_command_log_data(log), size);
    bytes[size - 1U] ^= UINT8_C(1);
    REQUIRE(minisnn_worlds_kernel_command_log_from_bytes(bytes, size, &imported) ==
            MINISNN_WORLDS_KERNEL_ERROR_COMMAND_LOG_INVALID_FORMAT);
    REQUIRE(imported == NULL);
    free(bytes);
    minisnn_worlds_kernel_command_log_destroy(log);
    minisnn_worlds_kernel_destroy(source);
    return 0;
}


static int create_stateful_baseline(MiniSNNWorldsKernel **out_kernel)
{
    MiniSNNWorldsKernelCommandId command;

    *out_kernel = create_kernel();
    if (*out_kernel == NULL ||
        minisnn_worlds_kernel_queue_create_entity(*out_kernel, 1U, 0U, id(0U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_create_entity(*out_kernel, 1U, 1U, id(0U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !step_kernel(*out_kernel) ||
        minisnn_worlds_kernel_queue_place_entity(*out_kernel, 2U, 0U, id(0U), id(1U),
                                                  transform(0, 0), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(*out_kernel, 2U, 1U, id(0U), id(2U),
                                                  transform(50, 0), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !step_kernel(*out_kernel) ||
        minisnn_worlds_kernel_queue_set_occupancy(*out_kernel, 3U, 0U, id(0U), id(1U),
                                                   occupancy(5), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_create_spatial_link(*out_kernel, 3U, 1U, id(0U),
                                                         id(1U), id(2U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !step_kernel(*out_kernel))
    {
        minisnn_worlds_kernel_destroy(*out_kernel);
        *out_kernel = NULL;
        return 0;
    }
    return 1;
}

static int test_transform_occupancy_and_link_divergence(void)
{
    MiniSNNWorldsKernel *baseline = NULL;
    MiniSNNWorldsKernel *candidate = NULL;
    MiniSNNWorldsKernelSnapshot *snapshot = NULL;
    MiniSNNWorldsKernelCommandLog *empty_log = NULL;
    MiniSNNWorldsKernelReplaySession *session = NULL;
    MiniSNNWorldsKernelCommandId command;
    uint64_t baseline_hash;

    REQUIRE(create_stateful_baseline(&baseline));
    REQUIRE(minisnn_worlds_kernel_state_hash(baseline, &baseline_hash) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_snapshot_capture(baseline, &snapshot) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_command_log_create(&empty_log) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);

    REQUIRE(minisnn_worlds_kernel_create_from_snapshot(snapshot, &candidate) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(candidate, 4U, 0U, id(0U), id(1U),
                                                    1, 0, &command) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(step_kernel(candidate));
    REQUIRE(minisnn_worlds_kernel_replay_session_create(empty_log, 0U, baseline_hash,
                                                        &session) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(divergence_is_atomic(session, candidate));
    minisnn_worlds_kernel_replay_session_destroy(session);
    minisnn_worlds_kernel_destroy(candidate);
    session = NULL;
    candidate = NULL;

    REQUIRE(minisnn_worlds_kernel_create_from_snapshot(snapshot, &candidate) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_clear_occupancy(candidate, 4U, 0U, id(0U), id(1U),
                                                        &command) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(step_kernel(candidate));
    REQUIRE(minisnn_worlds_kernel_replay_session_create(empty_log, 0U, baseline_hash,
                                                        &session) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(divergence_is_atomic(session, candidate));
    minisnn_worlds_kernel_replay_session_destroy(session);
    minisnn_worlds_kernel_destroy(candidate);
    session = NULL;
    candidate = NULL;

    REQUIRE(minisnn_worlds_kernel_create_from_snapshot(snapshot, &candidate) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_remove_spatial_link(candidate, 4U, 0U, id(0U),
                                                            id(1U), id(2U), &command) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(step_kernel(candidate));
    REQUIRE(minisnn_worlds_kernel_replay_session_create(empty_log, 0U, baseline_hash,
                                                        &session) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(divergence_is_atomic(session, candidate));
    minisnn_worlds_kernel_replay_session_destroy(session);
    minisnn_worlds_kernel_destroy(candidate);

    minisnn_worlds_kernel_command_log_destroy(empty_log);
    minisnn_worlds_kernel_snapshot_destroy(snapshot);
    minisnn_worlds_kernel_destroy(baseline);
    return 0;
}

int main(void)
{
    if (test_wrong_fresh_states() != 0 ||
        test_wrong_checkpoint_tick_ids_and_snapshot() != 0 ||
        test_transform_occupancy_and_link_divergence() != 0 ||
        test_malformed_record_and_cursor() != 0)
    {
        return 1;
    }
    puts("K2-D negative state-binding validation OK");
    return 0;
}