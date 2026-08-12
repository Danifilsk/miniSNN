#ifndef MINISNN_WORLDS_KERNEL_SPATIAL_LINK_H
#define MINISNN_WORLDS_KERNEL_SPATIAL_LINK_H

#include "minisnn_worlds_kernel_types.h"
#include "minisnn_worlds_kernel_space.h"

typedef struct
{
    MiniSNNWorldsKernelEntityId parent;
    MiniSNNWorldsKernelEntityId child;
    MiniSNNWorldsKernelScalar offset_x;
    MiniSNNWorldsKernelScalar offset_y;
} MiniSNNWorldsKernelSpatialLink;

typedef struct
{
    MiniSNNWorldsKernelEntityId parent;
    MiniSNNWorldsKernelEntityId child;
} MiniSNNWorldsKernelSpatialLinkEndpoints;

#endif