#ifndef MINISNN_WORLDS_TERRAIN_H
#define MINISNN_WORLDS_TERRAIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "minisnn_worlds_kernel.h"

#define MINISNN_WORLDS_TERRAIN_HASH_VERSION_V1 UINT32_C(1)

typedef struct MiniSNNWorldsTerrain MiniSNNWorldsTerrain;

typedef enum
{
    MINISNN_WORLDS_TERRAIN_TILE_WATER = 1,
    MINISNN_WORLDS_TERRAIN_TILE_LAND = 2
} MiniSNNWorldsTerrainTile;

typedef enum
{
    MINISNN_WORLDS_TERRAIN_ERROR_NONE = 0,
    MINISNN_WORLDS_TERRAIN_ERROR_NULL_ARGUMENT,
    MINISNN_WORLDS_TERRAIN_ERROR_INVALID_CONFIG,
    MINISNN_WORLDS_TERRAIN_ERROR_INVALID_TILE,
    MINISNN_WORLDS_TERRAIN_ERROR_OUT_OF_BOUNDS,
    MINISNN_WORLDS_TERRAIN_ERROR_FINALIZED,
    MINISNN_WORLDS_TERRAIN_ERROR_DUPLICATE_ROCK,
    MINISNN_WORLDS_TERRAIN_ERROR_ROCK_NOT_FOUND,
    MINISNN_WORLDS_TERRAIN_ERROR_ALLOCATION,
    MINISNN_WORLDS_TERRAIN_ERROR_OVERFLOW,
    MINISNN_WORLDS_TERRAIN_ERROR_INVALID_STATE,
    MINISNN_WORLDS_TERRAIN_ERROR_KERNEL_FAILURE,
    MINISNN_WORLDS_TERRAIN_ERROR_KERNEL_BOUNDS_MISMATCH,
    MINISNN_WORLDS_TERRAIN_ERROR_ALREADY_MATERIALIZED
} MiniSNNWorldsTerrainError;

typedef struct
{
    uint32_t width;
    uint32_t height;
    MiniSNNWorldsKernelScalar tile_size;
    MiniSNNWorldsKernelScalar origin_x;
    MiniSNNWorldsKernelScalar origin_y;
} MiniSNNWorldsTerrainConfig;

/* Tile (0,0) owns [origin, origin + tile_size); coordinates grow +X, +Y. */
MiniSNNWorldsTerrain *minisnn_worlds_terrain_create(
    const MiniSNNWorldsTerrainConfig *config,
    MiniSNNWorldsTerrainError *out_error);
void minisnn_worlds_terrain_destroy(MiniSNNWorldsTerrain *terrain);
MiniSNNWorldsTerrainError minisnn_worlds_terrain_last_error(
    const MiniSNNWorldsTerrain *terrain);
uint32_t minisnn_worlds_terrain_width(const MiniSNNWorldsTerrain *terrain);
uint32_t minisnn_worlds_terrain_height(const MiniSNNWorldsTerrain *terrain);
MiniSNNWorldsKernelScalar minisnn_worlds_terrain_tile_size(
    const MiniSNNWorldsTerrain *terrain);
bool minisnn_worlds_terrain_is_finalized(const MiniSNNWorldsTerrain *terrain);

MiniSNNWorldsTerrainError minisnn_worlds_terrain_get_tile(
    const MiniSNNWorldsTerrain *terrain, uint32_t tile_x, uint32_t tile_y,
    MiniSNNWorldsTerrainTile *out_tile);
MiniSNNWorldsTerrainError minisnn_worlds_terrain_set_tile(
    MiniSNNWorldsTerrain *terrain, uint32_t tile_x, uint32_t tile_y,
    MiniSNNWorldsTerrainTile tile);
MiniSNNWorldsTerrainError minisnn_worlds_terrain_world_to_tile(
    const MiniSNNWorldsTerrain *terrain, MiniSNNWorldsKernelPosition world_position,
    uint32_t *out_tile_x, uint32_t *out_tile_y);
MiniSNNWorldsTerrainError minisnn_worlds_terrain_tile_to_world(
    const MiniSNNWorldsTerrain *terrain, uint32_t tile_x, uint32_t tile_y,
    MiniSNNWorldsKernelPosition *out_world_position);
MiniSNNWorldsTerrainError minisnn_worlds_terrain_space_bounds(
    const MiniSNNWorldsTerrain *terrain,
    MiniSNNWorldsKernelSpaceBounds *out_bounds);

MiniSNNWorldsTerrainError minisnn_worlds_terrain_add_rock(
    MiniSNNWorldsTerrain *terrain, uint32_t tile_x, uint32_t tile_y);
MiniSNNWorldsTerrainError minisnn_worlds_terrain_remove_rock(
    MiniSNNWorldsTerrain *terrain, uint32_t tile_x, uint32_t tile_y);
MiniSNNWorldsTerrainError minisnn_worlds_terrain_has_rock(
    const MiniSNNWorldsTerrain *terrain, uint32_t tile_x, uint32_t tile_y,
    bool *out_has_rock);
/* LAND and ROCK block. OUT_OF_BOUNDS is rejected and therefore blocked. */
MiniSNNWorldsTerrainError minisnn_worlds_terrain_is_blocked(
    const MiniSNNWorldsTerrain *terrain, uint32_t tile_x, uint32_t tile_y,
    bool *out_blocked);
size_t minisnn_worlds_terrain_land_count(const MiniSNNWorldsTerrain *terrain);
size_t minisnn_worlds_terrain_rock_count(const MiniSNNWorldsTerrain *terrain);

/* Freeze configuration before materialization. */
MiniSNNWorldsTerrainError minisnn_worlds_terrain_finalize(
    MiniSNNWorldsTerrain *terrain);
/*
 * Materializes generic Kernel blockers in canonical row-major order: LAND-only
 * cells first, then ROCK cells. A ROCK on LAND uses one ROCK blocker. Each
 * blocker uses a generic Terrain category and blocks every valid occupancy
 * category. The Kernel must use terrain_space_bounds() so out-of-map movement
 * is blocked. Publication is transactional: failure leaves Kernel state intact.
 */
MiniSNNWorldsTerrainError minisnn_worlds_terrain_materialize(
    MiniSNNWorldsTerrain *terrain, MiniSNNWorldsKernel *kernel);
MiniSNNWorldsTerrainError minisnn_worlds_terrain_hash(
    const MiniSNNWorldsTerrain *terrain, uint64_t *out_hash);

#endif
