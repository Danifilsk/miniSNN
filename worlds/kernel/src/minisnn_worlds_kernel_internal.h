#ifndef MINISNN_WORLDS_KERNEL_INTERNAL_H
#define MINISNN_WORLDS_KERNEL_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "minisnn_worlds_kernel.h"

typedef struct
{
    MiniSNNWorldsKernelEntityId entity_id;
    MiniSNNWorldsTick creation_tick;
    MiniSNNWorldsTick destruction_tick;
    int alive;
    int has_transform;
    MiniSNNWorldsKernelTransform transform;
    int has_occupancy;
    MiniSNNWorldsKernelOccupancy occupancy;
} EntityRecord;

typedef MiniSNNWorldsKernelSpatialLink SpatialLinkRecord;

typedef struct
{
    MiniSNNWorldsKernelRandomStreamKey key;
    uint64_t state;
    uint64_t sequence;
    uint64_t generated_u32_count;
} RandomStreamRecord;

struct MiniSNNWorldsKernelSnapshot
{
    uint8_t *data;
    size_t size;
    uint32_t format_version;
};

struct MiniSNNWorldsKernel
{
    MiniSNNWorldsTick tick;
    MiniSNNWorldsKernelState state;
    MiniSNNWorldsKernelError last_error;
    EntityRecord *entities;
    size_t entity_count;
    size_t entity_capacity;
    size_t alive_entity_count;
    SpatialLinkRecord *spatial_links;
    size_t spatial_link_count;
    size_t spatial_link_capacity;
    size_t placed_entity_count;
    size_t entities_with_occupancy;
    size_t active_occupancies;
    size_t blocking_occupancies;
    MiniSNNWorldsKernelSpaceBounds space_bounds;
    MiniSNNWorldsKernelCommandInfo *pending_commands;
    size_t pending_command_count;
    size_t pending_command_capacity;
    MiniSNNWorldsKernelEvent *last_tick_events;
    size_t last_tick_event_count;
    MiniSNNWorldsKernelEntityId next_entity_id;
    MiniSNNWorldsKernelCommandId next_command_id;
    MiniSNNWorldsKernelEventId next_event_id;
    uint64_t total_entities_created;
    uint64_t total_entities_destroyed;
    uint64_t total_entities_placed;
    uint64_t total_entities_removed_from_space;
    uint64_t total_occupancies_set;
    uint64_t total_occupancies_cleared;
    uint64_t total_occupancy_conflicts_rejected;
    uint64_t total_movement_commands_processed;
    uint64_t total_entities_moved;
    uint64_t total_movement_overflows_rejected;
    uint64_t total_spatial_links_created;
    uint64_t total_spatial_links_removed;
    uint64_t total_spatial_link_commands_processed;
    uint64_t total_commands_submitted;
    uint64_t total_commands_applied;
    uint64_t total_commands_rejected;
    uint64_t total_events_emitted;
    uint64_t master_seed;
    RandomStreamRecord *random_streams;
    size_t random_stream_count;
    size_t random_stream_capacity;
    uint64_t total_random_u32_generated;
    int command_batch_active;
    MiniSNNWorldsTick command_batch_begin_tick;
    size_t command_batch_pending_command_count;
    MiniSNNWorldsKernelCommandId command_batch_next_command_id;
    uint64_t command_batch_total_commands_submitted;
    MiniSNNWorldsKernelError command_batch_last_error;
};

void *minisnn_worlds_kernel_internal_allocate(size_t size);

/* Internal-only validation used by transactional snapshot restore. */
MiniSNNWorldsKernelError minisnn_worlds_kernel_internal_validate_invariants(
    const MiniSNNWorldsKernel *kernel);

#endif