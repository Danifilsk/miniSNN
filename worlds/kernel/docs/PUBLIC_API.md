# API publica K0-A e K0-B

O header agregado e `minisnn_worlds_kernel.h`. Todos os simbolos publicos usam
o namespace `minisnn_worlds_kernel_*`, os tipos usam `MiniSNNWorldsKernel*` e
as macros usam `MINISNN_WORLDS_KERNEL_*`.

`MiniSNNWorldsKernelConfig` contem `struct_size` e `format_version`. A funcao
`minisnn_worlds_kernel_config_default()` produz a configuracao deterministica
da versao atual. `create(NULL, ...)` usa esse default; uma configuracao
explicita exige versao atual e `struct_size` suficiente. K0-A le primeiro
`struct_size`, confirma que o prefixo contem `format_version` e somente entao
le a versao. Estruturas pequenas demais sao rejeitadas antes da leitura de
campo ausente ou alocacao. Uma estrutura maior com versao V1 aceita a cauda
desconhecida sem copia ou interpretacao.

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
