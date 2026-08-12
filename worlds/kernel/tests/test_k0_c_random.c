#include "minisnn_worlds_kernel.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define CHECK(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            fprintf(stderr, "K0-C random test failed: %s at line %d\n", #condition, __LINE__); \
            return 1; \
        } \
    } while (0)

typedef struct
{
    uint32_t struct_size;
    uint32_t format_version;
} LegacyConfig;

typedef struct
{
    MiniSNNWorldsKernelConfig config;
    uint64_t unknown_tail;
} LargerConfig;

static MiniSNNWorldsKernel *create_with_seed(uint64_t seed)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;

    config.master_seed = seed;
    return minisnn_worlds_kernel_create(&config, &error);
}

static int draw_three(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelRandomStreamKey key,
    uint32_t values[3])
{
    size_t index;

    for (index = 0U; index < 3U; ++index)
    {
        if (minisnn_worlds_kernel_random_u32(kernel, key, &values[index]) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return 0;
        }
    }
    return 1;
}

static int stream_info_matches(
    const MiniSNNWorldsKernelRandomStreamInfo *left,
    const MiniSNNWorldsKernelRandomStreamInfo *right)
{
    return left->key.namespace_id == right->key.namespace_id &&
           left->key.stream_id == right->key.stream_id &&
           left->state == right->state &&
           left->sequence == right->sequence &&
           left->generated_u32_count == right->generated_u32_count;
}

int main(void)
{
    const MiniSNNWorldsKernelRandomStreamKey key_a = { UINT64_C(1), UINT64_C(1) };
    const MiniSNNWorldsKernelRandomStreamKey key_b = { UINT64_C(1), UINT64_C(2) };
    const MiniSNNWorldsKernelRandomStreamKey key_c = { UINT64_C(9), UINT64_C(9) };
    const MiniSNNWorldsKernelRandomStreamKey invalid_key = { UINT64_C(0), UINT64_C(0) };
    MiniSNNWorldsKernelRandomStreamInfo stream_info;
    MiniSNNWorldsKernelRandomStreamInfo preserved_info;
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernelDiagnostics diagnostics;
    MiniSNNWorldsKernelConfig default_config;
    LegacyConfig legacy_config;
    LargerConfig larger_config;
    MiniSNNWorldsKernel *default_kernel;
    MiniSNNWorldsKernel *legacy_kernel;
    MiniSNNWorldsKernel *larger_kernel;
    MiniSNNWorldsKernel *zero_kernel;
    MiniSNNWorldsKernel *vector_zero;
    MiniSNNWorldsKernel *vector_one;
    MiniSNNWorldsKernel *vector_two;
    MiniSNNWorldsKernel *vector_max;
    MiniSNNWorldsKernel *first;
    MiniSNNWorldsKernel *second;
    MiniSNNWorldsKernel *allocation_kernel;
    MiniSNNWorldsKernel *overflow_kernel;
    uint32_t values_zero[3];
    uint32_t values_one[3];
    uint32_t values_two[3];
    uint32_t values_max[3];
    uint32_t first_a[3];
    uint32_t second_a[3];
    uint32_t value = UINT32_C(0);
    uint32_t preserved_value = UINT32_C(0xA5A5A5A5);
    uint64_t u64_value = UINT64_C(0);
    uint64_t before_hash;
    uint64_t after_hash;
    size_t index;

    default_config = minisnn_worlds_kernel_config_default();
    CHECK(default_config.master_seed == MINISNN_WORLDS_KERNEL_DEFAULT_MASTER_SEED);
    default_kernel = minisnn_worlds_kernel_create(NULL, &error);
    CHECK(default_kernel != NULL);
    CHECK(error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_master_seed(default_kernel) ==
          MINISNN_WORLDS_KERNEL_DEFAULT_MASTER_SEED);
    CHECK(minisnn_worlds_kernel_master_seed(NULL) ==
          MINISNN_WORLDS_KERNEL_DEFAULT_MASTER_SEED);

    legacy_config.struct_size = (uint32_t)sizeof(legacy_config);
    legacy_config.format_version = MINISNN_WORLDS_KERNEL_CONFIG_VERSION;
    legacy_kernel = minisnn_worlds_kernel_create(
        (const MiniSNNWorldsKernelConfig *)&legacy_config, &error);
    CHECK(legacy_kernel != NULL);
    CHECK(minisnn_worlds_kernel_master_seed(legacy_kernel) ==
          MINISNN_WORLDS_KERNEL_DEFAULT_MASTER_SEED);

    larger_config.config = default_config;
    larger_config.config.struct_size = (uint32_t)sizeof(larger_config);
    larger_config.config.master_seed = UINT64_C(314159);
    larger_config.unknown_tail = UINT64_MAX;
    larger_kernel = minisnn_worlds_kernel_create(&larger_config.config, &error);
    CHECK(larger_kernel != NULL && error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_master_seed(larger_kernel) == UINT64_C(314159));

    zero_kernel = create_with_seed(UINT64_C(0));
    CHECK(zero_kernel != NULL);
    CHECK(minisnn_worlds_kernel_master_seed(zero_kernel) == UINT64_C(0));

    vector_zero = create_with_seed(UINT64_C(0));
    vector_one = create_with_seed(UINT64_C(12345));
    vector_two = create_with_seed(UINT64_C(12345));
    vector_max = create_with_seed(UINT64_MAX);
    CHECK(vector_zero != NULL && vector_one != NULL && vector_two != NULL &&
          vector_max != NULL);
    CHECK(draw_three(vector_zero, key_a, values_zero));
    CHECK(draw_three(vector_one, key_a, values_one));
    CHECK(draw_three(vector_two, key_b, values_two));
    CHECK(draw_three(vector_max,
                     (MiniSNNWorldsKernelRandomStreamKey){ UINT64_MAX, UINT64_MAX },
                     values_max));

    CHECK(values_zero[0] == UINT32_C(855824397));
    CHECK(values_zero[1] == UINT32_C(4035378324));
    CHECK(values_zero[2] == UINT32_C(782970622));
    CHECK(values_one[0] == UINT32_C(1577453522));
    CHECK(values_one[1] == UINT32_C(3774853690));
    CHECK(values_one[2] == UINT32_C(244842771));
    CHECK(values_two[0] == UINT32_C(1681730853));
    CHECK(values_two[1] == UINT32_C(799097023));
    CHECK(values_two[2] == UINT32_C(4096513249));
    CHECK(values_max[0] == UINT32_C(4134754306));
    CHECK(values_max[1] == UINT32_C(31774962));
    CHECK(values_max[2] == UINT32_C(2162688377));
    CHECK(minisnn_worlds_kernel_random_u64(vector_one, key_c, &u64_value) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(u64_value != UINT64_C(0));
    CHECK(minisnn_worlds_kernel_random_stream_at(vector_one, 1U, &stream_info) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(stream_info.key.namespace_id == key_c.namespace_id &&
          stream_info.key.stream_id == key_c.stream_id &&
          stream_info.generated_u32_count == UINT64_C(2));

    CHECK(minisnn_worlds_kernel_random_u32(default_kernel, invalid_key, &preserved_value) ==
          MINISNN_WORLDS_KERNEL_ERROR_INVALID_RANDOM_STREAM_KEY);
    CHECK(preserved_value == UINT32_C(0xA5A5A5A5));
    CHECK(minisnn_worlds_kernel_random_bounded_u32(default_kernel, key_a, 0U,
                                                    &preserved_value) ==
          MINISNN_WORLDS_KERNEL_ERROR_INVALID_BOUND);
    CHECK(preserved_value == UINT32_C(0xA5A5A5A5));
    CHECK(minisnn_worlds_kernel_random_bounded_u32(default_kernel, key_a, 1U,
                                                    &value) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(value == 0U);
    CHECK(minisnn_worlds_kernel_random_bounded_u32(default_kernel, key_a,
                                                    UINT32_MAX, &value) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(value < UINT32_MAX);
    CHECK(minisnn_worlds_kernel_tick(default_kernel) == MINISNN_WORLDS_TICK_INITIAL);
    stream_info.key = invalid_key;
    stream_info.state = UINT64_C(9);
    preserved_info = stream_info;
    CHECK(minisnn_worlds_kernel_random_stream_at(default_kernel, 99U, &stream_info) ==
          MINISNN_WORLDS_KERNEL_ERROR_INDEX_OUT_OF_RANGE);
    CHECK(stream_info_matches(&stream_info, &preserved_info));

    first = create_with_seed(UINT64_C(12345));
    second = create_with_seed(UINT64_C(12345));
    CHECK(first != NULL && second != NULL);
    CHECK(draw_three(first, key_a, first_a));
    CHECK(minisnn_worlds_kernel_random_u32(second, key_a, &second_a[0]) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_random_u32(second, key_b, &value) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_random_u32(second, key_a, &second_a[1]) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_random_u32(second, key_a, &second_a[2]) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(first_a[0] == second_a[0] && first_a[1] == second_a[1] &&
          first_a[2] == second_a[2]);
    CHECK(minisnn_worlds_kernel_random_u32(first, key_b, &value) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_random_stream_count(first) == 2U);
    CHECK(minisnn_worlds_kernel_random_stream_at(first, 0U, &stream_info) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(stream_info.key.namespace_id == UINT64_C(1) &&
          stream_info.key.stream_id == UINT64_C(1));
    CHECK(minisnn_worlds_kernel_random_stream_at(first, 1U, &stream_info) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(stream_info.key.namespace_id == UINT64_C(1) &&
          stream_info.key.stream_id == UINT64_C(2));

    allocation_kernel = create_with_seed(UINT64_C(7));
    CHECK(allocation_kernel != NULL);
    CHECK(minisnn_worlds_kernel_state_hash(allocation_kernel, &before_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    minisnn_worlds_kernel_testing_fail_next_allocation();
    CHECK(minisnn_worlds_kernel_random_u32(allocation_kernel, key_a, &preserved_value) ==
          MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
    CHECK(preserved_value == UINT32_C(0xA5A5A5A5));
    CHECK(minisnn_worlds_kernel_random_stream_count(allocation_kernel) == 0U);
    CHECK(minisnn_worlds_kernel_state_hash(allocation_kernel, &after_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(before_hash == after_hash);
    CHECK(minisnn_worlds_kernel_random_u32(allocation_kernel, key_a, &value) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    for (index = 0U; index < 3U; ++index)
    {
        CHECK(minisnn_worlds_kernel_random_u32(
                  allocation_kernel,
                  (MiniSNNWorldsKernelRandomStreamKey){ UINT64_C(2), (uint64_t)index + 1U },
                  &value) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    CHECK(minisnn_worlds_kernel_state_hash(allocation_kernel, &before_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    minisnn_worlds_kernel_testing_fail_next_allocation();
    CHECK(minisnn_worlds_kernel_random_u32(
              allocation_kernel, (MiniSNNWorldsKernelRandomStreamKey){ UINT64_C(3), UINT64_C(1) },
              &preserved_value) == MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
    CHECK(minisnn_worlds_kernel_random_stream_count(allocation_kernel) == 4U);
    CHECK(minisnn_worlds_kernel_state_hash(allocation_kernel, &after_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(before_hash == after_hash);

    overflow_kernel = create_with_seed(UINT64_C(8));
    CHECK(overflow_kernel != NULL);
    CHECK(minisnn_worlds_kernel_random_u32(overflow_kernel, key_a, &value) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_testing_set_random_counts(
              overflow_kernel, key_a, UINT64_MAX, UINT64_MAX) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_state_hash(overflow_kernel, &before_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_random_u32(overflow_kernel, key_a, &preserved_value) ==
          MINISNN_WORLDS_KERNEL_ERROR_IDENTIFIER_OVERFLOW);
    CHECK(preserved_value == UINT32_C(0xA5A5A5A5));
    CHECK(minisnn_worlds_kernel_state_hash(overflow_kernel, &after_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(before_hash == after_hash);
    CHECK(minisnn_worlds_kernel_get_diagnostics(overflow_kernel, &diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(diagnostics.total_random_u32_generated == UINT64_MAX);

    minisnn_worlds_kernel_destroy(default_kernel);
    minisnn_worlds_kernel_destroy(legacy_kernel);
    minisnn_worlds_kernel_destroy(larger_kernel);
    minisnn_worlds_kernel_destroy(zero_kernel);
    minisnn_worlds_kernel_destroy(vector_zero);
    minisnn_worlds_kernel_destroy(vector_one);
    minisnn_worlds_kernel_destroy(vector_two);
    minisnn_worlds_kernel_destroy(vector_max);
    minisnn_worlds_kernel_destroy(first);
    minisnn_worlds_kernel_destroy(second);
    minisnn_worlds_kernel_destroy(allocation_kernel);
    minisnn_worlds_kernel_destroy(overflow_kernel);
    printf("K0-C random validation OK\n");
    return 0;
}
