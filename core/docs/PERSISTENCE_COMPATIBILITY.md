# Persistencia e compatibilidade

| Artefato | Versao/assinatura | Estado persistido | Load |
|---|---|---|---|
| Checkpoint de evolucao | texto versionado e sidecar estrutural quando ativo | genoma, topologia, pesos, delays e configuracao do modelo | Confere blueprint e assinaturas; C4 e tratado explicitamente como legado LIF. |
| Checkpoint C7 | manifesto e arquivos internos versionados | rede, schemas, encoder, decoder, AgentIO e ciclo | Confere contrato e hashes antes de publicar estado. |
| C6 associativa/sequencial | blueprint/checkpoint do Core | pesos, topologia, tipo, delay e modelo | Reconstrucao limpa nao depende de tensao, spikes ou traces de treino. |
| Configuracoes | `config_source.ini` e `config_used.ini` | fonte byte a byte e configuracao efetiva | A fonte nao e sobrescrita pela serializacao canonica. |

Nenhum formato persiste ponteiros, padding ou caminhos absolutos. Igualdade de
contagem nao substitui igualdade de assinatura. Truncamento e incompatibilidade
de schema/modelo falham claramente sem troca parcial de estado.
