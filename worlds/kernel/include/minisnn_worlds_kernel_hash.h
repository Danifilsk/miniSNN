#ifndef MINISNN_WORLDS_KERNEL_HASH_H
#define MINISNN_WORLDS_KERNEL_HASH_H

#include <stdint.h>

#include "minisnn_worlds_kernel_types.h"

#define MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V1 UINT32_C(1)
#define MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V2 UINT32_C(2)
#define MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V3 UINT32_C(3)
#define MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V4 UINT32_C(4)
#define MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V5 UINT32_C(5)
/* Current canonical state signature. Historical versions remain requestable. */
#define MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION \
    MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V5

MiniSNNWorldsKernelError minisnn_worlds_kernel_state_hash(
    const MiniSNNWorldsKernel *kernel,
    uint64_t *out_hash);

MiniSNNWorldsKernelError minisnn_worlds_kernel_state_hash_versioned(
    const MiniSNNWorldsKernel *kernel,
    uint32_t version,
    uint64_t *out_hash);

#endif
