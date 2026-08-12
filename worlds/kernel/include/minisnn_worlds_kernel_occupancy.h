#ifndef MINISNN_WORLDS_KERNEL_OCCUPANCY_H
#define MINISNN_WORLDS_KERNEL_OCCUPANCY_H

#include <stdint.h>

#include "minisnn_worlds_kernel_space.h"

/* Optional axis-aligned occupancy centered at an entity transform. */
typedef struct
{
    MiniSNNWorldsKernelScalar half_extent_x;
    MiniSNNWorldsKernelScalar half_extent_y;
    uint32_t category_bits;
    uint32_t blocking_mask;
} MiniSNNWorldsKernelOccupancy;

#endif
