#include "minisnn_worlds_kernel.h"

#include <inttypes.h>
#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "K1-B1 occupancy test failed: %s at line %d\n", #condition, __LINE__); \
    return 1; } } while (0)

static MiniSNNWorldsKernelEntityId id(uint64_t value)
{
    MiniSNNWorldsKernelEntityId result = { value };
    return result;
}

static MiniSNNWorldsKernelTransform at(
    MiniSNNWorldsKernelScalar x,
    MiniSNNWorldsKernelScalar y)
{
    MiniSNNWorldsKernelTransform result;

    result.position.x = x;
    result.position.y = y;
    result.orientation = UINT32_C(0);
    return result;
}

static MiniSNNWorldsKernelOccupancy occupancy(
    MiniSNNWorldsKernelScalar half_x,
    MiniSNNWorldsKernelScalar half_y,
    uint32_t category_bits,
    uint32_t blocking_mask)
{
    MiniSNNWorldsKernelOccupancy result;

    result.half_extent_x = half_x;
    result.half_extent_y = half_y;
    result.category_bits = category_bits;
    result.blocking_mask = blocking_mask;
    return result;
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

static int queue_set(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick tick,
    uint64_t entity,
    MiniSNNWorldsKernelOccupancy value)
{
    MiniSNNWorldsKernelCommandId command;

    return minisnn_worlds_kernel_queue_set_occupancy(
        kernel, tick, 0U, id(0U), id(entity), value, &command) ==
        MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int queue_place(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick tick,
    uint64_t entity,
    MiniSNNWorldsKernelScalar x,
    MiniSNNWorldsKernelScalar y)
{
    MiniSNNWorldsKernelCommandId command;

    return minisnn_worlds_kernel_queue_place_entity(
        kernel, tick, 0U, id(0U), id(entity), at(x, y), &command) ==
        MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int test_validation_and_atomicity(void)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelCommandId command = { UINT64_C(99) };
    MiniSNNWorldsKernelOccupancy saved = occupancy(9, 8, 7U, 6U);
    MiniSNNWorldsKernelDiagnostics diagnostics;
    uint64_t before;
    uint64_t after;

    config.space_bounds.min_x = -10;
    config.space_bounds.min_y = -10;
    config.space_bounds.max_x = 10;
    config.space_bounds.max_y = 10;
    kernel = minisnn_worlds_kernel_create(&config, &error);
    CHECK(kernel != NULL && error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_queue_set_occupancy(
              kernel, 1U, 0U, id(0U), id(1U), occupancy(0, 1, 1U, 0U), &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_INVALID_OCCUPANCY);
    CHECK(command.value == UINT64_C(99));
    CHECK(minisnn_worlds_kernel_pending_command_count(kernel) == 0U);
    CHECK(minisnn_worlds_kernel_queue_set_occupancy(
              kernel, 1U, 0U, id(0U), id(1U), occupancy(1, 1, 0U, 0U), &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_INVALID_OCCUPANCY);
    CHECK(minisnn_worlds_kernel_queue_set_occupancy(
              kernel, 1U, 0U, id(0U), id(1U), occupancy(11, 1, 1U, 0U), &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_INVALID_OCCUPANCY);
    CHECK(create_entities(kernel, 1U));
    CHECK(queue_set(kernel, 2U, 1U, occupancy(1, 1, 1U, 0U)));
    CHECK(minisnn_worlds_kernel_state_hash(kernel, &before) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    minisnn_worlds_kernel_testing_fail_next_allocation();
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
    CHECK(minisnn_worlds_kernel_tick(kernel) == 1U);
    CHECK(minisnn_worlds_kernel_state_hash(kernel, &after) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(before == after);
    CHECK(minisnn_worlds_kernel_pending_command_count(kernel) == 1U);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_occupied_entity_count(kernel) == 1U);
    CHECK(minisnn_worlds_kernel_entity_occupancy(kernel, id(1U), &saved) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(saved.blocking_mask == 0U && saved.category_bits == 1U);
    CHECK(minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(diagnostics.entities_with_occupancy == 1U &&
          diagnostics.active_occupancies == 0U &&
          diagnostics.total_occupancies_set == 1U);
    minisnn_worlds_kernel_destroy(kernel);
    return 0;
}

static int test_conflicts_lifecycle_and_hash(void)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelEvent event;
    MiniSNNWorldsKernelOccupancy readback = occupancy(44, 55, 66U, 77U);
    MiniSNNWorldsKernelDiagnostics diagnostics;
    MiniSNNWorldsKernelEntityId entity;
    uint64_t v1 = UINT64_C(123);
    uint64_t v2 = UINT64_C(456);
    uint64_t v3a;
    uint64_t v3b;

    config.space_bounds.min_x = -10000;
    config.space_bounds.min_y = -10000;
    config.space_bounds.max_x = 10000;
    config.space_bounds.max_y = 10000;
    kernel = minisnn_worlds_kernel_create(&config, &error);
    CHECK(kernel != NULL);
    CHECK(create_entities(kernel, 5U));
    CHECK(queue_set(kernel, 2U, 1U, occupancy(1000, 1000, 1U, 2U)));
    CHECK(queue_set(kernel, 2U, 2U, occupancy(1000, 1000, 2U, 0U)));
    CHECK(queue_set(kernel, 2U, 3U, occupancy(1000, 1000, 4U, 0U)));
    CHECK(queue_set(kernel, 2U, 4U, occupancy(1000, 1000, 8U, 1U)));
    CHECK(queue_set(kernel, 2U, 5U, occupancy(1000, 1000, 16U, 1U)));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_occupied_entity_count(kernel) == 5U);
    CHECK(minisnn_worlds_kernel_active_occupancy_count(kernel) == 0U);
    CHECK(minisnn_worlds_kernel_occupied_entity_at(kernel, 0U, &entity, &readback) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(entity.value == 1U && readback.category_bits == 1U);
    CHECK(queue_place(kernel, 3U, 1U, 0, 0));
    CHECK(queue_place(kernel, 3U, 2U, 0, 0));
    CHECK(queue_place(kernel, 3U, 3U, 0, 0));
    CHECK(queue_place(kernel, 3U, 4U, 2000, 0));
    CHECK(queue_place(kernel, 3U, 5U, 9500, 0));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_placed_entity_count(kernel) == 3U);
    CHECK(minisnn_worlds_kernel_active_occupancy_count(kernel) == 3U);
    CHECK(minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(diagnostics.blocking_occupancies == 2U &&
          diagnostics.total_occupancy_conflicts_rejected == 1U);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 1U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED &&
          event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_CONFLICT &&
          event.related_entity.value == 1U);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 4U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_OUT_OF_BOUNDS);
    CHECK(minisnn_worlds_kernel_state_hash_versioned(
              kernel, MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V1, &v1) ==
          MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE && v1 == UINT64_C(123));
    CHECK(minisnn_worlds_kernel_state_hash_versioned(
              kernel, MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V2, &v2) ==
          MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE && v2 == UINT64_C(456));
    CHECK(minisnn_worlds_kernel_state_hash(kernel, &v3a) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(diagnostics.state_hash_version == MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V3);
    CHECK(minisnn_worlds_kernel_queue_clear_occupancy(
              kernel, 4U, 0U, id(0U), id(1U), &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(queue_place(kernel, 4U, 2U, 0, 0));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_entity_is_placed(kernel, id(2U)));
    CHECK(!minisnn_worlds_kernel_entity_has_occupancy(kernel, id(1U)));
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_OCCUPANCY_CLEARED &&
          event.has_occupancy && event.occupancy.category_bits == 1U);
    CHECK(minisnn_worlds_kernel_queue_remove_entity_from_space(
              kernel, 5U, 0U, id(0U), id(4U), &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_queue_destroy_entity(
              kernel, 5U, 0U, id(0U), id(3U), &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_entity_has_occupancy(kernel, id(4U)));
    CHECK(!minisnn_worlds_kernel_entity_has_occupancy(kernel, id(3U)));
    CHECK(minisnn_worlds_kernel_active_occupancy_count(kernel) == 1U);
    readback = occupancy(44, 55, 66U, 77U);
    CHECK(minisnn_worlds_kernel_entity_occupancy(kernel, id(1U), &readback) ==
          MINISNN_WORLDS_KERNEL_ERROR_ENTITY_HAS_NO_OCCUPANCY);
    CHECK(readback.half_extent_x == 44 && readback.half_extent_y == 55);
    CHECK(minisnn_worlds_kernel_state_hash(kernel, &v3b) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(v3a != v3b);
    minisnn_worlds_kernel_destroy(kernel);
    return 0;
}

static int test_canonical_set_conflicts_and_contacts(void)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelEvent event;

    config.space_bounds.min_x = -10000;
    config.space_bounds.min_y = -10000;
    config.space_bounds.max_x = 10000;
    config.space_bounds.max_y = 10000;
    kernel = minisnn_worlds_kernel_create(&config, &error);
    CHECK(kernel != NULL);
    CHECK(create_entities(kernel, 4U));
    CHECK(queue_place(kernel, 2U, 1U, 0, 0));
    CHECK(queue_place(kernel, 2U, 2U, 0, 0));
    CHECK(queue_place(kernel, 2U, 3U, 2000, 0));
    CHECK(queue_place(kernel, 2U, 4U, 0, 2000));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_placed_entity_count(kernel) == 4U);
    CHECK(queue_set(kernel, 3U, 1U, occupancy(1000, 1000, 1U, 2U)));
    CHECK(queue_set(kernel, 3U, 2U, occupancy(1000, 1000, 2U, 1U)));
    CHECK(queue_set(kernel, 3U, 3U, occupancy(1000, 1000, 4U, 0U)));
    CHECK(queue_set(kernel, 3U, 4U, occupancy(1000, 1000, 8U, 0U)));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_entity_has_occupancy(kernel, id(1U)));
    CHECK(!minisnn_worlds_kernel_entity_has_occupancy(kernel, id(2U)));
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 1U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_CONFLICT &&
          event.related_entity.value == 1U);
    CHECK(minisnn_worlds_kernel_queue_clear_occupancy(
              kernel, 4U, 0U, id(0U), id(1U), &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_queue_remove_entity_from_space(
              kernel, 4U, 1U, id(0U), id(1U), &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_queue_set_occupancy(
              kernel, 4U, 2U, id(0U), id(2U), occupancy(1000, 1000, 2U, 1U),
              &command) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_entity_has_occupancy(kernel, id(2U)));
    CHECK(minisnn_worlds_kernel_queue_place_entity(
              kernel, 5U, 0U, id(0U), id(1U), at(9000, 9000), &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_entity_is_placed(kernel, id(1U)));
    minisnn_worlds_kernel_destroy(kernel);
    return 0;
}
int main(void)
{
    CHECK(test_validation_and_atomicity() == 0);
    CHECK(test_conflicts_lifecycle_and_hash() == 0);
    CHECK(test_canonical_set_conflicts_and_contacts() == 0);
    puts("K1-B1 occupancy, AABB, masks, lifecycle and hash validation OK");
    return 0;
}
