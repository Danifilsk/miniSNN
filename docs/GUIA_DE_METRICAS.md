# Guia de Metricas da miniSNN

As definicoes canonicas, formulas e nomes com prefixos estao em
`docs/GUIA_DE_DIAGNOSTICO.md`. `metrics.csv` e a fonte preferencial para runs
novas; a comparacao mantem fallback para runs antigas.

Geracao manual:

```powershell
python scripts/analyze_run.py results/scenarios/random_demo --level basic
```

Este guia resume as metricas geradas por:

```powershell
python scripts/compare_runs.py results/scenarios/run_a results/scenarios/run_b --out-name comparacao
```

Os resultados ficam em:

```text
results/comparisons/<comparison_name>/
```

Se a pasta ja existir, o comparador cria uma pasta unica com timestamp por
padrao. Use `--overwrite` apenas quando quiser reutilizar explicitamente uma
pasta existente.

Cada comparacao concluida registra uma linha em:

```text
results/comparisons/index.csv
```

Arquivos principais:

- `comparison_summary.csv`: uma linha por execucao, com metricas numericas.
- `comparison_report.txt`: relatorio em portugues com rankings e avisos.
- `comparison_metrics.png`: barras comparando metricas principais.
- `comparison_activity_overlay.png`: atividade populacional sobreposta no tempo.

## Metricas principais

Os nomes canonicos em runs novas sao:

- `activity_total_spikes`: soma de todos os spikes.
- `activity_mean_spikes_per_step`: media por timestep.
- `activity_max_spikes_per_step`: maior atividade em um timestep.
- `activity_fraction` e `silence_fraction`: fracoes ativa e silenciosa.
- `activity_active_timesteps` e `activity_silent_timesteps`: contagens.
- `activity_first_active_step` e `activity_last_active_step`: limites observados.
- `activity_has_late_activity`: atividade no ultimo quarto.
- `activity_span`: intervalo inclusivo entre primeira e ultima atividade.

`comparison_summary.csv` tambem expoe aliases historicos, como `total_spikes` e
`last_active_step`, para preservar consumidores antigos.

## Silencio, rajadas e estabilidade

`activity_longest_silence_streak`: maior sequencia sem spikes.

`activity_longest_activity_streak`: maior sequencia com spikes.

`burst_threshold_used`: limiar usado para classificar rajadas:

```text
max(1, media_de_spikes + burst_z_threshold * desvio_padrao)
```

`burst_count`: numero de grupos consecutivos acima do limiar que respeitam
`min_burst_steps`.

`burst_fraction`: fracao da simulacao classificada como rajada.

`diagnostic_stability_score`: media dos componentes de atividade, ausencia de
explosao, persistencia e distribuicao. A formula completa esta em
`GUIA_DE_DIAGNOSTICO.md`. E uma heuristica, nao uma verdade biologica.

## Atividade temporal

A simulacao e dividida em tres partes:

- `activity_early_mean` e `activity_early_fraction`
- `activity_middle_mean` e `activity_middle_fraction`
- `activity_late_mean` e `activity_late_fraction`

Essas metricas ajudam a ver se a rede morre, cresce, estabiliza ou volta a
ficar ativa no fim.

## Atividade por neuronio

Quando `raster.csv` esta disponivel:

`neuron_active_count`: neuronios que dispararam ao menos uma vez.

`neuron_inactive_count`: neuronios sem spikes.

`neuron_active_fraction`: fracao de neuronios ativos.

`neuron_dead_fraction`: fracao de neuronios silenciosos nesta execucao.

`neuron_mean_spikes`: media de spikes por neuronio, incluindo silenciosos.

`neuron_most_active` e `neuron_least_active_active`: indices dos extremos.

## Concentracao de spikes

`neuron_top_10_percent_spike_share`: fracao dos spikes produzida pelos 10% de
neuronios mais ativos.

`neuron_spike_gini`: medida de desigualdade da atividade por
neuronio. Valores maiores sugerem que poucos neuronios concentram muitos
spikes.

## EXC/INH

Quando for possivel inferir tipos de neuronios por `config_used.ini`,
`summary.txt` ou `raster.csv`, o comparador calcula:

- `network_excitatory_neuron_count`
- `network_inhibitory_neuron_count`
- `exc_total_spikes`
- `inh_total_spikes`
- `exc_mean_spikes_per_neuron`
- `inh_mean_spikes_per_neuron`
- `exc_inh_total_spike_ratio`

Se a inferencia nao for segura, os campos ficam vazios/`NA`.

## Sincronia aproximada

`activity_population_fano_factor`: variancia da atividade populacional dividida pela
media. Valores altos sugerem atividade mais irregular.

`activity_population_cv`: desvio padrao dividido pela media.

`activity_synchrony_proxy`: aproximacao simples:

```text
max_spikes_per_step / total_spikes
```

Ela indica concentracao temporal dos spikes, mas nao substitui uma analise
completa de sincronia neural.
