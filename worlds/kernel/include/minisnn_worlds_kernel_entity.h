#ifndef MINISNN_WORLDS_KERNEL_ENTITY_H
#define MINISNN_WORLDS_KERNEL_ENTITY_H

#include <stdbool.h>
#include <stddef.h>

#include "minisnn_worlds_kernel_types.h"
#include "minisnn_worlds_kernel_space.h"
#include "minisnn_worlds_kernel_occupancy.h"
#include "minisnn_worlds_kernel_spatial_link.h"

bool minisnn_worlds_kernel_entity_exists(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId entity_id);

size_t minisnn_worlds_kernel_entity_count(
    const MiniSNNWorldsKernel *kernel);

MiniSNNWorldsKernelError minisnn_worlds_kernel_entity_at(
    const MiniSNNWorldsKernel *kernel,
    size_t alive_index,
    MiniSNNWorldsKernelEntityId *out_entity_id);

bool minisnn_worlds_kernel_entity_is_placed(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId entity_id);

MiniSNNWorldsKernelError minisnn_worlds_kernel_entity_transform(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId entity_id,
    MiniSNNWorldsKernelTransform *out_transform);

size_t minisnn_worlds_kernel_placed_entity_count(
    const MiniSNNWorldsKernel *kernel);

MiniSNNWorldsKernelError minisnn_worlds_kernel_placed_entity_at(
    const MiniSNNWorldsKernel *kernel,
    size_t canonical_index,
    MiniSNNWorldsKernelEntityId *out_entity_id,
    MiniSNNWorldsKernelTransform *out_transform);

bool minisnn_worlds_kernel_entity_has_occupancy(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId entity_id);

MiniSNNWorldsKernelError minisnn_worlds_kernel_entity_occupancy(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId entity_id,
    MiniSNNWorldsKernelOccupancy *out_occupancy);

size_t minisnn_worlds_kernel_occupied_entity_count(
    const MiniSNNWorldsKernel *kernel);

bool minisnn_worlds_kernel_entity_has_spatial_parent(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId child);

MiniSNNWorldsKernelError minisnn_worlds_kernel_entity_spatial_parent(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId child,
    MiniSNNWorldsKernelSpatialLink *out_link);

size_t minisnn_worlds_kernel_spatial_link_count(
    const MiniSNNWorldsKernel *kernel);

MiniSNNWorldsKernelError minisnn_worlds_kernel_spatial_link_at(
    const MiniSNNWorldsKernel *kernel,
    size_t canonical_index,
    MiniSNNWorldsKernelSpatialLink *out_link);
size_t minisnn_worlds_kernel_active_occupancy_count(
    const MiniSNNWorldsKernel *kernel);

MiniSNNWorldsKernelError minisnn_worlds_kernel_occupied_entity_at(
    const MiniSNNWorldsKernel *kernel,
    size_t canonical_index,
    MiniSNNWorldsKernelEntityId *out_entity_id,
    MiniSNNWorldsKernelOccupancy *out_occupancy);

#endif
