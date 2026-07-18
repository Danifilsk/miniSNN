# Sequencias temporais e previsao C6.3

O arquivo `sequence_prediction_checkpoint.txt` preserva modelo, assinatura de
configuracao neuronal, tipos EXC/INH, topologia, pesos e delays. Ele reconstroi
uma rede limpa para prever; nao armazena prefixos, respostas, tensoes, spikes ou
traces. O teacher pulse e usado apenas no treino, nunca no delay/probe.

O protocolo C6.3 mede uma tarefa pequena de previsao do proximo padrao em uma
sequencia temporal. Durante o treino, os grupos de entrada sao apresentados em
ordem e um **teacher pulse supervisionado** ativa o grupo alvo de previsao para
produzir atividade pos-sinaptica para o STDP existente. O alvo recebe esse
estimulo somente no treino. Durante a avaliacao, a rede recebe somente um
prefixo incompleto; delay e probe nao recebem corrente externa, inclusive na
populacao de previsao.

O padrao previsto e decodificado pela maior atividade de spikes acumulada em
todos os passos do probe. A primeira resposta continua registrada apenas como
latencia. Somente depois dessa decodificacao o runner consulta o proximo padrao
esperado para calcular a pontuacao. O protocolo nao usa tabela de transicao,
copia do alvo ou estimulo direto do alvo no probe.

## Executar o demo

```powershell
mingw32-make scenario-sequence-prediction
```

O arquivo `configs/sequence_prediction_demo.ini` usa duas sequencias de quatro
padroes, vinte trials balanceados e um prefixo de tres padroes. A topologia
`sequence_prediction` e experimental e deterministica.

`configs/sequence_prediction_context_demo.ini` exercita memoria de contexto:
`A > B > C > D` e `X > Y > C > E`. O ultimo simbolo do prefixo e o mesmo (`C`),
mas o proximo esperado difere. As assemblies de entrada desse modo possuem
recorrencia interna minima; o controle que apresenta somente `C` mede o quanto
da decisao pode ser explicada sem a historia anterior.

## Configuracao

A secao `[sequence_prediction]` aceita:

| Chave | Significado |
| --- | --- |
| `enabled` | Ativa o protocolo. Requer STDP ativo. |
| `sequence_count`, `sequence_length` | Numero de sequencias e elementos por sequencia. Minimos: 2 e 3. |
| `training_epochs` | Repeticoes ordenadas de treino. |
| `pattern_steps`, `inter_pattern_gap_steps` | Duracao de cada padrao e intervalo entre padroes, em passos. |
| `prefix_length` | Quantidade de elementos apresentada antes da previsao; deve ser menor que o comprimento. |
| `prediction_delay_steps`, `prediction_probe_steps` | Janela sem entrada e janela observada para decodificar spikes. |
| `trial_count`, `seed` | Agenda balanceada e reproducivel de trials. |
| `pattern_mode` | `fixed`, `seeded` ou `contextual`, todos deterministas. O modo contextual exige `input_group_size >= 2`. |
| `input_start`, `input_group_size` | Populacao que recebe os padroes do prefixo. |
| `prediction_start`, `prediction_group_size` | Populacao separada observada no probe. |
| `freeze_plasticity_during_evaluation` | Congela STDP durante a medicao. |
| `reset_between_sequences` | Recria estado dinamico entre sequencias de treino. |
| `prediction_threshold` | Fracao minima de atividade do padrao esperado para considerar a previsao correta. |

## Metricas e controles

`sequence_prediction_trials.csv` contem um registro por trial, incluindo
prefixo, esperado, previsto, similaridade, erro, correcao, latencia e atividade.
`sequence_prediction_summary.txt` e `sequence_prediction_report.html` resumem:

- `next_pattern_accuracy`: fracao de trials corretos;
- `mean_prediction_similarity`: atividade normalizada no grupo esperado;
- `mean_prediction_error`: `1 - prediction_similarity`;
- `mean_prediction_latency`: primeiro passo de resposta no probe;
- controles sem treino, ordem embaralhada, STDP desligado e rotulos permutados;
- `prediction_margin`: accuracy treinada menos o maior accuracy de controle.

No modo `contextual`, o resumo tambem registra `context_accuracy`,
`last_symbol_only_control_accuracy` e `context_margin`. Eles comparam o prefixo
completo contra um controle que apresenta somente o ultimo simbolo compartilhado.

Depois do treino, o runner captura os pesos em um blueprint, destroi a rede e
recria redes limpas para cada trial. Assim, a avaliacao nao reutiliza tensoes,
spikes, traces ou outro estado dinamico do treino.

O HTML e local. Os CSVs continuam sendo a fonte cientifica bruta.

## Fitness evolutivo opcional

Com `[sequence_prediction] enabled = true`, uma configuracao de evolucao pode
usar `sequence_prediction_next_pattern_accuracy`,
`sequence_prediction_mean_prediction_similarity` ou
`sequence_prediction_prediction_margin`. Com o protocolo desativado, o fitness
historico permanece inalterado.

## Limites

Este e um teste controlado de ordem temporal para configuracoes pequenas. Um
resultado alto depende da topologia, pesos iniciais, STDP, teacher pulse de
treino e configuracao do experimento; ele nao demonstra linguagem, memoria
episodica, planejamento ou predicao geral.
