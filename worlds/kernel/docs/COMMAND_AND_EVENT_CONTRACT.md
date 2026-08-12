# Contrato de comandos e eventos

K0-B usa comandos agendados para toda mutacao de entidade. As APIs publicas
aceitam `CREATE_ENTITY` e `DESTROY_ENTITY` e retornam um
`MiniSNNWorldsKernelCommandId` somente apos a submissao ser aceita. Uma falha
de validacao ou alocacao nao consome Command ID e nao altera a fila.

Um comando deve apontar para um tick futuro. O emissor zero representa uma
origem externa. Um emissor diferente de zero precisa estar vivo no instante em
que o comando for aplicado. A destruicao exige um Entity ID nao zero; o alvo
pode deixar de estar vivo ate o tick, caso em que o comando e semanticamente
rejeitado.

Antes de cada tick, a fila elegivel e ordenada canonicamente por:

1. `target_tick` crescente;
2. `priority` crescente;
3. Entity ID do emissor crescente;
4. Command ID crescente.

Essa ordem nao depende da ordem de insercao, enderecos ou detalhes de
alocacao. `pending_command_at` expoe a mesma ordem sem permitir mutacao.

Para cada comando consumido, o Kernel emite exatamente um evento no mesmo
tick. Os tipos atuais sao `ENTITY_CREATED`, `ENTITY_DESTROYED` e
`COMMAND_REJECTED`. Eventos possuem `EventId` monotonicamente crescente e nao
reutilizado. A janela publica contem somente os eventos do ultimo tick
concluido; um tick bem-sucedido sem comandos limpa a janela.

Conflitos semanticos, como dois destroys para o mesmo alvo, sao sucesso do
motor: o primeiro comando aplicavel produz destruicao e o seguinte produz
`COMMAND_REJECTED` com motivo explicito. Falhas internas, como alocacao ou
overflow de identificador, nao consomem comandos, nao trocam a janela de
eventos, nao avancam o tick e nao mudam os contadores comprometidos.

O plano completo do tick e preparado em memoria temporaria: comandos ordenados,
eventos, registro de entidades e contadores futuros. Somente depois de toda a
prevalidacao o plano e promovido ao estado observavel. Portanto, uma rejeicao
semantica recebe evento; uma falha interna preserva o estado anterior.

O hash K0-C inclui a fila pendente em ordem canonica e a janela de eventos em
ordem de emissao. Sorteios do PRNG nao sao eventos de dominio.

## K1-A Space And Transforms

K0 and K1 are complete. K1-A through K1-C4 form the deterministic spatial foundation; K2-A through K2-D are complete; WD0 - Worlds Domain and WB0 - Brain Bridge minimo are complete; WD1 - Domain persistence is the next Worlds block. The public Worlds
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
## K1-C1 spatial-link commands and events

`CREATE_SPATIAL_LINK` and `REMOVE_SPATIAL_LINK` are ordered with every other
future command. Each consumed command emits exactly one event in C1. Accepted
events carry the full derived link; rejected events carry parent as `subject`
and child as `related_entity`. `affected_entity` exists in the event ABI now
and is zero for all C1 events. C2 uses nonzero values for canonical subtree
rejection members without changing V5.

## K1-C3 ordering completo

Dentro do mesmo tick, os comandos observam somente o estado planejado anterior na ordem `target_tick -> priority -> issuer EntityId -> CommandId`. A precedencia de validacao de create/remove link, movimento e lifecycle esta consolidada em `K1_C3_ORDERING_AND_HARDENING_AUDIT.md`. Eventos de movimento de subarvore permanecem agrupados: raiz, depois descendentes BFS causais.

## Provisional command batch

The Kernel exposes a generic provisional command batch for higher-level API
consumers that must submit several commands as one cross-layer operation.
`minisnn_worlds_kernel_command_batch_begin()` records a transient checkpoint of
pending-command count, next CommandId, submitted-command total, last error and
begin tick. Existing `queue_*` APIs remain the only submission APIs.

Before the tick advances, `command_batch_rollback()` removes only commands
submitted after begin and restores that checkpoint without allocation. Capacity
may remain larger because it is not semantic state. CommandIds created in a
rolled-back batch were never committed and may be reused; committed CommandIds
remain monotonic. `command_batch_commit()` only closes the transient batch.
Nested batches, commit without a batch and rollback without a batch are invalid.
Rollback after the tick advanced is invalid and never attempts to undo a
committed tick.

A snapshot capture or command-log submission capture while a batch is active is
rejected with `INVALID_STATE`. The batch marker is not part of State Hash V5,
Snapshot Format V1 or Command Log Format V1. It is integration state, not
persistence state.
