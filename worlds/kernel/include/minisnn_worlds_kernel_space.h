#ifndef MINISNN_WORLDS_KERNEL_SPACE_H
#define MINISNN_WORLDS_KERNEL_SPACE_H

#include <stdint.h>

/* Authoritative mathematical space: +X is east and +Y is north. */
typedef int64_t MiniSNNWorldsKernelScalar;

#define MINISNN_WORLDS_KERNEL_SCALAR_SCALE INT64_C(1000)
#define MINISNN_WORLDS_KERNEL_SCALAR_ZERO INT64_C(0)

typedef uint32_t MiniSNNWorldsKernelOrientation;

#define MINISNN_WORLDS_KERNEL_ORIENTATION_FULL_TURN UINT32_C(360000)

typedef struct
{
    MiniSNNWorldsKernelScalar x;
    MiniSNNWorldsKernelScalar y;
} MiniSNNWorldsKernelPosition;

typedef struct
{
    MiniSNNWorldsKernelPosition position;
    MiniSNNWorldsKernelOrientation orientation;
} MiniSNNWorldsKernelTransform;

typedef struct
{
    MiniSNNWorldsKernelScalar min_x;
    MiniSNNWorldsKernelScalar min_y;
    MiniSNNWorldsKernelScalar max_x;
    MiniSNNWorldsKernelScalar max_y;
} MiniSNNWorldsKernelSpaceBounds;

#endif
