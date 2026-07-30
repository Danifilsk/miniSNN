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
