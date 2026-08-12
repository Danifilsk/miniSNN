#include "minisnn_worlds_kernel.h"

#include <stdio.h>
#include <stdlib.h>

#define REQUIRE(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            fprintf(stderr, "requirement failed: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
            exit(1); \
        } \
    } while (0)

static MiniSNNWorldsKernelEntityId entity_id(uint64_t value)
{
    MiniSNNWorldsKernelEntityId entity;

    entity.value = value;
    return entity;
}

static MiniSNNWorldsKernelTransform transform(
    MiniSNNWorldsKernelScalar x,
    MiniSNNWorldsKernelScalar y,
    uint32_t orientation)
{
    MiniSNNWorldsKernelTransform value;

    value.position.x = x;
    value.position.y = y;
    value.orientation = orientation;
    return value;
}

static MiniSNNWorldsKernel *new_kernel(void)
{
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel = minisnn_worlds_kernel_create(NULL, &error);

    REQUIRE(kernel != NULL);
    REQUIRE(error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    return kernel;
}

static void create_entities(MiniSNNWorldsKernel *kernel, size_t count)
{
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsTick tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    size_t index;

    for (index = 0U; index < count; ++index)
    {
        REQUIRE(minisnn_worlds_kernel_queue_create_entity(
                    kernel, tick, MINISNN_WORLDS_KERNEL_COMMAND_PRIORITY_DEFAULT,
                    entity_id(UINT64_C(0)), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
}

static void place_entity(
    MiniSNNWorldsKernel *kernel,
    uint64_t id,
    MiniSNNWorldsKernelScalar x,
    MiniSNNWorldsKernelScalar y,
    uint32_t orientation)
{
    MiniSNNWorldsKernelCommandId command_id;

    REQUIRE(minisnn_worlds_kernel_queue_place_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1),
                MINISNN_WORLDS_KERNEL_COMMAND_PRIORITY_DEFAULT,
                entity_id(UINT64_C(0)), entity_id(id), transform(x, y, orientation),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
}

static void place_three(MiniSNNWorldsKernel *kernel)
{
    place_entity(kernel, UINT64_C(1), INT64_C(100), INT64_C(-50), UINT32_C(4));
    place_entity(kernel, UINT64_C(2), INT64_C(-25), INT64_C(50), UINT32_C(7));
    place_entity(kernel, UINT64_C(3), INT64_C(0), INT64_C(10), UINT32_C(1));
}

static MiniSNNWorldsKernelEvent last_event(const MiniSNNWorldsKernel *kernel)
{
    MiniSNNWorldsKernelEvent event;
    size_t count = minisnn_worlds_kernel_last_tick_event_count(kernel);

    REQUIRE(count != 0U);
    REQUIRE(minisnn_worlds_kernel_last_tick_event_at(kernel, count - UINT64_C(1),
                                                      &event) ==
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

static void test_public_contract_and_simple_link(void)
{
    MiniSNNWorldsKernel *kernel = new_kernel();
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelCommandInfo command;
    MiniSNNWorldsKernelEvent event;
    MiniSNNWorldsKernelSpatialLink link;
    MiniSNNWorldsKernelSpatialLink copied_link;
    MiniSNNWorldsKernelDiagnostics diagnostics;

    create_entities(kernel, 3U);
    place_three(kernel);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1),
                UINT32_C(7), entity_id(UINT64_C(0)), entity_id(UINT64_C(1)),
                entity_id(UINT64_C(2)), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_pending_command_at(kernel, 0U, &command) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(command.type == MINISNN_WORLDS_KERNEL_COMMAND_CREATE_SPATIAL_LINK);
    REQUIRE(command.target_entity.value == UINT64_C(1));
    REQUIRE(command.has_spatial_link_endpoints);
    REQUIRE(command.spatial_link_endpoints.parent.value == UINT64_C(1));
    REQUIRE(command.spatial_link_endpoints.child.value == UINT64_C(2));
    REQUIRE(current_hash(kernel) != UINT64_C(0));
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    event = last_event(kernel);
    REQUIRE(event.type == MINISNN_WORLDS_KERNEL_EVENT_SPATIAL_LINK_CREATED);
    REQUIRE(event.subject.value == UINT64_C(1));
    REQUIRE(event.related_entity.value == UINT64_C(2));
    REQUIRE(event.has_spatial_link);
    REQUIRE(event.affected_entity.value == UINT64_C(0));
    REQUIRE(event.spatial_link.parent.value == UINT64_C(1));
    REQUIRE(event.spatial_link.child.value == UINT64_C(2));
    REQUIRE(event.spatial_link.offset_x == INT64_C(-125));
    REQUIRE(event.spatial_link.offset_y == INT64_C(100));
    REQUIRE(minisnn_worlds_kernel_spatial_link_count(kernel) == 1U);
    REQUIRE(minisnn_worlds_kernel_entity_has_spatial_parent(kernel, entity_id(UINT64_C(2))));
    REQUIRE(!minisnn_worlds_kernel_entity_has_spatial_parent(kernel, entity_id(UINT64_C(1))));
    REQUIRE(minisnn_worlds_kernel_entity_spatial_parent(kernel, entity_id(UINT64_C(2)),
                                                         &link) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    copied_link = link;
    copied_link.offset_x = INT64_C(999);
    REQUIRE(copied_link.offset_x == INT64_C(999));
    REQUIRE(minisnn_worlds_kernel_spatial_link_at(kernel, 0U, &link) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(link.offset_x == INT64_C(-125));
    REQUIRE(minisnn_worlds_kernel_entity_spatial_parent(kernel, entity_id(UINT64_C(3)),
                                                         &link) ==
            MINISNN_WORLDS_KERNEL_ERROR_SPATIAL_LINK_NOT_FOUND);
    REQUIRE(minisnn_worlds_kernel_spatial_link_at(kernel, 1U, &link) ==
            MINISNN_WORLDS_KERNEL_ERROR_INDEX_OUT_OF_RANGE);
    REQUIRE(minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(diagnostics.active_spatial_links == UINT64_C(1));
    REQUIRE(diagnostics.total_spatial_links_created == UINT64_C(1));
    REQUIRE(diagnostics.total_spatial_links_removed == UINT64_C(0));
    REQUIRE(diagnostics.total_spatial_link_commands_processed == UINT64_C(1));
    REQUIRE(diagnostics.state_hash_version == MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V5);
    minisnn_worlds_kernel_destroy(kernel);
}

static void test_submission_and_forest_rejections(void)
{
    MiniSNNWorldsKernel *kernel = new_kernel();
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelEvent event;

    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, UINT64_C(1), UINT32_C(0), entity_id(UINT64_C(0)),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(1)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_INVALID_ENTITY_ID);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, UINT64_C(1), UINT32_C(0), entity_id(UINT64_C(0)),
                entity_id(UINT64_C(1)), entity_id(UINT64_C(0)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_INVALID_ENTITY_ID);
    REQUIRE(minisnn_worlds_kernel_pending_command_count(kernel) == 0U);
    create_entities(kernel, 3U);
    place_three(kernel);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(1)), entity_id(UINT64_C(1)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    event = last_event(kernel);
    REQUIRE(event.type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED);
    REQUIRE(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_SELF);
    REQUIRE(!event.has_spatial_link);
    REQUIRE(event.affected_entity.value == UINT64_C(0));
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(1)), entity_id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(1)), entity_id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    event = last_event(kernel);
    REQUIRE(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_DUPLICATE);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(3)), entity_id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    event = last_event(kernel);
    REQUIRE(event.rejection ==
            MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_CHILD_HAS_PARENT);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(2)), entity_id(UINT64_C(1)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    event = last_event(kernel);
    REQUIRE(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_CYCLE);
    minisnn_worlds_kernel_destroy(kernel);
}

static void test_lifecycle_move_and_remove(void)
{
    MiniSNNWorldsKernel *kernel = new_kernel();
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelEvent event;
    MiniSNNWorldsKernelTransform before;
    MiniSNNWorldsKernelTransform after;
    MiniSNNWorldsTick tick;

    create_entities(kernel, 3U);
    place_three(kernel);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(1)), entity_id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(1),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(2)), entity_id(UINT64_C(3)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(2)), INT64_C(1), INT64_C(0),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    event = last_event(kernel);
    REQUIRE(event.rejection ==
            MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_HAS_SPATIAL_PARENT);
    REQUIRE(event.related_entity.value == UINT64_C(1));
    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(1)), INT64_C(1), INT64_C(0),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_last_tick_event_count(kernel) == 3U);
    event = last_event(kernel);
    REQUIRE(event.type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED);
    REQUIRE(event.subject.value == UINT64_C(3));
    REQUIRE(event.related_entity.value == UINT64_C(2));
    REQUIRE(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_remove_entity_from_space(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(1)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    event = last_event(kernel);
    REQUIRE(event.rejection ==
            MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_HAS_SPATIAL_LINKS);
    REQUIRE(event.related_entity.value == UINT64_C(2));
    REQUIRE(event.affected_entity.value == UINT64_C(0));
    REQUIRE(minisnn_worlds_kernel_queue_destroy_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(2)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    event = last_event(kernel);
    REQUIRE(event.rejection ==
            MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_HAS_SPATIAL_LINKS);
    REQUIRE(event.related_entity.value == UINT64_C(1));
    REQUIRE(minisnn_worlds_kernel_entity_transform(kernel, entity_id(UINT64_C(1)), &before) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    REQUIRE(minisnn_worlds_kernel_queue_remove_spatial_link(
                kernel, tick, UINT32_C(0), entity_id(UINT64_C(0)), entity_id(UINT64_C(1)),
                entity_id(UINT64_C(2)), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_remove_spatial_link(
                kernel, tick, UINT32_C(1), entity_id(UINT64_C(0)), entity_id(UINT64_C(2)),
                entity_id(UINT64_C(3)), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_spatial_link_count(kernel) == 0U);
    REQUIRE(minisnn_worlds_kernel_entity_transform(kernel, entity_id(UINT64_C(1)), &after) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(before.position.x == after.position.x && before.position.y == after.position.y &&
            before.orientation == after.orientation);
    minisnn_worlds_kernel_destroy(kernel);
}

static void test_v5_and_atomicity(void)
{
    MiniSNNWorldsKernel *kernel = new_kernel();
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelDiagnostics diagnostics;
    uint64_t pending_hash;
    uint64_t v5_hash;

    create_entities(kernel, 2U);
    place_entity(kernel, UINT64_C(1), INT64_C(0), INT64_C(0), UINT32_C(0));
    place_entity(kernel, UINT64_C(2), INT64_C(10), INT64_C(0), UINT32_C(0));
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(1)), entity_id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(diagnostics.state_hash_version == MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V5);
    REQUIRE(minisnn_worlds_kernel_state_hash_versioned(
                kernel, MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V4, &v5_hash) ==
            MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(current_hash(kernel) == UINT64_C(0xF6C92E0389E076CD));

    REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(1)), INT64_C(2), INT64_C(0),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    pending_hash = current_hash(kernel);
    minisnn_worlds_kernel_testing_fail_allocation_after(3U);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
    REQUIRE(minisnn_worlds_kernel_tick(kernel) == UINT64_C(4));
    REQUIRE(minisnn_worlds_kernel_pending_command_count(kernel) == 1U);
    REQUIRE(current_hash(kernel) == pending_hash);
    minisnn_worlds_kernel_testing_fail_allocation_after(SIZE_MAX);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_spatial_link_count(kernel) == 1U);
    REQUIRE(minisnn_worlds_kernel_queue_remove_spatial_link(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(1)), entity_id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    {
        MiniSNNWorldsKernelEvent removal_event = last_event(kernel);

        REQUIRE(removal_event.type == MINISNN_WORLDS_KERNEL_EVENT_SPATIAL_LINK_REMOVED);
        REQUIRE(removal_event.has_spatial_link);
        REQUIRE(removal_event.spatial_link.parent.value == UINT64_C(1));
        REQUIRE(removal_event.spatial_link.child.value == UINT64_C(2));
        REQUIRE(removal_event.spatial_link.offset_x == INT64_C(10));
        REQUIRE(removal_event.spatial_link.offset_y == INT64_C(0));
        REQUIRE(removal_event.affected_entity.value == UINT64_C(0));
    }
    REQUIRE(minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(diagnostics.state_hash_version == MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V5);
    REQUIRE(diagnostics.total_spatial_links_created == UINT64_C(1));
    REQUIRE(diagnostics.total_spatial_links_removed == UINT64_C(1));
    minisnn_worlds_kernel_destroy(kernel);
}

static MiniSNNWorldsKernel *create_linked_order_kernel(int reverse_order)
{
    MiniSNNWorldsKernel *kernel = new_kernel();
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsTick tick;

    create_entities(kernel, 4U);
    place_entity(kernel, UINT64_C(1), INT64_C(0), INT64_C(0), UINT32_C(2));
    place_entity(kernel, UINT64_C(2), INT64_C(10), INT64_C(0), UINT32_C(3));
    place_entity(kernel, UINT64_C(3), INT64_C(-10), INT64_C(5), UINT32_C(4));
    place_entity(kernel, UINT64_C(4), INT64_C(0), INT64_C(0), UINT32_C(5));
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    if (reverse_order != 0)
    {
        REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                    kernel, tick, UINT32_C(0), entity_id(UINT64_C(0)),
                    entity_id(UINT64_C(1)), entity_id(UINT64_C(4)), &command_id) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
        REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                    kernel, tick, UINT32_C(0), entity_id(UINT64_C(0)),
                    entity_id(UINT64_C(1)), entity_id(UINT64_C(2)), &command_id) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
        REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                    kernel, tick, UINT32_C(0), entity_id(UINT64_C(0)),
                    entity_id(UINT64_C(1)), entity_id(UINT64_C(3)), &command_id) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    else
    {
        REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                    kernel, tick, UINT32_C(0), entity_id(UINT64_C(0)),
                    entity_id(UINT64_C(1)), entity_id(UINT64_C(3)), &command_id) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
        REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                    kernel, tick, UINT32_C(0), entity_id(UINT64_C(0)),
                    entity_id(UINT64_C(1)), entity_id(UINT64_C(2)), &command_id) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
        REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                    kernel, tick, UINT32_C(0), entity_id(UINT64_C(0)),
                    entity_id(UINT64_C(1)), entity_id(UINT64_C(4)), &command_id) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    return kernel;
}

static void test_canonical_order_offsets_and_scale(void)
{
    MiniSNNWorldsKernel *first = create_linked_order_kernel(0);
    MiniSNNWorldsKernel *second = create_linked_order_kernel(1);
    MiniSNNWorldsKernelSpatialLink link;
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelEvent event;
    size_t index;
    const size_t chain_count = 48U;
    MiniSNNWorldsKernel *chain = new_kernel();

    REQUIRE(current_hash(first) == current_hash(second));
    REQUIRE(minisnn_worlds_kernel_spatial_link_count(first) == 3U);
    REQUIRE(minisnn_worlds_kernel_spatial_link_at(first, 0U, &link) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(link.parent.value == UINT64_C(1) && link.child.value == UINT64_C(2));
    REQUIRE(link.offset_x == INT64_C(10) && link.offset_y == INT64_C(0));
    REQUIRE(minisnn_worlds_kernel_spatial_link_at(first, 1U, &link) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(link.parent.value == UINT64_C(1) && link.child.value == UINT64_C(3));
    REQUIRE(link.offset_x == INT64_C(-10) && link.offset_y == INT64_C(5));
    REQUIRE(minisnn_worlds_kernel_spatial_link_at(first, 2U, &link) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(link.parent.value == UINT64_C(1) && link.child.value == UINT64_C(4));
    REQUIRE(link.offset_x == INT64_C(0) && link.offset_y == INT64_C(0));
    minisnn_worlds_kernel_destroy(first);
    minisnn_worlds_kernel_destroy(second);

    create_entities(chain, chain_count);
    for (index = 1U; index <= chain_count; ++index)
    {
        place_entity(chain, (uint64_t)index, (MiniSNNWorldsKernelScalar)(index * 10U),
                     (MiniSNNWorldsKernelScalar)(index * -2), UINT32_C(0));
    }
    for (index = chain_count; index > 1U; --index)
    {
        REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                    chain, minisnn_worlds_kernel_tick(chain) + UINT64_C(1), UINT32_C(0),
                    entity_id(UINT64_C(0)), entity_id((uint64_t)(index - 1U)),
                    entity_id((uint64_t)index), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    REQUIRE(minisnn_worlds_kernel_step(chain) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_spatial_link_count(chain) == chain_count - 1U);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                chain, minisnn_worlds_kernel_tick(chain) + UINT64_C(1), UINT32_C(0),
                entity_id(UINT64_C(0)), entity_id((uint64_t)chain_count), entity_id(UINT64_C(1)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(chain) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    event = last_event(chain);
    REQUIRE(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_CYCLE);
    REQUIRE(minisnn_worlds_kernel_queue_remove_spatial_link(
                chain, minisnn_worlds_kernel_tick(chain) + UINT64_C(1), UINT32_C(0),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(1)), entity_id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_remove_spatial_link(
                chain, minisnn_worlds_kernel_tick(chain) + UINT64_C(1), UINT32_C(1),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(24)), entity_id(UINT64_C(25)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_remove_spatial_link(
                chain, minisnn_worlds_kernel_tick(chain) + UINT64_C(1), UINT32_C(2),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(47)), entity_id(UINT64_C(48)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(chain) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_spatial_link_count(chain) == chain_count - 4U);
    minisnn_worlds_kernel_destroy(chain);
}

static void test_offset_overflow_rejections(void)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelEvent event;
    MiniSNNWorldsKernel *kernel;

    config.space_bounds.min_x = INT64_MIN;
    config.space_bounds.max_x = INT64_MAX;
    config.space_bounds.min_y = INT64_MIN;
    config.space_bounds.max_y = INT64_MAX;
    kernel = minisnn_worlds_kernel_create(&config, &error);
    REQUIRE(kernel != NULL && error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    create_entities(kernel, 2U);
    place_entity(kernel, UINT64_C(1), INT64_MIN, INT64_C(0), UINT32_C(0));
    place_entity(kernel, UINT64_C(2), INT64_MAX, INT64_C(0), UINT32_C(0));
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(1)), entity_id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    event = last_event(kernel);
    REQUIRE(event.rejection ==
            MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_OFFSET_OVERFLOW);
    REQUIRE(minisnn_worlds_kernel_queue_remove_entity_from_space(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(1)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_remove_entity_from_space(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(1),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(2)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    place_entity(kernel, UINT64_C(1), INT64_C(0), INT64_MIN, UINT32_C(0));
    place_entity(kernel, UINT64_C(2), INT64_C(0), INT64_MAX, UINT32_C(0));
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), UINT32_C(0),
                entity_id(UINT64_C(0)), entity_id(UINT64_C(1)), entity_id(UINT64_C(2)),
                &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    event = last_event(kernel);
    REQUIRE(event.rejection ==
            MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_OFFSET_OVERFLOW);
    minisnn_worlds_kernel_destroy(kernel);
}
int main(void)
{
    test_public_contract_and_simple_link();
    test_submission_and_forest_rejections();
    test_lifecycle_move_and_remove();
    test_v5_and_atomicity();
    test_canonical_order_offsets_and_scale();
    test_offset_overflow_rejections();
    puts("K1-C1 spatial link, lifecycle, atomicity and V5 validation OK");
    return 0;
}