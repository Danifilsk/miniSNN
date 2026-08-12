#include "minisnn_worlds_kernel.h"

#include <stdio.h>

static const char *state_name(MiniSNNWorldsKernelState state)
{
    return state == MINISNN_WORLDS_KERNEL_STATE_READY ? "READY" : "FAULTED";
}

int main(void)
{
    const int steps_requested = 10;
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernelDiagnostics diagnostics;
    MiniSNNWorldsKernel *kernel;
    int step;

    kernel = minisnn_worlds_kernel_create(NULL, &error);
    if (kernel == NULL)
    {
        return 1;
    }

    printf("miniSNN Worlds Kernel K0-A tick demo\n");
    printf("initial_tick=%llu\n", (unsigned long long)minisnn_worlds_kernel_tick(kernel));
    printf("steps_requested=%d\n", steps_requested);
    for (step = 0; step < steps_requested; ++step)
    {
        if (minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            minisnn_worlds_kernel_destroy(kernel);
            return 1;
        }
    }
    if (minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) !=
        MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 1;
    }

    printf("completed_ticks=%llu\n", (unsigned long long)diagnostics.completed_ticks);
    printf("state=%s\n", state_name(diagnostics.state));
    printf("status=OK\n");
    minisnn_worlds_kernel_destroy(kernel);
    return 0;
}
