#ifndef MINISNN_WORLDS_KERNEL_DIAGNOSTICS_H
#define MINISNN_WORLDS_KERNEL_DIAGNOSTICS_H

#include "minisnn_worlds_kernel_types.h"

typedef struct
{
    MiniSNNWorldsTick completed_ticks;
    MiniSNNWorldsKernelState state;
    MiniSNNWorldsKernelError last_error;
} MiniSNNWorldsKernelDiagnostics;

#endif
