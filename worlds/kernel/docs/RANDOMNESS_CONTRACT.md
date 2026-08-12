# Contrato de aleatoriedade K0-C

K0-C define uma fonte deterministica de inteiros para o Worlds Kernel. A seed
mestra e `uint64_t master_seed` na cauda versionada de
`MiniSNNWorldsKernelConfig`. Configuracoes V1 antigas cujo `struct_size` nao
alcanca esse campo usam `MINISNN_WORLDS_KERNEL_DEFAULT_MASTER_SEED`. Seed zero
e valida e nunca significa consultar relogio real.

O algoritmo publico e PCG32 XSH-RR 64/32, versao
`MINISNN_WORLDS_KERNEL_PRNG_VERSION == 1`. Ele usa apenas inteiros de largura
fixa e nao chama `rand`, `srand`, tempo real ou um PRNG compartilhado da libc.

Cada stream e identificado pela chave forte `(namespace_id, stream_id)`. Apenas
`(0, 0)` e invalida. Um stream e derivado exclusivamente de seed, versao e
chave; criar ou consumir outro stream nao altera sua sequencia. A criacao e
lazy, atomica e a enumeracao publica e sempre ordenada por namespace e depois
por stream.

`random_u32` consome um valor bruto; `random_u64` consome dois; e uma chamada
bounded pode consumir mais de um por rejection sampling. O intervalo bounded
e `[0, bound)` e `bound == 0` retorna `INVALID_BOUND`. Nenhuma API publica
altera seed, estado de stream ou output em caso de falha.

Os vetores da versao 1 estao travados em `test_k0_c_random.c`, incluindo seeds
0, 12345 e `UINT64_MAX`. Uma mudanca de algoritmo exige nova versao, novos
vetores e incompatibilidade declarada.

K0-D consumes configured streams only through the public API. Its normalized
scenario signature includes the seed and stream keys, while technical artifact
files record the resulting stream and draw counters without recording each draw.
