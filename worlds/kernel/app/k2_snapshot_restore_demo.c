#include "k2_snapshot_file.h"

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
    char summary_path[512];
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *source = NULL;
    MiniSNNWorldsKernel *restored = NULL;
    MiniSNNWorldsKernelSnapshot *captured = NULL;
    MiniSNNWorldsKernelSnapshot *loaded = NULL;
    MiniSNNWorldsKernelSnapshot *after_restore = NULL;
    MiniSNNWorldsKernelSnapshot *final_source = NULL;
    MiniSNNWorldsKernelSnapshot *final_restored = NULL;
    MiniSNNWorldsKernelCommandId command;
    uint64_t checkpoint_source_hash;
    uint64_t checkpoint_restored_hash;
    uint64_t final_source_hash;
    uint64_t final_restored_hash;
    FILE *summary = NULL;
    int snapshot_path_length;
    int summary_path_length;
    int summary_ok;

    snapshot_path_length = snprintf(snapshot_path, sizeof(snapshot_path), "%s/snapshot.bin",
                                    output_directory);
    summary_path_length = snprintf(summary_path, sizeof(summary_path), "%s/summary.txt",
                                   output_directory);
    if (argc > 2 || snapshot_path_length < 0 ||
        (size_t)snapshot_path_length >= sizeof(snapshot_path) || summary_path_length < 0 ||
        (size_t)summary_path_length >= sizeof(summary_path))
    {
        fprintf(stderr, "usage: %s [output_directory]\n", argv[0]);
        return 1;
    }
    config.master_seed = UINT64_C(0x4B32425F44454D4F);
    source = minisnn_worlds_kernel_create(&config, &error);
    if (source == NULL ||
        minisnn_worlds_kernel_queue_create_entity(source, 1U, 0U, id(0U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_create_entity(source, 1U, 1U, id(0U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(source) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(source, 2U, 0U, id(0U), id(1U),
                                                  transform(10, 20), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(source, 2U, 1U, id(0U), id(2U),
                                                  transform(30, 20), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(source) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_destroy_entity(source, 3U, 0U, id(0U), id(2U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_snapshot_capture(source, &captured) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_snapshot_save_file(snapshot_path, captured) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_snapshot_load_file(snapshot_path, &loaded) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_create_from_snapshot(loaded, &restored) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_snapshot_capture(restored, &after_restore) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(source, &checkpoint_source_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(restored, &checkpoint_restored_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_snapshot_size(captured) !=
            minisnn_worlds_kernel_snapshot_size(after_restore) ||
        memcmp(minisnn_worlds_kernel_snapshot_data(captured),
               minisnn_worlds_kernel_snapshot_data(after_restore),
               minisnn_worlds_kernel_snapshot_size(captured)) != 0 ||
        checkpoint_source_hash != checkpoint_restored_hash ||
        minisnn_worlds_kernel_step(source) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(restored) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_snapshot_capture(source, &final_source) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_snapshot_capture(restored, &final_restored) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(source, &final_source_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(restored, &final_restored_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_snapshot_size(final_source) !=
            minisnn_worlds_kernel_snapshot_size(final_restored) ||
        memcmp(minisnn_worlds_kernel_snapshot_data(final_source),
               minisnn_worlds_kernel_snapshot_data(final_restored),
               minisnn_worlds_kernel_snapshot_size(final_source)) != 0 ||
        final_source_hash != final_restored_hash)
    {
        fprintf(stderr, "K2-B snapshot restore demo failed\n");
        minisnn_worlds_kernel_snapshot_destroy(final_restored);
        minisnn_worlds_kernel_snapshot_destroy(final_source);
        minisnn_worlds_kernel_snapshot_destroy(after_restore);
        minisnn_worlds_kernel_destroy(restored);
        minisnn_worlds_kernel_snapshot_destroy(loaded);
        minisnn_worlds_kernel_snapshot_destroy(captured);
        minisnn_worlds_kernel_destroy(source);
        return 1;
    }
    summary = fopen(summary_path, "wb");
    summary_ok = summary != NULL && fprintf(summary, "snapshot_format=1\n") >= 0 &&
                                  fprintf(summary, "checkpoint_tick=2\n") >= 0 &&
                                  fprintf(summary, "final_tick=3\n") >= 0 &&
                                  fprintf(summary, "snapshot_size=%zu\n",
                         minisnn_worlds_kernel_snapshot_size(captured)) >= 0 &&
                 fprintf(summary, "snapshot_digest=0x%016" PRIX64 "\n",
                         fnv1a(minisnn_worlds_kernel_snapshot_data(captured),
                               minisnn_worlds_kernel_snapshot_size(captured))) >= 0 &&
                 fprintf(summary, "checkpoint_state_hash=0x%016" PRIX64 "\n",
                         checkpoint_source_hash) >= 0 &&
                 fprintf(summary, "restored_checkpoint_state_hash=0x%016" PRIX64 "\n",
                         checkpoint_restored_hash) >= 0 &&
                 fprintf(summary, "final_continuous_state_hash=0x%016" PRIX64 "\n",
                         final_source_hash) >= 0 &&
                 fprintf(summary, "final_restored_state_hash=0x%016" PRIX64 "\n",
                         final_restored_hash) >= 0 &&
                 fprintf(summary, "divergence_tick=NA\n") >= 0 &&
                 fprintf(summary, "round_trip=PASSOU\n") >= 0;
    if (summary != NULL && fclose(summary) != 0)
    {
        summary_ok = 0;
    }
    if (!summary_ok)
    {
        fprintf(stderr, "could not write K2-B summary\n");
        minisnn_worlds_kernel_snapshot_destroy(final_restored);
        minisnn_worlds_kernel_snapshot_destroy(final_source);
        minisnn_worlds_kernel_snapshot_destroy(after_restore);
        minisnn_worlds_kernel_destroy(restored);
        minisnn_worlds_kernel_snapshot_destroy(loaded);
        minisnn_worlds_kernel_snapshot_destroy(captured);
        minisnn_worlds_kernel_destroy(source);
        return 1;
    }
    printf("K2-B demo OK: %s\n", snapshot_path);
    minisnn_worlds_kernel_snapshot_destroy(final_restored);
    minisnn_worlds_kernel_snapshot_destroy(final_source);
    minisnn_worlds_kernel_snapshot_destroy(after_restore);
    minisnn_worlds_kernel_destroy(restored);
    minisnn_worlds_kernel_snapshot_destroy(loaded);
    minisnn_worlds_kernel_snapshot_destroy(captured);
    minisnn_worlds_kernel_destroy(source);
    return 0;
}