# WD0 Minimal Domain Contract

## Architecture

The dependency direction is exactly `Worlds Domain -> Worlds Kernel public API`.
Domain headers include `minisnn_worlds_kernel.h`; they do not include Kernel
private headers and do not include Core headers. Kernel and Core do not include
Domain.

`MiniSNNWorldsDomain` is opaque and holds a non-owning Kernel pointer. Spatial
identity is the Kernel `EntityId`; Domain keeps no duplicate x/y or occupancy.

## Semantic records

WD0 has exactly two semantic kinds: `ORGANISM` and `FOOD`.

A species has a nonzero id, `max_energy`, `metabolism_per_tick`,
`move_energy_cost`, and nonnegative `eat_range`. An organism records the Kernel
entity id, species id, and integer energy. Hunger is derived exactly as
`max_energy - energy`; it is never stored independently. Food records only the
Kernel entity id and positive integer nutrition.

Energy is saturated when nutrition is added and saturated at zero by
metabolism. It never becomes negative or exceeds `max_energy`.

## Tick and actions

One Domain tick equals one Kernel logical tick. Before every step the Domain
requires `domain_tick == kernel_tick`.

1. Copy and order action intents by actor `EntityId` ascending, then input index.
2. Reject a second action from the same actor and complete semantic preflight.
3. Reserve event capacity and prove Domain event-id/counter headroom before touching the Kernel.
4. Open a generic provisional Kernel command batch and submit MOVE/EAT commands.
5. If queueing or `kernel_step` fails, roll the batch back; the official Kernel and Domain state stay unchanged.
6. If `kernel_step` succeeds, commit the batch, then confirm Kernel command events.
7. Charge movement only after `ENTITY_MOVED`; finalize food only after `ENTITY_DESTROYED`.
8. Apply metabolism and emit Domain events from pre-reserved storage. No post-commit allocation is required.

`WAIT` is accepted without a Kernel command. `MOVE` is an intent translated to
`MOVE_ENTITY`; Kernel collision and bounds rejection become Domain
`KERNEL_REJECTED` and cost no movement energy. `EAT` uses public transforms and
Manhattan distance. Same-tick competition is won by the lower actor `EntityId`;
the other action is `TARGET_NOT_AVAILABLE`.

## Perception and observability

`DomainPerception` carries self energy, derived hunger, and the nearest food.
Distance is Manhattan fixed-point distance; ties use lower food `EntityId`.
There are no neural fields.

Domain events are `ACTION_APPLIED`, `ACTION_REJECTED`, `FOOD_CONSUMED`, and
`ENERGY_CHANGED`. They have monotonic event ids, ticks, subject and related
entities, action/reason, and energy before/after. Domain State Hash V1 hashes
only semantic state: tick, species, organisms, foods, events, ids, and counters.
Kernel transform, occupancy, and spatial links remain outside the Domain hash.

## Persistence limit

Kernel K2 can persist Kernel state. WD0 does not persist Domain semantic state.
No cue, food, organism energy, species, or Domain event is checkpointable yet.
This is an explicit limitation, not an implicit fallback.
## Cross-layer atomicity

WD0 uses the Kernel provisional command batch as a generic integration boundary.
A failed Domain step leaves no queued Domain commands, does not advance either
tick, and preserves Kernel snapshot/hash plus Domain semantic hash. A retry after
an injected Kernel allocation failure is required to match a clean run exactly.
The batch marker is transient and is not part of Kernel Snapshot V1, Command Log
V1, or State Hash V5.
