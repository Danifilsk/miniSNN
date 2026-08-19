# WD2 Organism Lifecycle V1

WD2 makes organism lifecycle explicit in the Worlds Domain.

## Lifecycle

An organism is either ALIVE or DEAD. ALIVE uses canonical
death_cause=NONE and death_tick=0. WD2 V1 provides the causal death
STARVATION. A dead organism has zero energy, a non-NONE cause, and a
causal tick no later than the Domain tick.

The historical WD0 tick order remains unchanged:

1. action effects, including MOVE cost and successful EAT nutrition;
2. metabolism;
3. one-way ALIVE to DEAD transition when final energy is zero.

Therefore food can save a critically low-energy organism when the historical
EAT ordering allows it.

## Terminal behavior

A dead actor receives rejected WAIT, MOVE, and EAT results with
ACTOR_DEAD. It does not metabolize, move, consume food, or emit another
death event. organism_at remains an observer query and reports the lifecycle;
domain_perceive rejects a dead actor with ACTOR_DEAD.

The Domain emits one canonical ORGANISM_DIED event with the organism,
causal tick, and death cause. Physical removal, corpses, meat, and
decomposition remain deferred. Terminal reward and episode policy belong to
future consumers, not the Domain.

## Persistence and hash

The writer emits Domain Snapshot V2, persisting lifecycle and death-event
cause. The reader accepts V1 and V2. V1 organisms restore as ALIVE/NONE/0,
including a historical V1 organism with energy zero. Invalid lifecycle
combinations are rejected transactionally.

Lifecycle participates in Domain hashing. Fully default ALIVE/NONE/0 states
retain their WD0/WD1-compatible hash representation; a canonical lifecycle
extension is added only when non-default lifecycle state exists.

## Focused commands

    mingw32-make test-wd2-lifecycle
    mingw32-make test-wd2-persistence
    mingw32-make test-wd2-atomicity
    mingw32-make demo-wd2
    mingw32-make check-wd2
    mingw32-make audit-wd2