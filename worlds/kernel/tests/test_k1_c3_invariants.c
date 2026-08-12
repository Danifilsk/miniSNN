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

static MiniSNNWorldsKernelTransform transform(int64_t x, int64_t y)
{
    MiniSNNWorldsKernelTransform result;
    result.position.x = x;
    result.position.y = y;
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

static void create_tree(MiniSNNWorldsKernel *kernel)
{
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsTick tick;
    size_t index;

    for (index = 0U; index < 4U; ++index)
    {
        REQUIRE(minisnn_worlds_kernel_queue_create_entity(
                    kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1),
                    (uint32_t)index, id(UINT64_C(0)), &command_id) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    step_ok(kernel);
    for (index = 0U; index < 4U; ++index)
    {
        REQUIRE(minisnn_worlds_kernel_queue_place_entity(
                    kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                    id(UINT64_C(0)), id((uint64_t)index + UINT64_C(1)),
                    transform((int64_t)index * INT64_C(10), INT64_C(0)), &command_id) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
        step_ok(kernel);
    }
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(2)), id(UINT64_C(3)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(2), id(UINT64_C(0)), id(UINT64_C(3)), id(UINT64_C(4)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step_ok(kernel);
}

static uint64_t state_hash(const MiniSNNWorldsKernel *kernel)
{
    uint64_t result;
    REQUIRE(minisnn_worlds_kernel_state_hash(kernel, &result) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    return result;
}

static void test_validator_detects_each_corruption(void)
{
    const MiniSNNWorldsKernelTestingCorruption corruptions[] =
    {
        MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_SPATIAL_LINK_CYCLE,
        MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_CHILD_HAS_TWO_PARENTS,
        MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_DUPLICATE_SPATIAL_LINK,
        MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_UNSORTED_SPATIAL_LINKS,
        MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_DEAD_LINK_ENDPOINT,
        MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_UNPLACED_LINK_ENDPOINT,
        MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_LINK_OFFSET,
        MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_UNKNOWN_LINK_ENDPOINT,
        MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_COUNTERS
    };
    size_t index;

    for (index = 0U; index < sizeof(corruptions) / sizeof(corruptions[0U]); ++index)
    {
        MiniSNNWorldsKernel *kernel = new_kernel();
        MiniSNNWorldsTick before_tick;
        uint64_t before_hash;

        create_tree(kernel);
        REQUIRE(minisnn_worlds_kernel_testing_inject_corruption(kernel, corruptions[index]) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
        REQUIRE(minisnn_worlds_kernel_testing_validate_invariants(kernel) ==
                MINISNN_WORLDS_KERNEL_ERROR_INTERNAL);
        before_tick = minisnn_worlds_kernel_tick(kernel);
        before_hash = state_hash(kernel);
        REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_INTERNAL);
        REQUIRE(minisnn_worlds_kernel_tick(kernel) == before_tick);
        REQUIRE(state_hash(kernel) == before_hash);
        minisnn_worlds_kernel_destroy(kernel);
    }
}

static void test_causal_events_are_grouped_and_queries_do_not_mutate(void)
{
    MiniSNNWorldsKernel *kernel = new_kernel();
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelEvent root_event;
    MiniSNNWorldsKernelEvent child_event;
    MiniSNNWorldsKernelSpatialLink link_value;
    uint64_t before_hash;
    uint64_t after_hash;

    create_tree(kernel);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                id(UINT64_C(0)), id(UINT64_C(1)), INT64_C(1), INT64_C(0), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step_ok(kernel);
    REQUIRE(minisnn_worlds_kernel_last_tick_event_count(kernel) == 4U);
    REQUIRE(minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &root_event) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_last_tick_event_at(kernel, 1U, &child_event) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(root_event.subject.value == UINT64_C(1));
    REQUIRE(child_event.related_entity.value == UINT64_C(1));
    REQUIRE(root_event.command_id.value == child_event.command_id.value);
    before_hash = state_hash(kernel);
    REQUIRE(minisnn_worlds_kernel_spatial_link_at(kernel, 0U, &link_value) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_last_tick_event_at(kernel, 3U, &child_event) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    after_hash = state_hash(kernel);
    REQUIRE(before_hash == after_hash);
    minisnn_worlds_kernel_destroy(kernel);
}

int main(void)
{
    test_validator_detects_each_corruption();
    test_causal_events_are_grouped_and_queries_do_not_mutate();
    puts("K1-C3 planned and official invariant validation OK");
    return 0;
}