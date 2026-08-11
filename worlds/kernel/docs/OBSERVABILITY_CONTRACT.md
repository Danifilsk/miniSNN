# Contrato de observabilidade K0-C

`MiniSNNWorldsKernelDiagnostics` preserva os campos anteriores e adiciona
seed, numero de streams, total de draws brutos, hash corrente e versoes de
hash e PRNG. O diagnostico e uma copia: em erro, o buffer do consumidor nao e
alterado.

`minisnn_worlds_kernel_random_stream_at` devolve uma copia de key, state,
sequence e contador de draws. Nao existe setter publico de stream. A API
`minisnn_worlds_kernel_capture_trace_point` resume tick, hash, entidades
vivas, fila pendente, eventos do ultimo tick e estatisticas de aleatoriedade.

Eventos continuam representando somente fatos causais de comandos. K0-C nao
emite um evento por draw. Um erro de API, como chave invalida, fica observavel
por `last_error`, mas nao faz parte do estado causal nem do hash. K0-C tambem
nao preserva historico de traces ou eventos entre execucoes.

K0-D is the external consumer that converts copied diagnostics and events into
canonical technical CSV artifacts. The Kernel itself still retains no file,
CSV, manifest, or report responsibility.

## K1-A Space And Transforms

K0 and K1 are complete. K1-A through K1-C4 form the deterministic spatial foundation; K2-A through K2-D are complete; WD0 - Worlds Domain is complete; WB0 - Brain Bridge minimo is the next Worlds block. The public Worlds
Kernel now exposes a single immutable 2D fixed-point space, optional entity
transforms, and command-only placement/removal. See the K1-A coordinate and
transform contracts in worlds/kernel/docs. K2-A through K2-D are complete; WD0 - Worlds Domain is complete; WB0 - Brain Bridge minimo is the next Worlds block.
## K1-B1 Occupancy

K1-B1/B2 and K1-C1..C4 are complete; canonical spatial links, rigid subtree translation and V5 are part of completed K1. K2-A through K2-D are complete; WD0 - Worlds Domain is complete; WB0 - Brain Bridge minimo is the next Worlds block. The Worlds Kernel supports one optional fixed-point axis-aligned occupancy per
entity, generic category bits and blocking masks, command-only set/clear,
deterministic conflict rejection with related_entity, diagnostics, and
canonical state hash v3. Orientation does not rotate the AABB. See
worlds/kernel/docs/OCCUPANCY_AND_BARRIER_CONTRACT.md and
worlds/kernel/docs/K1_B1_OCCUPANCY_BARRIER_AUDIT.md.

## K1-B2 Atomic Movement

K1-B2 is complete. MOVE_ENTITY applies checked fixed-point deltas in canonical tick order, preserves orientation, validates the destination and optional AABB, and emits origin/destination event data. Semantic rejection never mutates official placement. See MOVEMENT_AND_DISPLACEMENT_CONTRACT.md and K1_B2_MOVEMENT_AUDIT.md.

K1-B2 adds `total_movement_commands_processed`, `total_entities_moved` and
`total_movement_overflows_rejected`. Applied movement events carry both
previous and destination transforms; rejected moves retain the canonical
rejection and optional `related_entity`.
## K1-C1 observability

Diagnostics expose active links plus total created, removed and processed
link commands. Events expose an explicit optional link payload and an
`affected_entity` field set to zero in C1. Copy queries use canonical
`(parent, child)` order and never expose mutable internal records.

## K1-C3

Os contadores de entidades, ocupacao, movimento, links, comandos e eventos sao verificados antes de serem promovidos. Em builds de teste, o validador de invariantes tambem confirma que `active_spatial_links` deriva do vetor canonico de links e que eventos causais permanecem coerentes.
