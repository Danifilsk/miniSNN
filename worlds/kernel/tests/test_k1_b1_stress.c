#include "minisnn_worlds_kernel.h"

#include <stdio.h>

#define ENTITY_COUNT 20000U
#define OCCUPANCY_COUNT 15000U
#define PLACE_COUNT 10000U
#define CONFLICT_COUNT 1000U

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "K1-B1 stress failed: %s at line %d\n", #condition, __LINE__); \
    return 0; } } while (0)

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

static MiniSNNWorldsKernelOccupancy occupancy(uint32_t mask)
{
    MiniSNNWorldsKernelOccupancy result;

    result.half_extent_x = 1;
    result.half_extent_y = 1;
    result.category_bits = 1U;
    result.blocking_mask = mask;
    return result;
}

static int verify_conflict_events(const MiniSNNWorldsKernel *kernel)
{
    const size_t samples[] = { 0U, CONFLICT_COUNT / 2U, CONFLICT_COUNT - 1U };
    MiniSNNWorldsKernelEvent event;
    size_t index;

    CHECK(minisnn_worlds_kernel_last_tick_event_count(kernel) == CONFLICT_COUNT);
    for (index = 0U; index < sizeof(samples) / sizeof(samples[0]); ++index)
    {
        const size_t sample = samples[index];
        const MiniSNNWorldsKernelEntityId candidate =
            id((uint64_t)PLACE_COUNT + (uint64_t)sample + 1U);
        const MiniSNNWorldsKernelEntityId blocker =
            id(UINT64_C(1) + UINT64_C(2) * (uint64_t)sample);

        CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, sample, &event) ==
              MINISNN_WORLDS_KERNEL_ERROR_NONE);
        CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED);
        CHECK(event.rejection ==
              MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_CONFLICT);
        CHECK(event.subject.value == candidate.value);
        CHECK(event.related_entity.value == blocker.value);
        CHECK(!minisnn_worlds_kernel_entity_is_placed(kernel, candidate));
    }
    return 1;
}

static int run(uint64_t *out_hash, uint64_t *out_conflicts)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelDiagnostics diagnostics;
    size_t index;

    config.space_bounds.min_x = -50000;
    config.space_bounds.min_y = -10;
    config.space_bounds.max_x = 50000;
    config.space_bounds.max_y = 10;
    kernel = minisnn_worlds_kernel_create(&config, &error);
    CHECK(kernel != NULL);
    for (index = 0U; index < ENTITY_COUNT; ++index)
    {
        CHECK(minisnn_worlds_kernel_queue_create_entity(
                  kernel, 1U, 0U, id(0U), &command) ==
              MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    for (index = 0U; index < OCCUPANCY_COUNT; ++index)
    {
        CHECK(minisnn_worlds_kernel_queue_set_occupancy(
                  kernel, 2U, 0U, id(0U), id((uint64_t)index + 1U),
                  occupancy((index % 2U) == 0U ? 1U : 0U), &command) ==
              MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    for (index = 0U; index < PLACE_COUNT; ++index)
    {
        CHECK(minisnn_worlds_kernel_queue_place_entity(
                  kernel, 3U, 0U, id(0U), id((uint64_t)index + 1U),
                  at((MiniSNNWorldsKernelScalar)(-40000 + index * 4U), 0),
                  &command) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_active_occupancy_count(kernel) == PLACE_COUNT);

    /* Candidates overlap active blocking AABBs by positive area in canonical order. */
    for (index = 0U; index < CONFLICT_COUNT; ++index)
    {
        const size_t blocker_index = index * 2U;

        CHECK(minisnn_worlds_kernel_queue_place_entity(
                  kernel, 4U, 0U, id(0U),
                  id((uint64_t)PLACE_COUNT + (uint64_t)index + 1U),
                  at((MiniSNNWorldsKernelScalar)(-40000 + blocker_index * 4U), 0),
                  &command) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(verify_conflict_events(kernel));
    CHECK(minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(diagnostics.total_occupancy_conflicts_rejected == CONFLICT_COUNT);

    for (index = 0U; index < PLACE_COUNT; index += 3U)
    {
        CHECK(minisnn_worlds_kernel_queue_clear_occupancy(
                  kernel, 5U, 0U, id(0U), id((uint64_t)index + 1U), &command) ==
              MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    for (index = 1U; index < PLACE_COUNT; index += 4U)
    {
        CHECK(minisnn_worlds_kernel_queue_remove_entity_from_space(
                  kernel, 5U, 1U, id(0U), id((uint64_t)index + 1U), &command) ==
              MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    for (index = 2U; index < PLACE_COUNT; index += 5U)
    {
        CHECK(minisnn_worlds_kernel_queue_destroy_entity(
                  kernel, 6U, 0U, id(0U), id((uint64_t)index + 1U), &command) ==
              MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(diagnostics.entities_with_occupancy <= OCCUPANCY_COUNT);
    CHECK(diagnostics.active_occupancies <= diagnostics.entities_with_occupancy);
    CHECK(diagnostics.total_occupancy_conflicts_rejected == CONFLICT_COUNT);
    CHECK(minisnn_worlds_kernel_state_hash(kernel, out_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    *out_conflicts = diagnostics.total_occupancy_conflicts_rejected;
    minisnn_worlds_kernel_destroy(kernel);
    return 1;
}

int main(void)
{
    uint64_t first_hash;
    uint64_t second_hash;
    uint64_t first_conflicts;
    uint64_t second_conflicts;

    if (!run(&first_hash, &first_conflicts) ||
        !run(&second_hash, &second_conflicts) ||
        first_hash != second_hash || first_conflicts != second_conflicts)
    {
        return 1;
    }
    puts("K1-B1 occupancy deterministic stress validation OK");
    printf("entities=%u\n", ENTITY_COUNT);
    printf("occupancies=%u\n", OCCUPANCY_COUNT);
    printf("placements=%u\n", PLACE_COUNT);
    printf("occupancy_conflicts_rejected=%llu\n",
           (unsigned long long)first_conflicts);
    puts("repeat_match=yes");
    return 0;
}