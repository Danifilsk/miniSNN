# Contrato de Entity IDs

K0-B introduz `MiniSNNWorldsKernelEntityId` como um tipo forte de 64 bits.
O valor zero e invalido. IDs validos comecam em 1, crescem monotonicamente e
nunca sao reutilizados, inclusive depois da destruicao de uma entidade.

Uma entidade e criada somente por um comando `CREATE_ENTITY` aplicado em um
tick comprometido. Ela e destruida somente por um comando `DESTROY_ENTITY`
aplicado em um tick comprometido. A API publica nao oferece mutacao direta do
registro de entidades.

As consultas sao somente leitura:

- `entity_exists` retorna verdadeiro apenas para uma entidade viva;
- `entity_count` retorna a quantidade viva;
- `entity_at` enumera apenas entidades vivas em ordem crescente de ID.

`entity_at` rejeita indice invalido sem escrever no buffer de saida. Uma
destruicao torna o ID inexistente para consultas vivas, mas o identificador
permanece reservado e jamais volta a ser emitido.

O Kernel detecta exaustao antes de emitir um ID que voltaria a zero. A falha
preserva o tick, a fila, os eventos e os contadores ja comprometidos.

O registro inicial privilegia determinismo, clareza e auditabilidade, nao uma
estrutura espacial ou de desempenho final. A estrategia de ID podera ser
reavaliada somente antes de uma futura estabilidade definitiva do Kernel.

O hash K0-C inclui registros vivos e mortos em ordem crescente de Entity ID,
pois a reserva causal de IDs faz parte do estado deterministico.

K0-B nao define componentes, posicao, espacamento, corpo, atributos ou regras
de dominio para entidades. Esses conceitos permanecem fora do Worlds Kernel.
