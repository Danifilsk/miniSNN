# K1-C2 Subtree Translation Audit

**Status:** PASS

## Scope

K1-C2 replaces the temporary root-with-children movement rejection with one
atomic rigid translation. K1-C1 links remain a canonical directed forest; a
child still cannot receive a direct `MOVE_ENTITY`.

## Contract

The collector is iterative and breadth-first: root first, then each parent's
children in increasing EntityId order. It works from the planned entities and
canonical `(parent, child)` links, so earlier same-tick link commands are
observed.

For a nonzero root movement, every member receives the same checked delta.
All positions and occupancy envelopes are validated before transforms are
changed. Internal subtree occupancies are excluded only from this joint
preflight. External conflicts select the lowest blocker EntityId, then the
lowest affected movable EntityId. Overflow selects the lowest affected member.

A successful command emits the root `ENTITY_MOVED` followed by one causal
`ENTITY_MOVED` per descendant. They share the command id; descendants record
their immediate parent in `related_entity`. Rejections emit exactly one event.
`affected_entity` is zero for accepted movement and direct-child rejection,
and identifies the canonical overflowing or colliding member otherwise.

Delta zero retains K1-B2 behavior: only the root event is emitted and no
entity-moved counter is added.

## Atomicity and V5

Subtree collection, destinations, external collisions, EventId capacity,
event growth and counter limits are all preflighted inside `StepPlan`. Any
internal failure discards the whole plan: tick, pending commands, events,
links, transforms, occupancies, diagnostics and state hash remain official
unchanged. V5 already encodes the relevant transforms, ordered events,
EventIds, `affected_entity`, counts and spatial links. The displacement field
is derivable from an accepted event's previous and resulting transforms; no V6
is introduced.

## Evidence

`test_k1_c2_subtree_movement.c` covers rigid trees, canonical causal events,
zero delta, direct-child rejection, ignored internal overlap, canonical
external conflict and canonical overflow. It covers create-link then move,
move then create-link, remove-link then move-child, independent roots in one
tick, a 256-member chain, allocation rollback, EventId exhaustion and both
movement/event counter overflows. The O0/O2, sanitizer-classification and
POSIX gates reuse the shared C1 harness policy.

## Boundary

K1-C1 e K1-C2 estao concluidos. K1-C3 e K1-C4 foram posteriormente concluidos; este documento preserva apenas o escopo C2. This block does not add reparenting, offset mutation,
rotation inheritance, physics, pathfinding, persistence, a parser or a demo.
## Continuidade para K1-C3

K1-C3 posteriormente preservou a translacao rigida C2 e ampliou somente ordering, invariantes, limites e long runs. K1-C4 fechou demo, artefatos e stress sem ampliar a mecanica C2.
