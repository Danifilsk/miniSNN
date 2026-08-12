# K1-B1 Occupancy And Generic Barrier Contract

K1-B1 adds one optional axis-aligned bounding box (AABB) descriptor to each
alive Worlds Kernel entity. The descriptor is fixed-point and centered on the
authoritative entity transform:

- half_extent_x and half_extent_y must both be greater than zero;
- category_bits must be nonzero;
- blocking_mask may be zero;
- transform orientation remains authoritative entity state, but never rotates
  the AABB.

An occupancy is active only when its entity is alive, placed, and has a
descriptor. A configured descriptor on an unplaced entity is inactive.

The checked envelope is position plus or minus half_extent on each axis. The
complete envelope must remain in space bounds. Edge contact is allowed: only
positive intersection area is overlap. Two active AABBs conflict only when
they have positive overlap and either blocking mask contains a category bit of
the other descriptor. Blocking is intentionally unilateral-capable.

Production mutation is command-only through SET_OCCUPANCY and CLEAR_OCCUPANCY.
A SET replaces the existing descriptor atomically and is idempotent. CLEAR
removes only the descriptor; it preserves a valid transform. Removing an
entity from space preserves its descriptor but deactivates it. Destroying an
entity removes both transform and descriptor while emitting only
ENTITY_DESTROYED.

Placement validates the descriptor envelope and conflicts before committing.
A deterministic linear scan by ascending Entity ID selects the lowest
conflicting entity for related_entity. The Kernel does not perform movement,
swept tests, rotation, spatial queries, links, or domain interpretation.
A nonempty blocking mask is merely a generic barrier; category meanings are
outside the Kernel.


Stress validation includes a dedicated phase of 1000 positive-area, blocking AABB overlaps. Each placement is semantically rejected with OCCUPANCY_CONFLICT, retains no transform on the candidate entity, and reports the canonical related_entity.

## K1-B2 Atomic Movement

K1-B2 is complete. MOVE_ENTITY applies checked fixed-point deltas in canonical tick order, preserves orientation, validates the destination and optional AABB, and emits origin/destination event data. Semantic rejection never mutates official placement. See MOVEMENT_AND_DISPLACEMENT_CONTRACT.md and K1_B2_MOVEMENT_AUDIT.md.
