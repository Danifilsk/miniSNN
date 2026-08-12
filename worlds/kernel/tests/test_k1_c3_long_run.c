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

#define LONG_RUN_TICKS 1000U
#define ROOT_COUNT 16U

typedef struct
{
    uint64_t final_hash;
    uint64_t event_signature;
    MiniSNNWorldsKernelDiagnostics diagnostics;
} LongRunResult;

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

static MiniSNNWorldsKernelOccupancy passive_occupancy(void)
{
    MiniSNNWorldsKernelOccupancy result;
    memset(&result, 0, sizeof(result));
    result.half_extent_x = INT64_C(1);
    result.half_extent_y = INT64_C(1);
    result.category_bits = UINT32_C(1);
    result.blocking_mask = UINT32_C(0);
    return result;
}

static uint64_t fnv_append(uint64_t hash, uint64_t value)
{
    size_t byte_index;
    for (byte_index = 0U; byte_index < 8U; ++byte_index)
    {
        hash ^= (value >> (byte_index * 8U)) & UINT64_C(0xff);
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static void step_ok(MiniSNNWorldsKernel *kernel, uint64_t *signature)
{
    size_t index;
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_testing_validate_invariants(kernel) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    for (index = 0U; index < minisnn_worlds_kernel_last_tick_event_count(kernel); ++index)
    {
        MiniSNNWorldsKernelEvent event;
        REQUIRE(minisnn_worlds_kernel_last_tick_event_at(kernel, index, &event) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
        *signature = fnv_append(*signature, event.event_id.value);
        *signature = fnv_append(*signature, (uint64_t)event.type);
        *signature = fnv_append(*signature, event.subject.value);
        *signature = fnv_append(*signature, event.related_entity.value);
        *signature = fnv_append(*signature, (uint64_t)event.rejection);
    }
}

static void queue_initial_forest(MiniSNNWorldsKernel *kernel, uint64_t *signature)
{
    MiniSNNWorldsKernelCommandId command_id;
    size_t index;
    MiniSNNWorldsTick tick;

    for (index = 0U; index < ROOT_COUNT * 2U; ++index)
    {
        REQUIRE(minisnn_worlds_kernel_queue_create_entity(
                    kernel, UINT64_C(1), (uint32_t)index, id(UINT64_C(0)), &command_id) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    step_ok(kernel, signature);
    for (index = 0U; index < ROOT_COUNT * 2U; ++index)
    {
        REQUIRE(minisnn_worlds_kernel_queue_place_entity(
                    kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), (uint32_t)index,
                    id(UINT64_C(0)), id((uint64_t)index + UINT64_C(1)),
                    transform((int64_t)index * INT64_C(20)), &command_id) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    step_ok(kernel, signature);
    tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    for (index = 0U; index < ROOT_COUNT; ++index)
    {
        uint64_t root = (uint64_t)(index * 2U) + UINT64_C(1);
        REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                    kernel, tick, (uint32_t)index, id(UINT64_C(0)), id(root), id(root + UINT64_C(1)),
                    &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    step_ok(kernel, signature);
}

static LongRunResult run_workload(void)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelCommandId command_id;
    LongRunResult result;
    uint64_t signature = UINT64_C(14695981039346656037);
    size_t tick_index;

    config.master_seed = UINT64_C(0x6b314333);
    kernel = minisnn_worlds_kernel_create(&config, &error);
    REQUIRE(kernel != NULL);
    REQUIRE(error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    queue_initial_forest(kernel, &signature);
    for (tick_index = 0U; tick_index < LONG_RUN_TICKS; ++tick_index)
    {
        MiniSNNWorldsTick tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
        uint64_t root = (uint64_t)((tick_index % ROOT_COUNT) * 2U) + UINT64_C(1);
        uint64_t child = root + UINT64_C(1);

        if (tick_index == 0U)
        {
            REQUIRE(minisnn_worlds_kernel_queue_create_entity(
                        kernel, tick, UINT32_C(0), id(UINT64_C(0)), &command_id) ==
                    MINISNN_WORLDS_KERNEL_ERROR_NONE);
        }
        else if (tick_index == 1U)
        {
            REQUIRE(minisnn_worlds_kernel_queue_place_entity(
                        kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(33)),
                        transform(INT64_C(10000)), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
        }
        else if (tick_index == 2U)
        {
            REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                        kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(33)),
                        &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
        }
        else if (tick_index == 3U)
        {
            REQUIRE(minisnn_worlds_kernel_queue_remove_spatial_link(
                        kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)), id(UINT64_C(33)),
                        &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
        }
        else if (tick_index == 4U)
        {
            REQUIRE(minisnn_worlds_kernel_queue_destroy_entity(
                        kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(33)), &command_id) ==
                    MINISNN_WORLDS_KERNEL_ERROR_NONE);
        }
        else if ((tick_index % 4U) == 0U)
        {
            REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                        kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(root), INT64_C(1), INT64_C(0),
                        &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
        }
        else if ((tick_index % 4U) == 1U)
        {
            REQUIRE(minisnn_worlds_kernel_queue_remove_spatial_link(
                        kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(root), id(child), &command_id) ==
                    MINISNN_WORLDS_KERNEL_ERROR_NONE);
            REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                        kernel, tick, UINT32_C(1), id(UINT64_C(0)), id(child), INT64_C(1), INT64_C(0),
                        &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
        }
        else if ((tick_index % 4U) == 2U)
        {
            REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                        kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(root), id(child), &command_id) ==
                    MINISNN_WORLDS_KERNEL_ERROR_NONE);
        }
        else
        {
            REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                        kernel, tick, UINT32_C(0), id(UINT64_C(0)), id(root), INT64_C(-1), INT64_C(0),
                        &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
        }
        if ((tick_index % 10U) == 5U)
        {
            REQUIRE(minisnn_worlds_kernel_queue_set_occupancy(
                        kernel, tick, UINT32_C(3), id(UINT64_C(0)), id(root), passive_occupancy(),
                        &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
        }
        else if ((tick_index % 10U) == 6U)
        {
            REQUIRE(minisnn_worlds_kernel_queue_clear_occupancy(
                        kernel, tick, UINT32_C(3), id(UINT64_C(0)), id(root), &command_id) ==
                    MINISNN_WORLDS_KERNEL_ERROR_NONE);
        }
        step_ok(kernel, &signature);
    }
    REQUIRE(minisnn_worlds_kernel_state_hash(kernel, &result.final_hash) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_get_diagnostics(kernel, &result.diagnostics) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    result.event_signature = signature;
    minisnn_worlds_kernel_destroy(kernel);
    return result;
}

int main(void)
{
    LongRunResult first = run_workload();
    LongRunResult second = run_workload();

    REQUIRE(first.final_hash == second.final_hash);
    REQUIRE(first.event_signature == second.event_signature);
    REQUIRE(first.diagnostics.total_commands_applied == second.diagnostics.total_commands_applied);
    REQUIRE(first.diagnostics.total_commands_rejected == second.diagnostics.total_commands_rejected);
    REQUIRE(first.diagnostics.total_events_emitted == second.diagnostics.total_events_emitted);
    REQUIRE(first.diagnostics.active_spatial_links == second.diagnostics.active_spatial_links);
    REQUIRE(first.diagnostics.total_spatial_links_created == second.diagnostics.total_spatial_links_created);
    REQUIRE(first.diagnostics.total_spatial_links_removed == second.diagnostics.total_spatial_links_removed);
    printf("K1-C3 long run deterministic OK hash=%" PRIu64 " events=%" PRIu64 "\n",
           first.final_hash, first.diagnostics.total_events_emitted);
    return 0;
}