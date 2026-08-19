#include "minisnn_worlds_terrain.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define TERRAIN_FNV_OFFSET UINT64_C(14695981039346656037)
#define TERRAIN_FNV_PRIME UINT64_C(1099511628211)
#define TERRAIN_BLOCKER_CATEGORY UINT32_C(1)

struct MiniSNNWorldsTerrain
{
    MiniSNNWorldsTerrainConfig config;
    uint8_t *tiles;
    uint8_t *rocks;
    size_t cell_count;
    size_t land_count;
    size_t rock_count;
    bool finalized;
    bool materialized;
    const MiniSNNWorldsKernel *materialized_kernel;
    MiniSNNWorldsTerrainError last_error;
};

static void set_error(MiniSNNWorldsTerrain *terrain,
                      MiniSNNWorldsTerrainError error)
{
    if (terrain != NULL)
    {
        terrain->last_error = error;
    }
}

static int size_product(size_t left, size_t right, size_t *out_result)
{
    if (out_result == NULL || (right != 0U && left > SIZE_MAX / right))
    {
        return 0;
    }
    *out_result = left * right;
    return 1;
}

static int scalar_add(MiniSNNWorldsKernelScalar left,
                      MiniSNNWorldsKernelScalar right,
                      MiniSNNWorldsKernelScalar *out_result)
{
    if (out_result == NULL ||
        (right > 0 && left > INT64_MAX - right) ||
        (right < 0 && left < INT64_MIN - right))
    {
        return 0;
    }
    *out_result = left + right;
    return 1;
}

static int scalar_subtract(MiniSNNWorldsKernelScalar left,
                           MiniSNNWorldsKernelScalar right,
                           MiniSNNWorldsKernelScalar *out_result)
{
    if (out_result == NULL ||
        (right > 0 && left < INT64_MIN + right) ||
        (right < 0 && left > INT64_MAX + right))
    {
        return 0;
    }
    *out_result = left - right;
    return 1;
}

static int scalar_times_u32(MiniSNNWorldsKernelScalar value,
                            uint32_t multiplier,
                            MiniSNNWorldsKernelScalar *out_result)
{
    if (out_result == NULL || value < 0 ||
        (multiplier != 0U &&
         value > INT64_MAX / (MiniSNNWorldsKernelScalar)multiplier))
    {
        return 0;
    }
    *out_result = value * (MiniSNNWorldsKernelScalar)multiplier;
    return 1;
}

static int terrain_index(const MiniSNNWorldsTerrain *terrain,
                         uint32_t tile_x, uint32_t tile_y, size_t *out_index)
{
    size_t row;

    if (terrain == NULL || out_index == NULL ||
        tile_x >= terrain->config.width || tile_y >= terrain->config.height ||
        !size_product((size_t)tile_y, (size_t)terrain->config.width, &row))
    {
        return 0;
    }
    *out_index = row + (size_t)tile_x;
    return 1;
}

static int terrain_bounds(const MiniSNNWorldsTerrain *terrain,
                          MiniSNNWorldsKernelSpaceBounds *out_bounds)
{
    MiniSNNWorldsKernelScalar span_x;
    MiniSNNWorldsKernelScalar span_y;

    if (terrain == NULL || out_bounds == NULL ||
        !scalar_times_u32(terrain->config.tile_size, terrain->config.width,
                          &span_x) ||
        !scalar_times_u32(terrain->config.tile_size, terrain->config.height,
                          &span_y) ||
        !scalar_add(terrain->config.origin_x, span_x - 1,
                    &out_bounds->max_x) ||
        !scalar_add(terrain->config.origin_y, span_y - 1,
                    &out_bounds->max_y))
    {
        return 0;
    }
    out_bounds->min_x = terrain->config.origin_x;
    out_bounds->min_y = terrain->config.origin_y;
    return 1;
}

static int terrain_config_valid(const MiniSNNWorldsTerrainConfig *config)
{
    size_t count;
    MiniSNNWorldsTerrain temporary;
    MiniSNNWorldsKernelSpaceBounds bounds;

    if (config == NULL || config->width == 0U || config->height == 0U ||
        config->tile_size <= 0 ||
        !size_product((size_t)config->width, (size_t)config->height, &count) ||
        count == 0U)
    {
        return 0;
    }
    memset(&temporary, 0, sizeof(temporary));
    temporary.config = *config;
    return terrain_bounds(&temporary, &bounds);
}

static int terrain_tile_valid(MiniSNNWorldsTerrainTile tile)
{
    return tile == MINISNN_WORLDS_TERRAIN_TILE_WATER ||
           tile == MINISNN_WORLDS_TERRAIN_TILE_LAND;
}

static void fnv_byte(uint64_t *in_out_hash, uint8_t value)
{
    *in_out_hash ^= value;
    *in_out_hash *= TERRAIN_FNV_PRIME;
}

static void fnv_u32(uint64_t *in_out_hash, uint32_t value)
{
    unsigned int index;

    for (index = 0U; index < 4U; ++index)
    {
        fnv_byte(in_out_hash, (uint8_t)(value >> (index * 8U)));
    }
}

static void fnv_u64(uint64_t *in_out_hash, uint64_t value)
{
    unsigned int index;

    for (index = 0U; index < 8U; ++index)
    {
        fnv_byte(in_out_hash, (uint8_t)(value >> (index * 8U)));
    }
}

static void fnv_literal(uint64_t *in_out_hash, const char *value)
{
    size_t index;

    for (index = 0U; value[index] != '\0'; ++index)
    {
        fnv_byte(in_out_hash, (uint8_t)value[index]);
    }
}

MiniSNNWorldsTerrain *minisnn_worlds_terrain_create(
    const MiniSNNWorldsTerrainConfig *config,
    MiniSNNWorldsTerrainError *out_error)
{
    MiniSNNWorldsTerrain *terrain;
    size_t cell_count;

    if (out_error != NULL)
    {
        *out_error = MINISNN_WORLDS_TERRAIN_ERROR_NONE;
    }
    if (!terrain_config_valid(config))
    {
        if (out_error != NULL)
        {
            *out_error = MINISNN_WORLDS_TERRAIN_ERROR_INVALID_CONFIG;
        }
        return NULL;
    }
    if (!size_product((size_t)config->width, (size_t)config->height,
                      &cell_count))
    {
        if (out_error != NULL)
        {
            *out_error = MINISNN_WORLDS_TERRAIN_ERROR_OVERFLOW;
        }
        return NULL;
    }
    terrain = calloc(1U, sizeof(*terrain));
    if (terrain == NULL)
    {
        if (out_error != NULL)
        {
            *out_error = MINISNN_WORLDS_TERRAIN_ERROR_ALLOCATION;
        }
        return NULL;
    }
    terrain->tiles = malloc(cell_count);
    terrain->rocks = calloc(cell_count, sizeof(*terrain->rocks));
    if (terrain->tiles == NULL || terrain->rocks == NULL)
    {
        free(terrain->rocks);
        free(terrain->tiles);
        free(terrain);
        if (out_error != NULL)
        {
            *out_error = MINISNN_WORLDS_TERRAIN_ERROR_ALLOCATION;
        }
        return NULL;
    }
    memset(terrain->tiles, MINISNN_WORLDS_TERRAIN_TILE_WATER, cell_count);
    terrain->config = *config;
    terrain->cell_count = cell_count;
    terrain->last_error = MINISNN_WORLDS_TERRAIN_ERROR_NONE;
    return terrain;
}

void minisnn_worlds_terrain_destroy(MiniSNNWorldsTerrain *terrain)
{
    if (terrain != NULL)
    {
        free(terrain->rocks);
        free(terrain->tiles);
        free(terrain);
    }
}

MiniSNNWorldsTerrainError minisnn_worlds_terrain_last_error(
    const MiniSNNWorldsTerrain *terrain)
{
    return terrain == NULL ? MINISNN_WORLDS_TERRAIN_ERROR_NULL_ARGUMENT :
           terrain->last_error;
}

uint32_t minisnn_worlds_terrain_width(const MiniSNNWorldsTerrain *terrain)
{
    return terrain == NULL ? 0U : terrain->config.width;
}

uint32_t minisnn_worlds_terrain_height(const MiniSNNWorldsTerrain *terrain)
{
    return terrain == NULL ? 0U : terrain->config.height;
}

MiniSNNWorldsKernelScalar minisnn_worlds_terrain_tile_size(
    const MiniSNNWorldsTerrain *terrain)
{
    return terrain == NULL ? 0 : terrain->config.tile_size;
}

bool minisnn_worlds_terrain_is_finalized(const MiniSNNWorldsTerrain *terrain)
{
    return terrain != NULL && terrain->finalized;
}

MiniSNNWorldsTerrainError minisnn_worlds_terrain_get_tile(
    const MiniSNNWorldsTerrain *terrain, uint32_t tile_x, uint32_t tile_y,
    MiniSNNWorldsTerrainTile *out_tile)
{
    size_t index;

    if (terrain == NULL || out_tile == NULL)
    {
        return MINISNN_WORLDS_TERRAIN_ERROR_NULL_ARGUMENT;
    }
    if (!terrain_index(terrain, tile_x, tile_y, &index))
    {
        set_error((MiniSNNWorldsTerrain *)terrain,
                  MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS);
        return MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS;
    }
    *out_tile = (MiniSNNWorldsTerrainTile)terrain->tiles[index];
    set_error((MiniSNNWorldsTerrain *)terrain, MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    return MINISNN_WORLDS_TERRAIN_ERROR_NONE;
}

MiniSNNWorldsTerrainError minisnn_worlds_terrain_set_tile(
    MiniSNNWorldsTerrain *terrain, uint32_t tile_x, uint32_t tile_y,
    MiniSNNWorldsTerrainTile tile)
{
    size_t index;
    uint8_t previous;

    if (terrain == NULL)
    {
        return MINISNN_WORLDS_TERRAIN_ERROR_NULL_ARGUMENT;
    }
    if (terrain->finalized)
    {
        set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_FINALIZED);
        return MINISNN_WORLDS_TERRAIN_ERROR_FINALIZED;
    }
    if (!terrain_tile_valid(tile))
    {
        set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_INVALID_TILE);
        return MINISNN_WORLDS_TERRAIN_ERROR_INVALID_TILE;
    }
    if (!terrain_index(terrain, tile_x, tile_y, &index))
    {
        set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS);
        return MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS;
    }
    previous = terrain->tiles[index];
    terrain->tiles[index] = (uint8_t)tile;
    if (previous != terrain->tiles[index])
    {
        if (tile == MINISNN_WORLDS_TERRAIN_TILE_LAND)
        {
            ++terrain->land_count;
        }
        else
        {
            --terrain->land_count;
        }
    }
    set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    return MINISNN_WORLDS_TERRAIN_ERROR_NONE;
}

MiniSNNWorldsTerrainError minisnn_worlds_terrain_world_to_tile(
    const MiniSNNWorldsTerrain *terrain, MiniSNNWorldsKernelPosition world_position,
    uint32_t *out_tile_x, uint32_t *out_tile_y)
{
    MiniSNNWorldsKernelScalar dx;
    MiniSNNWorldsKernelScalar dy;
    MiniSNNWorldsKernelScalar x;
    MiniSNNWorldsKernelScalar y;

    if (terrain == NULL || out_tile_x == NULL || out_tile_y == NULL)
    {
        return MINISNN_WORLDS_TERRAIN_ERROR_NULL_ARGUMENT;
    }
    if (!scalar_subtract(world_position.x, terrain->config.origin_x, &dx) ||
        !scalar_subtract(world_position.y, terrain->config.origin_y, &dy))
    {
        set_error((MiniSNNWorldsTerrain *)terrain,
                  MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS);
        return MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS;
    }
    x = dx / terrain->config.tile_size;
    y = dy / terrain->config.tile_size;
    if (dx < 0 && dx % terrain->config.tile_size != 0)
    {
        --x;
    }
    if (dy < 0 && dy % terrain->config.tile_size != 0)
    {
        --y;
    }
    if (x < 0 || y < 0 || (uint64_t)x >= terrain->config.width ||
        (uint64_t)y >= terrain->config.height)
    {
        set_error((MiniSNNWorldsTerrain *)terrain,
                  MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS);
        return MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS;
    }
    *out_tile_x = (uint32_t)x;
    *out_tile_y = (uint32_t)y;
    set_error((MiniSNNWorldsTerrain *)terrain, MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    return MINISNN_WORLDS_TERRAIN_ERROR_NONE;
}

MiniSNNWorldsTerrainError minisnn_worlds_terrain_tile_to_world(
    const MiniSNNWorldsTerrain *terrain, uint32_t tile_x, uint32_t tile_y,
    MiniSNNWorldsKernelPosition *out_world_position)
{
    MiniSNNWorldsKernelScalar offset_x;
    MiniSNNWorldsKernelScalar offset_y;
    size_t ignored;

    if (terrain == NULL || out_world_position == NULL)
    {
        return MINISNN_WORLDS_TERRAIN_ERROR_NULL_ARGUMENT;
    }
    if (!terrain_index(terrain, tile_x, tile_y, &ignored))
    {
        set_error((MiniSNNWorldsTerrain *)terrain,
                  MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS);
        return MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS;
    }
    if (!scalar_times_u32(terrain->config.tile_size, tile_x, &offset_x) ||
        !scalar_times_u32(terrain->config.tile_size, tile_y, &offset_y) ||
        !scalar_add(terrain->config.origin_x, offset_x,
                    &out_world_position->x) ||
        !scalar_add(terrain->config.origin_y, offset_y,
                    &out_world_position->y) ||
        !scalar_add(out_world_position->x, terrain->config.tile_size / 2,
                    &out_world_position->x) ||
        !scalar_add(out_world_position->y, terrain->config.tile_size / 2,
                    &out_world_position->y))
    {
        set_error((MiniSNNWorldsTerrain *)terrain,
                  MINISNN_WORLDS_TERRAIN_ERROR_OVERFLOW);
        return MINISNN_WORLDS_TERRAIN_ERROR_OVERFLOW;
    }
    set_error((MiniSNNWorldsTerrain *)terrain, MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    return MINISNN_WORLDS_TERRAIN_ERROR_NONE;
}

MiniSNNWorldsTerrainError minisnn_worlds_terrain_space_bounds(
    const MiniSNNWorldsTerrain *terrain, MiniSNNWorldsKernelSpaceBounds *out_bounds)
{
    if (terrain == NULL || out_bounds == NULL)
    {
        return MINISNN_WORLDS_TERRAIN_ERROR_NULL_ARGUMENT;
    }
    if (!terrain_bounds(terrain, out_bounds))
    {
        set_error((MiniSNNWorldsTerrain *)terrain,
                  MINISNN_WORLDS_TERRAIN_ERROR_OVERFLOW);
        return MINISNN_WORLDS_TERRAIN_ERROR_OVERFLOW;
    }
    set_error((MiniSNNWorldsTerrain *)terrain, MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    return MINISNN_WORLDS_TERRAIN_ERROR_NONE;
}


MiniSNNWorldsTerrainError minisnn_worlds_terrain_add_rock(
    MiniSNNWorldsTerrain *terrain, uint32_t tile_x, uint32_t tile_y)
{
    size_t index;

    if (terrain == NULL)
    {
        return MINISNN_WORLDS_TERRAIN_ERROR_NULL_ARGUMENT;
    }
    if (terrain->finalized)
    {
        set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_FINALIZED);
        return MINISNN_WORLDS_TERRAIN_ERROR_FINALIZED;
    }
    if (!terrain_index(terrain, tile_x, tile_y, &index))
    {
        set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS);
        return MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS;
    }
    if (terrain->rocks[index] != 0U)
    {
        set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_DUPLICATE_ROCK);
        return MINISNN_WORLDS_TERRAIN_ERROR_DUPLICATE_ROCK;
    }
    terrain->rocks[index] = 1U;
    ++terrain->rock_count;
    set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    return MINISNN_WORLDS_TERRAIN_ERROR_NONE;
}

MiniSNNWorldsTerrainError minisnn_worlds_terrain_remove_rock(
    MiniSNNWorldsTerrain *terrain, uint32_t tile_x, uint32_t tile_y)
{
    size_t index;

    if (terrain == NULL)
    {
        return MINISNN_WORLDS_TERRAIN_ERROR_NULL_ARGUMENT;
    }
    if (terrain->finalized)
    {
        set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_FINALIZED);
        return MINISNN_WORLDS_TERRAIN_ERROR_FINALIZED;
    }
    if (!terrain_index(terrain, tile_x, tile_y, &index))
    {
        set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS);
        return MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS;
    }
    if (terrain->rocks[index] == 0U)
    {
        set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_ROCK_NOT_FOUND);
        return MINISNN_WORLDS_TERRAIN_ERROR_ROCK_NOT_FOUND;
    }
    terrain->rocks[index] = 0U;
    --terrain->rock_count;
    set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    return MINISNN_WORLDS_TERRAIN_ERROR_NONE;
}

MiniSNNWorldsTerrainError minisnn_worlds_terrain_has_rock(
    const MiniSNNWorldsTerrain *terrain, uint32_t tile_x, uint32_t tile_y,
    bool *out_has_rock)
{
    size_t index;

    if (terrain == NULL || out_has_rock == NULL)
    {
        return MINISNN_WORLDS_TERRAIN_ERROR_NULL_ARGUMENT;
    }
    if (!terrain_index(terrain, tile_x, tile_y, &index))
    {
        set_error((MiniSNNWorldsTerrain *)terrain,
                  MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS);
        return MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS;
    }
    *out_has_rock = terrain->rocks[index] != 0U;
    set_error((MiniSNNWorldsTerrain *)terrain, MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    return MINISNN_WORLDS_TERRAIN_ERROR_NONE;
}

MiniSNNWorldsTerrainError minisnn_worlds_terrain_is_blocked(
    const MiniSNNWorldsTerrain *terrain, uint32_t tile_x, uint32_t tile_y,
    bool *out_blocked)
{
    size_t index;

    if (terrain == NULL || out_blocked == NULL)
    {
        return MINISNN_WORLDS_TERRAIN_ERROR_NULL_ARGUMENT;
    }
    if (!terrain_index(terrain, tile_x, tile_y, &index))
    {
        set_error((MiniSNNWorldsTerrain *)terrain,
                  MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS);
        return MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS;
    }
    *out_blocked = terrain->tiles[index] == MINISNN_WORLDS_TERRAIN_TILE_LAND ||
                   terrain->rocks[index] != 0U;
    set_error((MiniSNNWorldsTerrain *)terrain, MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    return MINISNN_WORLDS_TERRAIN_ERROR_NONE;
}

size_t minisnn_worlds_terrain_land_count(const MiniSNNWorldsTerrain *terrain)
{
    return terrain == NULL ? 0U : terrain->land_count;
}

size_t minisnn_worlds_terrain_rock_count(const MiniSNNWorldsTerrain *terrain)
{
    return terrain == NULL ? 0U : terrain->rock_count;
}

MiniSNNWorldsTerrainError minisnn_worlds_terrain_finalize(
    MiniSNNWorldsTerrain *terrain)
{
    if (terrain == NULL)
    {
        return MINISNN_WORLDS_TERRAIN_ERROR_NULL_ARGUMENT;
    }
    terrain->finalized = true;
    set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    return MINISNN_WORLDS_TERRAIN_ERROR_NONE;
}

static int terrain_blocker_count(const MiniSNNWorldsTerrain *terrain,
                                 size_t *out_count)
{
    size_t index;
    size_t count = 0U;

    if (terrain == NULL || out_count == NULL)
    {
        return 0;
    }
    for (index = 0U; index < terrain->cell_count; ++index)
    {
        if ((terrain->tiles[index] == MINISNN_WORLDS_TERRAIN_TILE_LAND &&
             terrain->rocks[index] == 0U) || terrain->rocks[index] != 0U)
        {
            ++count;
        }
    }
    *out_count = count;
    return 1;
}

static int queue_creates(MiniSNNWorldsKernel *kernel, size_t count)
{
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelEntityId no_entity = { UINT64_C(0) };
    MiniSNNWorldsTick target_tick;
    size_t index;

    target_tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    if (target_tick == 0U ||
        minisnn_worlds_kernel_command_batch_begin(kernel) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    for (index = 0U; index < count; ++index)
    {
        if (minisnn_worlds_kernel_queue_create_entity(
                kernel, target_tick, 0U, no_entity, &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            (void)minisnn_worlds_kernel_command_batch_rollback(kernel);
            return 0;
        }
    }
    if (minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        (void)minisnn_worlds_kernel_command_batch_rollback(kernel);
        return 0;
    }
    return minisnn_worlds_kernel_command_batch_commit(kernel) ==
           MINISNN_WORLDS_KERNEL_ERROR_NONE;
}


static int queue_placements(
    MiniSNNWorldsTerrain *terrain, MiniSNNWorldsKernel *kernel,
    const MiniSNNWorldsKernelEntityId *blockers, size_t blocker_count)
{
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelEntityId no_entity = { UINT64_C(0) };
    MiniSNNWorldsKernelTransform transform;
    MiniSNNWorldsTick target_tick;
    uint32_t x;
    uint32_t y;
    size_t index = 0U;
    size_t cell;

    target_tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    if (target_tick == 0U ||
        minisnn_worlds_kernel_command_batch_begin(kernel) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    for (y = 0U; y < terrain->config.height; ++y)
    {
        for (x = 0U; x < terrain->config.width; ++x)
        {
            if (!terrain_index(terrain, x, y, &cell) ||
                terrain->tiles[cell] != MINISNN_WORLDS_TERRAIN_TILE_LAND ||
                terrain->rocks[cell] != 0U)
            {
                continue;
            }
            if (index >= blocker_count ||
                minisnn_worlds_terrain_tile_to_world(
                    terrain, x, y, &transform.position) !=
                    MINISNN_WORLDS_TERRAIN_ERROR_NONE)
            {
                (void)minisnn_worlds_kernel_command_batch_rollback(kernel);
                return 0;
            }
            transform.orientation = UINT32_C(0);
            if (minisnn_worlds_kernel_queue_place_entity(
                    kernel, target_tick, 0U, no_entity, blockers[index++],
                    transform, &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
            {
                (void)minisnn_worlds_kernel_command_batch_rollback(kernel);
                return 0;
            }
        }
    }
    for (y = 0U; y < terrain->config.height; ++y)
    {
        for (x = 0U; x < terrain->config.width; ++x)
        {
            if (!terrain_index(terrain, x, y, &cell) ||
                terrain->rocks[cell] == 0U)
            {
                continue;
            }
            if (index >= blocker_count ||
                minisnn_worlds_terrain_tile_to_world(
                    terrain, x, y, &transform.position) !=
                    MINISNN_WORLDS_TERRAIN_ERROR_NONE)
            {
                (void)minisnn_worlds_kernel_command_batch_rollback(kernel);
                return 0;
            }
            transform.orientation = UINT32_C(0);
            if (minisnn_worlds_kernel_queue_place_entity(
                    kernel, target_tick, 0U, no_entity, blockers[index++],
                    transform, &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
            {
                (void)minisnn_worlds_kernel_command_batch_rollback(kernel);
                return 0;
            }
        }
    }
    if (index != blocker_count)
    {
        (void)minisnn_worlds_kernel_command_batch_rollback(kernel);
        return 0;
    }
    if (minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        (void)minisnn_worlds_kernel_command_batch_rollback(kernel);
        return 0;
    }
    return minisnn_worlds_kernel_command_batch_commit(kernel) ==
           MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int queue_occupancies(
    const MiniSNNWorldsTerrain *terrain, MiniSNNWorldsKernel *kernel,
    const MiniSNNWorldsKernelEntityId *blockers, size_t blocker_count)
{
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelEntityId no_entity = { UINT64_C(0) };
    MiniSNNWorldsKernelOccupancy occupancy;
    MiniSNNWorldsTick target_tick;
    size_t index;

    occupancy.half_extent_x = terrain->config.tile_size > 2 ?
        (terrain->config.tile_size - 1) / 2 : 1;
    occupancy.half_extent_y = occupancy.half_extent_x;
    occupancy.category_bits = TERRAIN_BLOCKER_CATEGORY;
    occupancy.blocking_mask = UINT32_MAX;
    target_tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    if (target_tick == 0U ||
        minisnn_worlds_kernel_command_batch_begin(kernel) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    for (index = 0U; index < blocker_count; ++index)
    {
        if (minisnn_worlds_kernel_queue_set_occupancy(
                kernel, target_tick, 0U, no_entity, blockers[index], occupancy,
                &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            (void)minisnn_worlds_kernel_command_batch_rollback(kernel);
            return 0;
        }
    }
    if (minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        (void)minisnn_worlds_kernel_command_batch_rollback(kernel);
        return 0;
    }
    return minisnn_worlds_kernel_command_batch_commit(kernel) ==
           MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsTerrainError minisnn_worlds_terrain_materialize(
    MiniSNNWorldsTerrain *terrain, MiniSNNWorldsKernel *kernel)
{
    MiniSNNWorldsKernelSpaceBounds expected_bounds;
    MiniSNNWorldsKernelSpaceBounds actual_bounds;
    MiniSNNWorldsKernelEntityId *blockers = NULL;
    MiniSNNWorldsKernelSnapshot *baseline = NULL;
    MiniSNNWorldsKernelSnapshot *completed = NULL;
    MiniSNNWorldsKernel *working = NULL;
    size_t count;
    size_t base_count;
    size_t index;

    if (terrain == NULL || kernel == NULL)
    {
        return MINISNN_WORLDS_TERRAIN_ERROR_NULL_ARGUMENT;
    }
    if (!terrain->finalized)
    {
        set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_INVALID_STATE);
        return MINISNN_WORLDS_TERRAIN_ERROR_INVALID_STATE;
    }
    if (terrain->materialized)
    {
        set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_ALREADY_MATERIALIZED);
        return MINISNN_WORLDS_TERRAIN_ERROR_ALREADY_MATERIALIZED;
    }
    if (!terrain_bounds(terrain, &expected_bounds) ||
        minisnn_worlds_kernel_space_bounds(kernel, &actual_bounds) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        memcmp(&expected_bounds, &actual_bounds, sizeof(expected_bounds)) != 0)
    {
        set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_KERNEL_BOUNDS_MISMATCH);
        return MINISNN_WORLDS_TERRAIN_ERROR_KERNEL_BOUNDS_MISMATCH;
    }
    if (!terrain_blocker_count(terrain, &count))
    {
        set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_INVALID_STATE);
        return MINISNN_WORLDS_TERRAIN_ERROR_INVALID_STATE;
    }
    if (count == 0U)
    {
        terrain->materialized = true;
        terrain->materialized_kernel = kernel;
        set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_NONE);
        return MINISNN_WORLDS_TERRAIN_ERROR_NONE;
    }
    blockers = calloc(count, sizeof(*blockers));
    if (blockers == NULL)
    {
        set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_ALLOCATION);
        return MINISNN_WORLDS_TERRAIN_ERROR_ALLOCATION;
    }
    if (minisnn_worlds_kernel_snapshot_capture(kernel, &baseline) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_create_from_snapshot(baseline, &working) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        goto kernel_failure;
    }
    base_count = minisnn_worlds_kernel_entity_count(working);
    if (!queue_creates(working, count))
    {
        goto kernel_failure;
    }
    for (index = 0U; index < count; ++index)
    {
        if (minisnn_worlds_kernel_entity_at(working, base_count + index,
                                            &blockers[index]) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            goto kernel_failure;
        }
    }
    if (!queue_placements(terrain, working, blockers, count) ||
        !queue_occupancies(terrain, working, blockers, count) ||
        minisnn_worlds_kernel_snapshot_capture(working, &completed) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_snapshot_restore(kernel, completed) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        goto kernel_failure;
    }
    minisnn_worlds_kernel_snapshot_destroy(completed);
    minisnn_worlds_kernel_snapshot_destroy(baseline);
    minisnn_worlds_kernel_destroy(working);
    free(blockers);
    terrain->materialized = true;
    terrain->materialized_kernel = kernel;
    set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    return MINISNN_WORLDS_TERRAIN_ERROR_NONE;

kernel_failure:
    minisnn_worlds_kernel_snapshot_destroy(completed);
    minisnn_worlds_kernel_snapshot_destroy(baseline);
    minisnn_worlds_kernel_destroy(working);
    free(blockers);
    set_error(terrain, MINISNN_WORLDS_TERRAIN_ERROR_KERNEL_FAILURE);
    return MINISNN_WORLDS_TERRAIN_ERROR_KERNEL_FAILURE;
}


MiniSNNWorldsTerrainError minisnn_worlds_terrain_hash(
    const MiniSNNWorldsTerrain *terrain, uint64_t *out_hash)
{
    uint64_t hash = TERRAIN_FNV_OFFSET;
    size_t index;

    if (terrain == NULL || out_hash == NULL)
    {
        return MINISNN_WORLDS_TERRAIN_ERROR_NULL_ARGUMENT;
    }
    fnv_literal(&hash, "MSWT_TERRAIN_V1");
    fnv_u32(&hash, MINISNN_WORLDS_TERRAIN_HASH_VERSION_V1);
    fnv_u32(&hash, terrain->config.width);
    fnv_u32(&hash, terrain->config.height);
    fnv_u64(&hash, (uint64_t)terrain->config.tile_size);
    fnv_u64(&hash, (uint64_t)terrain->config.origin_x);
    fnv_u64(&hash, (uint64_t)terrain->config.origin_y);
    fnv_byte(&hash, terrain->finalized ? UINT8_C(1) : UINT8_C(0));
    for (index = 0U; index < terrain->cell_count; ++index)
    {
        fnv_byte(&hash, terrain->tiles[index]);
        fnv_byte(&hash, terrain->rocks[index]);
    }
    *out_hash = hash;
    set_error((MiniSNNWorldsTerrain *)terrain, MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    return MINISNN_WORLDS_TERRAIN_ERROR_NONE;
}
