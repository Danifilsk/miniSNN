#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minisnn_worlds_kernel.h"
void minisnn_worlds_kernel_testing_fail_allocation_after(size_t successful_allocations);

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

static MiniSNNWorldsKernel *new_kernel(void)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel = minisnn_worlds_kernel_create(&config, &error);

    REQUIRE(kernel != NULL);
    REQUIRE(error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    return kernel;
}

static MiniSNNWorldsKernel *new_full_range_kernel(void)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;

    config.space_bounds.min_x = INT64_MIN;
    config.space_bounds.min_y = INT64_MIN;
    config.space_bounds.max_x = INT64_MAX;
    config.space_bounds.max_y = INT64_MAX;
    kernel = minisnn_worlds_kernel_create(&config, &error);
    REQUIRE(kernel != NULL);
    REQUIRE(error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    return kernel;
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
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
}

static void place(MiniSNNWorldsKernel *kernel, uint64_t entity,
                  int64_t x, int64_t y, uint32_t orientation)
{
    MiniSNNWorldsKernelTransform transform;
    MiniSNNWorldsKernelCommandId command_id;

    transform.position.x = x;
    transform.position.y = y;
    transform.orientation = orientation;
    REQUIRE(minisnn_worlds_kernel_queue_place_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                id(UINT64_C(0)), id(entity), transform, &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
}

static void link(MiniSNNWorldsKernel *kernel, uint64_t parent, uint64_t child)
{
    MiniSNNWorldsKernelCommandId command_id;

    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                id(UINT64_C(0)), id(parent), id(child), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
}

static MiniSNNWorldsKernelCommandId queue_move(MiniSNNWorldsKernel *kernel,
                                                 uint64_t entity, int64_t dx, int64_t dy)
{
    MiniSNNWorldsKernelCommandId command_id;

    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                id(UINT64_C(0)), id(entity), dx, dy, &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    return command_id;
}

static MiniSNNWorldsKernelTransform transform_of(
    const MiniSNNWorldsKernel *kernel, uint64_t entity)
{
    MiniSNNWorldsKernelTransform transform;

    REQUIRE(minisnn_worlds_kernel_entity_transform(kernel, id(entity), &transform) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    return transform;
}

static MiniSNNWorldsKernelEvent event_at(
    const MiniSNNWorldsKernel *kernel, size_t index)
{
    MiniSNNWorldsKernelEvent event;

    REQUIRE(minisnn_worlds_kernel_last_tick_event_at(kernel, index, &event) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    return event;
}

static uint64_t current_hash(const MiniSNNWorldsKernel *kernel)
{
    uint64_t hash;

    REQUIRE(minisnn_worlds_kernel_state_hash(kernel, &hash) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    return hash;
}

static void set_blocking_occupancy(MiniSNNWorldsKernel *kernel, uint64_t entity,
                                   int64_t half_x, int64_t half_y)
{
    MiniSNNWorldsKernelOccupancy occupancy;
    MiniSNNWorldsKernelCommandId command_id;

    memset(&occupancy, 0, sizeof(occupancy));
    occupancy.half_extent_x = half_x;
    occupancy.half_extent_y = half_y;
    occupancy.category_bits = UINT32_C(1);
    occupancy.blocking_mask = UINT32_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_set_occupancy(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                id(UINT64_C(0)), id(entity), occupancy, &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
}

static MiniSNNWorldsKernel *make_tree(void)
{
    MiniSNNWorldsKernel *kernel = new_kernel();

    create_entities(kernel, 4U);
    place(kernel, UINT64_C(1), INT64_C(0), INT64_C(0), UINT32_C(1));
    place(kernel, UINT64_C(2), INT64_C(10), INT64_C(0), UINT32_C(2));
    place(kernel, UINT64_C(3), INT64_C(0), INT64_C(10), UINT32_C(3));
    place(kernel, UINT64_C(4), INT64_C(20), INT64_C(0), UINT32_C(4));
    link(kernel, UINT64_C(1), UINT64_C(3));
    link(kernel, UINT64_C(2), UINT64_C(4));
    link(kernel, UINT64_C(1), UINT64_C(2));
    return kernel;
}

static void test_rigid_tree_events_and_zero_delta(void)
{
    MiniSNNWorldsKernel *kernel = make_tree();
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelDiagnostics diagnostics;
    MiniSNNWorldsKernelEvent event;
    size_t index;
    uint64_t previous_event_id = UINT64_C(0);
    const uint64_t expected_subjects[] = { UINT64_C(1), UINT64_C(2), UINT64_C(3), UINT64_C(4) };
    const uint64_t expected_parents[] = { UINT64_C(0), UINT64_C(1), UINT64_C(1), UINT64_C(2) };

    command_id = queue_move(kernel, UINT64_C(1), INT64_C(5), INT64_C(-3));
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_last_tick_event_count(kernel) == 4U);
    for (index = 0U; index < 4U; ++index)
    {
        event = event_at(kernel, index);
        REQUIRE(event.type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED);
        REQUIRE(event.command_id.value == command_id.value);
        REQUIRE(event.subject.value == expected_subjects[index]);
        REQUIRE(event.related_entity.value == expected_parents[index]);
        REQUIRE(event.affected_entity.value == UINT64_C(0));
        REQUIRE(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_NONE);
        REQUIRE(event.has_previous_transform && event.has_transform && event.has_displacement);
        REQUIRE(event.displacement.x == INT64_C(5) && event.displacement.y == INT64_C(-3));
        if (index != 0U)
        {
            REQUIRE(event.event_id.value == previous_event_id + UINT64_C(1));
        }
        previous_event_id = event.event_id.value;
    }
    REQUIRE(transform_of(kernel, UINT64_C(1)).position.x == INT64_C(5));
    REQUIRE(transform_of(kernel, UINT64_C(2)).position.x == INT64_C(15));
    REQUIRE(transform_of(kernel, UINT64_C(3)).position.y == INT64_C(7));
    REQUIRE(transform_of(kernel, UINT64_C(4)).position.x == INT64_C(25));
    REQUIRE(transform_of(kernel, UINT64_C(4)).orientation == UINT32_C(4));
    REQUIRE(minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(diagnostics.total_entities_moved == UINT64_C(4));
    REQUIRE(diagnostics.total_events_emitted >= UINT64_C(4));
    REQUIRE(current_hash(kernel) == UINT64_C(0x13AF7118E68BB848));

    queue_move(kernel, UINT64_C(1), INT64_C(0), INT64_C(0));
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_last_tick_event_count(kernel) == 1U);
    event = event_at(kernel, 0U);
    REQUIRE(event.subject.value == UINT64_C(1));
    REQUIRE(event.related_entity.value == UINT64_C(0));
    REQUIRE(event.has_displacement);
    REQUIRE(event.displacement.x == INT64_C(0) && event.displacement.y == INT64_C(0));
    REQUIRE(minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(diagnostics.total_entities_moved == UINT64_C(4));
    minisnn_worlds_kernel_destroy(kernel);
}

static void test_child_guard_and_external_conflict(void)
{
    MiniSNNWorldsKernel *kernel = make_tree();
    MiniSNNWorldsKernelEvent event;
    MiniSNNWorldsKernelTransform before;
    MiniSNNWorldsKernelCommandId command_id;

    queue_move(kernel, UINT64_C(2), INT64_C(1), INT64_C(0));
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    event = event_at(kernel, 0U);
    REQUIRE(event.rejection ==
            MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_HAS_SPATIAL_PARENT);
    REQUIRE(event.subject.value == UINT64_C(2));
    REQUIRE(event.related_entity.value == UINT64_C(1));
    REQUIRE(event.affected_entity.value == UINT64_C(0));
    minisnn_worlds_kernel_destroy(kernel);

    kernel = new_kernel();
    create_entities(kernel, 3U);
    place(kernel, UINT64_C(1), INT64_C(0), INT64_C(0), UINT32_C(0));
    place(kernel, UINT64_C(2), INT64_C(10), INT64_C(0), UINT32_C(0));
    place(kernel, UINT64_C(3), INT64_C(20), INT64_C(0), UINT32_C(0));
    set_blocking_occupancy(kernel, UINT64_C(2), INT64_C(2), INT64_C(2));
    set_blocking_occupancy(kernel, UINT64_C(3), INT64_C(2), INT64_C(2));
    link(kernel, UINT64_C(1), UINT64_C(2));
    before = transform_of(kernel, UINT64_C(2));
    command_id = queue_move(kernel, UINT64_C(1), INT64_C(10), INT64_C(0));
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_last_tick_event_count(kernel) == 1U);
    event = event_at(kernel, 0U);
    REQUIRE(event.command_id.value == command_id.value);
    REQUIRE(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_CONFLICT);
    REQUIRE(event.subject.value == UINT64_C(1));
    REQUIRE(event.related_entity.value == UINT64_C(3));
    REQUIRE(event.affected_entity.value == UINT64_C(2));
    REQUIRE(transform_of(kernel, UINT64_C(2)).position.x == before.position.x);
    minisnn_worlds_kernel_destroy(kernel);
}

static void test_overflow_ordering_and_rollback(void)
{
    MiniSNNWorldsKernel *kernel = new_full_range_kernel();
    MiniSNNWorldsKernelEvent event;
    uint64_t before_hash;
    MiniSNNWorldsTick before_tick;
    size_t failures;

    create_entities(kernel, 3U);
    place(kernel, UINT64_C(1), INT64_C(0), INT64_C(0), UINT32_C(0));
    place(kernel, UINT64_C(2), INT64_MAX, INT64_C(0), UINT32_C(0));
    place(kernel, UINT64_C(3), INT64_MAX, INT64_C(1), UINT32_C(0));
    link(kernel, UINT64_C(1), UINT64_C(3));
    link(kernel, UINT64_C(1), UINT64_C(2));
    queue_move(kernel, UINT64_C(1), INT64_C(1), INT64_C(0));
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    event = event_at(kernel, 0U);
    REQUIRE(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_DESTINATION_OVERFLOW);
    REQUIRE(event.subject.value == UINT64_C(1));
    REQUIRE(event.related_entity.value == UINT64_C(0));
    REQUIRE(event.affected_entity.value == UINT64_C(2));
    REQUIRE(transform_of(kernel, UINT64_C(1)).position.x == INT64_C(0));
    minisnn_worlds_kernel_destroy(kernel);

    kernel = make_tree();
    queue_move(kernel, UINT64_C(1), INT64_C(2), INT64_C(0));
    before_hash = current_hash(kernel);
    before_tick = minisnn_worlds_kernel_tick(kernel);
    for (failures = 0U; failures < 16U; ++failures)
    {
        minisnn_worlds_kernel_testing_fail_allocation_after(failures);
        if (minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION)
        {
            REQUIRE(minisnn_worlds_kernel_tick(kernel) == before_tick);
            REQUIRE(minisnn_worlds_kernel_pending_command_count(kernel) == 1U);
            REQUIRE(current_hash(kernel) == before_hash);
            break;
        }
    }
    REQUIRE(failures < 16U);
    minisnn_worlds_kernel_testing_fail_allocation_after(SIZE_MAX);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    minisnn_worlds_kernel_destroy(kernel);
}

static void test_same_tick_link_and_move_and_scale(void)
{
    const size_t chain_count = 256U;
    MiniSNNWorldsKernel *kernel = new_kernel();
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsTick tick;
    size_t index;

    create_entities(kernel, chain_count);
    for (index = 1U; index <= chain_count; ++index)
    {
        place(kernel, (uint64_t)index, (int64_t)(index * 10U), INT64_C(0), UINT32_C(0));
    }
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    for (index = chain_count; index > 1U; --index)
    {
        REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                    kernel, tick, UINT32_C(0), id(UINT64_C(0)),
                    id((uint64_t)(index - 1U)), id((uint64_t)index), &command_id) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(1)),
                INT64_C(-3), INT64_C(4), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_last_tick_event_count(kernel) == chain_count * 2U - 1U);
    REQUIRE(transform_of(kernel, UINT64_C(1)).position.x == INT64_C(7));
    REQUIRE(transform_of(kernel, (uint64_t)chain_count).position.x ==
            (int64_t)(chain_count * 10U - 3U));
    REQUIRE(transform_of(kernel, (uint64_t)chain_count).position.y == INT64_C(4));
    minisnn_worlds_kernel_destroy(kernel);
}


static void test_internal_collision_and_canonical_external_conflict(void)
{
    MiniSNNWorldsKernel *kernel = new_kernel();
    MiniSNNWorldsKernelEvent event;

    create_entities(kernel, 2U);
    place(kernel, UINT64_C(1), INT64_C(0), INT64_C(0), UINT32_C(0));
    place(kernel, UINT64_C(2), INT64_C(10), INT64_C(0), UINT32_C(0));
    set_blocking_occupancy(kernel, UINT64_C(1), INT64_C(2), INT64_C(2));
    set_blocking_occupancy(kernel, UINT64_C(2), INT64_C(2), INT64_C(2));
    link(kernel, UINT64_C(1), UINT64_C(2));
    queue_move(kernel, UINT64_C(1), INT64_C(10), INT64_C(0));
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_last_tick_event_count(kernel) == 2U);
    REQUIRE(transform_of(kernel, UINT64_C(1)).position.x == INT64_C(10));
    REQUIRE(transform_of(kernel, UINT64_C(2)).position.x == INT64_C(20));
    minisnn_worlds_kernel_destroy(kernel);

    kernel = new_kernel();
    create_entities(kernel, 4U);
    place(kernel, UINT64_C(1), INT64_C(0), INT64_C(0), UINT32_C(0));
    place(kernel, UINT64_C(2), INT64_C(10), INT64_C(0), UINT32_C(0));
    place(kernel, UINT64_C(3), INT64_C(30), INT64_C(0), UINT32_C(0));
    place(kernel, UINT64_C(4), INT64_C(20), INT64_C(0), UINT32_C(0));
    set_blocking_occupancy(kernel, UINT64_C(1), INT64_C(2), INT64_C(2));
    set_blocking_occupancy(kernel, UINT64_C(2), INT64_C(2), INT64_C(2));
    set_blocking_occupancy(kernel, UINT64_C(3), INT64_C(2), INT64_C(2));
    set_blocking_occupancy(kernel, UINT64_C(4), INT64_C(2), INT64_C(2));
    link(kernel, UINT64_C(1), UINT64_C(2));
    queue_move(kernel, UINT64_C(1), INT64_C(20), INT64_C(0));
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    event = event_at(kernel, 0U);
    REQUIRE(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_CONFLICT);
    REQUIRE(event.related_entity.value == UINT64_C(3));
    REQUIRE(event.affected_entity.value == UINT64_C(2));
    REQUIRE(transform_of(kernel, UINT64_C(1)).position.x == INT64_C(0));
    REQUIRE(transform_of(kernel, UINT64_C(2)).position.x == INT64_C(10));
    minisnn_worlds_kernel_destroy(kernel);
}

static void test_same_tick_link_move_ordering_and_two_roots(void)
{
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelSpatialLink spatial_link;
    MiniSNNWorldsTick tick;

    kernel = new_kernel();
    create_entities(kernel, 2U);
    place(kernel, UINT64_C(1), INT64_C(0), INT64_C(0), UINT32_C(0));
    place(kernel, UINT64_C(2), INT64_C(10), INT64_C(0), UINT32_C(0));
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)),
                id(UINT64_C(2)), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(1)),
                INT64_C(5), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_last_tick_event_count(kernel) == 3U);
    REQUIRE(event_at(kernel, 0U).type == MINISNN_WORLDS_KERNEL_EVENT_SPATIAL_LINK_CREATED);
    REQUIRE(event_at(kernel, 1U).subject.value == UINT64_C(1));
    REQUIRE(event_at(kernel, 2U).subject.value == UINT64_C(2));
    REQUIRE(transform_of(kernel, UINT64_C(2)).position.x == INT64_C(15));
    minisnn_worlds_kernel_destroy(kernel);

    kernel = new_kernel();
    create_entities(kernel, 2U);
    place(kernel, UINT64_C(1), INT64_C(0), INT64_C(0), UINT32_C(0));
    place(kernel, UINT64_C(2), INT64_C(10), INT64_C(0), UINT32_C(0));
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)),
                INT64_C(5), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(1)),
                id(UINT64_C(2)), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_last_tick_event_count(kernel) == 2U);
    REQUIRE(event_at(kernel, 0U).subject.value == UINT64_C(1));
    REQUIRE(event_at(kernel, 1U).type == MINISNN_WORLDS_KERNEL_EVENT_SPATIAL_LINK_CREATED);
    REQUIRE(minisnn_worlds_kernel_spatial_link_at(kernel, 0U, &spatial_link) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(spatial_link.offset_x == INT64_C(5));
    minisnn_worlds_kernel_destroy(kernel);

    kernel = new_kernel();
    create_entities(kernel, 2U);
    place(kernel, UINT64_C(1), INT64_C(0), INT64_C(0), UINT32_C(0));
    place(kernel, UINT64_C(2), INT64_C(10), INT64_C(0), UINT32_C(0));
    link(kernel, UINT64_C(1), UINT64_C(2));
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_remove_spatial_link(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)),
                id(UINT64_C(2)), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(2)),
                INT64_C(4), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_spatial_link_count(kernel) == 0U);
    REQUIRE(transform_of(kernel, UINT64_C(2)).position.x == INT64_C(14));
    minisnn_worlds_kernel_destroy(kernel);

    kernel = new_kernel();
    create_entities(kernel, 4U);
    place(kernel, UINT64_C(1), INT64_C(0), INT64_C(0), UINT32_C(0));
    place(kernel, UINT64_C(2), INT64_C(10), INT64_C(0), UINT32_C(0));
    place(kernel, UINT64_C(3), INT64_C(100), INT64_C(0), UINT32_C(0));
    place(kernel, UINT64_C(4), INT64_C(110), INT64_C(0), UINT32_C(0));
    link(kernel, UINT64_C(1), UINT64_C(2));
    link(kernel, UINT64_C(3), UINT64_C(4));
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(3)),
                INT64_C(-1), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(1)),
                INT64_C(1), INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_last_tick_event_count(kernel) == 4U);
    REQUIRE(event_at(kernel, 0U).subject.value == UINT64_C(3));
    REQUIRE(event_at(kernel, 1U).subject.value == UINT64_C(4));
    REQUIRE(event_at(kernel, 2U).subject.value == UINT64_C(1));
    REQUIRE(event_at(kernel, 3U).subject.value == UINT64_C(2));
    minisnn_worlds_kernel_destroy(kernel);
}

static void test_internal_failure_rollbacks(void)
{
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelTransform before;
    uint64_t before_hash;
    MiniSNNWorldsTick before_tick;
    size_t failures;

    kernel = make_tree();
    REQUIRE(minisnn_worlds_kernel_testing_set_next_event_id(
                kernel, (MiniSNNWorldsKernelEventId){ UINT64_MAX - UINT64_C(3) }) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    queue_move(kernel, UINT64_C(1), INT64_C(1), INT64_C(0));
    before = transform_of(kernel, UINT64_C(1));
    before_hash = current_hash(kernel);
    before_tick = minisnn_worlds_kernel_tick(kernel);
    REQUIRE(minisnn_worlds_kernel_step(kernel) ==
            MINISNN_WORLDS_KERNEL_ERROR_IDENTIFIER_OVERFLOW);
    REQUIRE(minisnn_worlds_kernel_tick(kernel) == before_tick);
    REQUIRE(minisnn_worlds_kernel_pending_command_count(kernel) == 1U);
    REQUIRE(current_hash(kernel) == before_hash);
    REQUIRE(transform_of(kernel, UINT64_C(1)).position.x == before.position.x);
    minisnn_worlds_kernel_destroy(kernel);

    kernel = make_tree();
    REQUIRE(minisnn_worlds_kernel_testing_set_observability_counters(
                kernel, UINT64_MAX - UINT64_C(3), UINT64_C(0)) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    queue_move(kernel, UINT64_C(1), INT64_C(1), INT64_C(0));
    before_hash = current_hash(kernel);
    before_tick = minisnn_worlds_kernel_tick(kernel);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_INTERNAL);
    REQUIRE(minisnn_worlds_kernel_tick(kernel) == before_tick);
    REQUIRE(minisnn_worlds_kernel_pending_command_count(kernel) == 1U);
    REQUIRE(current_hash(kernel) == before_hash);
    minisnn_worlds_kernel_destroy(kernel);

    kernel = make_tree();
    REQUIRE(minisnn_worlds_kernel_testing_set_observability_counters(
                kernel, UINT64_C(0), UINT64_MAX - UINT64_C(3)) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    queue_move(kernel, UINT64_C(1), INT64_C(1), INT64_C(0));
    before_hash = current_hash(kernel);
    before_tick = minisnn_worlds_kernel_tick(kernel);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_INTERNAL);
    REQUIRE(minisnn_worlds_kernel_tick(kernel) == before_tick);
    REQUIRE(minisnn_worlds_kernel_pending_command_count(kernel) == 1U);
    REQUIRE(current_hash(kernel) == before_hash);
    minisnn_worlds_kernel_destroy(kernel);

    kernel = make_tree();
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                id(UINT64_C(0)), id(UINT64_C(1)), INT64_C(1), INT64_C(0), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(1),
                id(UINT64_C(0)), id(UINT64_C(1)), INT64_C(2), INT64_C(0), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    before_hash = current_hash(kernel);
    before_tick = minisnn_worlds_kernel_tick(kernel);
    for (failures = 0U; failures < 32U; ++failures)
    {
        minisnn_worlds_kernel_testing_fail_allocation_after(failures);
        if (minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION)
        {
            REQUIRE(minisnn_worlds_kernel_tick(kernel) == before_tick);
            REQUIRE(minisnn_worlds_kernel_pending_command_count(kernel) == 2U);
            REQUIRE(current_hash(kernel) == before_hash);
        }
        else
        {
            break;
        }
    }
    REQUIRE(failures < 32U);
    minisnn_worlds_kernel_testing_fail_allocation_after(SIZE_MAX);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    minisnn_worlds_kernel_destroy(kernel);
}
int main(void)
{
    test_rigid_tree_events_and_zero_delta();
    test_child_guard_and_external_conflict();
    test_overflow_ordering_and_rollback();
    test_same_tick_link_and_move_and_scale();
    test_internal_collision_and_canonical_external_conflict();
    test_same_tick_link_move_ordering_and_two_roots();
    test_internal_failure_rollbacks();
    puts("K1-C2 subtree translation, events, atomicity and V5 validation OK");
    return 0;
}