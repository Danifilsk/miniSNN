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
