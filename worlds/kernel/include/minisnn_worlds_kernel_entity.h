#ifndef MINISNN_WORLDS_KERNEL_ENTITY_H
#define MINISNN_WORLDS_KERNEL_ENTITY_H

#include <stdbool.h>
#include <stddef.h>

#include "minisnn_worlds_kernel_types.h"

bool minisnn_worlds_kernel_entity_exists(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId entity_id);

size_t minisnn_worlds_kernel_entity_count(
    const MiniSNNWorldsKernel *kernel);

MiniSNNWorldsKernelError minisnn_worlds_kernel_entity_at(
    const MiniSNNWorldsKernel *kernel,
    size_t alive_index,
    MiniSNNWorldsKernelEntityId *out_entity_id);

#endif
