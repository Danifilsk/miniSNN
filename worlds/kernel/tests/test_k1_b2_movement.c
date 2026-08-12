#include "minisnn_worlds_kernel.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "K1-B2 movement test failed: %s at line %d\n", #condition, __LINE__); \
    return 0; } } while (0)

static MiniSNNWorldsKernelEntityId id(uint64_t value)
{
    MiniSNNWorldsKernelEntityId result = { value };
    return result;
}

static MiniSNNWorldsKernelTransform at(
    MiniSNNWorldsKernelScalar x,
    MiniSNNWorldsKernelScalar y,
    MiniSNNWorldsKernelOrientation orientation)
{
    MiniSNNWorldsKernelTransform result;

    result.position.x = x;
    result.position.y = y;
    result.orientation = orientation;
    return result;
}

static MiniSNNWorldsKernelOccupancy occupancy(
    MiniSNNWorldsKernelScalar half_x,
    MiniSNNWorldsKernelScalar half_y,
    uint32_t category,
    uint32_t blocking_mask)
{
    MiniSNNWorldsKernelOccupancy result;

    result.half_extent_x = half_x;
    result.half_extent_y = half_y;
    result.category_bits = category;
    result.blocking_mask = blocking_mask;
    return result;
}

static MiniSNNWorldsKernel *create_kernel(
    MiniSNNWorldsKernelScalar min_x,
    MiniSNNWorldsKernelScalar min_y,
    MiniSNNWorldsKernelScalar max_x,
    MiniSNNWorldsKernelScalar max_y)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;

    config.space_bounds.min_x = min_x;
    config.space_bounds.min_y = min_y;
    config.space_bounds.max_x = max_x;
    config.space_bounds.max_y = max_y;
    return minisnn_worlds_kernel_create(&config, &error);
}

static int create_entities(MiniSNNWorldsKernel *kernel, size_t count)
{
    MiniSNNWorldsKernelCommandId command;
    size_t index;

    for (index = 0U; index < count; ++index)
    {
        if (minisnn_worlds_kernel_queue_create_entity(
                kernel, 1U, 0U, id(0U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return 0;
        }
    }
    return minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int queue_place(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick tick,
    uint32_t priority,
    uint64_t entity,
    MiniSNNWorldsKernelScalar x,
    MiniSNNWorldsKernelScalar y,
    MiniSNNWorldsKernelOrientation orientation)
{
    MiniSNNWorldsKernelCommandId command;

    return minisnn_worlds_kernel_queue_place_entity(
               kernel, tick, priority, id(0U), id(entity),
               at(x, y, orientation), &command) == MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int queue_move(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick tick,
    uint32_t priority,
    uint64_t entity,
    MiniSNNWorldsKernelScalar delta_x,
    MiniSNNWorldsKernelScalar delta_y)
{
    MiniSNNWorldsKernelCommandId command;

    return minisnn_worlds_kernel_queue_move_entity(
               kernel, tick, priority, id(0U), id(entity), delta_x, delta_y,
               &command) == MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int queue_set(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick tick,
    uint32_t priority,
    uint64_t entity,
    MiniSNNWorldsKernelOccupancy value)
{
    MiniSNNWorldsKernelCommandId command;

    return minisnn_worlds_kernel_queue_set_occupancy(
               kernel, tick, priority, id(0U), id(entity), value,
               &command) == MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int test_basic_motion_event_and_noop(void)
{
    MiniSNNWorldsKernel *kernel = create_kernel(-100, -100, 100, 100);
    MiniSNNWorldsKernelCommandInfo command;
    MiniSNNWorldsKernelEvent event;
    MiniSNNWorldsKernelTransform transform;
    MiniSNNWorldsKernelDiagnostics diagnostics;

    CHECK(kernel != NULL);
    CHECK(create_entities(kernel, 2U));
    CHECK(queue_place(kernel, 2U, 0U, 1U, 0, 0, 123U));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(queue_move(kernel, 3U, 0U, 1U, 10, -5));
    CHECK(queue_move(kernel, 3U, 1U, 2U, 1, 1));
    CHECK(minisnn_worlds_kernel_pending_command_at(kernel, 0U, &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(command.type == MINISNN_WORLDS_KERNEL_COMMAND_MOVE_ENTITY &&
          command.has_displacement && command.displacement.x == 10 &&
          command.displacement.y == -5);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_entity_transform(kernel, id(1U), &transform) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(transform.position.x == 10 && transform.position.y == -5 &&
          transform.orientation == 123U);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED &&
          event.subject.value == 1U && event.has_previous_transform &&
          event.previous_transform.position.x == 0 &&
          event.previous_transform.position.y == 0 &&
          event.previous_transform.orientation == 123U && event.has_transform &&
          event.transform.position.x == 10 && event.transform.position.y == -5 &&
          event.transform.orientation == 123U);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 1U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED &&
          event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_NOT_PLACED);
    CHECK(queue_move(kernel, 4U, 0U, 1U, 0, 0));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED &&
          event.has_previous_transform && event.has_transform &&
          event.previous_transform.position.x == event.transform.position.x &&
          event.previous_transform.position.y == event.transform.position.y &&
          event.previous_transform.orientation == event.transform.orientation);
    CHECK(minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(diagnostics.total_movement_commands_processed == 3U &&
          diagnostics.total_entities_moved == 1U &&
          diagnostics.total_movement_overflows_rejected == 0U &&
          diagnostics.state_hash_version == MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V4);
    minisnn_worlds_kernel_destroy(kernel);
    return 1;
}

static int test_occupancy_conflict_ordering_and_preservation(void)
{
    MiniSNNWorldsKernel *kernel = create_kernel(-100, -100, 100, 100);
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelEvent event;
    MiniSNNWorldsKernelTransform transform;
    MiniSNNWorldsKernelOccupancy before;
    MiniSNNWorldsKernelOccupancy after;
    MiniSNNWorldsKernelDiagnostics diagnostics;
    uint64_t v3 = UINT64_C(17);
    uint64_t v4a;
    uint64_t v4b;

    CHECK(kernel != NULL);
    CHECK(create_entities(kernel, 4U));
    CHECK(queue_set(kernel, 2U, 0U, 1U, occupancy(2, 2, 2U, 0U)));
    CHECK(queue_set(kernel, 2U, 0U, 2U, occupancy(2, 2, 2U, 0U)));
    CHECK(queue_set(kernel, 2U, 0U, 3U, occupancy(2, 2, 1U, 2U)));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(queue_place(kernel, 3U, 0U, 1U, 10, 0, 77U));
    CHECK(queue_place(kernel, 3U, 0U, 2U, 10, 0, 88U));
    CHECK(queue_place(kernel, 3U, 0U, 3U, 0, 0, 99U));
    CHECK(queue_place(kernel, 3U, 0U, 4U, -20, 0, 11U));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_entity_occupancy(kernel, id(3U), &before) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(queue_move(kernel, 4U, 0U, 3U, 10, 0));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_entity_transform(kernel, id(3U), &transform) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(transform.position.x == 0 && transform.position.y == 0 &&
          transform.orientation == 99U);
    CHECK(minisnn_worlds_kernel_entity_occupancy(kernel, id(3U), &after) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(after.half_extent_x == before.half_extent_x &&
          after.half_extent_y == before.half_extent_y &&
          after.category_bits == before.category_bits &&
          after.blocking_mask == before.blocking_mask);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED &&
          event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_CONFLICT &&
          event.related_entity.value == 1U && !event.has_previous_transform);
    CHECK(minisnn_worlds_kernel_state_hash_versioned(
              kernel, MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V3, &v3) ==
          MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE && v3 == UINT64_C(17));
    CHECK(minisnn_worlds_kernel_state_hash(kernel, &v4a) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(queue_move(kernel, 5U, 1U, 3U, 10, 0));
    CHECK(minisnn_worlds_kernel_queue_clear_occupancy(
              kernel, 5U, 0U, id(0U), id(1U), &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 1U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED &&
          event.related_entity.value == 2U);
    CHECK(minisnn_worlds_kernel_queue_clear_occupancy(
              kernel, 6U, 0U, id(0U), id(2U), &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(queue_move(kernel, 6U, 1U, 3U, 10, 0));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_entity_transform(kernel, id(3U), &transform) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(transform.position.x == 10 && transform.orientation == 99U);
    CHECK(minisnn_worlds_kernel_state_hash(kernel, &v4b) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(v4a != v4b);
    CHECK(minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(diagnostics.total_occupancy_conflicts_rejected == 2U &&
          diagnostics.total_entities_moved == 1U);
    minisnn_worlds_kernel_destroy(kernel);
    return 1;
}

static int test_planned_ordering_and_global_atomicity(void)
{
    MiniSNNWorldsKernel *kernel = create_kernel(-100, -100, 100, 100);
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelTransform transform;
    uint64_t before;
    uint64_t after;

    CHECK(kernel != NULL);
    CHECK(create_entities(kernel, 2U));
    CHECK(queue_place(kernel, 2U, 0U, 1U, 0, 0, 42U));
    CHECK(queue_place(kernel, 3U, 0U, 2U, -10, 0, 7U));
    CHECK(queue_move(kernel, 3U, 1U, 2U, 5, 3));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_entity_transform(kernel, id(2U), &transform) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(transform.position.x == -5 && transform.position.y == 3 &&
          transform.orientation == 7U);
    CHECK(queue_move(kernel, 4U, 0U, 1U, 1, 1));
    CHECK(minisnn_worlds_kernel_queue_remove_entity_from_space(
              kernel, 4U, 1U, id(0U), id(1U), &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(!minisnn_worlds_kernel_entity_is_placed(kernel, id(1U)));
    CHECK(queue_move(kernel, 5U, 1U, 1U, 1, 1));
    CHECK(minisnn_worlds_kernel_queue_remove_entity_from_space(
              kernel, 5U, 0U, id(0U), id(1U), &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_last_tick_event_count(kernel) == 2U);
    CHECK(queue_move(kernel, 6U, 0U, 2U, 1, 1));
    CHECK(minisnn_worlds_kernel_state_hash(kernel, &before) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    minisnn_worlds_kernel_testing_fail_next_allocation();
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
    CHECK(minisnn_worlds_kernel_tick(kernel) == 5U);
    CHECK(minisnn_worlds_kernel_state_hash(kernel, &after) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(before == after && minisnn_worlds_kernel_pending_command_count(kernel) == 1U);
    CHECK(minisnn_worlds_kernel_entity_transform(kernel, id(2U), &transform) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(transform.position.x == -5 && transform.position.y == 3);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_entity_transform(kernel, id(2U), &transform) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(transform.position.x == -4 && transform.position.y == 4);
    minisnn_worlds_kernel_destroy(kernel);
    return 1;
}

static int test_overflow_and_destination_aabb(void)
{
    MiniSNNWorldsKernel *kernel = create_kernel(INT64_MIN, -10, INT64_MAX, 10);
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelEvent event;
    MiniSNNWorldsKernelTransform transform;
    MiniSNNWorldsKernelDiagnostics diagnostics;

    CHECK(kernel != NULL);
    CHECK(create_entities(kernel, 2U));
    CHECK(queue_place(kernel, 2U, 0U, 1U, INT64_MAX, 0, 2U));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(queue_move(kernel, 3U, 0U, 1U, 1, 0));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_DESTINATION_OVERFLOW);
    CHECK(minisnn_worlds_kernel_entity_transform(kernel, id(1U), &transform) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE && transform.position.x == INT64_MAX);
    CHECK(queue_set(kernel, 4U, 0U, 2U, occupancy(1, 1, 1U, 0U)));
    CHECK(queue_place(kernel, 5U, 0U, 2U, INT64_MAX - 1, 0, 3U));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(queue_move(kernel, 6U, 0U, 2U, 1, 0));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_DESTINATION_OVERFLOW);
    CHECK(minisnn_worlds_kernel_entity_transform(kernel, id(2U), &transform) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE && transform.position.x == INT64_MAX - 1);
    CHECK(minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(diagnostics.total_movement_overflows_rejected == 2U);
    CHECK(minisnn_worlds_kernel_queue_destroy_entity(
              kernel, 7U, 0U, id(0U), id(1U), &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    minisnn_worlds_kernel_destroy(kernel);
    return 1;
}

static int test_submission_bounds_and_same_tick_occupancy(void)
{
    MiniSNNWorldsKernel *kernel = create_kernel(-10, -10, 10, 10);
    MiniSNNWorldsKernelCommandId command = { UINT64_C(99) };
    MiniSNNWorldsKernelEvent event;
    MiniSNNWorldsKernelTransform transform;

    CHECK(kernel != NULL);
    CHECK(minisnn_worlds_kernel_queue_move_entity(
              kernel, 0U, 0U, id(0U), id(1U), 1, 0, &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_INVALID_TICK);
    CHECK(command.value == UINT64_C(99));
    CHECK(minisnn_worlds_kernel_queue_move_entity(
              kernel, 1U, 0U, id(0U), id(0U), 1, 0, &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_INVALID_ENTITY_ID);
    CHECK(command.value == UINT64_C(99));
    CHECK(create_entities(kernel, 2U));

    CHECK(queue_place(kernel, 2U, 0U, 1U, 0, 0, 41U));
    CHECK(queue_set(kernel, 2U, 1U, 2U, occupancy(2, 2, 1U, 0U)));
    CHECK(queue_place(kernel, 2U, 2U, 2U, 7, 0, 42U));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);

    CHECK(queue_set(kernel, 3U, 0U, 1U, occupancy(1, 1, 1U, 0U)));
    CHECK(queue_move(kernel, 3U, 1U, 1U, 1, 0));
    CHECK(queue_move(kernel, 3U, 2U, 2U, 1, 0));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_entity_transform(kernel, id(1U), &transform) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(transform.position.x == 1 && transform.orientation == 41U);
    CHECK(minisnn_worlds_kernel_entity_transform(kernel, id(2U), &transform) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(transform.position.x == 8 && transform.orientation == 42U);

    CHECK(queue_move(kernel, 4U, 0U, 1U, 10, 0));
    CHECK(queue_move(kernel, 4U, 1U, 2U, 1, 0));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED &&
          event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_POSITION_OUT_OF_BOUNDS);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 1U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED &&
          event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_OUT_OF_BOUNDS);
    CHECK(minisnn_worlds_kernel_entity_transform(kernel, id(1U), &transform) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE && transform.position.x == 1);
    CHECK(minisnn_worlds_kernel_entity_transform(kernel, id(2U), &transform) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE && transform.position.x == 8);
    minisnn_worlds_kernel_destroy(kernel);
    return 1;
}

static int test_negative_and_y_overflow(void)
{
    MiniSNNWorldsKernel *kernel = create_kernel(INT64_MIN, INT64_MIN,
                                                INT64_MAX, INT64_MAX);
    MiniSNNWorldsKernelEvent event;
    MiniSNNWorldsKernelDiagnostics diagnostics;

    CHECK(kernel != NULL);
    CHECK(create_entities(kernel, 2U));
    CHECK(queue_place(kernel, 2U, 0U, 1U, INT64_MIN, 0, 0U));
    CHECK(queue_place(kernel, 2U, 0U, 2U, 0, INT64_MAX, 0U));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(queue_move(kernel, 3U, 0U, 1U, -1, 0));
    CHECK(queue_move(kernel, 3U, 1U, 2U, 0, 1));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_DESTINATION_OVERFLOW);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 1U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_DESTINATION_OVERFLOW);
    CHECK(minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(diagnostics.total_movement_overflows_rejected == 2U);
    minisnn_worlds_kernel_destroy(kernel);
    return 1;
}
static int run_deterministic_motion(uint64_t *out_hash)
{
    MiniSNNWorldsKernel *kernel = create_kernel(-100, -100, 100, 100);

    CHECK(kernel != NULL);
    CHECK(create_entities(kernel, 1U));
    CHECK(queue_place(kernel, 2U, 0U, 1U, -10, 4, 91U));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(queue_move(kernel, 3U, 0U, 1U, 7, -2));
    CHECK(queue_move(kernel, 4U, 0U, 1U, -3, 9));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_state_hash(kernel, out_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    minisnn_worlds_kernel_destroy(kernel);
    return 1;
}

int main(void)
{
    uint64_t first_hash;
    uint64_t second_hash;

    CHECK(test_basic_motion_event_and_noop());
    CHECK(test_occupancy_conflict_ordering_and_preservation());
    CHECK(test_planned_ordering_and_global_atomicity());
    CHECK(test_overflow_and_destination_aabb());
    CHECK(test_submission_bounds_and_same_tick_occupancy());
    CHECK(test_negative_and_y_overflow());
    CHECK(run_deterministic_motion(&first_hash));
    CHECK(run_deterministic_motion(&second_hash));
    CHECK(first_hash == second_hash);
    puts("K1-B2 atomic movement, occupancy, overflow, ordering and hash validation OK");
    printf("deterministic_hash=0x%016" PRIX64 "\n", first_hash);
    return 0;
}