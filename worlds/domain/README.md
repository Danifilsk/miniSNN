# miniSNN Worlds Domain

`worlds/domain/` is the WD0 semantic layer, extended by WD1 persistence. It is a C11, headless library that
uses only the public API of `worlds/kernel/`. It does not depend on `core/`,
AgentIO, neurons, rendering, or a Brain Bridge.

The Kernel remains the authority for EntityId, transform, placement, occupancy,
movement, collision, commands, events, logical ticks, and Kernel state hash.
The Domain adds only the meanings `ORGANISM` and `FOOD`, species parameters,
integer energy, derived hunger, semantic actions, perception, events, counters,
and Domain State Hash V1.

## Build

```powershell
mingw32-make test-wd0-domain
mingw32-make test-wd0-actions
mingw32-make demo-wd0
mingw32-make check-wd0
mingw32-make audit-wd0
mingw32-make test-wd1-snapshot
mingw32-make test-wd1-file-roundtrip
mingw32-make demo-wd1
mingw32-make check-wd1
mingw32-make audit-wd1
```

The deterministic demo writes only under `build/worlds/domain/results/wd0_demo/`.
It creates two organisms, three foods, and one Kernel occupancy obstacle.

## Ownership and persistence

A Domain keeps a non-owning `MiniSNNWorldsKernel *`. Destroying the Domain does
not destroy the Kernel. Destroying the Kernel before its Domain is invalid.

Kernel Snapshot V1 persists generic Kernel state. Domain Snapshot V1 persists semantic
Domain state separately and binds restore to the matching Kernel tick and state hash.
An integrated Brain/Core checkpoint is still outside WD1.

## Scope

WD0 has no Brain Bridge, SNN, genetics, reproduction, health, damage, death,
corpses, predators, combat, social behavior, communication, pathfinding,
physics, rendering, or concrete fish. WD1 is complete. The next official milestone is
`WF0 - organismo headless`.

See `docs/WD0_MINIMAL_DOMAIN_CONTRACT.md`, `docs/WD0_MINIMAL_DOMAIN_AUDIT.md`,
`docs/WD1_DOMAIN_SNAPSHOT_CONTRACT.md`, and `docs/WD1_DOMAIN_PERSISTENCE_AUDIT.md`.
## Atomicidade de integracao

`minisnn_worlds_domain_step()` usa o provisional command batch generico do
Worlds Kernel. Falhas de queue/step fazem rollback antes do retorno, impedindo
comandos fantasmas e preservando a equivalencia entre estado semantico e estado
espacial.
