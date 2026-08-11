#ifndef MINISNN_WORLDS_KERNEL_SNAPSHOT_FILE_H
#define MINISNN_WORLDS_KERNEL_SNAPSHOT_FILE_H

#include "minisnn_worlds_kernel.h"

/* App-layer binary persistence. The Kernel library remains memory-only. */
MiniSNNWorldsKernelError minisnn_worlds_kernel_snapshot_save_file(
    const char *path,
    const MiniSNNWorldsKernelSnapshot *snapshot);

MiniSNNWorldsKernelError minisnn_worlds_kernel_snapshot_load_file(
    const char *path,
    MiniSNNWorldsKernelSnapshot **out_snapshot);

#endif