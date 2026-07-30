#ifndef MINISNN_WORLDS_KERNEL_RANDOM_H
#define MINISNN_WORLDS_KERNEL_RANDOM_H

#include <stddef.h>
#include <stdint.h>

#include "minisnn_worlds_kernel_types.h"

#define MINISNN_WORLDS_KERNEL_PRNG_VERSION UINT32_C(1)

typedef struct
{
    uint64_t namespace_id;
    uint64_t stream_id;
} MiniSNNWorldsKernelRandomStreamKey;

typedef struct
{
    MiniSNNWorldsKernelRandomStreamKey key;
    uint64_t state;
    uint64_t sequence;
    uint64_t generated_u32_count;
} MiniSNNWorldsKernelRandomStreamInfo;

MiniSNNWorldsKernelError minisnn_worlds_kernel_random_u32(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelRandomStreamKey key,
    uint32_t *out_value);

MiniSNNWorldsKernelError minisnn_worlds_kernel_random_u64(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelRandomStreamKey key,
    uint64_t *out_value);

MiniSNNWorldsKernelError minisnn_worlds_kernel_random_bounded_u32(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelRandomStreamKey key,
    uint32_t exclusive_upper_bound,
    uint32_t *out_value);

size_t minisnn_worlds_kernel_random_stream_count(
    const MiniSNNWorldsKernel *kernel);

MiniSNNWorldsKernelError minisnn_worlds_kernel_random_stream_at(
    const MiniSNNWorldsKernel *kernel,
    size_t canonical_index,
    MiniSNNWorldsKernelRandomStreamInfo *out_stream);

#endif
