#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "minisnn_worlds_kernel.h"

#define REQUIRE(condition) \
    do { if (!(condition)) { \
        fprintf(stderr, "requirement failed: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
        exit(1); \
    } } while (0)

static MiniSNNWorldsKernelEntityId id(uint64_t value)
{
    MiniSNNWorldsKernelEntityId result;
    result.value = value;
    return result;
}

static MiniSNNWorldsKernelTransform transform(int64_t x)
{
    MiniSNNWorldsKernelTransform result;
    result.position.x = x;
    result.position.y = INT64_C(0);
    result.orientation = UINT32_C(0);
    return result;
}

static MiniSNNWorldsKernel *new_kernel(void)
{
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernel *kernel = minisnn_worlds_kernel_create(&config, &error);

    REQUIRE(kernel != NULL);
    REQUIRE(error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    return kernel;
}

static void step_ok(MiniSNNWorldsKernel *kernel)
{
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_testing_validate_invariants(kernel) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
}

static MiniSNNWorldsKernel *placed_entity(void)
{
    MiniSNNWorldsKernel *kernel = new_kernel();
    MiniSNNWorldsKernelCommandId command_id;

    REQUIRE(minisnn_worlds_kernel_queue_create_entity(
                kernel, UINT64_C(1), UINT32_C(0), id(UINT64_C(0)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step_ok(kernel);
    REQUIRE(minisnn_worlds_kernel_queue_place_entity(
                kernel, UINT64_C(2), UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)),
                transform(INT64_C(0)), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step_ok(kernel);
    return kernel;
}

static uint64_t state_hash(const MiniSNNWorldsKernel *kernel)
{
    uint64_t result;
    REQUIRE(minisnn_worlds_kernel_state_hash(kernel, &result) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    return result;
}

static void test_identifier_limits(void)
{
    MiniSNNWorldsKernel *kernel = new_kernel();
    MiniSNNWorldsKernelCommandId command_id;
    uint64_t before_hash;

    REQUIRE(minisnn_worlds_kernel_testing_set_next_command_id(
                kernel, (MiniSNNWorldsKernelCommandId){ UINT64_MAX }) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_entity(
                kernel, UINT64_C(1), UINT32_C(0), id(UINT64_C(0)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_IDENTIFIER_OVERFLOW);
    minisnn_worlds_kernel_destroy(kernel);

    kernel = new_kernel();
    REQUIRE(minisnn_worlds_kernel_queue_create_entity(
                kernel, UINT64_C(1), UINT32_C(0), id(UINT64_C(0)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_testing_set_next_entity_id(
                kernel, (MiniSNNWorldsKernelEntityId){ UINT64_MAX }) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    before_hash = state_hash(kernel);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_IDENTIFIER_OVERFLOW);
    REQUIRE(state_hash(kernel) == before_hash);
    minisnn_worlds_kernel_destroy(kernel);

    kernel = placed_entity();
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                id(UINT64_C(0)), id(UINT64_C(1)), INT64_C(1), INT64_C(0), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_testing_set_next_event_id(
                kernel, (MiniSNNWorldsKernelEventId){ UINT64_MAX }) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    before_hash = state_hash(kernel);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_IDENTIFIER_OVERFLOW);
    REQUIRE(state_hash(kernel) == before_hash);
    minisnn_worlds_kernel_destroy(kernel);
}

static void test_counter_promotion_is_atomic(void)
{
    MiniSNNWorldsKernel *kernel = placed_entity();
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsTick before_tick;
    uint64_t before_hash;

    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                id(UINT64_C(0)), id(UINT64_C(1)), INT64_C(1), INT64_C(0), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_testing_set_all_counters(kernel, UINT64_MAX) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    before_tick = minisnn_worlds_kernel_tick(kernel);
    before_hash = state_hash(kernel);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_INTERNAL);
    REQUIRE(minisnn_worlds_kernel_tick(kernel) == before_tick);
    REQUIRE(state_hash(kernel) == before_hash);
    minisnn_worlds_kernel_destroy(kernel);

    kernel = placed_entity();
    REQUIRE(minisnn_worlds_kernel_testing_set_all_counters(kernel, UINT64_MAX) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                id(UINT64_C(0)), id(UINT64_C(1)), INT64_C(1), INT64_C(0), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_INTERNAL);
    minisnn_worlds_kernel_destroy(kernel);
}

static void test_allocation_failure_matrix(void)
{
    size_t failure_index;
    int saw_allocation = 0;
    int saw_success = 0;

    for (failure_index = 0U; failure_index < 16U; ++failure_index)
    {
        MiniSNNWorldsKernel *kernel = placed_entity();
        MiniSNNWorldsKernelCommandId command_id;
        MiniSNNWorldsTick before_tick;
        uint64_t before_hash;
        MiniSNNWorldsKernelError error;

        REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                    kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                    id(UINT64_C(0)), id(UINT64_C(1)), INT64_C(1), INT64_C(0), &command_id) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
        REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                    kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(1),
                    id(UINT64_C(0)), id(UINT64_C(1)), INT64_C(2), INT64_C(0), &command_id) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
        before_tick = minisnn_worlds_kernel_tick(kernel);
        before_hash = state_hash(kernel);
        minisnn_worlds_kernel_testing_fail_allocation_after(failure_index);
        error = minisnn_worlds_kernel_step(kernel);
        minisnn_worlds_kernel_testing_fail_allocation_after(SIZE_MAX);
        if (error == MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION)
        {
            saw_allocation = 1;
            REQUIRE(minisnn_worlds_kernel_tick(kernel) == before_tick);
            REQUIRE(state_hash(kernel) == before_hash);
        }
        else
        {
            REQUIRE(error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
            saw_success = 1;
        }
        minisnn_worlds_kernel_destroy(kernel);
    }
    REQUIRE(saw_allocation != 0);
    REQUIRE(saw_success != 0);
}

static void test_link_and_queue_capacity(void)
{
    MiniSNNWorldsKernel *kernel = new_kernel();
    MiniSNNWorldsKernelCommandId command_id;
    size_t index;
    MiniSNNWorldsTick tick;

    for (index = 0U; index < 64U; ++index)
    {
        REQUIRE(minisnn_worlds_kernel_queue_create_entity(
                    kernel, UINT64_C(1), (uint32_t)index, id(UINT64_C(0)), &command_id) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    step_ok(kernel);
    for (index = 0U; index < 64U; ++index)
    {
        REQUIRE(minisnn_worlds_kernel_queue_place_entity(
                    kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), (uint32_t)index,
                    id(UINT64_C(0)), id((uint64_t)index + UINT64_C(1)),
                    transform((int64_t)index * INT64_C(10)), &command_id) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    step_ok(kernel);
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    for (index = 1U; index < 64U; ++index)
    {
        REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                    kernel, tick, (uint32_t)index, id(UINT64_C(0)), id((uint64_t)index),
                    id((uint64_t)index + UINT64_C(1)), &command_id) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    step_ok(kernel);
    REQUIRE(minisnn_worlds_kernel_spatial_link_count(kernel) == 63U);
    minisnn_worlds_kernel_destroy(kernel);
}

int main(void)
{
    test_identifier_limits();
    test_counter_promotion_is_atomic();
    test_allocation_failure_matrix();
    test_link_and_queue_capacity();
    puts("K1-C3 limits, identifiers, counters and allocation atomicity OK");
    return 0;
}