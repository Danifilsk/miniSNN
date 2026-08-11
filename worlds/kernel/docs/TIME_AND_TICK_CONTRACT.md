# Contrato de tempo logico

`MiniSNNWorldsTick` representa tempo logico, nao tempo real. O valor inicial e
zero e significa que nenhum tick foi concluido. Depois de N passos bem-sucedidos,
o valor e N.

Cada `minisnn_worlds_kernel_step` executa um ciclo atomico com as fases internas
de preflight, resolucao canonica e commit. O incremento ocorre somente no
commit. Uma falha antes dele preserva o tick comprometido anterior.

Em K0-B, um comando deve ter `target_tick` estritamente maior que o tick atual.
No passo para `N + 1`, somente comandos com alvo `N + 1` sao consumidos. Eles
sao resolvidos por tick, prioridade, emissor e Command ID. Cada comando
consumido gera um evento, inclusive uma rejeicao semantica. Um passo sem
comandos conclui normalmente e deixa a janela de eventos vazia.

O Kernel nao consulta relogio real, nao dorme e nao cria threads. K0-C oferece
um PRNG interno deterministico, mas sorteios nao avancam o tick e nao emitem
eventos. A velocidade visual futura nao participara do tempo logico.

O estado normal e `READY`. `STEPPING` e transitorio durante o passo. `FAULTED`
fica reservado a falha interna irrecuperavel. Overflow e detectado antes do
incremento; o tick maximo nunca volta para zero e o estado retorna a `READY`.

## K1-A Space And Transforms

K0 and K1 are complete. K1-A through K1-C4 form the deterministic spatial foundation; K2-A through K2-D are complete; WD0 - Worlds Domain is complete; WB0 - Brain Bridge minimo is the next Worlds block. The public Worlds
Kernel now exposes a single immutable 2D fixed-point space, optional entity
transforms, and command-only placement/removal. See the K1-A coordinate and
transform contracts in worlds/kernel/docs. K2-A through K2-D are complete; WD0 - Worlds Domain is complete; WB0 - Brain Bridge minimo is the next Worlds block.

## K1-B2 Atomic Movement

K1-B2 is complete. MOVE_ENTITY applies checked fixed-point deltas in canonical tick order, preserves orientation, validates the destination and optional AABB, and emits origin/destination event data. Semantic rejection never mutates official placement. See MOVEMENT_AND_DISPLACEMENT_CONTRACT.md and K1_B2_MOVEMENT_AUDIT.md.
## K1-C1 planned structural state

During a tick, spatial links are copied into `StepPlan` with entities. Later
commands in canonical order observe prior accepted create/remove operations.
Promotion of entities, links, events, counters, IDs and tick remains one
atomic commit.
