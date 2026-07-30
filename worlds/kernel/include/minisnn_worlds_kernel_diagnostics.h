#ifndef MINISNN_WORLDS_KERNEL_DIAGNOSTICS_H
#define MINISNN_WORLDS_KERNEL_DIAGNOSTICS_H

#include "minisnn_worlds_kernel_types.h"
#include "minisnn_worlds_kernel_space.h"

typedef struct
{
    MiniSNNWorldsTick completed_ticks;
    MiniSNNWorldsKernelState state;
    MiniSNNWorldsKernelError last_error;
    uint64_t alive_entities;
    uint64_t total_entities_created;
    uint64_t total_entities_destroyed;
    uint64_t placed_entities;
    uint64_t total_entities_placed;
    uint64_t total_entities_removed_from_space;
    uint64_t pending_commands;
    uint64_t total_commands_submitted;
    uint64_t total_commands_applied;
    uint64_t total_commands_rejected;
    uint64_t last_tick_events;
    uint64_t total_events_emitted;
    uint64_t master_seed;
    uint64_t random_streams;
    uint64_t total_random_u32_generated;
    uint64_t current_state_hash;
    uint32_t state_hash_version;
    uint32_t prng_version;
    MiniSNNWorldsKernelScalar space_min_x;
    MiniSNNWorldsKernelScalar space_min_y;
    MiniSNNWorldsKernelScalar space_max_x;
    MiniSNNWorldsKernelScalar space_max_y;
    int64_t scalar_scale;
} MiniSNNWorldsKernelDiagnostics;

typedef struct
{
    MiniSNNWorldsTick tick;
    uint64_t state_hash;
    uint64_t alive_entities;
    uint64_t pending_commands;
    uint64_t last_tick_events;
    uint64_t random_streams;
    uint64_t total_random_u32_generated;
} MiniSNNWorldsKernelTracePoint;

#endif
