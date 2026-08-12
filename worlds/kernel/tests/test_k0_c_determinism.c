#include "minisnn_worlds_kernel.h"

#include <stdint.h>
#include <stdio.h>

#define CHECK(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            fprintf(stderr, "K0-C determinism test failed: %s at line %d\n", #condition, __LINE__); \
            return 1; \
        } \
    } while (0)

typedef struct
{
    uint32_t first_draw;
    uint32_t second_draw;
    uint32_t other_stream_draw;
    uint64_t hashes[5];
    MiniSNNWorldsKernelDiagnostics diagnostics;
    MiniSNNWorldsKernelEvent events[2];
    MiniSNNWorldsKernelRandomStreamInfo streams[2];
} Snapshot;

static int snapshot_matches(const Snapshot *left, const Snapshot *right)
{
    size_t index;

    if (left->first_draw != right->first_draw ||
        left->second_draw != right->second_draw ||
        left->other_stream_draw != right->other_stream_draw ||
        left->diagnostics.completed_ticks != right->diagnostics.completed_ticks ||
        left->diagnostics.current_state_hash != right->diagnostics.current_state_hash ||
        left->diagnostics.total_random_u32_generated !=
            right->diagnostics.total_random_u32_generated ||
        left->diagnostics.random_streams != right->diagnostics.random_streams)
    {
        return 0;
    }
    for (index = 0U; index < 5U; ++index)
    {
        if (left->hashes[index] != right->hashes[index])
        {
            return 0;
        }
    }
    for (index = 0U; index < 2U; ++index)
    {
        if (left->events[index].event_id.value != right->events[index].event_id.value ||
            left->events[index].type != right->events[index].type ||
            left->streams[index].key.namespace_id != right->streams[index].key.namespace_id ||
            left->streams[index].key.stream_id != right->streams[index].key.stream_id ||
            left->streams[index].state != right->streams[index].state ||
            left->streams[index].generated_u32_count != right->streams[index].generated_u32_count)
        {
            return 0;
        }
    }
    return 1;
}

static int run_trajectory(uint64_t seed, Snapshot *out_snapshot)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelRandomStreamKey first = { UINT64_C(1), UINT64_C(1) };
    MiniSNNWorldsKernelRandomStreamKey second = { UINT64_C(1), UINT64_C(2) };
    MiniSNNWorldsKernelEntityId external = { UINT64_C(0) };
    MiniSNNWorldsKernelEntityId entity_one = { UINT64_C(1) };
    MiniSNNWorldsKernelEntityId entity_two = { UINT64_C(2) };
    MiniSNNWorldsKernelCommandId command;

    config.master_seed = seed;
    kernel = minisnn_worlds_kernel_create(&config, &error);
    if (kernel == NULL ||
        minisnn_worlds_kernel_state_hash(kernel, &out_snapshot->hashes[0]) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_random_u32(kernel, first, &out_snapshot->first_draw) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(kernel, &out_snapshot->hashes[1]) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_random_u32(kernel, second,
                                         &out_snapshot->other_stream_draw) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 1U, external, &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 0U, external, &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(kernel, &out_snapshot->hashes[2]) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &out_snapshot->events[0]) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_last_tick_event_at(kernel, 1U, &out_snapshot->events[1]) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_random_u32(kernel, first, &out_snapshot->second_draw) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_destroy_entity(kernel, 2U, 0U, entity_one, entity_two,
                                                    &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(kernel, &out_snapshot->hashes[3]) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(kernel, &out_snapshot->hashes[4]) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_random_stream_at(kernel, 0U, &out_snapshot->streams[0]) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_random_stream_at(kernel, 1U, &out_snapshot->streams[1]) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_get_diagnostics(kernel, &out_snapshot->diagnostics) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    minisnn_worlds_kernel_destroy(kernel);
    return 1;
}

int main(void)
{
    Snapshot first = { 0 };
    Snapshot second = { 0 };
    Snapshot third = { 0 };
    Snapshot different_one = { 0 };
    Snapshot different_two = { 0 };

    CHECK(run_trajectory(UINT64_C(12345), &first));
    CHECK(run_trajectory(UINT64_C(12345), &second));
    CHECK(run_trajectory(UINT64_C(12345), &third));
    CHECK(snapshot_matches(&first, &second));
    CHECK(snapshot_matches(&first, &third));
    CHECK(run_trajectory(UINT64_C(12346), &different_one));
    CHECK(run_trajectory(UINT64_C(12346), &different_two));
    CHECK(snapshot_matches(&different_one, &different_two));
    CHECK(first.first_draw != different_one.first_draw);
    CHECK(first.hashes[0] != different_one.hashes[0]);
    CHECK(first.diagnostics.completed_ticks == UINT64_C(2));
    CHECK(first.diagnostics.random_streams == UINT64_C(2));
    CHECK(first.diagnostics.total_random_u32_generated == UINT64_C(3));
    printf("K0-C deterministic random/hash validation OK\n");
    return 0;
}
