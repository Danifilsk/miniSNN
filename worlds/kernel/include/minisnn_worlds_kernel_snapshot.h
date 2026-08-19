#ifndef MINISNN_WORLDS_KERNEL_SNAPSHOT_H
#define MINISNN_WORLDS_KERNEL_SNAPSHOT_H

#include <stddef.h>
#include <stdint.h>

#include "minisnn_worlds_kernel_types.h"

/* Snapshot V1 is a canonical little-endian in-memory representation. */
#define MINISNN_WORLDS_KERNEL_SNAPSHOT_FORMAT_VERSION_V1 UINT32_C(1)
#define MINISNN_WORLDS_KERNEL_SNAPSHOT_FORMAT_VERSION \
    MINISNN_WORLDS_KERNEL_SNAPSHOT_FORMAT_VERSION_V1

/* V1 implicitly freezes these Kernel semantics. Changing either requires V2. */
#define MINISNN_WORLDS_KERNEL_SNAPSHOT_V1_PRNG_VERSION UINT32_C(1)
#define MINISNN_WORLDS_KERNEL_SNAPSHOT_V1_SCALAR_SCALE INT64_C(1000)

typedef struct MiniSNNWorldsKernelSnapshot MiniSNNWorldsKernelSnapshot;

/* Captures a new immutable snapshot without changing kernel state. */
MiniSNNWorldsKernelError minisnn_worlds_kernel_snapshot_capture(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelSnapshot **out_snapshot);

void minisnn_worlds_kernel_snapshot_destroy(
    MiniSNNWorldsKernelSnapshot *snapshot);

uint32_t minisnn_worlds_kernel_snapshot_format_version(
    const MiniSNNWorldsKernelSnapshot *snapshot);

size_t minisnn_worlds_kernel_snapshot_size(
    const MiniSNNWorldsKernelSnapshot *snapshot);

const uint8_t *minisnn_worlds_kernel_snapshot_data(
    const MiniSNNWorldsKernelSnapshot *snapshot);

/* Copies and fully validates an immutable Snapshot Format V1 byte sequence. */
MiniSNNWorldsKernelError minisnn_worlds_kernel_snapshot_from_bytes(
    const uint8_t *data,
    size_t size,
    MiniSNNWorldsKernelSnapshot **out_snapshot);

/* Builds a new Kernel only after a complete transactional V1 restore. */
MiniSNNWorldsKernelError minisnn_worlds_kernel_create_from_snapshot(
    const MiniSNNWorldsKernelSnapshot *snapshot,
    MiniSNNWorldsKernel **out_kernel);

/* Replaces a stable Kernel instance only after a complete transactional restore.
 * On failure, the destination Kernel is left unchanged. */
MiniSNNWorldsKernelError minisnn_worlds_kernel_snapshot_restore(
    MiniSNNWorldsKernel *kernel,
    const MiniSNNWorldsKernelSnapshot *snapshot);

#ifdef MINISNN_WORLDS_KERNEL_TESTING
/* Test-only checked arithmetic hooks; no decode or restore API is exposed. */
int minisnn_worlds_kernel_snapshot_testing_size_add(
    size_t left,
    size_t right,
    size_t *out_result);

int minisnn_worlds_kernel_snapshot_testing_size_multiply(
    size_t left,
    size_t right,
    size_t *out_result);
#endif

#endif