# K1-C3 Ordering and Hardening Audit

Status: PASSOU em validacao automatizada do K1-C3; a validacao humana dos produtos Worlds ainda pertence aos blocos posteriores.

## Contrato preservado

A ordem canonica de comandos permanece, sem desempates adicionais:

```text
target_tick -> priority -> issuer EntityId -> CommandId
```

Nao existe prioridade por tipo, alvo ou nocao de simultaneidade. Cada comando observa o estado planejado produzido por todos os comandos canonicos anteriores do mesmo tick.

## Precedencia documentada

- Criar link: parent vivo, child vivo, self, parent colocado, child colocado, duplicata, child com pai, ciclo, overflow de offset, capacidade/recursos.
- Remover link: parent vivo, child vivo, self, parent colocado, child colocado, link existente.
- Mover: alvo vivo, displacement presente, alvo colocado, alvo sem pai, coleta BFS, overflow/bounds, conflito externo, capacidade de eventos e recursos.
- Lifecycle: alvo vivo, incidencia de links, depois transform/occupancy e destruicao ou remocao do espaco.

Links e ocupacao sao sempre avaliados no estado planejado. Rejeicoes produzem evento canonico, sem promover mutacao parcial.

## Hardening C3

O preflight passa a verificar todos os incrementos de diagnostico antes da promocao: entidades, placement, ocupacao, conflito, movimento, links, comandos e eventos. IDs de entidade, comando e evento nao fazem wrap. A fila de comandos, copias planejadas, links, BFS, destinos e eventos continuam sujeitos a falha atomica de alocacao.

Sob `MINISNN_WORLDS_KERNEL_TESTING` mais `MINISNN_WORLDS_KERNEL_C3_INVARIANT_TESTING`, um validador interno inspeciona o estado oficial antes do plano, o plano apos cada comando, e o estado promovido. Ele verifica entidades, transforms, ocupacoes, links ordenados, pais unicos, ciclos, offsets, contadores estruturais e agrupamento causal de eventos. Testes herdados mantem os hooks de teste sem pagar essa auditoria por tick; o build normal nao inclui esse custo nem simbolos de teste.

## Matriz e long run

Os testes C3 cobrem as ordens create/remove link, move raiz/child, lifecycle, placement, set/clear de occupancy, conflitos de floresta, ciclos, churn remove/create/remove, consultas canonicas, corrupcao simulada, limites, matriz de falha de alocacao e long run de 1000 ticks. O long run combina floresta dinamica, churn de links, lifecycle, ocupacao passiva e movimento, repetido para comparar hash, eventos e diagnosticos.

`test-k1-c3-optimization-determinism` compara O0 e O2. Sanitizer e smoke POSIX seguem a classificacao explicita de indisponibilidade herdada; no Windows atual podem informar indisponibilidade sem mascarar falha de projeto.

## Complexidade observada

As consultas de parent e de filhos, a insercao/remocao canonica de links e a busca de conflitos de occupancy usam os vetores ordenados atuais: custo linear em entidades ou links relevantes. A coleta BFS de subarvore combina enumeracoes lineares de filhos e, portanto, cresce com a arvore e o vetor de links; a deteccao de ciclo percorre ancestrais por buscas de parent. A emissao de eventos e linear no numero de membros efetivamente afetados. K1-C3 documenta e testa esses caminhos sem introduzir indices, tabelas hash ou arvores balanceadas; essa otimizacao permanece fora do escopo.
## Hash V5

K1-C3 nao cria V6. A V5 continua incluindo links, comandos/eventos relacionados e contadores de spatial links. Diferencas de links, offsets, transform, ocupacao, eventos e diagnosticos relevantes devem distinguir hash. Consultas de copia canonica nao mutam estado nem hash. V1 a V4 permanecem historicos e estaveis.

## Fora de escopo e proximo bloco

K1-C3 nao implementa reparenting, link IDs, save/load, replay ou integracao ao Core. K1-C4 posteriormente entregou parser limitado de demo, artefatos, golden e stress final sem criar essas mecanicas. K2 permanece responsavel por snapshot, save/load e replay.