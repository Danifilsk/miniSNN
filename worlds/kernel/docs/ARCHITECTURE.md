# Arquitetura do Worlds Kernel

O Worlds Kernel e um produto C11 headless, deterministico e independente.

- Kernel nao e Core.
- Kernel nao e Domain.
- Kernel nao e App.
- Kernel nao conhece Brain Bridge.

K0-A fornece configuracao, lifecycle, estados, erros, diagnostico e tempo
logico. K0-B adiciona somente um registro de entidades por ID forte, uma fila
de comandos futuros e uma janela de eventos. K0-C adiciona seed explicita,
streams PCG32 independentes, hash canonico e observabilidade por copia. A
execucao de cada tick prepara um plano temporario completo e o promove em um
unico commit. K0-D coloca parser INI, runner, CSV, manifesto e report somente
em `app/`; a biblioteca continua sem filesystem, CLI ou formatos de artefato.

A biblioteca nao inclui headers do Core, Win32 ou APIs de thread. O Core nao
inclui headers do Kernel. Uma integracao futura pertence somente a Brain
Bridge, que ainda nao existe. O Kernel nao define componentes, espacamento,
mapas, agentes ou semantica de dominio.

Os outputs ficam em `build/worlds/kernel/`; fontes e headers nunca recebem
objetos ou executaveis.

## K1-A Space And Transforms

K0, K1 and K2 are complete. K1-A through K1-C4 form the deterministic spatial foundation; K2-A through K2-D freeze snapshot, restore, replay and state-bound persistence. WD0 - Worlds Domain and WB0 - Brain Bridge minimo are complete; WD1 - Domain persistence is the next Worlds block. The public Worlds
Kernel now exposes a single immutable 2D fixed-point space, optional entity
transforms, and command-only placement/removal. See the K1-A coordinate and
transform contracts in worlds/kernel/docs.

## K1-B1 Occupancy

K1-B1/B2 e K1-C1..C4 estao concluidos: occupancy, links canonicos, translacao rigida, V5, demo integrado e stress. K2-A ate K2-D concluem snapshot canonico, restore/save-load, command replay e state-binding de persistencia; WD0 - Domain minimo e WB0 - Brain Bridge minimo estao concluidos; WD1 - persistencia do Domain e o proximo bloco. The Worlds Kernel supports one optional fixed-point axis-aligned occupancy per
entity, generic category bits and blocking masks, command-only set/clear,
deterministic conflict rejection with related_entity, diagnostics, and
canonical state hash v3. Orientation does not rotate the AABB. See
worlds/kernel/docs/OCCUPANCY_AND_BARRIER_CONTRACT.md and
worlds/kernel/docs/K1_B1_OCCUPANCY_BARRIER_AUDIT.md.

## K1-B2 Atomic Movement

K1-B2 is complete. MOVE_ENTITY applies checked fixed-point deltas in canonical tick order, preserves orientation, validates the destination and optional AABB, and emits origin/destination event data. Semantic rejection never mutates official placement. See MOVEMENT_AND_DISPLACEMENT_CONTRACT.md and K1_B2_MOVEMENT_AUDIT.md.
## K1-C1 spatial-link layer

K1-C1 extends only the Worlds Kernel command planner with canonical structural
link storage and V5 hashing. K1-C2 adds planned atomic subtree translation
without Domain semantics, rendering, physics or neural integration. K1-C3 consolida ordering e hardening; K1-C4 fecha a integracao, artefatos e stress.

## K1-C3: ordering e hardening

O planejador unico continua a resolver comandos por `target_tick -> priority -> issuer EntityId -> CommandId`. C3 adiciona validacao de invariantes em builds de teste e preflight uniforme de contadores antes da promocao; nao cria um segundo motor de movimento ou links.

## K2-A Snapshot Boundary

`src/minisnn_worlds_kernel_snapshot.c` owns the canonical little-endian V1
encoder. It shares only the private Kernel state representation through the
internal source boundary; no private type is exposed through `include/`.
The snapshot module allocates immutable memory and has no filesystem. K2-B
consumes those exact V1 bytes through the memory-only import and transactional
restore APIs in `src/minisnn_worlds_kernel_restore.c`; file save/load remains
in the app layer. It does not create a parallel state representation or a
command replay log.

## K2-C Command Replay Boundary

`src/minisnn_worlds_kernel_command_log.c` owns canonical memory-only command
records and incremental replay through the existing queue API. It is separate
from snapshots and events: snapshots are full state, events are derived output,
and command logs are accepted external input history. The app adapter in
`app/k2_command_log_file.c` owns file I/O. No command-log code adds Core,
Domain, networking or filesystem dependencies to the Kernel library.