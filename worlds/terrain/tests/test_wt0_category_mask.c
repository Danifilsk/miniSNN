#include <stdio.h>

#include "minisnn_worlds_terrain.h"

#define CHECK(condition) \
    do { if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        goto done; \
    } } while (0)

static MiniSNNWorldsKernelEntityId no_entity(void)
{
    MiniSNNWorldsKernelEntityId value = { UINT64_C(0) };
    return value;
}

static int queue_step(MiniSNNWorldsKernel *kernel,
                      MiniSNNWorldsKernelError error)
{
    return error == MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static MiniSNNWorldsTerrain *make_terrain(void)
{
    MiniSNNWorldsTerrainConfig config = { 4U, 4U, 1000, -500, -500 };
    MiniSNNWorldsTerrainError error;
    MiniSNNWorldsTerrain *terrain = minisnn_worlds_terrain_create(&config, &error);

    if (terrain == NULL || error != MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        minisnn_worlds_terrain_set_tile(
            terrain, 1U, 0U, MINISNN_WORLDS_TERRAIN_TILE_LAND) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        minisnn_worlds_terrain_add_rock(terrain, 2U, 2U) !=
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

static int create_actor(MiniSNNWorldsKernel *kernel,
                        MiniSNNWorldsKernelPosition position,
                        uint32_t category,
                        MiniSNNWorldsKernelEntityId *out_actor)
{
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelTransform transform;
    MiniSNNWorldsKernelOccupancy occupancy;
    size_t count;

    if (!queue_step(kernel, minisnn_worlds_kernel_queue_create_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
                no_entity(), &command)))
    {
        return 0;
    }
    count = minisnn_worlds_kernel_entity_count(kernel);
    if (count == 0U || minisnn_worlds_kernel_entity_at(
            kernel, count - 1U, out_actor) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    transform.position = position;
    transform.orientation = 0U;
    if (!queue_step(kernel, minisnn_worlds_kernel_queue_place_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
                no_entity(), *out_actor, transform, &command)))
    {
        return 0;
    }
    occupancy.half_extent_x = 499;
    occupancy.half_extent_y = 499;
    occupancy.category_bits = category;
    occupancy.blocking_mask = 0U;
    return queue_step(kernel, minisnn_worlds_kernel_queue_set_occupancy(
        kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
        no_entity(), *out_actor, occupancy, &command));
}

static int movement_result(MiniSNNWorldsKernel *kernel,
                           MiniSNNWorldsKernelEntityId actor,
                           MiniSNNWorldsKernelScalar dx,
                           MiniSNNWorldsKernelScalar dy,
                           int *out_moved)
{
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelEvent event;

    if (out_moved == NULL || !queue_step(kernel,
            minisnn_worlds_kernel_queue_move_entity(
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

static int move_from_water(uint32_t category, uint32_t start_x, uint32_t start_y,
                           uint32_t target_x, uint32_t target_y, int expected_moved)
{
    MiniSNNWorldsTerrain *terrain = NULL;
    MiniSNNWorldsKernel *kernel = NULL;
    MiniSNNWorldsKernelEntityId actor;
    MiniSNNWorldsKernelPosition start;
    MiniSNNWorldsKernelPosition target;
    int moved;
    int result = 0;

    terrain = make_terrain();
    kernel = make_kernel(terrain);
    if (terrain == NULL || kernel == NULL ||
        minisnn_worlds_terrain_materialize(terrain, kernel) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        minisnn_worlds_terrain_tile_to_world(terrain, start_x, start_y, &start) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        minisnn_worlds_terrain_tile_to_world(terrain, target_x, target_y, &target) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        !create_actor(kernel, start, category, &actor) ||
        !movement_result(kernel, actor, target.x - start.x, target.y - start.y, &moved) ||
        moved != expected_moved)
    {
        goto done;
    }
    result = 1;
done:
    minisnn_worlds_kernel_destroy(kernel);
    minisnn_worlds_terrain_destroy(terrain);
    return result;
}

static int water_tile_accepts_actor(uint32_t tile_x, uint32_t tile_y)
{
    MiniSNNWorldsTerrain *terrain = NULL;
    MiniSNNWorldsKernel *kernel = NULL;
    MiniSNNWorldsKernelEntityId actor;
    MiniSNNWorldsKernelPosition position;
    int result = 0;

    terrain = make_terrain();
    kernel = make_kernel(terrain);
    if (terrain == NULL || kernel == NULL ||
        minisnn_worlds_terrain_materialize(terrain, kernel) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        minisnn_worlds_terrain_tile_to_world(terrain, tile_x, tile_y, &position) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        !create_actor(kernel, position, UINT32_C(4), &actor))
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
    static const uint32_t categories[] = { UINT32_C(1), UINT32_C(2), UINT32_C(4) };
    static const uint32_t neighbor_x[] = { 1U, 3U, 2U, 2U };
    static const uint32_t neighbor_y[] = { 2U, 2U, 1U, 3U };
    size_t index;

    for (index = 0U; index < sizeof(categories) / sizeof(categories[0]); ++index)
    {
        CHECK(move_from_water(categories[index], 0U, 0U, 0U, 1U, 1));
        CHECK(move_from_water(categories[index], 0U, 0U, 1U, 0U, 0));
        CHECK(move_from_water(categories[index], 2U, 1U, 2U, 2U, 0));
        printf("category=%u water=PASS land=BLOCKED rock=BLOCKED\n", categories[index]);
    }
    for (index = 0U; index < sizeof(neighbor_x) / sizeof(neighbor_x[0]); ++index)
    {
        CHECK(water_tile_accepts_actor(neighbor_x[index], neighbor_y[index]));
    }
    printf("WT0 category mask tests OK\n");
    return 0;

done:
    return 1;
}
