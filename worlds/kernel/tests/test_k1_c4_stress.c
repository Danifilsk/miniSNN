#include "minisnn_worlds_kernel.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#ifndef K1_C4_STRESS_ENTITY_COUNT
#if defined(MINISNN_K1_C4_SANITIZER_STRESS)
#define K1_C4_STRESS_ENTITY_COUNT 256U
#else
#define K1_C4_STRESS_ENTITY_COUNT 2048U
#endif
#endif

#ifndef K1_C4_STRESS_WIDE_CHILD_COUNT
#if defined(MINISNN_K1_C4_SANITIZER_STRESS)
#define K1_C4_STRESS_WIDE_CHILD_COUNT 32U
#else
#define K1_C4_STRESS_WIDE_CHILD_COUNT 64U
#endif
#endif

#ifndef K1_C4_STRESS_DEEP_TREE_COUNT
#if defined(MINISNN_K1_C4_SANITIZER_STRESS)
#define K1_C4_STRESS_DEEP_TREE_COUNT 6U
#else
#define K1_C4_STRESS_DEEP_TREE_COUNT 12U
#endif
#endif

#ifndef K1_C4_STRESS_DEEP_TREE_DEPTH
#if defined(MINISNN_K1_C4_SANITIZER_STRESS)
#define K1_C4_STRESS_DEEP_TREE_DEPTH 16U
#else
#define K1_C4_STRESS_DEEP_TREE_DEPTH 80U
#endif
#endif

#define ENTITY_COUNT K1_C4_STRESS_ENTITY_COUNT
#define WIDE_CHILD_COUNT K1_C4_STRESS_WIDE_CHILD_COUNT
#define DEEP_TREE_COUNT K1_C4_STRESS_DEEP_TREE_COUNT
#define DEEP_TREE_DEPTH K1_C4_STRESS_DEEP_TREE_DEPTH
#define LINK_COUNT (WIDE_CHILD_COUNT + DEEP_TREE_COUNT * DEEP_TREE_DEPTH)
#define STRESS_TICKS 64U
#define FIRST_DEEP_ROOT_ENTITY_ID (UINT64_C(2) + (uint64_t)WIDE_CHILD_COUNT)
#define CHURN_TARGET_ENTITY_ID \
    (FIRST_DEEP_ROOT_ENTITY_ID + \
     (uint64_t)DEEP_TREE_COUNT * ((uint64_t)DEEP_TREE_DEPTH + UINT64_C(1)))
#define REPLACEMENT_ENTITY_ID ((uint64_t)ENTITY_COUNT + UINT64_C(1))

#if defined(MINISNN_K1_C4_SANITIZER_STRESS)
#define BLOCKER_ENTITY_ID ((uint64_t)ENTITY_COUNT - UINT64_C(16))
#define DESTROYABLE_ENTITY_ID ((uint64_t)ENTITY_COUNT - UINT64_C(8))
#else
#define BLOCKER_ENTITY_ID UINT64_C(1800)
#define DESTROYABLE_ENTITY_ID UINT64_C(2000)
#endif

typedef struct
{
    uint64_t hash;
    MiniSNNWorldsKernelDiagnostics diagnostics;
    uint64_t peak_active_spatial_links;
} StressResult;

static MiniSNNWorldsKernelEntityId id(uint64_t value)
{
    MiniSNNWorldsKernelEntityId result = { value };
    return result;
}

static MiniSNNWorldsKernelTransform at(int64_t x, int64_t y)
{
    MiniSNNWorldsKernelTransform result;

    result.position.x = x;
    result.position.y = y;
    result.orientation = 0U;
    return result;
}

static int step_checked(
    MiniSNNWorldsKernel *kernel,
    StressResult *result,
    int validate_invariants)
{
    MiniSNNWorldsKernelDiagnostics diagnostics;

    if (minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    if (diagnostics.active_spatial_links > result->peak_active_spatial_links)
    {
        result->peak_active_spatial_links = diagnostics.active_spatial_links;
    }
    return validate_invariants == 0 ||
           minisnn_worlds_kernel_testing_validate_invariants(kernel) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int run_stress(StressResult *out_result)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelOccupancy occupancy;
    uint64_t entity;
    uint64_t root;
    uint64_t previous;
    uint64_t next = FIRST_DEEP_ROOT_ENTITY_ID;
    unsigned int tree;
    unsigned int depth;

    memset(out_result, 0, sizeof(*out_result));
    config.master_seed = UINT64_C(0xC4C4C4C4);
    config.space_bounds.min_x = INT64_C(-200000000);
    config.space_bounds.min_y = INT64_C(-200000000);
    config.space_bounds.max_x = INT64_C(200000000);
    config.space_bounds.max_y = INT64_C(200000000);
    kernel = minisnn_worlds_kernel_create(&config, &error);
    if (kernel == NULL)
    {
        return 0;
    }

    for (entity = 1U; entity <= ENTITY_COUNT; ++entity)
    {
        if (minisnn_worlds_kernel_queue_create_entity(
                kernel, 1U, 0U, id(0U), &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
    }
    if (!step_checked(kernel, out_result, 0))
    {
        goto fail;
    }

    for (entity = 1U; entity <= CHURN_TARGET_ENTITY_ID; ++entity)
    {
        if (minisnn_worlds_kernel_queue_place_entity(
                kernel, 2U, 0U, id(0U), id(entity),
                at((int64_t)entity * INT64_C(10000),
                   (int64_t)(entity % 17U) * INT64_C(10000)),
                &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
    }
    if (minisnn_worlds_kernel_queue_place_entity(
            kernel, 2U, 0U, id(0U), id(BLOCKER_ENTITY_ID),
            at(INT64_C(15000), INT64_C(10000)), &command_id) !=
        MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        goto fail;
    }
    if (!step_checked(kernel, out_result, 0))
    {
        goto fail;
    }

    occupancy.half_extent_x = INT64_C(7000);
    occupancy.half_extent_y = INT64_C(7000);
    occupancy.category_bits = UINT32_C(1);
    occupancy.blocking_mask = UINT32_C(6);
    if (minisnn_worlds_kernel_queue_set_occupancy(
            kernel, 3U, 0U, id(0U), id(1U), occupancy, &command_id) !=
        MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        goto fail;
    }
    occupancy.category_bits = UINT32_C(2);
    occupancy.blocking_mask = UINT32_C(1);
    if (minisnn_worlds_kernel_queue_set_occupancy(
            kernel, 3U, 0U, id(0U), id(2U), occupancy, &command_id) !=
        MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        goto fail;
    }

    for (entity = 2U; entity < FIRST_DEEP_ROOT_ENTITY_ID; ++entity)
    {
        if (minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, 3U, 0U, id(0U), id(1U), id(entity),
                &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
    }
    for (tree = 0U; tree < DEEP_TREE_COUNT; ++tree)
    {
        root = next++;
        previous = root;
        for (depth = 0U; depth < DEEP_TREE_DEPTH; ++depth)
        {
            if (minisnn_worlds_kernel_queue_create_spatial_link(
                    kernel, 3U, 0U, id(0U), id(previous), id(next),
                    &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
            {
                goto fail;
            }
            previous = next++;
        }
    }
    if (next != CHURN_TARGET_ENTITY_ID || !step_checked(kernel, out_result, 0) ||
        minisnn_worlds_kernel_spatial_link_count(kernel) != LINK_COUNT)
    {
        goto fail;
    }

    if (minisnn_worlds_kernel_queue_move_entity(
            kernel, 4U, 0U, id(0U), id(1U), 17, -11, &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_move_entity(
            kernel, 4U, 1U, id(0U), id(FIRST_DEEP_ROOT_ENTITY_ID), -13, 7, &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_move_entity(
            kernel, 4U, 2U, id(0U), id(2U), 1, 0, &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !step_checked(kernel, out_result, 0))
    {
        goto fail;
    }

    occupancy.half_extent_x = INT64_C(1000);
    occupancy.half_extent_y = INT64_C(1000);
    occupancy.category_bits = UINT32_C(4);
    occupancy.blocking_mask = UINT32_C(1);
    for (entity = 5U; entity <= STRESS_TICKS; ++entity)
    {
        MiniSNNWorldsKernelEntityId moving_root =
            id(entity % 2U == 0U ? 1U : 66U);

        if (minisnn_worlds_kernel_queue_move_entity(
                kernel, entity, 0U, id(0U), moving_root,
                entity % 3U == 0U ? -3 : 5,
                entity % 5U == 0U ? 0 : 2,
                &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
        if (entity == 6U &&
            minisnn_worlds_kernel_queue_move_entity(
                kernel, entity, 3U, id(0U), id(1U), 0, 0,
                &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
        if (entity == 10U &&
            minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, entity, 1U, id(0U), id(1U), id(BLOCKER_ENTITY_ID),
                &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
        if (entity == 11U &&
            minisnn_worlds_kernel_queue_remove_spatial_link(
                kernel, entity, 1U, id(0U), id(1U), id(BLOCKER_ENTITY_ID),
                &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
        if (entity == 12U &&
            minisnn_worlds_kernel_queue_set_occupancy(
                kernel, entity, 1U, id(0U), id(BLOCKER_ENTITY_ID), occupancy,
                &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
        if (entity == 15U &&
            minisnn_worlds_kernel_queue_move_entity(
                kernel, entity, 3U, id(0U), id(FIRST_DEEP_ROOT_ENTITY_ID), INT64_MAX, 0,
                &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
        if (entity == 14U &&
            minisnn_worlds_kernel_queue_destroy_entity(
                kernel, entity, 1U, id(0U), id(DESTROYABLE_ENTITY_ID),
                &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
        if (entity == 15U &&
            minisnn_worlds_kernel_queue_create_entity(
                kernel, entity, 1U, id(0U), &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
        if (entity == 16U &&
            minisnn_worlds_kernel_queue_place_entity(
                kernel, entity, 1U, id(0U), id(REPLACEMENT_ENTITY_ID),
                at(INT64_C(150000000), 0), &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
        if (entity == 16U &&
            minisnn_worlds_kernel_queue_clear_occupancy(
                kernel, entity, 2U, id(0U), id(BLOCKER_ENTITY_ID),
                &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
        if (entity == 17U &&
            minisnn_worlds_kernel_queue_destroy_entity(
                kernel, entity, 1U, id(0U), id(REPLACEMENT_ENTITY_ID),
                &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
        if (entity == 18U &&
            minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, entity, 1U, id(0U), id(1U), id(BLOCKER_ENTITY_ID),
                &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
        if (entity == 19U &&
            minisnn_worlds_kernel_queue_remove_spatial_link(
                kernel, entity, 1U, id(0U), id(1U), id(BLOCKER_ENTITY_ID),
                &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
        if (entity == 20U &&
            minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, entity, 1U, id(0U), id(FIRST_DEEP_ROOT_ENTITY_ID), id(CHURN_TARGET_ENTITY_ID),
                &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
        if (entity == 21U &&
            minisnn_worlds_kernel_queue_destroy_entity(
                kernel, entity, 1U, id(0U), id(CHURN_TARGET_ENTITY_ID),
                &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
        if (entity == 22U &&
            minisnn_worlds_kernel_queue_remove_spatial_link(
                kernel, entity, 1U, id(0U), id(FIRST_DEEP_ROOT_ENTITY_ID), id(CHURN_TARGET_ENTITY_ID),
                &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
        if (entity == 23U &&
            minisnn_worlds_kernel_queue_destroy_entity(
                kernel, entity, 1U, id(0U), id(CHURN_TARGET_ENTITY_ID),
                &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto fail;
        }
        if (!step_checked(
                kernel, out_result,
                entity == 32U || entity == STRESS_TICKS))
        {
            goto fail;
        }
    }

    if (minisnn_worlds_kernel_testing_validate_invariants(kernel) !=
        MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        goto fail;
    }
    if (minisnn_worlds_kernel_get_diagnostics(
            kernel, &out_result->diagnostics) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(kernel, &out_result->hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        out_result->diagnostics.total_spatial_links_created < LINK_COUNT ||
        out_result->diagnostics.total_spatial_links_removed < UINT64_C(3) ||
        out_result->diagnostics.total_commands_rejected == 0U ||
        out_result->diagnostics.total_occupancy_conflicts_rejected == 0U ||
        out_result->diagnostics.total_movement_overflows_rejected == 0U ||
        out_result->peak_active_spatial_links <= LINK_COUNT)
    {
        goto fail;
    }

    minisnn_worlds_kernel_destroy(kernel);
    return 1;

fail:
    minisnn_worlds_kernel_destroy(kernel);
    return 0;
}

int main(void)
{
    StressResult first;
    StressResult second;

    if (!run_stress(&first) || !run_stress(&second) ||
        first.hash != second.hash ||
        first.peak_active_spatial_links != second.peak_active_spatial_links ||
        memcmp(&first.diagnostics, &second.diagnostics,
               sizeof(first.diagnostics)) != 0)
    {
        fprintf(stderr, "K1-C4 stress FAILED\n");
        return 1;
    }

    printf("K1-C4 stress OK: entities=%" PRIu64 " links=%" PRIu64
           " peak_links=%" PRIu64 " ticks=%" PRIu64 " moved=%" PRIu64
           " rejected=%" PRIu64 " conflicts=%" PRIu64 " overflows=%" PRIu64
           " links_created=%" PRIu64 " links_removed=%" PRIu64
           " events=%" PRIu64 " hash=0x%016" PRIX64 "\n",
        first.diagnostics.alive_entities,
        first.diagnostics.active_spatial_links,
        first.peak_active_spatial_links,
        first.diagnostics.completed_ticks,
        first.diagnostics.total_entities_moved,
        first.diagnostics.total_commands_rejected,
        first.diagnostics.total_occupancy_conflicts_rejected,
        first.diagnostics.total_movement_overflows_rejected,
        first.diagnostics.total_spatial_links_created,
        first.diagnostics.total_spatial_links_removed,
        first.diagnostics.total_events_emitted,
        first.hash);
    return 0;
}