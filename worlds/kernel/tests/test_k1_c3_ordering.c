#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minisnn_worlds_kernel.h"

#define REQUIRE(condition) \
    do { if (!(condition)) { \
        fprintf(stderr, "requirement failed: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
        exit(1); \
    } } while (0)

typedef struct
{
    uint64_t state_hash;
    MiniSNNWorldsKernelDiagnostics diagnostics;
    size_t event_count;
    size_t link_count;
} Snapshot;

static MiniSNNWorldsKernelEntityId id(uint64_t value)
{
    MiniSNNWorldsKernelEntityId result;
    result.value = value;
    return result;
}

static MiniSNNWorldsKernel *new_kernel(void)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel = minisnn_worlds_kernel_create(&config, &error);

    REQUIRE(kernel != NULL);
    REQUIRE(error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    return kernel;
}

static MiniSNNWorldsKernelTransform transform(int64_t x, int64_t y)
{
    MiniSNNWorldsKernelTransform result;

    result.position.x = x;
    result.position.y = y;
    result.orientation = UINT32_C(0);
    return result;
}

static MiniSNNWorldsKernelOccupancy blocker(void)
{
    MiniSNNWorldsKernelOccupancy result;

    memset(&result, 0, sizeof(result));
    result.half_extent_x = INT64_C(1);
    result.half_extent_y = INT64_C(1);
    result.category_bits = UINT32_C(1);
    result.blocking_mask = UINT32_C(1);
    return result;
}

static void step(MiniSNNWorldsKernel *kernel)
{
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_testing_validate_invariants(kernel) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
}

static void create_entities(MiniSNNWorldsKernel *kernel, size_t count)
{
    MiniSNNWorldsKernelCommandId command_id;
    size_t index;

    for (index = 0U; index < count; ++index)
    {
        REQUIRE(minisnn_worlds_kernel_queue_create_entity(
                    kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1),
                    (uint32_t)index, id(UINT64_C(0)), &command_id) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    step(kernel);
}

static void place(MiniSNNWorldsKernel *kernel, uint64_t entity, int64_t x, int64_t y)
{
    MiniSNNWorldsKernelCommandId command_id;

    REQUIRE(minisnn_worlds_kernel_queue_place_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                id(UINT64_C(0)), id(entity), transform(x, y), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
}

static void link(MiniSNNWorldsKernel *kernel, uint64_t parent, uint64_t child)
{
    MiniSNNWorldsKernelCommandId command_id;

    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                id(UINT64_C(0)), id(parent), id(child), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
}

static int64_t x_of(const MiniSNNWorldsKernel *kernel, uint64_t entity)
{
    MiniSNNWorldsKernelTransform value;

    REQUIRE(minisnn_worlds_kernel_entity_transform(kernel, id(entity), &value) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    return value.position.x;
}

static MiniSNNWorldsKernelEvent event_at(const MiniSNNWorldsKernel *kernel, size_t index)
{
    MiniSNNWorldsKernelEvent event;

    REQUIRE(minisnn_worlds_kernel_last_tick_event_at(kernel, index, &event) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    return event;
}

static Snapshot snapshot(const MiniSNNWorldsKernel *kernel)
{
    Snapshot result;

    REQUIRE(minisnn_worlds_kernel_state_hash(kernel, &result.state_hash) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_get_diagnostics(kernel, &result.diagnostics) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    result.event_count = minisnn_worlds_kernel_last_tick_event_count(kernel);
    result.link_count = minisnn_worlds_kernel_spatial_link_count(kernel);
    return result;
}

static void require_rejection(const MiniSNNWorldsKernel *kernel,
                              MiniSNNWorldsKernelCommandRejection rejection)
{
    size_t index;
    size_t count = minisnn_worlds_kernel_last_tick_event_count(kernel);

    for (index = 0U; index < count; ++index)
    {
        MiniSNNWorldsKernelEvent event = event_at(kernel, index);

        if (event.type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED &&
            event.rejection == rejection)
        {
            return;
        }
    }
    REQUIRE(0);
}
static MiniSNNWorldsKernel *two_entities(void)
{
    MiniSNNWorldsKernel *kernel = new_kernel();

    create_entities(kernel, 2U);
    place(kernel, UINT64_C(1), INT64_C(0), INT64_C(0));
    place(kernel, UINT64_C(2), INT64_C(10), INT64_C(0));
    return kernel;
}

static MiniSNNWorldsKernel *tree_and_blocker(void)
{
    MiniSNNWorldsKernel *kernel = new_kernel();

    create_entities(kernel, 4U);
    place(kernel, UINT64_C(1), INT64_C(0), INT64_C(0));
    place(kernel, UINT64_C(2), INT64_C(10), INT64_C(0));
    place(kernel, UINT64_C(3), INT64_C(20), INT64_C(0));
    place(kernel, UINT64_C(4), INT64_C(30), INT64_C(0));
    link(kernel, UINT64_C(1), UINT64_C(2));
    return kernel;
}

static void test_create_move_ordering(void)
{
    MiniSNNWorldsKernel *forward = two_entities();
    MiniSNNWorldsKernel *reverse = two_entities();
    MiniSNNWorldsKernel *issuer_order = two_entities();
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsTick tick;

    tick = minisnn_worlds_kernel_tick(forward) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                forward, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                forward, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(1)),
                INT64_C(5), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(forward);
    REQUIRE(x_of(forward, UINT64_C(1)) == INT64_C(5));
    REQUIRE(x_of(forward, UINT64_C(2)) == INT64_C(15));
    REQUIRE(minisnn_worlds_kernel_last_tick_event_count(forward) == 3U);

    tick = minisnn_worlds_kernel_tick(reverse) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                reverse, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)),
                INT64_C(5), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                reverse, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(reverse);
    REQUIRE(x_of(reverse, UINT64_C(1)) == INT64_C(5));
    REQUIRE(x_of(reverse, UINT64_C(2)) == INT64_C(10));
    REQUIRE(minisnn_worlds_kernel_last_tick_event_count(reverse) == 2U);

    tick = minisnn_worlds_kernel_tick(issuer_order) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                issuer_order, tick, UINT32_C(7), id(UINT64_C(1)), id(UINT64_C(1)),
                id(UINT64_C(2)), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                issuer_order, tick, UINT32_C(7), id(UINT64_C(2)), id(UINT64_C(1)),
                INT64_C(5), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(issuer_order);
    REQUIRE(x_of(issuer_order, UINT64_C(2)) == INT64_C(15));

    minisnn_worlds_kernel_destroy(forward);
    minisnn_worlds_kernel_destroy(reverse);
    minisnn_worlds_kernel_destroy(issuer_order);
}

static void test_remove_move_and_repeated_root_ordering(void)
{
    MiniSNNWorldsKernel *forward = two_entities();
    MiniSNNWorldsKernel *reverse = two_entities();
    MiniSNNWorldsKernel *same_issuer = two_entities();
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsTick tick;

    link(forward, UINT64_C(1), UINT64_C(2));
    tick = minisnn_worlds_kernel_tick(forward) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_remove_spatial_link(
                forward, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                forward, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(2)),
                INT64_C(4), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(forward);
    REQUIRE(x_of(forward, UINT64_C(2)) == INT64_C(14));
    REQUIRE(minisnn_worlds_kernel_spatial_link_count(forward) == 0U);

    link(reverse, UINT64_C(1), UINT64_C(2));
    tick = minisnn_worlds_kernel_tick(reverse) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                reverse, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(2)),
                INT64_C(4), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_remove_spatial_link(
                reverse, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(reverse);
    require_rejection(reverse, MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_HAS_SPATIAL_PARENT);
    REQUIRE(x_of(reverse, UINT64_C(2)) == INT64_C(10));
    REQUIRE(minisnn_worlds_kernel_spatial_link_count(reverse) == 0U);

    link(same_issuer, UINT64_C(1), UINT64_C(2));
    tick = minisnn_worlds_kernel_tick(same_issuer) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                same_issuer, tick, UINT32_C(3), id(UINT64_C(1)), id(UINT64_C(1)),
                INT64_C(1), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                same_issuer, tick, UINT32_C(3), id(UINT64_C(1)), id(UINT64_C(1)),
                INT64_C(2), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(same_issuer);
    REQUIRE(x_of(same_issuer, UINT64_C(1)) == INT64_C(3));
    REQUIRE(x_of(same_issuer, UINT64_C(2)) == INT64_C(13));
    REQUIRE(minisnn_worlds_kernel_last_tick_event_count(same_issuer) == 4U);

    minisnn_worlds_kernel_destroy(forward);
    minisnn_worlds_kernel_destroy(reverse);
    minisnn_worlds_kernel_destroy(same_issuer);
}

static void test_lifecycle_and_link_precedence(void)
{
    MiniSNNWorldsKernel *kernel = two_entities();
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsTick tick;

    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_destroy_entity(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(1)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    require_rejection(kernel, MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_HAS_SPATIAL_LINKS);
    REQUIRE(minisnn_worlds_kernel_spatial_link_count(kernel) == 1U);

    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_remove_spatial_link(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_destroy_entity(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(2)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    REQUIRE(!minisnn_worlds_kernel_entity_exists(kernel, id(UINT64_C(2))));

    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    require_rejection(kernel, MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_CHILD_NOT_ALIVE);
    minisnn_worlds_kernel_destroy(kernel);

    kernel = two_entities();
    link(kernel, UINT64_C(1), UINT64_C(2));
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_remove_entity_from_space(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(2)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_remove_spatial_link(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    REQUIRE(!minisnn_worlds_kernel_entity_is_placed(kernel, id(UINT64_C(2))));
    minisnn_worlds_kernel_destroy(kernel);
}

static void test_occupancy_link_and_conflict_ordering(void)
{
    MiniSNNWorldsKernel *kernel = tree_and_blocker();
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelCommandInfo pending;
    MiniSNNWorldsTick tick;
    Snapshot before;
    Snapshot after;

    REQUIRE(minisnn_worlds_kernel_queue_set_occupancy(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                id(UINT64_C(0)), id(UINT64_C(1)), blocker(), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_set_occupancy(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(3)), blocker(), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(1)),
                INT64_C(20), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    require_rejection(kernel, MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_CONFLICT);
    REQUIRE(x_of(kernel, UINT64_C(1)) == INT64_C(0));

    before = snapshot(kernel);
    REQUIRE(minisnn_worlds_kernel_pending_command_at(kernel, 0U, &pending) ==
            MINISNN_WORLDS_KERNEL_ERROR_INDEX_OUT_OF_RANGE);
    after = snapshot(kernel);
    REQUIRE(before.state_hash == after.state_hash);
    REQUIRE(before.diagnostics.current_state_hash == after.diagnostics.current_state_hash);

    minisnn_worlds_kernel_destroy(kernel);
}

static void test_link_conflicts_and_canonical_queries(void)
{
    MiniSNNWorldsKernel *kernel = new_kernel();
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelCommandInfo first;
    MiniSNNWorldsKernelCommandInfo second;
    MiniSNNWorldsKernelSpatialLink link_value;
    uint64_t before_hash;
    uint64_t after_hash;
    MiniSNNWorldsTick tick;

    create_entities(kernel, 3U);
    place(kernel, UINT64_C(1), INT64_C(0), INT64_C(0));
    place(kernel, UINT64_C(2), INT64_C(10), INT64_C(0));
    place(kernel, UINT64_C(3), INT64_C(20), INT64_C(0));
    link(kernel, UINT64_C(1), UINT64_C(2));
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(3)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    require_rejection(kernel, MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_CHILD_HAS_PARENT);

    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(2)), id(UINT64_C(1)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    require_rejection(kernel, MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_CYCLE);

    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(3)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(3)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    before_hash = snapshot(kernel).state_hash;
    REQUIRE(minisnn_worlds_kernel_pending_command_at(kernel, 0U, &first) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_pending_command_at(kernel, 1U, &second) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(first.priority < second.priority);
    REQUIRE(minisnn_worlds_kernel_state_hash(kernel, &after_hash) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(before_hash == after_hash);
    step(kernel);
    require_rejection(kernel, MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_DUPLICATE);
    REQUIRE(minisnn_worlds_kernel_spatial_link_count(kernel) == 2U);
    REQUIRE(minisnn_worlds_kernel_spatial_link_at(kernel, 0U, &link_value) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(link_value.parent.value == UINT64_C(1));
    REQUIRE(link_value.child.value == UINT64_C(2));
    minisnn_worlds_kernel_destroy(kernel);
}

static void test_additional_lifecycle_placement_and_link_ordering(void)
{
    MiniSNNWorldsKernel *kernel = two_entities();
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsTick tick;

    link(kernel, UINT64_C(1), UINT64_C(2));
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_destroy_entity(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_remove_spatial_link(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    require_rejection(kernel, MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_HAS_SPATIAL_LINKS);
    REQUIRE(minisnn_worlds_kernel_entity_exists(kernel, id(UINT64_C(1))));
    REQUIRE(minisnn_worlds_kernel_spatial_link_count(kernel) == 0U);

    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_destroy_entity(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    REQUIRE(!minisnn_worlds_kernel_entity_exists(kernel, id(UINT64_C(1))));
    require_rejection(kernel, MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_PARENT_NOT_ALIVE);
    minisnn_worlds_kernel_destroy(kernel);

    kernel = new_kernel();
    create_entities(kernel, 2U);
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_place_entity(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)),
                transform(INT64_C(0), INT64_C(0)), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_place_entity(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(2)),
                transform(INT64_C(10), INT64_C(0)), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(2), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    REQUIRE(minisnn_worlds_kernel_spatial_link_count(kernel) == 1U);
    minisnn_worlds_kernel_destroy(kernel);

    kernel = new_kernel();
    create_entities(kernel, 2U);
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_place_entity(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(1)),
                transform(INT64_C(0), INT64_C(0)), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_place_entity(
                kernel, tick, UINT32_C(2), id(UINT64_C(0)), id(UINT64_C(2)),
                transform(INT64_C(10), INT64_C(0)), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    require_rejection(kernel, MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_PARENT_NOT_PLACED);
    REQUIRE(minisnn_worlds_kernel_spatial_link_count(kernel) == 0U);
    minisnn_worlds_kernel_destroy(kernel);

    kernel = two_entities();
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_remove_entity_from_space(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(2)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    require_rejection(kernel, MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_CHILD_NOT_PLACED);
    minisnn_worlds_kernel_destroy(kernel);

    kernel = two_entities();
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_remove_entity_from_space(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(2)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    require_rejection(kernel, MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_HAS_SPATIAL_LINKS);
    REQUIRE(minisnn_worlds_kernel_spatial_link_count(kernel) == 1U);
    minisnn_worlds_kernel_destroy(kernel);
}
static void test_additional_movement_churn_and_occupancy_ordering(void)
{
    MiniSNNWorldsKernel *kernel = tree_and_blocker();
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsTick tick;

    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)),
                INT64_C(3), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(2)),
                INT64_C(3), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    require_rejection(kernel, MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_HAS_SPATIAL_PARENT);
    REQUIRE(x_of(kernel, UINT64_C(1)) == INT64_C(3));
    REQUIRE(x_of(kernel, UINT64_C(2)) == INT64_C(13));

    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(3)),
                INT64_C(4), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)),
                INT64_C(4), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    REQUIRE(x_of(kernel, UINT64_C(1)) == INT64_C(7));
    REQUIRE(x_of(kernel, UINT64_C(2)) == INT64_C(17));
    REQUIRE(x_of(kernel, UINT64_C(3)) == INT64_C(24));
    minisnn_worlds_kernel_destroy(kernel);

    kernel = new_kernel();
    create_entities(kernel, 3U);
    place(kernel, UINT64_C(1), INT64_C(0), INT64_C(0));
    place(kernel, UINT64_C(2), INT64_C(10), INT64_C(0));
    place(kernel, UINT64_C(3), INT64_C(20), INT64_C(0));
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(2)), id(UINT64_C(3)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(2), id(UINT64_C(0)), id(UINT64_C(3)), id(UINT64_C(1)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    require_rejection(kernel, MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_CYCLE);
    REQUIRE(minisnn_worlds_kernel_spatial_link_count(kernel) == 2U);

    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_remove_spatial_link(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    REQUIRE(minisnn_worlds_kernel_spatial_link_count(kernel) == 2U);

    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_remove_spatial_link(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_remove_spatial_link(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    require_rejection(kernel, MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_NOT_FOUND);
    REQUIRE(minisnn_worlds_kernel_spatial_link_count(kernel) == 1U);
    minisnn_worlds_kernel_destroy(kernel);

    kernel = two_entities();
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)),
                INT64_C(5), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_set_occupancy(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(1)), blocker(),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_set_occupancy(
                kernel, tick, UINT32_C(2), id(UINT64_C(0)), id(UINT64_C(2)), blocker(),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, tick, UINT32_C(3), id(UINT64_C(0)), id(UINT64_C(2)),
                INT64_C(-5), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    require_rejection(kernel, MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_CONFLICT);
    REQUIRE(x_of(kernel, UINT64_C(1)) == INT64_C(5));
    REQUIRE(x_of(kernel, UINT64_C(2)) == INT64_C(10));
    REQUIRE(minisnn_worlds_kernel_entity_has_occupancy(kernel, id(UINT64_C(1))));
    minisnn_worlds_kernel_destroy(kernel);

    kernel = two_entities();
    REQUIRE(minisnn_worlds_kernel_queue_set_occupancy(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                id(UINT64_C(0)), id(UINT64_C(1)), blocker(), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    REQUIRE(minisnn_worlds_kernel_queue_set_occupancy(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                id(UINT64_C(0)), id(UINT64_C(2)), blocker(), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_clear_occupancy(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(2)),
                INT64_C(-10), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    REQUIRE(x_of(kernel, UINT64_C(2)) == INT64_C(0));
    minisnn_worlds_kernel_destroy(kernel);

    kernel = two_entities();
    REQUIRE(minisnn_worlds_kernel_queue_set_occupancy(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                id(UINT64_C(0)), id(UINT64_C(1)), blocker(), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    REQUIRE(minisnn_worlds_kernel_queue_set_occupancy(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                id(UINT64_C(0)), id(UINT64_C(2)), blocker(), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(2)),
                INT64_C(-10), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_clear_occupancy(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(1)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    step(kernel);
    require_rejection(kernel, MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_CONFLICT);
    REQUIRE(x_of(kernel, UINT64_C(2)) == INT64_C(10));
    REQUIRE(!minisnn_worlds_kernel_entity_has_occupancy(kernel, id(UINT64_C(1))));
    minisnn_worlds_kernel_destroy(kernel);
}
int main(void)
{
    test_create_move_ordering();
    test_remove_move_and_repeated_root_ordering();
    test_lifecycle_and_link_precedence();
    test_additional_lifecycle_placement_and_link_ordering();
    test_additional_movement_churn_and_occupancy_ordering();
    test_occupancy_link_and_conflict_ordering();
    test_link_conflicts_and_canonical_queries();
    puts("K1-C3 ordering, lifecycle, occupancy, links and canonical query validation OK");
    return 0;
}
