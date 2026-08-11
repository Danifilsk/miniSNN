# Transform And Placement Contract

A living entity may have zero or one transform. A transform contains a fixed-point
position and an orientation in thousandths of degrees. Orientation is in
[0, 359999], with 0 east and positive rotation counter-clockwise.

Transforms change only through future-tick PLACE_ENTITY and
REMOVE_ENTITY_FROM_SPACE commands. Placement requires a living, unplaced
target and a valid in-bounds transform. Removal requires a living, placed
target. Conflicts resolve in the existing canonical command order:
target tick, priority, issuer, command ID.

Destroying a placed entity clears its transform atomically and emits only the
existing entity-destroyed event. K1-A deliberately has no movement, occupancy,
shape, collision, barrier, spatial query, link, or domain semantics. Those are
reserved for K1-B and K1-C.
## K1-B1 Occupancy

K1-B1/B2 and K1-C1..C4 are complete; spatial links, rigid subtree translation and state hash V5 are part of completed K1. K2-A through K2-D are complete; WD0 - Worlds Domain is complete; WB0 - Brain Bridge minimo is the next Worlds block. The Worlds Kernel supports one optional fixed-point axis-aligned occupancy per
entity, generic category bits and blocking masks, command-only set/clear,
deterministic conflict rejection with related_entity, diagnostics, and
canonical state hash v3. Orientation does not rotate the AABB. See
worlds/kernel/docs/OCCUPANCY_AND_BARRIER_CONTRACT.md and
worlds/kernel/docs/K1_B1_OCCUPANCY_BARRIER_AUDIT.md.

## K1-B2 Atomic Movement

K1-B2 is complete. MOVE_ENTITY applies checked fixed-point deltas in canonical tick order, preserves orientation, validates the destination and optional AABB, and emits origin/destination event data. Semantic rejection never mutates official placement. See MOVEMENT_AND_DISPLACEMENT_CONTRACT.md and K1_B2_MOVEMENT_AUDIT.md.
## K1-C1 link lifecycle

A spatial link requires both endpoints to be alive and placed, but it neither
moves nor reorients either endpoint. Removing it preserves global transforms.
Destroying or removing an incident endpoint from space requires explicit
unlinking first; no implicit subtree or reparenting behavior exists in C1.
