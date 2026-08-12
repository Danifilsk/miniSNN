# Auditoria K0-B: entidades, comandos e eventos

## Escopo entregue

K0-B conclui o registro deterministico de entidades, a fila de comandos
futuros e a janela de eventos do ultimo tick. K0-A, K0-B e K0-C estao
concluidos; K0-D fechou o bloco K0 com um runner externo e artefatos tecnicos.

| ID | Severidade | Area | Problema | Correcao | Teste | Estado |
| --- | --- | --- | --- | --- | --- | --- |
| K0B-001 | alta | IDs | identidade podia depender de armazenamento | IDs fortes monotonicamente emitidos | `test-k0-b-entities` | PASS |
| K0B-002 | alta | registro | morte podia permitir renascimento do ID | registro vivo/morto e enumeracao crescente | `test-k0-b-entities` | PASS |
| K0B-003 | alta | fila | submissao podia alterar estado em falha | validacao e reserva antes de inserir | `test-k0-b-commands` | PASS |
| K0B-004 | alta | ordem | conflitos podiam depender da insercao | ordem por tick, prioridade, emissor e Command ID | `test-k0-b-commands`, `test-k0-b-determinism` | PASS |
| K0B-005 | alta | eventos | rejeicao podia ficar invisivel | um evento causal por comando consumido | `test-k0-b-events` | PASS |
| K0B-006 | alta | atomicidade | falha interna podia deixar estado parcial | plano temporario promovido somente no commit | `test-k0-b-events`, `test-k0-b-determinism` | PASS |
| K0B-007 | media | entrega | API e binario precisavam ser auditaveis | headers, simbolos, demo e sanitizer | `audit-k0-b` | PASS |

## Como auditar

```powershell
mingw32-make worlds-kernel-test
mingw32-make test-k0-b-sanitize
mingw32-make demo-k0-b
mingw32-make audit-k0-b
```

O demo `k0_entity_command_demo` exercita criacao, conflito de destruicao,
janela vazia e criacao futura. A auditoria verifica a saida estavel, os
simbolos da biblioteca normal, os limites arquiteturais e os testes C.

## Armazenamento, ordem e erros

O armazenamento inicial e um vetor crescente de registros com marca `alive`.
Ele preserva a identidade quando cresce e nao compacta entidades mortas. Essa
escolha favorece clareza e auditabilidade; nao e uma estrutura espacial final.

Comandos futuros sao inspecionados e aplicados em ordem total canonicamente
definida. Um conflito de estado e resolvido progressivamente nessa ordem e
produz evento causal. Falha de submissao ou preflight retorna erro; conflito
semantico consome o comando, mantem o tick valido e produz rejeicao explicita.

Os testes injetam falhas deterministicas na fila, no buffer ordenado, na janela
de eventos e no registro temporario de entidades. O smoke C11 e executado
quando ha compilador. ASan/UBSan e classificado como `PASS`, `FAIL` ou
`UNAVAILABLE` somente depois do probe da toolchain.

## Achados

Nao ha achado funcional aberto em K0-B quando `audit-k0-b` passa. O Kernel
continua sem performance espacial, persistencia ou historico de eventos;
essas sao limitacoes deliberadas, nao promessas implementadas.

## Limites de K0-B

K0-B nao introduz sistemas de simulacao, armazenamento de componentes,
espaco, mapa, persistencia, Domain, Brain Bridge ou Worlds App.
O Kernel continua C11 headless e sem dependencia do miniSNN Core.

K0-C concluiu PRNG deterministico, streams derivados, hash canonico e
observabilidade. K0-D entregou os artefatos integrados e o fechamento completo
do bloco K0; K1 e K2-A ate K2-D foram concluidos; WD0 - Domain minimo e WB0 - Brain Bridge minimo estao concluidos; WD1 - persistencia do Domain e o proximo bloco.

> Historical roadmap note: this closure was written before K2-B. K2-A through K2-D are now complete; WD0 - Worlds Domain and WB0 - Brain Bridge minimo are complete; WD1 - Domain persistence is the next Worlds block.
