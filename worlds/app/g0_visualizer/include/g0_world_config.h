#ifndef G0_WORLD_CONFIG_H
#define G0_WORLD_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#include "minisnn_worlds_terrain.h"

#define G0_WORLD_CONFIG_MIN_WIDTH UINT32_C(5)
#define G0_WORLD_CONFIG_MIN_HEIGHT UINT32_C(5)
#define G0_WORLD_CONFIG_MAX_WIDTH UINT32_C(64)
#define G0_WORLD_CONFIG_MAX_HEIGHT UINT32_C(64)

typedef enum
{
    G0_WORLD_TOOL_WATER = 0,
    G0_WORLD_TOOL_LAND,
    G0_WORLD_TOOL_ROCK,
    G0_WORLD_TOOL_FOOD,
    G0_WORLD_TOOL_ERASE,
    G0_WORLD_TOOL_FISH_SPAWN
} G0WorldTool;

/*
 * Editable app-level blueprint. It is deliberately independent of the Kernel:
 * terrain, objects, food and the WF0 spawn are materialized only by the
 * visualizer runtime after validation succeeds.
 */
typedef struct
{
    uint32_t width;
    uint32_t height;
    MiniSNNWorldsTerrainTile *tiles;
    uint8_t *rocks;
    uint8_t *foods;
    uint32_t fish_spawn_x;
    uint32_t fish_spawn_y;
} G0WorldConfig;

int g0_world_config_init(G0WorldConfig *config, uint32_t width, uint32_t height);
void g0_world_config_destroy(G0WorldConfig *config);
int g0_world_config_clone(const G0WorldConfig *source, G0WorldConfig *out_copy);
/* Transactional resize preserves the old/new intersection. New cells are WATER. */
int g0_world_config_resize(G0WorldConfig *config, uint32_t width, uint32_t height);
int g0_world_config_validate(const G0WorldConfig *config);
int g0_world_config_set_tile(
    G0WorldConfig *config,
    uint32_t tile_x,
    uint32_t tile_y,
    MiniSNNWorldsTerrainTile tile);
int g0_world_config_set_rock(G0WorldConfig *config, uint32_t tile_x, uint32_t tile_y,
                             int present);
int g0_world_config_set_food(G0WorldConfig *config, uint32_t tile_x, uint32_t tile_y,
                             int present);
int g0_world_config_set_fish_spawn(G0WorldConfig *config, uint32_t tile_x,
                                   uint32_t tile_y);
int g0_world_config_tile(const G0WorldConfig *config, uint32_t tile_x, uint32_t tile_y,
                         MiniSNNWorldsTerrainTile *out_tile);
int g0_world_config_has_rock(const G0WorldConfig *config, uint32_t tile_x,
                             uint32_t tile_y, int *out_present);
int g0_world_config_has_food(const G0WorldConfig *config, uint32_t tile_x,
                             uint32_t tile_y, int *out_present);
size_t g0_world_config_food_count(const G0WorldConfig *config);

#endif
