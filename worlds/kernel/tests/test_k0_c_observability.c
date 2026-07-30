#include "minisnn_worlds_kernel.h"

#include <stdint.h>
#include <stdio.h>

#define CHECK(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            fprintf(stderr, "K0-C observability test failed: %s at line %d\n", #condition, __LINE__); \
            return 1; \
        } \
    } while (0)

static MiniSNNWorldsKernel *create_with_seed(uint64_t seed)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;

    config.master_seed = seed;
    return minisnn_worlds_kernel_create(&config, &error);
}

int main(void)
{
    MiniSNNWorldsKernelRandomStreamKey high = { UINT64_C(7), UINT64_C(3) };
    MiniSNNWorldsKernelRandomStreamKey low = { UINT64_C(1), UINT64_C(9) };
    MiniSNNWorldsKernelRandomStreamInfo info;
    MiniSNNWorldsKernelRandomStreamInfo preserved_info;
    MiniSNNWorldsKernelDiagnostics diagnostics;
    MiniSNNWorldsKernelDiagnostics preserved_diagnostics;
    MiniSNNWorldsKernelTracePoint trace;
    MiniSNNWorldsKernelTracePoint preserved_trace;
    MiniSNNWorldsKernel *first = create_with_seed(UINT64_C(42));
    MiniSNNWorldsKernel *second = create_with_seed(UINT64_C(42));
    uint32_t value;
    uint64_t state_hash;

    CHECK(first != NULL && second != NULL);
    diagnostics.master_seed = UINT64_C(7);
    preserved_diagnostics = diagnostics;
    CHECK(minisnn_worlds_kernel_get_diagnostics(NULL, &diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT);
    CHECK(diagnostics.master_seed == preserved_diagnostics.master_seed);
    trace.tick = UINT64_C(99);
    preserved_trace = trace;
    CHECK(minisnn_worlds_kernel_capture_trace_point(NULL, &trace) ==
          MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT);
    CHECK(trace.tick == preserved_trace.tick);
    CHECK(minisnn_worlds_kernel_random_u32(first, high, &value) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_random_u32(first, low, &value) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_get_diagnostics(first, &diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(diagnostics.master_seed == UINT64_C(42));
    CHECK(diagnostics.random_streams == UINT64_C(2));
    CHECK(diagnostics.total_random_u32_generated == UINT64_C(2));
    CHECK(diagnostics.state_hash_version == MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION);
    CHECK(diagnostics.prng_version == MINISNN_WORLDS_KERNEL_PRNG_VERSION);
    CHECK(minisnn_worlds_kernel_state_hash(first, &state_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(diagnostics.current_state_hash == state_hash);
    CHECK(minisnn_worlds_kernel_capture_trace_point(first, &trace) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(trace.tick == 0U && trace.state_hash == state_hash &&
          trace.random_streams == UINT64_C(2) &&
          trace.total_random_u32_generated == UINT64_C(2));
    CHECK(minisnn_worlds_kernel_random_stream_at(first, 0U, &info) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(info.key.namespace_id == low.namespace_id && info.key.stream_id == low.stream_id);
    CHECK(minisnn_worlds_kernel_random_stream_at(first, 1U, &info) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(info.key.namespace_id == high.namespace_id && info.key.stream_id == high.stream_id);
    info.key.namespace_id = UINT64_C(99);
    preserved_info = info;
    CHECK(minisnn_worlds_kernel_random_stream_at(first, 2U, &info) ==
          MINISNN_WORLDS_KERNEL_ERROR_INDEX_OUT_OF_RANGE);
    CHECK(info.key.namespace_id == preserved_info.key.namespace_id);
    CHECK(minisnn_worlds_kernel_random_u32(second, low, &value) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_random_u32(second, high, &value) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_state_hash(second, &state_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(state_hash == diagnostics.current_state_hash);
    minisnn_worlds_kernel_destroy(first);
    minisnn_worlds_kernel_destroy(second);
    printf("K0-C observability validation OK\n");
    return 0;
}
