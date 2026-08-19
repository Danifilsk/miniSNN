#include <stdio.h>

#include "minisnn_worlds_terrain.h"

#define CHECK(condition) \
    do { if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } } while (0)

static MiniSNNWorldsKernelEntityId no_entity(void)
{
    MiniSNNWorldsKernelEntityId value = { UINT64_C(0) };
    return value;
}

static int step_queue(MiniSNNWorldsKernel *kernel,
                      MiniSNNWorldsKernelError result)
{
    return result == MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int add_actor(MiniSNNWorldsKernel *kernel,
                     MiniSNNWorldsKernelPosition position,
                     MiniSNNWorldsKernelEntityId *out_actor)
{
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelTransform transform;
    MiniSNNWorldsKernelOccupancy occupancy;
    size_t count;

    if (!step_queue(kernel, minisnn_worlds_kernel_queue_create_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
                no_entity(), &command)))
    {
        return 0;
    }
    count = minisnn_worlds_kernel_entity_count(kernel);
    if (count == 0U ||
        minisnn_worlds_kernel_entity_at(kernel, count - 1U, out_actor) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    transform.position = position;
    transform.orientation = 0U;
    if (!step_queue(kernel, minisnn_worlds_kernel_queue_place_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
                no_entity(), *out_actor, transform, &command)))
    {
        return 0;
    }
    occupancy.half_extent_x = 499;
    occupancy.half_extent_y = 499;
    occupancy.category_bits = UINT32_C(2);
    occupancy.blocking_mask = 0U;
    return step_queue(kernel, minisnn_worlds_kernel_queue_set_occupancy(
        kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
        no_entity(), *out_actor, occupancy, &command));
}

static int move(MiniSNNWorldsKernel *kernel,
                MiniSNNWorldsKernelEntityId actor,
                MiniSNNWorldsKernelScalar dx,
                MiniSNNWorldsKernelScalar dy,
                int *out_moved)
{
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelEvent event;

    if (out_moved == NULL ||
        !step_queue(kernel, minisnn_worlds_kernel_queue_move_entity(
            kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
            no_entity(), actor, dx, dy, &command)) ||
        minisnn_worlds_kernel_last_tick_event_count(kernel) != 1U ||
        minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &event) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    *out_moved = event.type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED;
    return *out_moved || event.type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED;
}

static MiniSNNWorldsTerrain *make_map(void)
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

static int materialized_hash(uint64_t *out_terrain_hash, uint64_t *out_kernel_hash)
{
    MiniSNNWorldsTerrain *terrain = make_map();
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelSpaceBounds bounds;
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelEntityId actor;
    MiniSNNWorldsKernelPosition start;
    int moved;
    int result = 0;

    if (terrain == NULL ||
        minisnn_worlds_terrain_space_bounds(terrain, &bounds) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE)
    {
        minisnn_worlds_terrain_destroy(terrain);
        return 0;
    }
    config.space_bounds = bounds;
    kernel = minisnn_worlds_kernel_create(&config, &error);
    if (kernel == NULL || error != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_terrain_materialize(terrain, kernel) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        minisnn_worlds_terrain_tile_to_world(terrain, 0U, 0U, &start) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        !add_actor(kernel, start, &actor) ||
        !move(kernel, actor, 1000, 0, &moved) || !moved ||
        !move(kernel, actor, 0, 1000, &moved) || moved ||
        !move(kernel, actor, 1000, 0, &moved) || !moved ||
        !move(kernel, actor, 1000, 0, &moved) || moved ||
        !move(kernel, actor, -2000, 2000, &moved) || moved ||
        minisnn_worlds_terrain_hash(terrain, out_terrain_hash) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(kernel, out_kernel_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        goto done;
    }
    result = 1;
done:
    minisnn_worlds_kernel_destroy(kernel);
    minisnn_worlds_terrain_destroy(terrain);
    return result;
}

int main(void)
{
    MiniSNNWorldsTerrainConfig bad = { 0U, 1U, 1000, 0, 0 };
    MiniSNNWorldsTerrainConfig config = { 3U, 2U, 1000, -1000, -1000 };
    MiniSNNWorldsTerrainError error;
    MiniSNNWorldsTerrain *terrain;
    MiniSNNWorldsTerrainTile tile;
    MiniSNNWorldsKernelPosition position;
    MiniSNNWorldsKernelPosition center;
    bool value;
    uint32_t x;
    uint32_t y;
    uint64_t terrain_hash_a;
    uint64_t terrain_hash_b;
    uint64_t kernel_hash_a;
    uint64_t kernel_hash_b;

    CHECK(minisnn_worlds_terrain_create(&bad, &error) == NULL);
    CHECK(error == MINISNN_WORLDS_TERRAIN_ERROR_INVALID_CONFIG);
    bad.width = 1U;
    bad.height = 0U;
    CHECK(minisnn_worlds_terrain_create(&bad, &error) == NULL);
    bad.height = 1U;
    bad.tile_size = 0;
    CHECK(minisnn_worlds_terrain_create(&bad, &error) == NULL);

    terrain = minisnn_worlds_terrain_create(&config, &error);
    CHECK(terrain != NULL && error == MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    CHECK(minisnn_worlds_terrain_width(terrain) == 3U);
    CHECK(minisnn_worlds_terrain_height(terrain) == 2U);
    CHECK(minisnn_worlds_terrain_get_tile(terrain, 0U, 0U, &tile) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    CHECK(tile == MINISNN_WORLDS_TERRAIN_TILE_WATER);
    CHECK(minisnn_worlds_terrain_set_tile(
              terrain, 1U, 1U, MINISNN_WORLDS_TERRAIN_TILE_LAND) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    CHECK(minisnn_worlds_terrain_get_tile(terrain, 1U, 1U, &tile) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE &&
          tile == MINISNN_WORLDS_TERRAIN_TILE_LAND);
    CHECK(minisnn_worlds_terrain_add_rock(terrain, 1U, 0U) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    CHECK(minisnn_worlds_terrain_add_rock(terrain, 1U, 0U) ==
          MINISNN_WORLDS_TERRAIN_ERROR_DUPLICATE_ROCK);
    CHECK(minisnn_worlds_terrain_has_rock(terrain, 1U, 0U, &value) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE && value);
    CHECK(minisnn_worlds_terrain_remove_rock(terrain, 1U, 0U) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    CHECK(minisnn_worlds_terrain_remove_rock(terrain, 1U, 0U) ==
          MINISNN_WORLDS_TERRAIN_ERROR_ROCK_NOT_FOUND);
    CHECK(minisnn_worlds_terrain_add_rock(terrain, 3U, 0U) ==
          MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS);
    position.x = -1000;
    position.y = -1000;
    CHECK(minisnn_worlds_terrain_world_to_tile(terrain, position, &x, &y) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE && x == 0U && y == 0U);
    CHECK(minisnn_worlds_terrain_tile_to_world(terrain, 2U, 1U, &center) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE && center.x == 1500 && center.y == 500);
    position.x = -1001;
    CHECK(minisnn_worlds_terrain_world_to_tile(terrain, position, &x, &y) ==
          MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS);
    CHECK(minisnn_worlds_terrain_is_blocked(terrain, 0U, 0U, &value) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE && !value);
    CHECK(minisnn_worlds_terrain_is_blocked(terrain, 1U, 1U, &value) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE && value);
    CHECK(minisnn_worlds_terrain_finalize(terrain) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    CHECK(minisnn_worlds_terrain_set_tile(
              terrain, 0U, 0U, MINISNN_WORLDS_TERRAIN_TILE_LAND) ==
          MINISNN_WORLDS_TERRAIN_ERROR_FINALIZED);
    minisnn_worlds_terrain_destroy(terrain);

    CHECK(materialized_hash(&terrain_hash_a, &kernel_hash_a));
    CHECK(materialized_hash(&terrain_hash_b, &kernel_hash_b));
    CHECK(terrain_hash_a == terrain_hash_b);
    CHECK(kernel_hash_a == kernel_hash_b);
    printf("WT0 terrain tests OK\n");
    return 0;
}
