# Ownership e ciclo de vida

Esta politica descreve a API candidata de D1-A; ela nao congela ABI nem API.

| Objeto | Criacao e ownership | Destruicao/reset | Copias e referencias |
|---|---|---|---|
| `MiniSNN` | `minisnn_create*` retorna ownership ao chamador. | `minisnn_destroy(&ptr)` aceita `NULL`, libera e zera o ponteiro. Reset limpa estado dinamico, nao torna a topologia externa proprietaria. | Getters devolvem valores ou copiam para buffers do chamador. |
| Schemas C7 | `minisnn_*_schema_create` retorna ownership. | `*_destroy(&ptr)` libera de modo idempotente. | Contextos clonam schema; nao guardam um ponteiro mutavel do chamador. |
| Frames C7 | Chamador inicializa e destroi frame. | `*_frame_reset` limpa valores e metadados do frame. | `set_values` copia valores; consume copia para `out_frame`. |
| Encoder/decoder/cycle | `*_create` retorna ownership ao chamador. | `*_destroy(&ptr)` e seguro com ponteiro nulo. | Schemas sao copiados no create; rede e AgentIO usados pelo ciclo sao referencias nao proprietarias enquanto o ciclo existir. |
| Configuracoes | Structs por valor preenchidas por `*_default`. | Nao possuem destroy. | APIs leem configuracao durante a chamada; nao retencao ponteiros do chamador. |
| Checkpoints | `save` escreve arquivos versionados. | `load` valida antes de publicar estado. | Nenhum ponteiro, padding ou caminho absoluto entra no formato ou assinatura. |

Depois de qualquer falha, outputs documentados como atomicos permanecem
inalterados. O chamador continua dono de todos os buffers fornecidos. APIs
test-only ficam protegidas por `MINISNN_TESTING` e nao fazem parte da
biblioteca de producao.
