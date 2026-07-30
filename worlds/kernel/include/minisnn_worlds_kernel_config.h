#ifndef MINISNN_WORLDS_KERNEL_CONFIG_H
#define MINISNN_WORLDS_KERNEL_CONFIG_H

#include <stdint.h>

#include "minisnn_worlds_kernel_space.h"

/* Spatial bounds are an optional struct_size tail of format v1. */
#define MINISNN_WORLDS_KERNEL_CONFIG_VERSION UINT32_C(1)
#define MINISNN_WORLDS_KERNEL_DEFAULT_MASTER_SEED UINT64_C(0x4D696E69534E4E31)

typedef struct
{
    uint32_t struct_size;
    uint32_t format_version;
    uint64_t master_seed;
    MiniSNNWorldsKernelSpaceBounds space_bounds;
} MiniSNNWorldsKernelConfig;

MiniSNNWorldsKernelConfig minisnn_worlds_kernel_config_default(void);

#endif
