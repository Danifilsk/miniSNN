# Auditoria C7 da interface cerebro-agente

## Escopo

C7 fecha uma fronteira numerica e independente de dominio entre sinais externos
e a rede miniSNN. C7.5-B executa uma matriz representativa com LIF, AdEx e
Hodgkin-Huxley. O contrato de replay e: mesma configuracao, modelo, estado e
eventos produzem a mesma continuacao. Isto nao define um mundo, corpo, tarefa
ou agente geral.

`mingw32-make test-c7` executa os contratos C7. O long run separado executa
100000 passos neurais por modelo. O cenario autoritativo
`configs/c7_integrated_audit.ini` gera resultados em
`results/scenarios/c7_integrated_audit_demo/`.

## API, ownership e atomicidade

| Modulo | Ownership | Estado | Efeito e atomicidade | Persistencia |
| --- | --- | --- | --- | --- |
| AgentIO e frames | Contexto e buffers pertencem ao chamador | Tick valido | Submit/consume por copia; falha nao publica frame parcial | Schemas e frames pendentes |
| Sensor encoder | Encoder possui mappings | Sensor pendente | Produz input sem avancar a rede | Mappings e fases |
| Action decoder | Decoder possui mappings | Atividade capturada | Decodifica antes de submeter acao | Mappings e diagnosticos |
| Agent Cycle | Chamador possui o ciclo | READY ou ACTION_PENDING | Unico orquestrador C7 que avanca a rede | Estado temporal, feedback e rede |
| Checkpoint C7.5-A | Diretorio do chamador | READY ou ACTION_PENDING | Load invalido permanece atomico | Rede, schemas, mappings e filas |

As APIs sao reentrantes por instancia, mas nao prometem thread safety para a
mesma instancia concorrente. Nenhuma expoe ponteiro interno ou usa nome de
canal como politica. Assinaturas de schema e mapping sao verificadas antes da
publicacao de dados.

## Matriz C7.5-B

- **LIF:** integracao completa com STDP, R-STDP, homeostase, plasticidade
  estrutural, feedback, reset, checkpoint e evolucao abstrata externa.
- **AdEx e Hodgkin-Huxley:** caminho end-to-end representativo, reset,
  checkpoint READY/ACTION_PENDING, replay, atividade neural real e estados
  finitos. A fixture usa 64 passos por tick; `lif_input_drive=1000`,
  `adex_input_drive=500` e `hodgkin_huxley_input_drive=12`, valores expostos
  no INI e baseados nos demonstradores C5 validados.
- **Long run:** multiplos episodios, feedback positivo/negativo/zero,
  checkpoints e 100000 passos neurais por modelo.
- **Evolucao:** o harness calcula fitness apenas a partir de action frames
  numericos observados; schemas e significados nao sao evoluidos pelo Core.

O fingerprint mistura configuracao efetiva, modelo, seed, assinaturas, sinais,
acoes, diagnosticos e fronteiras de checkpoint. Ele exclui caminho absoluto,
tempo de parede, endereco de memoria e ordem de diretorio. Ele complementa,
mas nao substitui, comparacoes contratuais diretas.

Silencio nao e convertido em uma decisao artificial: a configuracao da
auditoria usa limiar positivo e grupos WTA com ativacao e confianca minimas
positivas. Uma populacao silenciosa conserva os defaults; somente atividade
capturada pelo frame C7.3 pode produzir uma acao nao padrao. Cada checkpoint
usa um diretorio relativo exclusivo por modelo, repeticao e fronteira, e a
linha CSV registra save/load/resume/replay efetivamente concluidos.

## Resultados e limites

`config_source.ini` preserva os bytes fornecidos; `config_used.ini` registra a
configuracao canonica usada. O cenario gera CSVs de runs, acoes, feedback e
checkpoints, alem de resumo por modelo, manifesto e HTML local. Esses artefatos
sao locais e nao sao versionados.

Preflight invalido nao muda a rede. Erro apos o primeiro passo deixa o ciclo em
`FAULTED` e nenhuma action parcial e publicada. Reset explicito recupera quando
nao ha acao pendente. Checkpoint truncado, incompativel ou com assinatura
adulterada e rejeitado sem converter corrupcao em sucesso.

## Candidata para D1

As APIs C7 sao candidatas a estabilidade, nao congeladas definitivamente. D1
deve revisar ergonomia de erros, thread safety, limites de arquivos e a primeira
integracao concreta Brain Bridge. C7 nao implementa Worlds, corpo, movimento,
semantica de acao, planejamento, organismo ou agente geral.
