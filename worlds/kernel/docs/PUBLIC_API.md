# API publica K0-A

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
