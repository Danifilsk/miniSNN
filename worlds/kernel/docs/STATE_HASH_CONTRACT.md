# Contrato de hash de estado K0-C

`minisnn_worlds_kernel_state_hash` produz uma assinatura diagnostica
deterministica, nao criptografica, baseada em FNV-1a 64-bit e na versao
`MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION == 1`. Ela pode ser calculada apenas
quando o Kernel esta `READY`, nao aloca e nao altera o objeto nem `last_error`.

O encoding e explicito: inteiros usam bytes little-endian, booleanos usam um
byte zero ou um e enums usam `uint32_t`. Nenhuma struct e hasheada como bloco
de memoria. O contrato inclui magic, versoes, seed, tick, proximos IDs,
entidades vivas e mortas por ID, comandos pendentes em ordem canonica, eventos
do ultimo tick em ordem de emissao, contadores causais e streams por chave
canonica com estado, sequencia e draws.

O hash exclui ponteiros, padding, capacidades, ordem fisica de armazenamento,
`last_error`, estado temporario, caminhos, timestamps e metadados de build.
Logo, uma chamada invalida que apenas atualiza `last_error` nao muda a
assinatura, mas um comando aceito, draw, tick ou evento comprometido muda.

Vetores para estados vazios, tick vazio, comando pendente, entidade criada e
primeiro draw estao em `test_k0_c_hash.c`. Uma alteracao deliberada do encoding
exige nova versao do hash e migracao documentada.

K0-D writes this value as fixed hexadecimal in `trace.csv` and checks that the
last trace value equals the manifest final hash before publishing artifacts.
Its separate scenario configuration signature is never named a state hash.
