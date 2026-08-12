# API publica K0-A, K0-B e K0-C

O header agregado e `minisnn_worlds_kernel.h`. Todos os simbolos publicos usam
o namespace `minisnn_worlds_kernel_*`, os tipos usam `MiniSNNWorldsKernel*` e
as macros usam `MINISNN_WORLDS_KERNEL_*`.

`MiniSNNWorldsKernelConfig` contem `struct_size`, `format_version`, `master_seed`
e o tail opcional `space_bounds`. A funcao
`minisnn_worlds_kernel_config_default()` produz a configuracao deterministica
da versao atual. `create(NULL, ...)` usa esse default; uma configuracao
explicita exige versao atual e `struct_size` suficiente. O Kernel le primeiro
`struct_size`, confirma que o prefixo contem `format_version` e somente entao
le a versao. Estruturas pequenas demais sao rejeitadas antes da leitura de
campo ausente ou alocacao. Uma configuracao V1 cujo tamanho alcanca por
completo `master_seed` preserva essa seed; se nao alcanca `space_bounds`, usa
bounds default. Quando alcanca `space_bounds` completo, o Kernel le e valida
esse campo mesmo se a struct tiver uma cauda maior desconhecida. Os bytes
posteriores nao sao copiados nem interpretados.

Se uma configuracao V1 historica nao alcanca `master_seed`, o Kernel usa
`MINISNN_WORLDS_KERNEL_DEFAULT_MASTER_SEED`; seed zero continua valida.

O chamador ainda deve fornecer um ponteiro valido para pelo menos o primeiro
campo `struct_size`; um ponteiro totalmente invalido nao pode ser detectado de
forma portavel. K0-A nao promete aceitar versoes de formato que nao reconhece.

`minisnn_worlds_kernel_destroy(NULL)` e seguro. Os getters com kernel nulo
retornam respectivamente tick inicial, estado `FAULTED` e erro `NULL_ARGUMENT`.
`get_diagnostics` rejeita kernel ou output nulo sem alterar o output fornecido.

As interfaces sob `MINISNN_WORLDS_KERNEL_TESTING` sao excluidas da biblioteca
normal e existem apenas para os testes de overflow e alocacao.

K0-B acrescenta os headers `minisnn_worlds_kernel_entity.h`,
`minisnn_worlds_kernel_command.h` e `minisnn_worlds_kernel_event.h`, todos
incluidos pelo header agregado. `EntityId`, `CommandId` e `EventId` sao structs
fortes de 64 bits; zero e invalido. As consultas de entidades sao somente
leitura. Criacao e destruicao ocorrem exclusivamente por comandos agendados.

`queue_create_entity` e `queue_destroy_entity` exigem tick futuro e devolvem
Command ID apenas em sucesso. `pending_command_at` expoe a fila em ordem
canonica. `last_tick_event_at` expoe apenas a janela do ultimo tick concluido.
Consulte os contratos detalhados de [entidades](ENTITY_ID_CONTRACT.md) e de
[comandos e eventos](COMMAND_AND_EVENT_CONTRACT.md).

`MiniSNNWorldsKernelDiagnostics` conserva os campos K0-A e acrescenta entidades
vivas, totais criados e destruidos, fila pendente, comandos submetidos,
aplicados e rejeitados, eventos do ultimo tick e total emitido. Uma falha de
submissao nao incrementa `submitted`; uma rejeicao semantica durante o tick
incrementa `rejected` e o total de eventos.

K0-C acrescenta `minisnn_worlds_kernel_random.h` e
`minisnn_worlds_kernel_hash.h`. `random_u32`, `random_u64` e
`random_bounded_u32` usam uma chave forte de dois `uint64_t` e publicam output
somente em sucesso. `random_stream_at` devolve uma copia em ordem canonica.
`state_hash` e `capture_trace_point` sao consultas sem alocacao que exigem
estado `READY`.

K0-D nao amplia a API da biblioteca. O executavel `k0_scenario_runner` e uma
ferramenta externa que inclui somente este header agregado e usa essas APIs
para produzir artefatos tecnicos.

## K1-A Space And Transforms

K0, K1 and K2 are complete. K1-A through K1-C4 form the deterministic spatial foundation; K2-A through K2-D freeze snapshot, restore, replay and state-bound persistence. WD0 - Worlds Domain and WB0 - Brain Bridge minimo are complete; WD1 - Domain persistence is the next Worlds block. The public Worlds
Kernel now exposes a single immutable 2D fixed-point space, optional entity
transforms, and command-only placement/removal. See the K1-A coordinate and
transform contracts in worlds/kernel/docs. K2-A through K2-D are complete; WD0 - Worlds Domain and WB0 - Brain Bridge minimo are complete; WD1 - Domain persistence is the next Worlds block.
## K1-B1 Occupancy

K1-B1/B2 and K1-C1..C4 are complete; canonical spatial links, rigid subtree translation and V5 are part of completed K1. K2-A through K2-D are complete; WD0 - Worlds Domain and WB0 - Brain Bridge minimo are complete; WD1 - Domain persistence is the next Worlds block. The Worlds Kernel supports one optional fixed-point axis-aligned occupancy per
entity, generic category bits and blocking masks, command-only set/clear,
deterministic conflict rejection with related_entity, diagnostics, and
canonical state hash v3. Orientation does not rotate the AABB. See
worlds/kernel/docs/OCCUPANCY_AND_BARRIER_CONTRACT.md and
worlds/kernel/docs/K1_B1_OCCUPANCY_BARRIER_AUDIT.md.

## K1-B2 Atomic Movement

K1-B2 is complete. MOVE_ENTITY applies checked fixed-point deltas in canonical tick order, preserves orientation, validates the destination and optional AABB, and emits origin/destination event data. Semantic rejection never mutates official placement. See MOVEMENT_AND_DISPLACEMENT_CONTRACT.md and K1_B2_MOVEMENT_AUDIT.md.
## K1-C1 spatial links

`minisnn_worlds_kernel_spatial_link.h` defines the public link and endpoint
value types. `queue_create_spatial_link` and `queue_remove_spatial_link` use
`target_entity = parent` and duplicate that parent in explicit endpoints. Zero
parent or child is rejected synchronously; a nonzero self-link reaches the tick
pipeline and is rejected semantically. Query functions return copies only.
## K2-A Canonical Snapshot

`minisnn_worlds_kernel_snapshot.h`, included by the aggregate header, declares
an opaque caller-owned immutable snapshot. `snapshot_capture()` returns a
memory-only V1 byte blob; `snapshot_destroy()` releases it; version, size and
const data accessors support comparison and inspection. Capture does not modify
the Kernel and exposes no internal pointers. The blob records the state hash
and state-hash version for audit, but is not itself a new state hash.

## K2-B Restore And Persistence

`snapshot_from_bytes()` copies and fully validates a V1 byte sequence;
`create_from_snapshot()` transactionally constructs a new Kernel only after
semantic validation, invariant checking, state-hash verification and a
canonical byte-for-byte re-capture. These APIs remain memory-only. File
save/load lives only in `app/k2_snapshot_file.c`, which writes and reads
exactly the V1 bytes. See `K2_B_RESTORE_AND_PERSISTENCE_CONTRACT.md`.

## K2-C Command Log And Replay

`minisnn_worlds_kernel_command_log.h`, included by the aggregate header,
declares an opaque canonical V1 command log. Capture copies an accepted pending
submission by CommandId; accessors expose copies or immutable bytes only;
`command_log_from_bytes()` validates canonical little-endian bytes; and
`command_log_replay_next()` reuses the public queue API only when logical time
and the next expected CommandId match. A mismatch returns
`MINISNN_WORLDS_KERNEL_ERROR_REPLAY_DIVERGENCE` before queue mutation. File
save/load remains in `app/k2_command_log_file.c`; see
`K2_C_COMMAND_REPLAY_CONTRACT.md`.

## K2-D State-Bound Replay

MiniSNNWorldsKernelReplaySession is opaque and caller-owned. Creation binds a
command-log cursor to an expected canonical Kernel state hash. validate() and
replay_next() return REPLAY_DIVERGENCE before queue mutation when the binding,
logical tick or next CommandId is incompatible. The cursor advances only after
a successful replay. The session does not own or mutate Command Log V1 bytes.

Snapshot Format V1 and Command Log Format V1 are frozen after K2. K2-D file
persistence remains in app and the library remains filesystem-free. See
K2_D_PERSISTENCE_CLOSURE_AUDIT.md.

## Provisional command batches

`minisnn_worlds_kernel_command_batch_begin`, `_commit`, `_rollback` and
`_active` provide a small generic transaction boundary around normal command
submissions. The API has no knowledge of any consumer-specific semantics. A
rollback before tick advancement restores the official queue and command-id
provenance to the begin checkpoint. Active batches cannot be captured by the
V1 snapshot or command-log capture APIs.
