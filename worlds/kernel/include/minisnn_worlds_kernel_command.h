#ifndef MINISNN_WORLDS_KERNEL_COMMAND_H
#define MINISNN_WORLDS_KERNEL_COMMAND_H

#include <stdbool.h>
#include <stddef.h>

#include "minisnn_worlds_kernel_types.h"
#include "minisnn_worlds_kernel_space.h"
#include "minisnn_worlds_kernel_occupancy.h"
#include "minisnn_worlds_kernel_spatial_link.h"

#define MINISNN_WORLDS_KERNEL_COMMAND_PRIORITY_DEFAULT UINT32_C(0)

typedef enum
{
    MINISNN_WORLDS_KERNEL_COMMAND_CREATE_ENTITY = 1,
    MINISNN_WORLDS_KERNEL_COMMAND_DESTROY_ENTITY = 2,
    MINISNN_WORLDS_KERNEL_COMMAND_PLACE_ENTITY = 3,
    MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_ENTITY_FROM_SPACE = 4,
    MINISNN_WORLDS_KERNEL_COMMAND_SET_OCCUPANCY = 5,
    MINISNN_WORLDS_KERNEL_COMMAND_CLEAR_OCCUPANCY = 6,
    MINISNN_WORLDS_KERNEL_COMMAND_MOVE_ENTITY = 7,
    MINISNN_WORLDS_KERNEL_COMMAND_CREATE_SPATIAL_LINK = 8,
    MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_SPATIAL_LINK = 9
} MiniSNNWorldsKernelCommandType;

typedef struct
{
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsTick target_tick;
    uint32_t priority;
    MiniSNNWorldsKernelEntityId issuer;
    MiniSNNWorldsKernelCommandType type;
    MiniSNNWorldsKernelEntityId target_entity;
    bool has_transform;
    MiniSNNWorldsKernelTransform transform;
    bool has_displacement;
    MiniSNNWorldsKernelPosition displacement;
    bool has_occupancy;
    MiniSNNWorldsKernelOccupancy occupancy;
    bool has_spatial_link_endpoints;
    MiniSNNWorldsKernelSpatialLinkEndpoints spatial_link_endpoints;
} MiniSNNWorldsKernelCommandInfo;

MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_create_entity(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelCommandId *out_command_id);

MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_destroy_entity(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelEntityId target_entity,
    MiniSNNWorldsKernelCommandId *out_command_id);

MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_place_entity(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelEntityId target_entity,
    MiniSNNWorldsKernelTransform transform,
    MiniSNNWorldsKernelCommandId *out_command_id);

MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_remove_entity_from_space(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelEntityId target_entity,
    MiniSNNWorldsKernelCommandId *out_command_id);

/* Queues one structural parent-to-child link; transforms remain global in K1-C1. */
MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_create_spatial_link(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelEntityId parent,
    MiniSNNWorldsKernelEntityId child,
    MiniSNNWorldsKernelCommandId *out_command_id);

/* Removes one structural parent-to-child link without moving either endpoint. */
MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_remove_spatial_link(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelEntityId parent,
    MiniSNNWorldsKernelEntityId child,
    MiniSNNWorldsKernelCommandId *out_command_id);

/* Queues one atomic fixed-point displacement for a placed entity. */
MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_move_entity(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelEntityId target_entity,
    MiniSNNWorldsKernelScalar delta_x,
    MiniSNNWorldsKernelScalar delta_y,
    MiniSNNWorldsKernelCommandId *out_command_id);

MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_set_occupancy(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelEntityId target_entity,
    MiniSNNWorldsKernelOccupancy occupancy,
    MiniSNNWorldsKernelCommandId *out_command_id);

MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_clear_occupancy(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelEntityId target_entity,
    MiniSNNWorldsKernelCommandId *out_command_id);

size_t minisnn_worlds_kernel_pending_command_count(
    const MiniSNNWorldsKernel *kernel);

MiniSNNWorldsKernelError minisnn_worlds_kernel_pending_command_at(
    const MiniSNNWorldsKernel *kernel,
    size_t canonical_index,
    MiniSNNWorldsKernelCommandInfo *out_command);

#endif
