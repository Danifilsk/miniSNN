#include <stdio.h>
#include <stdlib.h>

#include "minisnn_worlds_terrain.h"

static MiniSNNWorldsKernelEntityId no_entity(void)
{
    MiniSNNWorldsKernelEntityId value = { UINT64_C(0) };
    return value;
}

static int queue_step(MiniSNNWorldsKernel *kernel,
                      MiniSNNWorldsKernelError queue_error)
{
    return queue_error == MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int create_actor(MiniSNNWorldsKernel *kernel,
                        MiniSNNWorldsKernelPosition position,
                        MiniSNNWorldsKernelScalar tile_size,
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
    if (count == 0U ||
        minisnn_worlds_kernel_entity_at(kernel, count - 1U, out_actor) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    transform.position = position;
    transform.orientation = UINT32_C(0);
    if (!queue_step(kernel, minisnn_worlds_kernel_queue_place_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
                no_entity(), *out_actor, transform, &command)))
    {
        return 0;
    }
    occupancy.half_extent_x = (tile_size - 1) / 2;
    occupancy.half_extent_y = occupancy.half_extent_x;
    occupancy.category_bits = UINT32_C(2);
    occupancy.blocking_mask = UINT32_C(0);
    return queue_step(kernel, minisnn_worlds_kernel_queue_set_occupancy(
        kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
        no_entity(), *out_actor, occupancy, &command));
}

static int move_result(MiniSNNWorldsKernel *kernel,
                       MiniSNNWorldsKernelEntityId actor,
                       MiniSNNWorldsKernelScalar dx,
                       MiniSNNWorldsKernelScalar dy,
                       int *out_moved)
{
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelEvent event;

    if (out_moved == NULL ||
        !queue_step(kernel, minisnn_worlds_kernel_queue_move_entity(
            kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
            no_entity(), actor, dx, dy, &command)) ||
        minisnn_worlds_kernel_last_tick_event_count(kernel) != 1U ||
        minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &event) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    *out_moved = event.type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED;
    return *out_moved ||
           (event.type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED &&
            event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_CONFLICT);
}

static MiniSNNWorldsTerrain *build_demo_terrain(void)
{
    MiniSNNWorldsTerrainConfig config = { 5U, 5U, 1000, 0, 0 };
    MiniSNNWorldsTerrainError error;
    MiniSNNWorldsTerrain *terrain = minisnn_worlds_terrain_create(&config, &error);
    uint32_t x;

    if (terrain == NULL || error != MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        minisnn_worlds_terrain_add_rock(terrain, 2U, 1U) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE)
    {
        minisnn_worlds_terrain_destroy(terrain);
        return NULL;
    }
    for (x = 1U; x <= 3U; ++x)
    {
        if (minisnn_worlds_terrain_set_tile(
                terrain, x, 3U, MINISNN_WORLDS_TERRAIN_TILE_LAND) !=
                MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
            minisnn_worlds_terrain_set_tile(
                terrain, x, 4U, MINISNN_WORLDS_TERRAIN_TILE_LAND) !=
                MINISNN_WORLDS_TERRAIN_ERROR_NONE)
        {
            minisnn_worlds_terrain_destroy(terrain);
            return NULL;
        }
    }
    if (minisnn_worlds_terrain_finalize(terrain) !=
        MINISNN_WORLDS_TERRAIN_ERROR_NONE)
    {
        minisnn_worlds_terrain_destroy(terrain);
        return NULL;
    }
    return terrain;
}

int main(int argc, char **argv)
{
    MiniSNNWorldsTerrain *terrain;
    MiniSNNWorldsKernelConfig kernel_config;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelError kernel_error;
    MiniSNNWorldsKernelSpaceBounds bounds;
    MiniSNNWorldsKernelEntityId actor;
    MiniSNNWorldsKernelPosition start;
    uint64_t terrain_hash;
    uint64_t kernel_hash;
    int moved;
    unsigned int successful_moves = 0U;
    unsigned int blocked_by_land = 0U;
    unsigned int blocked_by_rock = 0U;
    FILE *map_file;
    FILE *summary_file;
    const char *output_directory;
    char map_path[512];
    char summary_path[512];

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <output_directory>\n", argv[0]);
        return 1;
    }
    output_directory = argv[1];
    terrain = build_demo_terrain();
    if (terrain == NULL ||
        minisnn_worlds_terrain_space_bounds(terrain, &bounds) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        minisnn_worlds_terrain_hash(terrain, &terrain_hash) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE)
    {
        minisnn_worlds_terrain_destroy(terrain);
        return 1;
    }
    kernel_config = minisnn_worlds_kernel_config_default();
    kernel_config.space_bounds = bounds;
    kernel = minisnn_worlds_kernel_create(&kernel_config, &kernel_error);
    if (kernel == NULL || kernel_error != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_terrain_materialize(terrain, kernel) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        minisnn_worlds_terrain_tile_to_world(terrain, 1U, 2U, &start) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        !create_actor(kernel, start, minisnn_worlds_terrain_tile_size(terrain),
                      &actor))
    {
        minisnn_worlds_kernel_destroy(kernel);
        minisnn_worlds_terrain_destroy(terrain);
        return 1;
    }
    if (!move_result(kernel, actor, 1000, 0, &moved) || !moved)
    {
        return 1;
    }
    ++successful_moves;
    if (!move_result(kernel, actor, 0, -1000, &moved) || moved)
    {
        return 1;
    }
    ++blocked_by_rock;
    if (!move_result(kernel, actor, 0, 1000, &moved) || moved)
    {
        return 1;
    }
    ++blocked_by_land;
    if (!move_result(kernel, actor, 1000, 0, &moved) || !moved ||
        minisnn_worlds_kernel_state_hash(kernel, &kernel_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 1;
    }
    ++successful_moves;
    if (snprintf(map_path, sizeof(map_path), "%s/wt0_map.txt", output_directory) < 0 ||
        snprintf(summary_path, sizeof(summary_path), "%s/wt0_summary.txt", output_directory) < 0)
    {
        minisnn_worlds_kernel_destroy(kernel);
        minisnn_worlds_terrain_destroy(terrain);
        return 1;
    }
    map_file = fopen(map_path, "w");
    summary_file = fopen(summary_path, "w");
    if (map_file == NULL || summary_file == NULL)
    {
        if (map_file != NULL) fclose(map_file);
        if (summary_file != NULL) fclose(summary_file);
        minisnn_worlds_kernel_destroy(kernel);
        minisnn_worlds_terrain_destroy(terrain);
        return 1;
    }
    fprintf(map_file, "WWWWW\nWWRWW\nWWWWW\nWLLLW\nWLLLW\n");
    fprintf(summary_file,
            "width=5\nheight=5\ntile_size=1000\nwater_count=19\n"
            "land_count=6\nrock_count=1\nterrain_hash=%llu\n"
            "successful_moves=%u\nblocked_by_land=%u\nblocked_by_rock=%u\n"
            "final_kernel_hash=%llu\n",
            (unsigned long long)terrain_hash, successful_moves, blocked_by_land,
            blocked_by_rock, (unsigned long long)kernel_hash);
    fclose(map_file);
    fclose(summary_file);
    minisnn_worlds_kernel_destroy(kernel);
    minisnn_worlds_terrain_destroy(terrain);
    (void)output_directory;
    return 0;
}
