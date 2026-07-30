#include "minisnn_worlds_kernel.h"

#include <stdint.h>
#include <stdio.h>

typedef struct
{
    uint64_t initial_hash;
    uint64_t tick_one_hash;
    uint64_t tick_two_hash;
    uint32_t stream_one_value_one;
    uint32_t stream_two_value_one;
    uint32_t stream_one_value_two;
    MiniSNNWorldsKernelDiagnostics diagnostics;
} DemoResult;

static int run_trajectory(uint64_t seed, DemoResult *out_result)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelRandomStreamKey stream_one = { UINT64_C(1), UINT64_C(1) };
    MiniSNNWorldsKernelRandomStreamKey stream_two = { UINT64_C(1), UINT64_C(2) };
    MiniSNNWorldsKernelEntityId external = { UINT64_C(0) };
    MiniSNNWorldsKernelEntityId first = { UINT64_C(1) };
    MiniSNNWorldsKernelEntityId second = { UINT64_C(2) };
    MiniSNNWorldsKernelCommandId command_id;

    config.master_seed = seed;
    kernel = minisnn_worlds_kernel_create(&config, &error);
    if (kernel == NULL || minisnn_worlds_kernel_state_hash(kernel, &out_result->initial_hash) !=
                              MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_random_u32(kernel, stream_one,
                                         &out_result->stream_one_value_one) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_random_u32(kernel, stream_two,
                                         &out_result->stream_two_value_one) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 1U, external,
                                                   &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 0U, external,
                                                   &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(kernel, &out_result->tick_one_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_random_u32(kernel, stream_one,
                                         &out_result->stream_one_value_two) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_destroy_entity(kernel, 2U, 0U, first, second,
                                                    &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(kernel, &out_result->tick_two_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_get_diagnostics(kernel, &out_result->diagnostics) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    minisnn_worlds_kernel_destroy(kernel);
    return 1;
}

static int result_matches(const DemoResult *left, const DemoResult *right)
{
    return left->initial_hash == right->initial_hash &&
           left->tick_one_hash == right->tick_one_hash &&
           left->tick_two_hash == right->tick_two_hash &&
           left->stream_one_value_one == right->stream_one_value_one &&
           left->stream_two_value_one == right->stream_two_value_one &&
           left->stream_one_value_two == right->stream_one_value_two &&
           left->diagnostics.alive_entities == right->diagnostics.alive_entities &&
           left->diagnostics.random_streams == right->diagnostics.random_streams &&
           left->diagnostics.total_random_u32_generated ==
               right->diagnostics.total_random_u32_generated;
}

int main(void)
{
    DemoResult first = { 0 };
    DemoResult second = { 0 };
    DemoResult different = { 0 };
    int repeat_match;
    int different_seed_diverged;

    if (!run_trajectory(UINT64_C(12345), &first) ||
        !run_trajectory(UINT64_C(12345), &second) ||
        !run_trajectory(UINT64_C(12346), &different))
    {
        fprintf(stderr, "K0-C demo failed\n");
        return 1;
    }
    repeat_match = result_matches(&first, &second);
    different_seed_diverged = first.initial_hash != different.initial_hash &&
                              first.stream_one_value_one !=
                                  different.stream_one_value_one;
    if (repeat_match == 0 || different_seed_diverged == 0)
    {
        fprintf(stderr, "K0-C determinism contract failed\n");
        return 1;
    }

    printf("miniSNN Worlds Kernel K0-C random/hash demo\n");
    printf("seed=12345\n");
    printf("initial_hash=%llu\n", (unsigned long long)first.initial_hash);
    printf("stream_1_1_value_1=%u\n", first.stream_one_value_one);
    printf("stream_1_2_value_1=%u\n", first.stream_two_value_one);
    printf("tick=1 hash=%llu\n", (unsigned long long)first.tick_one_hash);
    printf("tick=2 hash=%llu\n", (unsigned long long)first.tick_two_hash);
    printf("repeat_match=yes\n");
    printf("different_seed_diverged=yes\n");
    printf("random_streams=%llu\n", (unsigned long long)first.diagnostics.random_streams);
    printf("status=OK\n");
    return 0;
}
