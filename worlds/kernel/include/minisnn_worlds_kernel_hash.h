#ifndef MINISNN_WORLDS_KERNEL_HASH_H
#define MINISNN_WORLDS_KERNEL_HASH_H

#include <stdint.h>

#include "minisnn_worlds_kernel_types.h"

#define MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION UINT32_C(1)

MiniSNNWorldsKernelError minisnn_worlds_kernel_state_hash(
    const MiniSNNWorldsKernel *kernel,
    uint64_t *out_hash);

#endif
