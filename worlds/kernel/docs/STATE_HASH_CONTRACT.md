# Contrato de hash de estado K0-C

`minisnn_worlds_kernel_state_hash` produz uma assinatura diagnostica
deterministica, nao criptografica, baseada em FNV-1a 64-bit e na versao
`MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V1 == 1` para estados K0 historicamente compativeis; a macro atual `MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION` e V5. Ela pode ser calculada apenas
quando o Kernel esta `READY`, nao aloca e nao altera o objeto nem `last_error`.

O encoding e explicito: inteiros usam bytes little-endian, booleanos usam um
byte zero ou um e enums usam `uint32_t`. Nenhuma struct e hasheada como bloco
de memoria. O contrato inclui magic, versoes, seed, tick, proximos IDs,
entidades vivas e mortas por ID, comandos pendentes em ordem canonica, eventos
do ultimo tick em ordem de emissao, contadores causais e streams por chave
canonica com estado, sequencia e draws.

O hash exclui ponteiros, padding, capacidades, ordem fisica de armazenamento,
`last_error`, estado temporario, caminhos, timestamps e metadados de build.
Logo, uma chamada invalida que apenas atualiza `last_error` nao muda a
assinatura, mas um comando aceito, draw, tick ou evento comprometido muda.

Vetores para estados vazios, tick vazio, comando pendente, entidade criada e
primeiro draw estao em `test_k0_c_hash.c`. Uma alteracao deliberada do encoding
exige nova versao do hash e migracao documentada.

K0-D writes this value as fixed hexadecimal in `trace.csv` and checks that the
last trace value equals the manifest final hash before publishing artifacts.
Its separate scenario configuration signature is never named a state hash.

## K1-A Space And Transforms

K0 and K1 are complete. K1-A through K1-C4 form the deterministic spatial foundation; K2-A through K2-D are complete; WD0 - Worlds Domain is complete; WB0 - Brain Bridge minimo is the next Worlds block. The public Worlds
Kernel now exposes a single immutable 2D fixed-point space, optional entity
transforms, and command-only placement/removal. See the K1-A coordinate and
transform contracts in worlds/kernel/docs.

## K1-B1 Occupancy

K1-B1 and K1-B2 are complete. K1-C1 now adds canonical spatial links while
preserving the historical V3 occupancy hash contract. The Worlds Kernel supports one optional fixed-point axis-aligned occupancy per
entity, generic category bits and blocking masks, command-only set/clear,
deterministic conflict rejection with related_entity, diagnostics, and
canonical state hash v3. Orientation does not rotate the AABB. See
worlds/kernel/docs/OCCUPANCY_AND_BARRIER_CONTRACT.md and
worlds/kernel/docs/K1_B1_OCCUPANCY_BARRIER_AUDIT.md.

## K1-B2 Atomic Movement

K1-B2 is complete. MOVE_ENTITY applies checked fixed-point deltas in canonical tick order, preserves orientation, validates the destination and optional AABB, and emits origin/destination event data. Semantic rejection never mutates official placement. See MOVEMENT_AND_DISPLACEMENT_CONTRACT.md and K1_B2_MOVEMENT_AUDIT.md.
## State hash V5

V5 extends V4 with canonical spatial links, pending endpoint payloads, link
event payloads including zero `affected_entity`, and K1-C1 counters. Dynamic
selection remains V1-V4 for strictly compatible historical states. Any active,
pending or observed K1-C1 state, or nonzero C1 counter, selects V5.

Reference V5 vector: two placed entities at (0, 0) and (10, 0) with an active
1 -> 2 spatial link hash to 0xF6C92E0389E076CD in test_k1_c1_spatial_links.c.

## K1-C2 subtree translation

State hash V5 remains the selected version for spatial-link state. A rigid subtree movement is represented by each resulting transform, the ordered root and causal movement events, their consecutive EventIds, event count, movement counters and rejection `affected_entity`. The public displacement is derivable from the previous and resulting transforms of every accepted movement event, so V5 remains unchanged. No V6 is introduced.

## K1-C3

C3 conserva a versao V5. O bloco adiciona testes de distincao e determinismo para variantes de links, transforms, ocupacao, eventos e diagnosticos, e confirma que consultas canonicas nao alteram o hash. Nao ha V6 neste bloco.
