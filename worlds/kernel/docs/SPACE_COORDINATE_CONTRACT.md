# Space Coordinate Contract

K1-A gives each Worlds Kernel instance one authoritative two-dimensional mathematical space.
The axes are +X east/right and +Y north/up. The origin has no domain meaning.
Screen coordinates, cameras, rendering, and floating point are outside the Kernel.

MiniSNNWorldsKernelScalar is signed int64 fixed point with scale 1000:
one internal unit is 0.001 conceptual units. Bounds and placement use inclusive
comparisons: min_x <= x <= max_x and min_y <= y <= max_y.

Bounds are immutable for the lifetime of an instance. The K0 configuration
prefix remains accepted and receives documented default bounds
[-1000000, 1000000] on both axes. A V1 struct that physically reaches the
complete `space_bounds` field supplies those bounds even when it has an
unknown trailing tail; only a physically truncated field receives defaults.
Arithmetic used by authoritative spatial state is checked before mutation; the
Kernel does not rely on floating point or compiler-specific extended integers.
## K1-B1 Occupancy

K1-B1/B2 and K1-C1..C4 are complete; canonical spatial links, rigid subtree translation and V5 are part of completed K1. K2-A through K2-D are complete; WD0 - Worlds Domain and WB0 - Brain Bridge minimo are complete; WD1 - Domain persistence is the next Worlds block. The Worlds Kernel supports one optional fixed-point axis-aligned occupancy per
entity, generic category bits and blocking masks, command-only set/clear,
deterministic conflict rejection with related_entity, diagnostics, and
canonical state hash v3. Orientation does not rotate the AABB. See
worlds/kernel/docs/OCCUPANCY_AND_BARRIER_CONTRACT.md and
worlds/kernel/docs/K1_B1_OCCUPANCY_BARRIER_AUDIT.md.

## K1-B2 Atomic Movement

K1-B2 is complete. MOVE_ENTITY applies checked fixed-point deltas in canonical tick order, preserves orientation, validates the destination and optional AABB, and emits origin/destination event data. Semantic rejection never mutates official placement. See MOVEMENT_AND_DISPLACEMENT_CONTRACT.md and K1_B2_MOVEMENT_AUDIT.md.
