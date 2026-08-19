#include "g0_world_config.h"

#include <stdlib.h>
#include <string.h>

static int g0_world_config_dimensions_valid(uint32_t width, uint32_t height)
{
    return width >= G0_WORLD_CONFIG_MIN_WIDTH &&
           width <= G0_WORLD_CONFIG_MAX_WIDTH &&
           height >= G0_WORLD_CONFIG_MIN_HEIGHT &&
           height <= G0_WORLD_CONFIG_MAX_HEIGHT;
}

static int g0_world_config_offset(const G0WorldConfig *config, uint32_t tile_x,
                                  uint32_t tile_y, size_t *out_offset)
{
    size_t offset;

    if (config == NULL || out_offset == NULL || config->tiles == NULL ||
        config->rocks == NULL || config->foods == NULL || tile_x >= config->width ||
        tile_y >= config->height)
    {
        return 0;
    }
    offset = (size_t)tile_y * (size_t)config->width + (size_t)tile_x;
    *out_offset = offset;
    return 1;
}

int g0_world_config_init(G0WorldConfig *config, uint32_t width, uint32_t height)
{
    size_t count;

    if (config == NULL || !g0_world_config_dimensions_valid(width, height))
    {
        return 0;
    }
    memset(config, 0, sizeof(*config));
    count = (size_t)width * (size_t)height;
    config->tiles = calloc(count, sizeof(*config->tiles));
    config->rocks = calloc(count, sizeof(*config->rocks));
    config->foods = calloc(count, sizeof(*config->foods));
    if (config->tiles == NULL || config->rocks == NULL || config->foods == NULL)
    {
        g0_world_config_destroy(config);
        return 0;
    }
    config->width = width;
    config->height = height;
    config->fish_spawn_x = width / UINT32_C(2);
    config->fish_spawn_y = height / UINT32_C(2);
    while (count > 0U)
    {
        config->tiles[--count] = MINISNN_WORLDS_TERRAIN_TILE_WATER;
    }
    return 1;
}

void g0_world_config_destroy(G0WorldConfig *config)
{
    if (config == NULL)
    {
        return;
    }
    free(config->tiles);
    free(config->rocks);
    free(config->foods);
    memset(config, 0, sizeof(*config));
}

int g0_world_config_clone(const G0WorldConfig *source, G0WorldConfig *out_copy)
{
    size_t count;

    if (source == NULL || out_copy == NULL || !g0_world_config_validate(source) ||
        !g0_world_config_init(out_copy, source->width, source->height))
    {
        return 0;
    }
    count = (size_t)source->width * (size_t)source->height;
    memcpy(out_copy->tiles, source->tiles, count * sizeof(*source->tiles));
    memcpy(out_copy->rocks, source->rocks, count * sizeof(*source->rocks));
    memcpy(out_copy->foods, source->foods, count * sizeof(*source->foods));
    out_copy->fish_spawn_x = source->fish_spawn_x;
    out_copy->fish_spawn_y = source->fish_spawn_y;
    return 1;
}

int g0_world_config_resize(G0WorldConfig *config, uint32_t width, uint32_t height)
{
    G0WorldConfig candidate;
    uint32_t copy_width;
    uint32_t copy_height;
    uint32_t tile_x;
    uint32_t tile_y;

    if (config == NULL || !g0_world_config_validate(config) ||
        !g0_world_config_dimensions_valid(width, height) ||
        config->fish_spawn_x >= width || config->fish_spawn_y >= height)
    {
        return 0;
    }
    if (width == config->width && height == config->height)
    {
        return 1;
    }
    memset(&candidate, 0, sizeof(candidate));
    if (!g0_world_config_init(&candidate, width, height))
    {
        return 0;
    }
    candidate.fish_spawn_x = config->fish_spawn_x;
    candidate.fish_spawn_y = config->fish_spawn_y;
    copy_width = width < config->width ? width : config->width;
    copy_height = height < config->height ? height : config->height;
    for (tile_y = 0U; tile_y < copy_height; ++tile_y)
    {
        for (tile_x = 0U; tile_x < copy_width; ++tile_x)
        {
            size_t source_offset = (size_t)tile_y * (size_t)config->width + (size_t)tile_x;
            size_t target_offset = (size_t)tile_y * (size_t)candidate.width + (size_t)tile_x;

            candidate.tiles[target_offset] = config->tiles[source_offset];
            candidate.rocks[target_offset] = config->rocks[source_offset];
            candidate.foods[target_offset] = config->foods[source_offset];
        }
    }
    if (!g0_world_config_validate(&candidate))
    {
        g0_world_config_destroy(&candidate);
        return 0;
    }
    g0_world_config_destroy(config);
    *config = candidate;
    return 1;
}

int g0_world_config_validate(const G0WorldConfig *config)
{
    size_t count;
    size_t index;

    if (config == NULL || !g0_world_config_dimensions_valid(config->width, config->height) ||
        config->tiles == NULL || config->rocks == NULL || config->foods == NULL ||
        config->fish_spawn_x >= config->width || config->fish_spawn_y >= config->height)
    {
        return 0;
    }
    count = (size_t)config->width * (size_t)config->height;
    for (index = 0U; index < count; ++index)
    {
        if ((config->tiles[index] != MINISNN_WORLDS_TERRAIN_TILE_WATER &&
             config->tiles[index] != MINISNN_WORLDS_TERRAIN_TILE_LAND) ||
            config->rocks[index] > 1U || config->foods[index] > 1U ||
            (config->tiles[index] == MINISNN_WORLDS_TERRAIN_TILE_LAND &&
             config->foods[index] != 0U))
        {
            return 0;
        }
    }
    index = (size_t)config->fish_spawn_y * (size_t)config->width +
            (size_t)config->fish_spawn_x;
    return config->tiles[index] == MINISNN_WORLDS_TERRAIN_TILE_WATER &&
           config->rocks[index] == 0U;
}

int g0_world_config_set_tile(
    G0WorldConfig *config,
    uint32_t tile_x,
    uint32_t tile_y,
    MiniSNNWorldsTerrainTile tile)
{
    size_t offset;

    if (!g0_world_config_offset(config, tile_x, tile_y, &offset) ||
        (tile != MINISNN_WORLDS_TERRAIN_TILE_WATER &&
         tile != MINISNN_WORLDS_TERRAIN_TILE_LAND))
    {
        return 0;
    }
    config->tiles[offset] = tile;
    return 1;
}

int g0_world_config_set_rock(G0WorldConfig *config, uint32_t tile_x, uint32_t tile_y,
                             int present)
{
    size_t offset;

    if (!g0_world_config_offset(config, tile_x, tile_y, &offset))
    {
        return 0;
    }
    config->rocks[offset] = present != 0 ? 1U : 0U;
    return 1;
}

int g0_world_config_set_food(G0WorldConfig *config, uint32_t tile_x, uint32_t tile_y,
                             int present)
{
    size_t offset;

    if (!g0_world_config_offset(config, tile_x, tile_y, &offset))
    {
        return 0;
    }
    if (present != 0 &&
        config->tiles[offset] != MINISNN_WORLDS_TERRAIN_TILE_WATER)
    {
        return 0;
    }
    config->foods[offset] = present != 0 ? 1U : 0U;
    return 1;
}

int g0_world_config_set_fish_spawn(G0WorldConfig *config, uint32_t tile_x,
                                   uint32_t tile_y)
{
    size_t offset;

    if (!g0_world_config_offset(config, tile_x, tile_y, &offset) ||
        config->tiles[offset] != MINISNN_WORLDS_TERRAIN_TILE_WATER ||
        config->rocks[offset] != 0U)
    {
        return 0;
    }
    config->fish_spawn_x = tile_x;
    config->fish_spawn_y = tile_y;
    return 1;
}

int g0_world_config_tile(const G0WorldConfig *config, uint32_t tile_x, uint32_t tile_y,
                         MiniSNNWorldsTerrainTile *out_tile)
{
    size_t offset;

    if (out_tile == NULL || !g0_world_config_offset(config, tile_x, tile_y, &offset))
    {
        return 0;
    }
    *out_tile = config->tiles[offset];
    return 1;
}

int g0_world_config_has_rock(const G0WorldConfig *config, uint32_t tile_x,
                             uint32_t tile_y, int *out_present)
{
    size_t offset;

    if (out_present == NULL || !g0_world_config_offset(config, tile_x, tile_y, &offset))
    {
        return 0;
    }
    *out_present = config->rocks[offset] != 0U;
    return 1;
}

int g0_world_config_has_food(const G0WorldConfig *config, uint32_t tile_x,
                             uint32_t tile_y, int *out_present)
{
    size_t offset;

    if (out_present == NULL || !g0_world_config_offset(config, tile_x, tile_y, &offset))
    {
        return 0;
    }
    *out_present = config->foods[offset] != 0U;
    return 1;
}

size_t g0_world_config_food_count(const G0WorldConfig *config)
{
    size_t count = 0U;
    size_t index;
    size_t tile_count;

    if (config == NULL || config->foods == NULL)
    {
        return 0U;
    }
    tile_count = (size_t)config->width * (size_t)config->height;
    for (index = 0U; index < tile_count; ++index)
    {
        count += config->foods[index] != 0U;
    }
    return count;
}
