#include "k2_snapshot_file.h"
#include "k2_command_log_file.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

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

static int capture_submission(MiniSNNWorldsKernelCommandLog *log,
                              MiniSNNWorldsKernel *kernel,
                              MiniSNNWorldsKernelCommandId command)
{
    return minisnn_worlds_kernel_command_log_capture_submission(
               log, kernel, minisnn_worlds_kernel_tick(kernel), command) ==
           MINISNN_WORLDS_KERNEL_ERROR_NONE;
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

int main(int argc, char **argv)
{
    const char *output_directory = argc == 2 ? argv[1] : ".";
    char snapshot_path[512];
    char command_log_path[512];
    char summary_path[512];
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *source = NULL;
    MiniSNNWorldsKernel *restored = NULL;
    MiniSNNWorldsKernelSnapshot *checkpoint = NULL;
    MiniSNNWorldsKernelSnapshot *loaded_snapshot = NULL;
    MiniSNNWorldsKernelSnapshot *source_final = NULL;
    MiniSNNWorldsKernelSnapshot *restored_final = NULL;
    MiniSNNWorldsKernelCommandLog *log = NULL;
    MiniSNNWorldsKernelCommandLog *loaded_log = NULL;
    MiniSNNWorldsKernelCommandId command;
    uint64_t initial_hash;
    uint64_t checkpoint_hash;
    uint64_t final_source_hash;
    uint64_t final_restored_hash;
    size_t replay_cursor;
    FILE *summary = NULL;
    int paths_ok;
    int summary_ok;

    paths_ok = argc <= 2 &&
        snprintf(snapshot_path, sizeof(snapshot_path), "%s/snapshot.bin", output_directory) >= 0 &&
        snprintf(snapshot_path, sizeof(snapshot_path), "%s/snapshot.bin", output_directory) <
            (int)sizeof(snapshot_path) &&
        snprintf(command_log_path, sizeof(command_log_path), "%s/command_log.bin", output_directory) >= 0 &&
        snprintf(command_log_path, sizeof(command_log_path), "%s/command_log.bin", output_directory) <
            (int)sizeof(command_log_path) &&
        snprintf(summary_path, sizeof(summary_path), "%s/summary.txt", output_directory) >= 0 &&
        snprintf(summary_path, sizeof(summary_path), "%s/summary.txt", output_directory) <
            (int)sizeof(summary_path);
    if (!paths_ok)
    {
        fprintf(stderr, "usage: %s [output_directory]\n", argv[0]);
        return 1;
    }
    config.master_seed = UINT64_C(0x4B32435F44454D4F);
    source = minisnn_worlds_kernel_create(&config, &error);
    if (source == NULL || minisnn_worlds_kernel_command_log_create(&log) !=
                              MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(source, &initial_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_create_entity(source, 1U, 1U, id(0U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE || !capture_submission(log, source, command) ||
        minisnn_worlds_kernel_queue_create_entity(source, 1U, 0U, id(0U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE || !capture_submission(log, source, command) ||
        minisnn_worlds_kernel_step(source) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(source, 2U, 1U, id(0U), id(1U),
                                                  transform(0, 0), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE || !capture_submission(log, source, command) ||
        minisnn_worlds_kernel_queue_place_entity(source, 2U, 0U, id(0U), id(2U),
                                                  transform(20, 0), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE || !capture_submission(log, source, command) ||
        minisnn_worlds_kernel_step(source) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_snapshot_capture(source, &checkpoint) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(source, &checkpoint_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_snapshot_save_file(snapshot_path, checkpoint) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_move_entity(source, 3U, 0U, id(0U), id(1U), 5, 0,
                                                &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !capture_submission(log, source, command) ||
        minisnn_worlds_kernel_command_log_save_file(command_log_path, log) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(source) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_snapshot_capture(source, &source_final) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(source, &final_source_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_snapshot_load_file(snapshot_path, &loaded_snapshot) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_create_from_snapshot(loaded_snapshot, &restored) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_command_log_load_file(command_log_path, &loaded_log) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_command_log_replay_next(loaded_log, 4U, restored) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(restored) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_snapshot_capture(restored, &restored_final) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(restored, &final_restored_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_snapshot_size(source_final) !=
            minisnn_worlds_kernel_snapshot_size(restored_final) ||
        memcmp(minisnn_worlds_kernel_snapshot_data(source_final),
               minisnn_worlds_kernel_snapshot_data(restored_final),
               minisnn_worlds_kernel_snapshot_size(source_final)) != 0 ||
        final_source_hash != final_restored_hash)
    {
        fprintf(stderr, "K2-C command replay demo failed\n");
        minisnn_worlds_kernel_snapshot_destroy(restored_final);
        minisnn_worlds_kernel_snapshot_destroy(source_final);
        minisnn_worlds_kernel_command_log_destroy(loaded_log);
        minisnn_worlds_kernel_destroy(restored);
        minisnn_worlds_kernel_snapshot_destroy(loaded_snapshot);
        minisnn_worlds_kernel_snapshot_destroy(checkpoint);
        minisnn_worlds_kernel_command_log_destroy(log);
        minisnn_worlds_kernel_destroy(source);
        return 1;
    }
    replay_cursor = 4U;
    summary = fopen(summary_path, "wb");
    summary_ok = summary != NULL &&
        fprintf(summary, "command_log_format=1\n") >= 0 &&
        fprintf(summary, "record_count=%zu\n", minisnn_worlds_kernel_command_log_count(log)) >= 0 &&
        fprintf(summary, "log_size=%zu\n", minisnn_worlds_kernel_command_log_size(log)) >= 0 &&
        fprintf(summary, "log_digest=0x%016" PRIX64 "\n",
                minisnn_worlds_kernel_command_log_digest(log)) >= 0 &&
        fprintf(summary, "initial_state_hash=0x%016" PRIX64 "\n", initial_hash) >= 0 &&
        fprintf(summary, "checkpoint_tick=2\n") >= 0 &&
        fprintf(summary, "checkpoint_record_cursor=%zu\n", replay_cursor) >= 0 &&
        fprintf(summary, "checkpoint_state_hash=0x%016" PRIX64 "\n", checkpoint_hash) >= 0 &&
        fprintf(summary, "final_original_hash=0x%016" PRIX64 "\n", final_source_hash) >= 0 &&
        fprintf(summary, "final_replay_hash=0x%016" PRIX64 "\n", final_restored_hash) >= 0 &&
        fprintf(summary, "final_snapshot_digest=0x%016" PRIX64 "\n",
                fnv1a(minisnn_worlds_kernel_snapshot_data(source_final),
                      minisnn_worlds_kernel_snapshot_size(source_final))) >= 0 &&
        fprintf(summary, "divergence_count=0\n") >= 0 &&
        fprintf(summary, "replay=PASSOU\n") >= 0;
    if (summary != NULL && fclose(summary) != 0)
    {
        summary_ok = 0;
    }
    if (!summary_ok)
    {
        fprintf(stderr, "could not write K2-C summary\n");
        minisnn_worlds_kernel_snapshot_destroy(restored_final);
        minisnn_worlds_kernel_snapshot_destroy(source_final);
        minisnn_worlds_kernel_command_log_destroy(loaded_log);
        minisnn_worlds_kernel_destroy(restored);
        minisnn_worlds_kernel_snapshot_destroy(loaded_snapshot);
        minisnn_worlds_kernel_snapshot_destroy(checkpoint);
        minisnn_worlds_kernel_command_log_destroy(log);
        minisnn_worlds_kernel_destroy(source);
        return 1;
    }
    printf("K2-C demo OK: %s\n", command_log_path);
    minisnn_worlds_kernel_snapshot_destroy(restored_final);
    minisnn_worlds_kernel_snapshot_destroy(source_final);
    minisnn_worlds_kernel_command_log_destroy(loaded_log);
    minisnn_worlds_kernel_destroy(restored);
    minisnn_worlds_kernel_snapshot_destroy(loaded_snapshot);
    minisnn_worlds_kernel_snapshot_destroy(checkpoint);
    minisnn_worlds_kernel_command_log_destroy(log);
    minisnn_worlds_kernel_destroy(source);
    return 0;
}