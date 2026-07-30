#include <stdio.h>

#include "minisnn_worlds_kernel.h"

int main(void)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelRandomStreamKey stream = { UINT64_C(1), UINT64_C(1) };
    MiniSNNWorldsKernelEntityId external = { 0U };
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelEvent event;
    MiniSNNWorldsKernelDiagnostics diagnostics;
    uint32_t random_value;
    uint64_t state_hash;

    config.master_seed = UINT64_C(12345);
    kernel = minisnn_worlds_kernel_create(&config, &error);
    if (kernel == NULL || error != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_random_u32(kernel, stream, &random_value) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(kernel, &state_hash) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 0U, external, &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &event) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE || event.event_id.value == 0U ||
        diagnostics.alive_entities != 1U || random_value == 0U || state_hash == 0U)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 1;
    }
    minisnn_worlds_kernel_destroy(kernel);
    printf("K0 external consumer validation OK\n");
    return 0;
}
