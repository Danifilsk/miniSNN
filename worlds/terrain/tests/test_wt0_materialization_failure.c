#include <stdint.h>
#include <stdio.h>

#include "minisnn_worlds_terrain.h"

#define CHECK(condition) \
    do { if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        goto done; \
    } } while (0)

typedef struct
{
    uint64_t terrain_hash;
    uint64_t kernel_hash;
    MiniSNNWorldsTick tick;
    size_t entities;
    size_t placed;
    size_t occupied;
    size_t active_occupancies;
    uint64_t pending_commands;
} MaterializationState;

static MiniSNNWorldsTerrain *make_terrain(void)
{
    MiniSNNWorldsTerrainConfig config = { 3U, 3U, 1000, -500, -500 };
    MiniSNNWorldsTerrainError error;
    MiniSNNWorldsTerrain *terrain = minisnn_worlds_terrain_create(&config, &error);

    if (terrain == NULL || error != MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        minisnn_worlds_terrain_set_tile(
            terrain, 0U, 2U, MINISNN_WORLDS_TERRAIN_TILE_LAND) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        minisnn_worlds_terrain_add_rock(terrain, 1U, 1U) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        minisnn_worlds_terrain_finalize(terrain) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE)
    {
        minisnn_worlds_terrain_destroy(terrain);
        return NULL;
    }
    return terrain;
}

static MiniSNNWorldsKernel *make_kernel(const MiniSNNWorldsTerrain *terrain)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelSpaceBounds bounds;
    MiniSNNWorldsKernelError error;

    if (minisnn_worlds_terrain_space_bounds(terrain, &bounds) !=
        MINISNN_WORLDS_TERRAIN_ERROR_NONE)
    {
        return NULL;
    }
    config.space_bounds = bounds;
    return minisnn_worlds_kernel_create(&config, &error);
}

static int capture_state(const MiniSNNWorldsTerrain *terrain,
                         const MiniSNNWorldsKernel *kernel,
                         MaterializationState *out_state)
{
    MiniSNNWorldsKernelDiagnostics diagnostics;

    if (terrain == NULL || kernel == NULL || out_state == NULL ||
        minisnn_worlds_terrain_hash(terrain, &out_state->terrain_hash) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(kernel, &out_state->kernel_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    out_state->tick = minisnn_worlds_kernel_tick(kernel);
    out_state->entities = minisnn_worlds_kernel_entity_count(kernel);
    out_state->placed = minisnn_worlds_kernel_placed_entity_count(kernel);
    out_state->occupied = minisnn_worlds_kernel_occupied_entity_count(kernel);
    out_state->active_occupancies =
        minisnn_worlds_kernel_active_occupancy_count(kernel);
    out_state->pending_commands = diagnostics.pending_commands;
    return 1;
}

static int states_equal(const MaterializationState *left,
                        const MaterializationState *right)
{
    return left->terrain_hash == right->terrain_hash &&
           left->kernel_hash == right->kernel_hash &&
           left->tick == right->tick && left->entities == right->entities &&
           left->placed == right->placed && left->occupied == right->occupied &&
           left->active_occupancies == right->active_occupancies &&
           left->pending_commands == right->pending_commands;
}

int main(void)
{
    MaterializationState clean;
    MaterializationState before;
    MaterializationState after_failure;
    MaterializationState after_retry;
    size_t failure_index;
    size_t failures_observed = 0U;
    int reached_success = 0;
    int result = 1;
    MiniSNNWorldsTerrain *terrain = NULL;
    MiniSNNWorldsKernel *kernel = NULL;

    terrain = make_terrain();
    kernel = make_kernel(terrain);
    CHECK(terrain != NULL && kernel != NULL);
    CHECK(minisnn_worlds_terrain_materialize(terrain, kernel) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    CHECK(capture_state(terrain, kernel, &clean));
    minisnn_worlds_kernel_destroy(kernel);
    minisnn_worlds_terrain_destroy(terrain);
    kernel = NULL;
    terrain = NULL;

    for (failure_index = 0U; failure_index < 64U; ++failure_index)
    {
        MiniSNNWorldsTerrainError materialize_result;

        terrain = make_terrain();
        kernel = make_kernel(terrain);
        CHECK(terrain != NULL && kernel != NULL);
        CHECK(capture_state(terrain, kernel, &before));
        minisnn_worlds_kernel_testing_fail_allocation_after(failure_index);
        materialize_result = minisnn_worlds_terrain_materialize(terrain, kernel);
        minisnn_worlds_kernel_testing_fail_allocation_after(SIZE_MAX);
        if (materialize_result == MINISNN_WORLDS_TERRAIN_ERROR_NONE)
        {
            reached_success = 1;
            minisnn_worlds_kernel_destroy(kernel);
            minisnn_worlds_terrain_destroy(terrain);
            kernel = NULL;
            terrain = NULL;
            break;
        }
        CHECK(materialize_result == MINISNN_WORLDS_TERRAIN_ERROR_KERNEL_FAILURE);
        CHECK(!minisnn_worlds_kernel_command_batch_active(kernel));
        CHECK(capture_state(terrain, kernel, &after_failure));
        CHECK(states_equal(&before, &after_failure));
        CHECK(minisnn_worlds_terrain_materialize(terrain, kernel) ==
              MINISNN_WORLDS_TERRAIN_ERROR_NONE);
        CHECK(capture_state(terrain, kernel, &after_retry));
        CHECK(states_equal(&clean, &after_retry));
        printf("fail_index=%zu result=KERNEL_FAILURE tick_after_failure=%llu "
               "entities_after_failure=%zu retry=SUCCESS final_kernel_hash=%llu\n",
               failure_index, (unsigned long long)after_failure.tick,
               after_failure.entities, (unsigned long long)after_retry.kernel_hash);
        ++failures_observed;
        minisnn_worlds_kernel_destroy(kernel);
        minisnn_worlds_terrain_destroy(terrain);
        kernel = NULL;
        terrain = NULL;
    }
    CHECK(reached_success);
    CHECK(failures_observed >= 6U);
    printf("WT0 materialization failure atomicity OK\n");
    result = 0;

done:
    minisnn_worlds_kernel_testing_fail_allocation_after(SIZE_MAX);
    minisnn_worlds_kernel_destroy(kernel);
    minisnn_worlds_terrain_destroy(terrain);
    return result;
}
