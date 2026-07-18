# Memoria Associativa C6.2

A associacao fica nos pesos. Ao final do treino, o runner grava
`associative_memory_checkpoint.txt` com modelo, assinatura neuronal, tipos,
topologia, pesos e delays. O recall usa uma rede reconstruida e limpa; tensoes,
spikes, traces e eligibility do treino nao sao persistidos. O alvo esperado e
usado somente depois da decodificacao neural para medir a resposta.

O protocolo C6.2 mede associacao temporal aprendida por STDP em uma rede SNN.
Ele nao copia o cue para a resposta: o padrao recuperado e decodificado apenas
dos spikes dos grupos alvo durante o probe.

## Fluxo

1. Cada par associa um grupo de cue a um grupo alvo diferente.
2. No treino, cue e alvo do mesmo par recebem coativacao; o alvo inicia um
   passo depois para criar uma relacao causal para o STDP existente.
3. No recall, apenas uma versao mascarada do cue recebe corrente externa.
4. Delay e probe recebem corrente externa zero.
5. O grupo alvo com maior contagem de spikes no probe e o padrao recuperado.

Os grupos usam padroes binarios balanceados e nao sobrepostos. `pattern_mode`
aceita `fixed` (pares ciclicos) ou `seeded` (sequencia pseudoaleatoria
deterministica).

## Configuracao

Use `configs/associative_memory_demo.ini` como ponto de partida:

```powershell
mingw32-make scenario-associative-memory
```

A secao `[associative_memory]` aceita:

| Chave | Significado |
| --- | --- |
| `enabled` | Ativa o protocolo C6.2. Requer plasticidade STDP ativa. |
| `pair_count` | Numero de pares cue-alvo, no minimo 2. |
| `training_epochs` | Repeticoes completas dos pares durante o treino. |
| `training_cue_steps`, `training_gap_steps` | Duracao da coativacao e intervalo entre pares. |
| `initial_association_weight` | Peso inicial fraco das sinapses candidatas cue-alvo. |
| `recall_cue_steps`, `recall_delay_steps`, `recall_probe_steps` | Fases temporais do recall. |
| `cue_corruption` | Fracao de neuronios do cue mascarados, em `[0, 1)`. |
| `trial_count`, `seed`, `pattern_mode` | Agenda deterministica de trials. |
| `reset_between_pairs` | Reconstrui o estado dinamico entre pares, preservando pesos treinados. |
| `freeze_plasticity_during_recall` | Congela STDP durante a medicao. |
| `recall_threshold` | Minimo para similaridade e completion corretos. |
| `cue_start`, `cue_group_size` | Inicio e tamanho dos grupos de cue. |
| `target_start`, `target_group_size` | Inicio e tamanho dos grupos alvo. |

Os grupos cue e alvo devem caber na rede e nao podem se sobrepor. O tempo
autoritativo e o numero inteiro de passos; milissegundos sao derivados de `dt`.

## Metricas

`associative_memory_trials.csv` registra por trial:

```text
trial,pair_id,cue_corruption,expected_pattern,recalled_pattern,
pattern_similarity,pattern_completion_score,recall_correct,
first_response_step,mean_target_activity
```

`pattern_similarity` e a fracao dos spikes alvo que ocorreu no grupo esperado.
`pattern_completion_score` e o indice de Jaccard entre as unidades ativas no
probe e as unidades do grupo alvo esperado. `recall_correct` exige que o grupo
decodificado seja o esperado e que ambos os scores alcancem
`recall_threshold`.

O resumo contem `recall_accuracy`, medias dos scores, latencia, `chance_accuracy`
e os tres controles: rede nao treinada, treino com os alvos deslocados entre
pares e treino com plasticidade congelada. `control_accuracy` e o maior desses
tres controles;
`association_margin = recall_accuracy - control_accuracy` e conservadora.

## Arquivos gerados

Na pasta normal da execucao:

```text
associative_memory_training.csv
associative_memory_trials.csv
associative_memory_summary.txt
associative_memory_report.html
```

O HTML local mostra configuracao, STDP, pesos antes/depois, trials, controles e
limitacoes. Um resultado bom neste demo depende da configuracao recorrente e
nao demonstra memoria associativa geral.

## Neuroevolucao

Quando `associative_memory.enabled = true`, termos de fitness opcionais sao:

```text
associative_memory_recall_accuracy
associative_memory_mean_pattern_similarity
associative_memory_mean_completion_score
associative_memory_association_margin
```

Esses nomes permanecem separados das metricas de memoria de trabalho C6.1.
