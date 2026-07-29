# Contrato de tempo logico

`MiniSNNWorldsTick` representa tempo logico, nao tempo real. O valor inicial e
zero e significa que nenhum tick foi concluido. Depois de N passos bem-sucedidos,
o valor e N.

Cada `minisnn_worlds_kernel_step` executa um ciclo atomico com as fases internas
`BEGIN`, `PROCESS`, `FINALIZE` e `COMMIT`. O incremento ocorre somente no
commit. Uma falha antes dele preserva o tick comprometido anterior.

K0-A nao consulta relogio real, nao dorme, nao cria threads e nao usa gerador
aleatorio. A velocidade visual futura nao participara do tempo logico.

O estado normal e `READY`. `STEPPING` e transitorio durante o passo. `FAULTED`
fica reservado a falha interna irrecuperavel. Overflow e detectado antes do
incremento; o tick maximo nunca volta para zero e o estado retorna a `READY`.
