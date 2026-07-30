#ifndef MINISNN_WORLDS_KERNEL_DIAGNOSTICS_H
#define MINISNN_WORLDS_KERNEL_DIAGNOSTICS_H

#include "minisnn_worlds_kernel_types.h"

typedef struct
{
    MiniSNNWorldsTick completed_ticks;
    MiniSNNWorldsKernelState state;
    MiniSNNWorldsKernelError last_error;
    uint64_t alive_entities;
    uint64_t total_entities_created;
    uint64_t total_entities_destroyed;
    uint64_t pending_commands;
    uint64_t total_commands_submitted;
    uint64_t total_commands_applied;
    uint64_t total_commands_rejected;
    uint64_t last_tick_events;
    uint64_t total_events_emitted;
} MiniSNNWorldsKernelDiagnostics;

#endif
