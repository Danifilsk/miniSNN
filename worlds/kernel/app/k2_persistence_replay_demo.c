#include "k2_snapshot_file.h"
#include "k2_command_log_file.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define K2D_TICKS 5U
#define K2D_CHECKPOINT_TICK 3U

static MiniSNNWorldsKernelEntityId id(uint64_t value)
{
    return (MiniSNNWorldsKernelEntityId){ value };
}

static MiniSNNWorldsKernelTransform transform(int64_t x, int64_t y)
{
    MiniSNNWorldsKernelTransform value;

    value.position.x = x;
    value.position.y = y;
    value.orientation = UINT32_C(0);
    return value;
}

static MiniSNNWorldsKernelOccupancy occupancy(int64_t half_extent)
{
    MiniSNNWorldsKernelOccupancy value;

    value.half_extent_x = half_extent;
    value.half_extent_y = half_extent;
    value.category_bits = UINT32_C(1);
    value.blocking_mask = UINT32_C(1);
    return value;
}

static int capture_submission(MiniSNNWorldsKernelCommandLog *log,
                              MiniSNNWorldsKernel *kernel,
                              MiniSNNWorldsKernelCommandId command)
{
    return minisnn_worlds_kernel_command_log_capture_submission(
               log, kernel, minisnn_worlds_kernel_tick(kernel), command) ==
           MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int queue_create(MiniSNNWorldsKernelCommandLog *log,
                        MiniSNNWorldsKernel *kernel,
                        MiniSNNWorldsTick target_tick,
                        uint32_t priority)
{
    MiniSNNWorldsKernelCommandId command;

    return minisnn_worlds_kernel_queue_create_entity(kernel, target_tick, priority, id(0U),
                                                      &command) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           capture_submission(log, kernel, command);
}

static int queue_place(MiniSNNWorldsKernelCommandLog *log,
                       MiniSNNWorldsKernel *kernel,
                       MiniSNNWorldsKernelEntityId entity,
                       MiniSNNWorldsTick target_tick,
                       uint32_t priority,
                       int64_t x,
                       int64_t y)
{
    MiniSNNWorldsKernelCommandId command;

    return minisnn_worlds_kernel_queue_place_entity(kernel, target_tick, priority, id(0U),
                                                     entity, transform(x, y), &command) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           capture_submission(log, kernel, command);
}

static int queue_occupancy(MiniSNNWorldsKernelCommandLog *log,
                           MiniSNNWorldsKernel *kernel,
                           MiniSNNWorldsKernelEntityId entity,
                           MiniSNNWorldsTick target_tick,
                           uint32_t priority)
{
    MiniSNNWorldsKernelCommandId command;

    return minisnn_worlds_kernel_queue_set_occupancy(kernel, target_tick, priority, id(0U),
                                                      entity, occupancy(10), &command) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           capture_submission(log, kernel, command);
}

static int queue_link(MiniSNNWorldsKernelCommandLog *log,
                      MiniSNNWorldsKernel *kernel,
                      MiniSNNWorldsTick target_tick)
{
    MiniSNNWorldsKernelCommandId command;

    return minisnn_worlds_kernel_queue_create_spatial_link(kernel, target_tick, 1U, id(0U),
                                                            id(1U), id(2U), &command) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           capture_submission(log, kernel, command);
}

static int queue_move(MiniSNNWorldsKernelCommandLog *log,
                      MiniSNNWorldsKernel *kernel,
                      MiniSNNWorldsTick target_tick,
                      MiniSNNWorldsKernelEntityId entity,
                      int64_t delta_x,
                      int64_t delta_y)
{
    MiniSNNWorldsKernelCommandId command;

    return minisnn_worlds_kernel_queue_move_entity(kernel, target_tick, 2U, id(0U), entity,
                                                    delta_x, delta_y, &command) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           capture_submission(log, kernel, command);
}

static int queue_destroy(MiniSNNWorldsKernelCommandLog *log,
                         MiniSNNWorldsKernel *kernel,
                         MiniSNNWorldsTick target_tick,
                         MiniSNNWorldsKernelEntityId entity,
                         uint32_t priority)
{
    MiniSNNWorldsKernelCommandId command;

    return minisnn_worlds_kernel_queue_destroy_entity(kernel, target_tick, priority, id(0U),
                                                       entity, &command) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           capture_submission(log, kernel, command);
}

static int queue_clear_occupancy(MiniSNNWorldsKernelCommandLog *log,
                                 MiniSNNWorldsKernel *kernel,
                                 MiniSNNWorldsTick target_tick,
                                 MiniSNNWorldsKernelEntityId entity)
{
    MiniSNNWorldsKernelCommandId command;

    return minisnn_worlds_kernel_queue_clear_occupancy(kernel, target_tick, 1U, id(0U),
                                                        entity, &command) ==
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

static int queue_remove_and_destroy_third(MiniSNNWorldsKernelCommandLog *log,
                                          MiniSNNWorldsKernel *kernel,
                                          MiniSNNWorldsTick target_tick)
{
    MiniSNNWorldsKernelCommandId remove_command;
    MiniSNNWorldsKernelCommandId destroy_command;

    return minisnn_worlds_kernel_queue_remove_entity_from_space(
               kernel, target_tick, 1U, id(0U), id(3U), &remove_command) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           capture_submission(log, kernel, remove_command) &&
           minisnn_worlds_kernel_queue_destroy_entity(
               kernel, target_tick, 2U, id(0U), id(3U), &destroy_command) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           capture_submission(log, kernel, destroy_command);
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
            return queue_occupancy(log, kernel, id(1U), 3U, 2U) &&
                   queue_occupancy(log, kernel, id(2U), 3U, 0U) &&
                   queue_link(log, kernel, 3U);
        case 3U:
            return queue_move(log, kernel, 4U, id(1U), 10, 0) &&
                   queue_destroy(log, kernel, 4U, id(1U), 0U) &&
                   queue_clear_occupancy(log, kernel, 4U, id(1U));
        case 4U:
            return queue_move(log, kernel, 5U, id(1U), INT64_MAX, 0) &&
                   queue_remove_link(log, kernel, 5U) &&
                   queue_remove_and_destroy_third(log, kernel, 5U);
        default:
            return 1;
    }
}

static int draw_random_and_step(MiniSNNWorldsKernel *kernel)
{
    MiniSNNWorldsKernelRandomStreamKey key = { UINT64_C(33), UINT64_C(9) };
    uint32_t value;

    return minisnn_worlds_kernel_random_u32(kernel, key, &value) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int replay_tick(MiniSNNWorldsKernelReplaySession *session,
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
    return draw_random_and_step(kernel);
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

static int snapshots_equal(const MiniSNNWorldsKernelSnapshot *left,
                           const MiniSNNWorldsKernelSnapshot *right)
{
    return left != NULL && right != NULL &&
           minisnn_worlds_kernel_snapshot_size(left) ==
               minisnn_worlds_kernel_snapshot_size(right) &&
           memcmp(minisnn_worlds_kernel_snapshot_data(left),
                  minisnn_worlds_kernel_snapshot_data(right),
                  minisnn_worlds_kernel_snapshot_size(left)) == 0;
}

static int make_path(char *buffer, size_t buffer_size, const char *directory, const char *name)
{
    int count = snprintf(buffer, buffer_size, "%s/%s", directory, name);

    return count >= 0 && (size_t)count < buffer_size;
}

static int save_snapshot(const char *path, const MiniSNNWorldsKernelSnapshot *snapshot)
{
    return minisnn_worlds_kernel_snapshot_save_file(path, snapshot) ==
           MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

int main(int argc, char **argv)
{
    const char *directory = argc == 2 ? argv[1] : ".";
    char command_log_path[512];
    char checkpoint_path[512];
    char final_snapshot_path[512];
    char continuous_path[512];
    char full_replay_path[512];
    char restored_path[512];
    char summary_path[512];
    char hashes_path[512];
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernel *continuous = NULL;
    MiniSNNWorldsKernel *full_replay = NULL;
    MiniSNNWorldsKernel *checkpoint_replay = NULL;
    MiniSNNWorldsKernelSnapshot *checkpoint = NULL;
    MiniSNNWorldsKernelSnapshot *loaded_checkpoint = NULL;
    MiniSNNWorldsKernelSnapshot *continuous_final = NULL;
    MiniSNNWorldsKernelSnapshot *full_final = NULL;
    MiniSNNWorldsKernelSnapshot *restored_final = NULL;
    MiniSNNWorldsKernelCommandLog *log = NULL;
    MiniSNNWorldsKernelCommandLog *loaded_log = NULL;
    MiniSNNWorldsKernelReplaySession *session = NULL;
    MiniSNNWorldsKernelDiagnostics diagnostics;
    uint64_t initial_hash;
    uint64_t checkpoint_hash;
    uint64_t continuous_hash;
    uint64_t full_hash;
    uint64_t restored_hash;
    uint64_t final_digest;
    size_t checkpoint_cursor = 0U;
    size_t tick;
    FILE *summary = NULL;
    FILE *hashes = NULL;
    int ok;

    ok = argc <= 2 &&
         make_path(command_log_path, sizeof(command_log_path), directory, "command_log.bin") &&
         make_path(checkpoint_path, sizeof(checkpoint_path), directory, "checkpoint_snapshot.bin") &&
         make_path(final_snapshot_path, sizeof(final_snapshot_path), directory, "final_snapshot.bin") &&
         make_path(continuous_path, sizeof(continuous_path), directory, "final_continuous.bin") &&
         make_path(full_replay_path, sizeof(full_replay_path), directory, "final_full_replay.bin") &&
         make_path(restored_path, sizeof(restored_path), directory, "final_restored_replay.bin") &&
         make_path(summary_path, sizeof(summary_path), directory, "summary.txt") &&
         make_path(hashes_path, sizeof(hashes_path), directory, "hashes.txt");
    if (!ok)
    {
        fprintf(stderr, "usage: %s [existing-output-directory]\n", argv[0]);
        return 1;
    }

    config.master_seed = UINT64_C(0x4B32445F44454D4F);
    continuous = minisnn_worlds_kernel_create(&config, NULL);
    full_replay = minisnn_worlds_kernel_create(&config, NULL);
    if (continuous == NULL || full_replay == NULL ||
        minisnn_worlds_kernel_command_log_create(&log) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(continuous, &initial_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        ok = 0;
        goto cleanup;
    }

    for (tick = 0U; tick < K2D_TICKS; ++tick)
    {
        if (!submit_workload_for_tick(log, continuous) || !draw_random_and_step(continuous))
        {
            ok = 0;
            goto cleanup;
        }
        if (minisnn_worlds_kernel_tick(continuous) == K2D_CHECKPOINT_TICK)
        {
            checkpoint_cursor = minisnn_worlds_kernel_command_log_count(log);
            if (minisnn_worlds_kernel_snapshot_capture(continuous, &checkpoint) !=
                    MINISNN_WORLDS_KERNEL_ERROR_NONE ||
                minisnn_worlds_kernel_state_hash(continuous, &checkpoint_hash) !=
                    MINISNN_WORLDS_KERNEL_ERROR_NONE ||
                !save_snapshot(checkpoint_path, checkpoint))
            {
                ok = 0;
                goto cleanup;
            }
        }
    }
    if (checkpoint == NULL || checkpoint_cursor == 0U ||
        minisnn_worlds_kernel_command_log_save_file(command_log_path, log) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_snapshot_capture(continuous, &continuous_final) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(continuous, &continuous_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !save_snapshot(final_snapshot_path, continuous_final) ||
        !save_snapshot(continuous_path, continuous_final) ||
        minisnn_worlds_kernel_command_log_load_file(command_log_path, &loaded_log) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        ok = 0;
        goto cleanup;
    }

    if (minisnn_worlds_kernel_replay_session_create(loaded_log, 0U, initial_hash, &session) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        ok = 0;
        goto cleanup;
    }
    while (minisnn_worlds_kernel_tick(full_replay) < K2D_TICKS)
    {
        if (!replay_tick(session, loaded_log, full_replay))
        {
            ok = 0;
            goto cleanup;
        }
    }
    if (minisnn_worlds_kernel_replay_session_cursor(session) !=
            minisnn_worlds_kernel_command_log_count(loaded_log) ||
        minisnn_worlds_kernel_snapshot_capture(full_replay, &full_final) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(full_replay, &full_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !save_snapshot(full_replay_path, full_final))
    {
        ok = 0;
        goto cleanup;
    }
    minisnn_worlds_kernel_replay_session_destroy(session);
    session = NULL;

    checkpoint_replay = minisnn_worlds_kernel_create(&config, NULL);
    if (checkpoint_replay == NULL ||
        minisnn_worlds_kernel_replay_session_create(loaded_log, 0U, initial_hash, &session) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        ok = 0;
        goto cleanup;
    }
    while (minisnn_worlds_kernel_tick(checkpoint_replay) < K2D_CHECKPOINT_TICK)
    {
        if (!replay_tick(session, loaded_log, checkpoint_replay))
        {
            ok = 0;
            goto cleanup;
        }
    }
    if (minisnn_worlds_kernel_replay_session_cursor(session) != checkpoint_cursor)
    {
        ok = 0;
        goto cleanup;
    }
    minisnn_worlds_kernel_replay_session_destroy(session);
    session = NULL;
    minisnn_worlds_kernel_destroy(checkpoint_replay);
    checkpoint_replay = NULL;
    if (minisnn_worlds_kernel_snapshot_load_file(checkpoint_path, &loaded_checkpoint) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_create_from_snapshot(loaded_checkpoint, &checkpoint_replay) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_replay_session_create(
            loaded_log, checkpoint_cursor, checkpoint_hash, &session) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_replay_session_validate(session, checkpoint_replay) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        ok = 0;
        goto cleanup;
    }
    while (minisnn_worlds_kernel_tick(checkpoint_replay) < K2D_TICKS)
    {
        if (!replay_tick(session, loaded_log, checkpoint_replay))
        {
            ok = 0;
            goto cleanup;
        }
    }
    if (minisnn_worlds_kernel_snapshot_capture(checkpoint_replay, &restored_final) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(checkpoint_replay, &restored_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !save_snapshot(restored_path, restored_final) ||
        !snapshots_equal(continuous_final, full_final) ||
        !snapshots_equal(continuous_final, restored_final) ||
        continuous_hash != full_hash || continuous_hash != restored_hash ||
        minisnn_worlds_kernel_get_diagnostics(continuous, &diagnostics) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        diagnostics.total_commands_rejected == 0U ||
        diagnostics.total_movement_overflows_rejected == 0U ||
        diagnostics.total_spatial_links_created == 0U)
    {
        ok = 0;
        goto cleanup;
    }

    final_digest = fnv1a(minisnn_worlds_kernel_snapshot_data(continuous_final),
                         minisnn_worlds_kernel_snapshot_size(continuous_final));
    summary = fopen(summary_path, "wb");
    hashes = fopen(hashes_path, "wb");
    if (summary == NULL || hashes == NULL ||
        fprintf(summary, "k2_final_scenario_version=1\n") < 0 ||
        fprintf(summary, "tick_count=%u\n", K2D_TICKS) < 0 ||
        fprintf(summary, "command_record_count=%zu\n",
                minisnn_worlds_kernel_command_log_count(log)) < 0 ||
        fprintf(summary, "command_log_format_version=1\n") < 0 ||
        fprintf(summary, "command_log_size=%zu\n",
                minisnn_worlds_kernel_command_log_size(log)) < 0 ||
        fprintf(summary, "command_log_digest=0x%016" PRIX64 "\n",
                minisnn_worlds_kernel_command_log_digest(log)) < 0 ||
        fprintf(summary, "initial_state_hash=0x%016" PRIX64 "\n", initial_hash) < 0 ||
        fprintf(summary, "checkpoint_tick=%u\n", K2D_CHECKPOINT_TICK) < 0 ||
        fprintf(summary, "checkpoint_record_cursor=%zu\n", checkpoint_cursor) < 0 ||
        fprintf(summary, "checkpoint_snapshot_size=%zu\n",
                minisnn_worlds_kernel_snapshot_size(checkpoint)) < 0 ||
        fprintf(summary, "checkpoint_snapshot_digest=0x%016" PRIX64 "\n",
                fnv1a(minisnn_worlds_kernel_snapshot_data(checkpoint),
                      minisnn_worlds_kernel_snapshot_size(checkpoint))) < 0 ||
        fprintf(summary, "checkpoint_state_hash=0x%016" PRIX64 "\n", checkpoint_hash) < 0 ||
        fprintf(summary, "final_continuous_hash=0x%016" PRIX64 "\n", continuous_hash) < 0 ||
        fprintf(summary, "final_full_replay_hash=0x%016" PRIX64 "\n", full_hash) < 0 ||
        fprintf(summary, "final_restored_replay_hash=0x%016" PRIX64 "\n", restored_hash) < 0 ||
        fprintf(summary, "final_snapshot_size=%zu\n",
                minisnn_worlds_kernel_snapshot_size(continuous_final)) < 0 ||
        fprintf(summary, "final_snapshot_digest=0x%016" PRIX64 "\n", final_digest) < 0 ||
        fprintf(summary, "divergence_count=0\n") < 0 ||
        fprintf(summary, "replay=PASSOU\n") < 0 ||
        fprintf(hashes, "initial=0x%016" PRIX64 "\n", initial_hash) < 0 ||
        fprintf(hashes, "checkpoint=0x%016" PRIX64 "\n", checkpoint_hash) < 0 ||
        fprintf(hashes, "continuous=0x%016" PRIX64 "\n", continuous_hash) < 0 ||
        fprintf(hashes, "full_replay=0x%016" PRIX64 "\n", full_hash) < 0 ||
        fprintf(hashes, "restored_replay=0x%016" PRIX64 "\n", restored_hash) < 0 ||
        fclose(summary) != 0 || fclose(hashes) != 0)
    {
        summary = NULL;
        hashes = NULL;
        ok = 0;
        goto cleanup;
    }
    summary = NULL;
    hashes = NULL;
    printf("K2-D demo OK: %s\n", directory);

cleanup:
    if (summary != NULL)
    {
        fclose(summary);
    }
    if (hashes != NULL)
    {
        fclose(hashes);
    }
    minisnn_worlds_kernel_replay_session_destroy(session);
    minisnn_worlds_kernel_snapshot_destroy(restored_final);
    minisnn_worlds_kernel_snapshot_destroy(full_final);
    minisnn_worlds_kernel_snapshot_destroy(continuous_final);
    minisnn_worlds_kernel_snapshot_destroy(loaded_checkpoint);
    minisnn_worlds_kernel_snapshot_destroy(checkpoint);
    minisnn_worlds_kernel_command_log_destroy(loaded_log);
    minisnn_worlds_kernel_command_log_destroy(log);
    minisnn_worlds_kernel_destroy(checkpoint_replay);
    minisnn_worlds_kernel_destroy(full_replay);
    minisnn_worlds_kernel_destroy(continuous);
    if (!ok)
    {
        fprintf(stderr, "K2-D persistence and replay demo failed\n");
        return 1;
    }
    return 0;
}