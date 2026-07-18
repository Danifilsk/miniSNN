# Memoria de trabalho temporal

## Objetivo

O protocolo C6.1 mede retencao temporal produzida pela dinamica da rede:

```text
cue -> delay sem estimulo -> probe/readout
```

Ele nao copia o padrao do cue para a resposta. O padrao recordado e decodificado
somente a partir dos spikes observados no grupo de readout durante o probe.

## Como ativar

Use a secao opcional `[working_memory]` em um cenario:

```ini
[working_memory]
enabled = true
trials = 8
cue_steps = 20
delay_steps = 40
probe_steps = 20
cue_pattern = alternating
cue_start = 0
cue_group_size = 2
readout_start = 0
readout_count = 2
readout_group_size = 2
seed = 2026
reset_between_trials = true
recall_tolerance = 0.25
recall_threshold = 0.75
```

O alvo pronto pode ser executado com:

```powershell
mingw32-make scenario-working-memory
```

Ele cria `results/scenarios/working_memory_demo/`.

## Chaves e limites

| Chave | Regra |
| --- | --- |
| `enabled` | `true` ativa o protocolo; ausente ou `false` preserva o fluxo historico. |
| `trials` | Inteiro positivo. |
| `cue_steps` | Inteiro positivo. |
| `delay_steps` | Inteiro maior ou igual a zero. |
| `probe_steps` | Inteiro positivo. |
| `cue_pattern` | `alternating` ou `seeded`. |
| `cue_start` | Primeiro neuronio da primeira populacao de cue. |
| `cue_group_size` | Quantidade de neuronios excitatorios em cada populacao de cue. |
| `readout_start` | Primeiro neuronio da primeira populacao de readout. |
| `readout_count` | Numero de canais/padroes; deve ser ao menos dois. |
| `readout_group_size` | Quantidade de neuronios excitatorios por populacao de readout. |
| `seed` | Semente do padrao `seeded`; usa PRNG deterministico local. |
| `reset_between_trials` | Recria a rede a partir do mesmo blueprint antes de cada trial. |
| `recall_tolerance` | Erro medio absoluto maximo, entre zero e um. |
| `recall_threshold` | Score minimo, maior que zero e no maximo um. |

As populacoes de cue e readout precisam caber na rede. O protocolo aplica
input somente na populacao de cue correspondente durante o cue; delay e probe
sempre recebem corrente externa zero.

`alternating` usa os canais em ordem por trial. `seeded` escolhe um canal por
um gerador xorshift32 deterministico com a seed da secao.

## Calculo de recall

Para cada trial, durante o probe o runner soma spikes de cada populacao de
readout. O canal com maior contagem e `recalled_pattern`. Empates usam o menor
indice do grupo por ordem deterministica.

O alvo esperado e one-hot no canal do cue. A distribuicao observada e a
contagem de spikes de cada canal dividida pelo total de spikes do readout. O
erro medio absoluto e a media das diferencas por canal, e:

```text
recall_score = 1 - erro_medio_absoluto
```

Um trial e correto somente se o canal recordado coincide com o esperado, o
score atinge `recall_threshold` e o erro nao excede `recall_tolerance`. Sem
spikes no probe, `recalled_pattern` e `-1` e o trial nao e correto.

`first_response_step` e o primeiro passo relativo ao inicio do probe com algum
spike no grupo de readout; e `-1` quando nao ha resposta. A latencia media usa
somente trials com resposta.

## Saidas

Quando ativado, a pasta normal da execucao recebe tambem:

```text
working_memory_trials.csv
working_memory_summary.txt
working_memory_report.html
```

O CSV por trial contem `trial`, padroes de cue/esperado/recordado, score,
correcao, controle embaralhado, delay, primeira resposta, atividade media e a
verificacao de entrada zero no delay/probe. O resumo tambem registra
`chance_accuracy`, `control_accuracy` e `retention_margin`. O controle usa uma
reordenacao deterministica e balanceada dos rotulos de trial, somente na
avaliacao; ele nao altera cue, sinapses ou readout.

O demo usa a topologia de cenario `working_memory`: duas assembleias EXC
recorrentes e dois neuronios INH que inibem apenas a assembleia concorrente.
Essa configuracao e uma demonstracao controlada, nao uma afirmacao de memoria
geral.

## Uso em neuroevolucao

Com memoria de trabalho ativa no cenario-base, termos opcionais de fitness
podem usar `working_memory_recall_accuracy` ou
`working_memory_mean_recall_score`. Sem a secao ativa, esses termos sao
rejeitados e os fitness historicos continuam inalterados.

## Limitacoes

Este protocolo mede um readout discreto de uma tarefa pequena. Atividade
persistente nao prova memoria geral, e um score baixo nao e corrigido por
alteracao oculta de neuronio, sinapse ou plasticidade. O demo LIF incluido e
reproduzivel e deve ser interpretado pelos valores gravados, nao por uma
expectativa de recall perfeito.

[Voltar ao indice da documentacao](INDICE_DA_DOCUMENTACAO.md)
