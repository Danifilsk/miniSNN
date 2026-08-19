# WT0 Terrain

WT0 is the static, headless Terrain Grid V1 for Worlds. Production Terrain depends only on the public Kernel API.

- Base tiles: WATER and LAND.
- Spatial object: ROCK, separate from the base tile, so WATER plus ROCK is valid.
- Coordinates use Kernel fixed-point integers. Tile (0,0) owns the half-open area beginning at the configured origin; tile centers are returned by the API.
- Configure, finalize, then materialize. Logical mutations are rejected after finalization.
- Materialization creates generic Kernel blockers in canonical row-major order: LAND-only cells first, then ROCK cells. Kernel EntityIds remain Kernel-owned.
- The scenario Kernel must be configured with minisnn_worlds_terrain_space_bounds(); this makes out-of-map movement reject through normal Kernel occupancy/bounds semantics.

Terrain Hash V1 covers geometry, origin, base tiles, rocks, and finalized state. It never includes allocation details, pointers, EntityIds, asset paths, or sprite names.

Terrain is static scenario configuration in WT0. There is no Terrain Snapshot V1: K2 persists the materialized Kernel blockers, while a scenario loader recreates the logical Terrain definition from configuration.

Run from the repository root:

mingw32-make test-wt0
mingw32-make demo-wt0
mingw32-make check-wt0
mingw32-make audit-wt0

The demo creates only wt0_map.txt and wt0_summary.txt in build/worlds/terrain/results/wt0_terrain_demo/.
