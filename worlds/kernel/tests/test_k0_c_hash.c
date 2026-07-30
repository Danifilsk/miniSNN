#include "minisnn_worlds_kernel.h"

#include <stdint.h>
#include <stdio.h>

#define CHECK(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            fprintf(stderr, "K0-C hash test failed: %s at line %d\n", #condition, __LINE__); \
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
    const MiniSNNWorldsKernelRandomStreamKey key_a = { UINT64_C(1), UINT64_C(1) };
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernelEntityId external = { UINT64_C(0) };
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernel *default_kernel;
    MiniSNNWorldsKernel *zero_kernel;
    MiniSNNWorldsKernel *seed_kernel;
    MiniSNNWorldsKernel *first;
    MiniSNNWorldsKernel *second;
    MiniSNNWorldsKernel *allocation_probe;
    MiniSNNWorldsKernelEntityId created_entity = { UINT64_C(1) };
    uint64_t default_hash;
    uint64_t zero_hash;
    uint64_t seed_hash;
    uint64_t empty_tick_hash;
    uint64_t pending_hash;
    uint64_t created_hash;
    uint64_t draw_hash;
    uint64_t destroyed_hash;
    uint64_t equivalent_first_hash;
    uint64_t equivalent_second_hash;
    uint64_t preserved_hash = UINT64_C(0x123456789abcdef0);
    uint32_t preserved_value = UINT32_C(0xa5a5a5a5);
    uint32_t value;

    default_kernel = minisnn_worlds_kernel_create(NULL, &error);
    zero_kernel = create_with_seed(UINT64_C(0));
    seed_kernel = create_with_seed(UINT64_C(12345));
    first = create_with_seed(UINT64_C(88));
    second = create_with_seed(UINT64_C(88));
    CHECK(default_kernel != NULL && zero_kernel != NULL && seed_kernel != NULL &&
          first != NULL && second != NULL);
    CHECK(minisnn_worlds_kernel_state_hash(default_kernel, &default_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_state_hash(zero_kernel, &zero_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_state_hash(seed_kernel, &seed_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(default_hash == UINT64_C(6726247500959070114));
    CHECK(zero_hash == UINT64_C(11553281521567982095));
    CHECK(seed_hash == UINT64_C(14445501914872736870));
    CHECK(default_hash != zero_hash && zero_hash != seed_hash && default_hash != seed_hash);
    CHECK(minisnn_worlds_kernel_state_hash(NULL, &preserved_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT);
    CHECK(preserved_hash == UINT64_C(0x123456789abcdef0));
    CHECK(minisnn_worlds_kernel_state_hash(seed_kernel, NULL) ==
          MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT);
    CHECK(minisnn_worlds_kernel_state_hash(seed_kernel, &preserved_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(preserved_hash == seed_hash);

    CHECK(minisnn_worlds_kernel_step(seed_kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_state_hash(seed_kernel, &empty_tick_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(empty_tick_hash == UINT64_C(11662403468076183495));
    CHECK(empty_tick_hash != seed_hash);
    CHECK(minisnn_worlds_kernel_queue_create_entity(seed_kernel, 2U, 0U, external,
                                                     &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_state_hash(seed_kernel, &pending_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(pending_hash == UINT64_C(18266128185322232342));
    CHECK(pending_hash != empty_tick_hash);
    CHECK(minisnn_worlds_kernel_step(seed_kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_state_hash(seed_kernel, &created_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(created_hash == UINT64_C(11972632868762724051));
    CHECK(created_hash != pending_hash);
    CHECK(minisnn_worlds_kernel_random_u32(seed_kernel, key_a, &value) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_state_hash(seed_kernel, &draw_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(draw_hash == UINT64_C(14081253814165122120));
    CHECK(draw_hash != created_hash);
    CHECK(minisnn_worlds_kernel_random_bounded_u32(seed_kernel,
                                                    (MiniSNNWorldsKernelRandomStreamKey){ 0U, 0U },
                                                    1U, &preserved_value) ==
          MINISNN_WORLDS_KERNEL_ERROR_INVALID_RANDOM_STREAM_KEY);
    CHECK(minisnn_worlds_kernel_last_error(seed_kernel) ==
          MINISNN_WORLDS_KERNEL_ERROR_INVALID_RANDOM_STREAM_KEY);
    CHECK(preserved_value == UINT32_C(0xa5a5a5a5));
    CHECK(minisnn_worlds_kernel_state_hash(seed_kernel, &preserved_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(preserved_hash == draw_hash);

    CHECK(minisnn_worlds_kernel_queue_destroy_entity(seed_kernel, 3U, 0U, external,
                                                      created_entity, &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(seed_kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_state_hash(seed_kernel, &destroyed_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(destroyed_hash != draw_hash);

    allocation_probe = create_with_seed(UINT64_C(5));
    CHECK(allocation_probe != NULL);
    minisnn_worlds_kernel_testing_fail_next_allocation();
    CHECK(minisnn_worlds_kernel_state_hash(allocation_probe, &preserved_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_random_u32(allocation_probe, key_a, &preserved_value) ==
          MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
    CHECK(preserved_value == UINT32_C(0xa5a5a5a5));

    CHECK(minisnn_worlds_kernel_random_u32(first, key_a, &value) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_random_u32(first,
                                            (MiniSNNWorldsKernelRandomStreamKey){ 2U, 1U }, &value) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_random_u32(second,
                                            (MiniSNNWorldsKernelRandomStreamKey){ 2U, 1U }, &value) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_random_u32(second, key_a, &value) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_state_hash(first, &equivalent_first_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_state_hash(second, &equivalent_second_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(equivalent_first_hash == equivalent_second_hash);

    minisnn_worlds_kernel_destroy(default_kernel);
    minisnn_worlds_kernel_destroy(zero_kernel);
    minisnn_worlds_kernel_destroy(seed_kernel);
    minisnn_worlds_kernel_destroy(first);
    minisnn_worlds_kernel_destroy(second);
    minisnn_worlds_kernel_destroy(allocation_probe);
    printf("K0-C state hash validation OK\n");
    return 0;
}
