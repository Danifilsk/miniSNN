#include "minisnn_worlds_kernel.h"

#include <stdint.h>
#include <stdio.h>

static int print_hash(const MiniSNNWorldsKernel *kernel)
{
    uint64_t hash;

    if (minisnn_worlds_kernel_state_hash(kernel, &hash) !=
        MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    printf("hash=%llu\n", (unsigned long long)hash);
    return 1;
}

int main(void)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelRandomStreamKey first = { UINT64_C(1), UINT64_C(1) };
    MiniSNNWorldsKernelRandomStreamKey second = { UINT64_C(9), UINT64_C(9) };
    MiniSNNWorldsKernelEntityId external = { UINT64_C(0) };
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelDiagnostics diagnostics;
    uint32_t value;
    uint64_t wide_value;

    config.master_seed = UINT64_C(12345);
    kernel = minisnn_worlds_kernel_create(&config, &error);
    if (kernel == NULL || !print_hash(kernel) ||
        minisnn_worlds_kernel_random_u32(kernel, first, &value) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 1;
    }
    printf("u32_first=%u\n", value);
    if (minisnn_worlds_kernel_random_u32(kernel, second, &value) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_random_bounded_u32(kernel, first, UINT32_C(17), &value) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_random_u64(kernel, first, &wide_value) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !print_hash(kernel) ||
        minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 1U, external, &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 0U, external, &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !print_hash(kernel) ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !print_hash(kernel) ||
        minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 1;
    }
    printf("bounded=%u\n", value);
    printf("u64=%llu\n", (unsigned long long)wide_value);
    printf("streams=%llu draws=%llu entities=%llu events=%llu\n",
           (unsigned long long)diagnostics.random_streams,
           (unsigned long long)diagnostics.total_random_u32_generated,
           (unsigned long long)diagnostics.alive_entities,
           (unsigned long long)diagnostics.last_tick_events);
    minisnn_worlds_kernel_destroy(kernel);
    return 0;
}
