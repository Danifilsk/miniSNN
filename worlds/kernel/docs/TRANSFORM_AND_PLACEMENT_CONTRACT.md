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
