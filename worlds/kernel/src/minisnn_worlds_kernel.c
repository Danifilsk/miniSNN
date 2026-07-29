#include "minisnn_worlds_kernel.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define MINISNN_WORLDS_KERNEL_CONFIG_MINIMUM_READABLE_SIZE \
    (offsetof(MiniSNNWorldsKernelConfig, format_version) + \
     sizeof(((MiniSNNWorldsKernelConfig *)0)->format_version))

struct MiniSNNWorldsKernel
{
    MiniSNNWorldsTick tick;
    MiniSNNWorldsKernelState state;
    MiniSNNWorldsKernelError last_error;
};

#ifdef MINISNN_WORLDS_KERNEL_TESTING
static int testing_fail_allocation = 0;
#endif

static void assign_error(
    MiniSNNWorldsKernelError *out_error,
    MiniSNNWorldsKernelError error)
{
    if (out_error != NULL)
    {
        *out_error = error;
    }
}

static int config_is_valid(const MiniSNNWorldsKernelConfig *config)
{
    const unsigned char *bytes;
    uint32_t struct_size;
    uint32_t format_version;

    if (config == NULL)
    {
        return 0;
    }

    bytes = (const unsigned char *)config;
    memcpy(&struct_size, bytes + offsetof(MiniSNNWorldsKernelConfig, struct_size),
           sizeof(struct_size));
    if (struct_size < (uint32_t)MINISNN_WORLDS_KERNEL_CONFIG_MINIMUM_READABLE_SIZE)
    {
        return 0;
    }

    memcpy(&format_version,
           bytes + offsetof(MiniSNNWorldsKernelConfig, format_version),
           sizeof(format_version));
    return format_version == MINISNN_WORLDS_KERNEL_CONFIG_VERSION;
}

static MiniSNNWorldsKernel *allocate_kernel(void)
{
#ifdef MINISNN_WORLDS_KERNEL_TESTING
    if (testing_fail_allocation != 0)
    {
        testing_fail_allocation = 0;
        return NULL;
    }
#endif
    return malloc(sizeof(MiniSNNWorldsKernel));
}

static MiniSNNWorldsKernelError run_tick_phases(MiniSNNWorldsKernel *kernel)
{
    /* BEGIN, PROCESS, and FINALIZE are deliberately explicit K0-A phase slots. */
    (void)kernel;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelConfig minisnn_worlds_kernel_config_default(void)
{
    MiniSNNWorldsKernelConfig config;

    config.struct_size = (uint32_t)sizeof(config);
    config.format_version = MINISNN_WORLDS_KERNEL_CONFIG_VERSION;
    return config;
}

MiniSNNWorldsKernel *minisnn_worlds_kernel_create(
    const MiniSNNWorldsKernelConfig *config,
    MiniSNNWorldsKernelError *out_error)
{
    MiniSNNWorldsKernelConfig default_config;
    MiniSNNWorldsKernel *kernel;

    assign_error(out_error, MINISNN_WORLDS_KERNEL_ERROR_NONE);
    if (config == NULL)
    {
        default_config = minisnn_worlds_kernel_config_default();
        config = &default_config;
    }
    if (!config_is_valid(config))
    {
        assign_error(out_error, MINISNN_WORLDS_KERNEL_ERROR_INVALID_CONFIG);
        return NULL;
    }

    kernel = allocate_kernel();
    if (kernel == NULL)
    {
        assign_error(out_error, MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
        return NULL;
    }

    kernel->tick = MINISNN_WORLDS_TICK_INITIAL;
    kernel->state = MINISNN_WORLDS_KERNEL_STATE_READY;
    kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_NONE;
    return kernel;
}

void minisnn_worlds_kernel_destroy(MiniSNNWorldsKernel *kernel)
{
    free(kernel);
}

MiniSNNWorldsTick minisnn_worlds_kernel_tick(const MiniSNNWorldsKernel *kernel)
{
    return kernel == NULL ? MINISNN_WORLDS_TICK_INITIAL : kernel->tick;
}

MiniSNNWorldsKernelState minisnn_worlds_kernel_state(
    const MiniSNNWorldsKernel *kernel)
{
    return kernel == NULL ? MINISNN_WORLDS_KERNEL_STATE_FAULTED : kernel->state;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_last_error(
    const MiniSNNWorldsKernel *kernel)
{
    return kernel == NULL ? MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT :
                            kernel->last_error;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_step(MiniSNNWorldsKernel *kernel)
{
    MiniSNNWorldsKernelError error;

    if (kernel == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (kernel->state != MINISNN_WORLDS_KERNEL_STATE_READY)
    {
        kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE;
        return kernel->last_error;
    }
    if (kernel->tick == UINT64_MAX)
    {
        kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_TICK_OVERFLOW;
        return kernel->last_error;
    }

    kernel->state = MINISNN_WORLDS_KERNEL_STATE_STEPPING;
    error = run_tick_phases(kernel);
    if (error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        kernel->state = MINISNN_WORLDS_KERNEL_STATE_FAULTED;
        kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
        return kernel->last_error;
    }

    ++kernel->tick;
    kernel->state = MINISNN_WORLDS_KERNEL_STATE_READY;
    kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_NONE;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_get_diagnostics(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelDiagnostics *out_diagnostics)
{
    MiniSNNWorldsKernelDiagnostics diagnostics;

    if (kernel == NULL || out_diagnostics == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }

    diagnostics.completed_ticks = kernel->tick;
    diagnostics.state = kernel->state;
    diagnostics.last_error = kernel->last_error;
    *out_diagnostics = diagnostics;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

#ifdef MINISNN_WORLDS_KERNEL_TESTING
void minisnn_worlds_kernel_testing_fail_next_allocation(void)
{
    testing_fail_allocation = 1;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_set_tick(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick tick)
{
    if (kernel == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    if (kernel->state != MINISNN_WORLDS_KERNEL_STATE_READY)
    {
        kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE;
        return kernel->last_error;
    }

    kernel->tick = tick;
    kernel->last_error = MINISNN_WORLDS_KERNEL_ERROR_NONE;
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}
#endif
