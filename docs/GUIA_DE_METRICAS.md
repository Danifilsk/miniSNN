# Guia de Metricas da miniSNN

Este guia resume as metricas geradas por:

```powershell
python scripts/compare_runs.py results/scenarios/run_a results/scenarios/run_b --out-name comparacao
```

Os resultados ficam em:

```text
results/comparisons/<comparison_name>/
```

Arquivos principais:

- `comparison_summary.csv`: uma linha por execucao, com metricas numericas.
- `comparison_report.txt`: relatorio em portugues com rankings e avisos.
- `comparison_metrics.png`: barras comparando metricas principais.
- `comparison_activity_overlay.png`: atividade populacional sobreposta no tempo.

## Metricas principais

`total_spikes`: soma de todos os spikes da execucao.

`mean_spikes_per_step`: media de spikes por timestep.

`max_spikes_per_step`: maior numero de spikes observado em um timestep.

`activity_fraction`: fracao de timesteps com pelo menos um spike.

`silence_fraction`: fracao de timesteps sem spikes.

`active_timesteps`: quantidade de timesteps ativos.

`silent_timesteps`: quantidade de timesteps silenciosos.

`first_active_step` e `last_active_step`: primeira e ultima atividade observada.

`has_late_activity`: indica se houve atividade no ultimo quarto da simulacao.

`activity_span`: distancia entre primeiro e ultimo timestep ativo.

## Silencio, rajadas e estabilidade

`longest_silence_streak`: maior sequencia de timesteps sem spikes.

`longest_activity_streak`: maior sequencia de timesteps com spikes.

`burst_threshold_used`: limiar usado para classificar rajadas:

```text
max(3, media_de_spikes + 2 * desvio_padrao)
```

`burst_count`: numero de timesteps acima desse limiar.

`burst_fraction`: fracao da simulacao classificada como rajada.

`explosion_score`: aproximacao de atividade explosiva:

```text
max_spikes_per_step / num_neurons
```

`stability_score`: heuristica entre 0 e 1. Penaliza silencio extremo, rajadas e
picos populacionais muito altos. Nao e uma verdade biologica absoluta; serve
como diagnostico rapido para comparar execucoes.

## Atividade temporal

A simulacao e dividida em tres partes:

- `early_activity_mean` e `early_activity_fraction`
- `middle_activity_mean` e `middle_activity_fraction`
- `late_activity_mean` e `late_activity_fraction`

Essas metricas ajudam a ver se a rede morre, cresce, estabiliza ou volta a
ficar ativa no fim.

## Atividade por neuronio

Quando `raster.csv` esta disponivel:

`active_neuron_count`: neuronios que dispararam ao menos uma vez.

`inactive_neuron_count`: neuronios sem spikes.

`active_neuron_fraction`: fracao de neuronios ativos.

`dead_neuron_fraction`: fracao de neuronios silenciosos.

`mean_spikes_per_neuron`: media de spikes por neuronio, incluindo silenciosos.

`mean_spikes_per_active_neuron`: media considerando apenas neuronios ativos.

`most_active_neuron` e `least_active_neuron`: indices dos neuronios com maior e
menor contagem de spikes.

## Concentracao de spikes

`spike_concentration_top_10_percent`: fracao dos spikes produzida pelos 10% de
neuronios mais ativos.

`spike_gini_approx`: medida aproximada de desigualdade da atividade por
neuronio. Valores maiores sugerem que poucos neuronios concentram muitos
spikes.

## EXC/INH

Quando for possivel inferir tipos de neuronios por `config_used.ini`,
`summary.txt` ou `raster.csv`, o comparador calcula:

- `excitatory_neuron_count`
- `inhibitory_neuron_count`
- `excitatory_total_spikes`
- `inhibitory_total_spikes`
- `excitatory_mean_spikes`
- `inhibitory_mean_spikes`
- `exc_inh_spike_ratio`

Se a inferencia nao for segura, os campos ficam vazios/`NA`.

## Sincronia aproximada

`population_fano_factor`: variancia da atividade populacional dividida pela
media. Valores altos sugerem atividade mais irregular.

`population_cv`: desvio padrao dividido pela media.

`synchrony_proxy`: aproximacao simples:

```text
max_spikes_per_step / total_spikes
```

Ela indica concentracao temporal dos spikes, mas nao substitui uma analise
completa de sincronia neural.
