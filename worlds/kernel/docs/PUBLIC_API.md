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

K0 is complete. K1-A is complete and K1 remains in progress. The public Worlds
Kernel now exposes a single immutable 2D fixed-point space, optional entity
transforms, and command-only placement/removal. See the K1-A coordinate and
transform contracts in worlds/kernel/docs. K1-B is next.