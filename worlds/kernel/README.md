# miniSNN Worlds Kernel

K0-A, K0-B, K0-C e K0-D fornecem uma biblioteca estatica C11 independente para
lifecycle, tempo logico deterministico, entidades, comandos futuros, eventos,
PRNG por streams, hash canonico e observabilidade. A
biblioteca resultante e
`build/worlds/kernel/lib/libminisnn_worlds_kernel.a`.

```powershell
mingw32-make worlds-kernel
mingw32-make worlds-kernel-test
mingw32-make audit-k0-a
mingw32-make audit-k0-b
mingw32-make audit-k0-c
mingw32-make demo-k0-d
mingw32-make audit-k0-d
mingw32-make audit-k0
```

Um consumidor normal inclui somente `minisnn_worlds_kernel.h`. O Kernel nao
usa relogio real, threads, Core, Studio ou conceitos de dominio. A
aleatoriedade K0-C e PCG32 interno, versionado e derivado de seed explicita.
Consulte [a arquitetura](docs/ARCHITECTURE.md), o
[contrato de tempo](docs/TIME_AND_TICK_CONTRACT.md), o
[contrato de Entity IDs](docs/ENTITY_ID_CONTRACT.md) e o
[contrato de comandos e eventos](docs/COMMAND_AND_EVENT_CONTRACT.md), o
[contrato de aleatoriedade](docs/RANDOMNESS_CONTRACT.md), o
[contrato de hash](docs/STATE_HASH_CONTRACT.md) e o
[contrato de observabilidade](docs/OBSERVABILITY_CONTRACT.md), a
[configuracao de cenarios](docs/SCENARIO_CONFIG_CONTRACT.md) e os
[artefatos tecnicos](docs/ARTIFACT_CONTRACT.md).

K0 e K1 estao concluidos como fundacoes deterministicas. K1-A, K1-B1/B2 e K1-C1..C4 cobrem espaco, occupancy, movimento e links; K2-A concluiu o snapshot canonico em memoria, K2-B concluiu restore e save/load e K2-C concluiu command replay deterministico e K2-D concluiu auditoria final de persistencia; WD0 - Domain minimo e WB0 - Brain Bridge minimo estao concluidos; WD1 - persistencia do Domain e o proximo bloco. Veja `docs/K2_A_CANONICAL_SNAPSHOT_CONTRACT.md`, `docs/K2_B_RESTORE_AND_PERSISTENCE_CONTRACT.md`, `docs/K2_C_COMMAND_REPLAY_CONTRACT.md` e `docs/K2_D_PERSISTENCE_CLOSURE_AUDIT.md`. O Kernel continua headless e nao implementa Domain, Brain Bridge ou App.

## K1-A Space And Transforms

K0 and K1 are complete. K1-A through K1-C4 form the deterministic spatial foundation; K2-A through K2-D are complete; WD0 - Worlds Domain and WB0 - Brain Bridge minimo are complete; WD1 - Domain persistence is the next Worlds block. The public Worlds
Kernel now exposes a single immutable 2D fixed-point space, optional entity
transforms, and command-only placement/removal. See the K1-A coordinate and
transform contracts in worlds/kernel/docs.

## K1-B1 Occupancy

K1-B1/B2 e K1-C1..C4 estao concluidos; K2-A ate K2-D estao concluidos; WD0 - Domain minimo e WB0 - Brain Bridge minimo estao concluidos; WD1 - persistencia do Domain e o proximo bloco. Nao ha Domain, Brain Bridge ou aplicacao. The Worlds
Kernel now supports one optional fixed-point axis-aligned occupancy per
entity, generic category bits and blocking masks, command-only set/clear,
deterministic conflict rejection with related_entity, diagnostics, and
canonical state hash v3. Orientation does not rotate the AABB. See
worlds/kernel/docs/OCCUPANCY_AND_BARRIER_CONTRACT.md and
worlds/kernel/docs/K1_B1_OCCUPANCY_BARRIER_AUDIT.md. Sanitizer gates use an independent ASan/UBSan probe; after a valid probe, project failures are FAIL. The stress gate includes 1000 real positive-overlap blocking conflicts.
## K1-B2 Atomic Movement

K1-B2 is complete. The Worlds Kernel now supports command-only atomic fixed-point displacement, destination AABB validation, canonical conflict rejection, movement diagnostics and state hash v4. See worlds/kernel/docs/MOVEMENT_AND_DISPLACEMENT_CONTRACT.md and worlds/kernel/docs/K1_B2_MOVEMENT_AUDIT.md.
## K1-C1 Spatial Links

K1-C1 is complete. The public Kernel now stores canonical directed spatial
links `(parent, child)` as a forest, derives checked X/Y offsets at creation,
and exposes command-only create/remove plus copy-only queries. Lifecycle never
auto-unlinks: destroy and remove-from-space reject incident links. Direct moves
of a linked child remains rejected; a valid root translates its subtree atomically in K1-C2.
State hash V5 records spatial-link state while V1-V4 remain strict for older
compatible states. See `docs/SPATIAL_LINKS_CONTRACT_PROPOSAL.md` and
`docs/K1_C1_SPATIAL_LINKS_AUDIT.md`.

K1-C2 adiciona movimento rigido atomico de subarvores C1; C3 consolida ordering e hardening, e C4 fecha a integracao com configuracao, demo, golden e stress.

## K1-C4

mingw32-make demo-k1-c4 executa o cenario integrado de spatial links, gera artefatos
canonicos e valida o golden V5. mingw32-make audit-k1-c4 agrega o demo, repeticao de
artefatos e stress final. Consulte docs/K1_C4_INTEGRATION_AND_CLOSURE_AUDIT.md.