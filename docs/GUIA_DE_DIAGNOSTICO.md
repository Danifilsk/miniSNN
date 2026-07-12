# Guia de Diagnostico da miniSNN

## 1. Niveis

Os cenarios podem conter:

```ini
[diagnostics]
level = basic
time_bin_steps = 10
burst_z_threshold = 2.0
min_burst_steps = 1
isi_min_spikes = 4
correlation_sample_size = 128
neuron_sample_limit = 1000
sample_stride = 1
```

- `off`: preserva a execucao historica e gera apenas o manifesto adicional.
- `basic`: gera `metrics.csv`; a analise Python gera relatorio e visao geral.
- `full`: inclui janelas, ISI, neuronios, correlacoes amostradas e graficos extras.

Configs antigas sem a secao usam `off`. O Studio inicia novos cenarios em
`basic`.

## 2. Execucao

```powershell
mingw32-make scenario-random
python scripts/analyze_run.py results/scenarios/random_demo --level basic
python scripts/analyze_run.py results/scenarios/random_demo --level full
```

O mesmo fluxo esta disponivel no Studio pelo seletor `DIAG` e pelos botoes
`GERAR DIAGNOSTICO`, `ABRIR METRICAS` e `ABRIR DIAGNOSTICO`.

## 3. Arquivos

O runner sempre cria `run_manifest.txt`. Nos niveis `basic` e `full`, tambem
cria um `metrics.csv` barato com proveniencia, atividade, conectividade e tempo.

A analise `basic` cria:

```text
metrics.csv
metrics_report.txt
diagnostics_overview.png
```

A analise `full` tambem cria:

```text
metrics_neurons.csv
metrics_windows.csv
diagnostics_activity.png
diagnostics_distribution.png
diagnostics_exc_inh.png
diagnostics_isi.png
diagnostics_correlation.png
```

Quando um grafico nao pode ser calculado, o relatorio registra `Nao
disponivel` e o motivo.

## 4. Metricas basicas

`metrics.csv` usa nomes estaveis e prefixos por dominio:

- `run_`: passos, `dt`, seed, duracao e neuronio detalhado.
- `network_`: tamanho, tipos, conexoes, pesos, delays e graus.
- `activity_`: spikes, atividade, silencio, segmentos e variabilidade.
- `burst_`: eventos consecutivos acima do threshold.
- `neuron_`: atividade e concentracao por neuronio.
- `exc_` e `inh_`: atividade dos dois grupos.
- `voltage_` e `current_`: sinal do neuronio detalhado.
- `performance_`: tempos e tamanhos de arquivos.
- `diagnostic_` e `stability_`: classificacao heuristica e seus componentes.

Os principais calculos populacionais sao:

```text
activity_fraction = timesteps ativos / timesteps
silence_fraction = timesteps silenciosos / timesteps
population_cv = desvio dos spikes por passo / media
population_fano_factor = variancia dos spikes por passo / media
synchrony_proxy = pico de spikes / spikes totais
temporal_concentration = spikes nos 10% de passos mais ativos / total
```

`synchrony_proxy` e apenas uma aproximacao de concentracao temporal, nao uma
medida completa de sincronia neural.

As taxas usam a unidade temporal definida pelo cenario. Como `dt` nao possui
uma unidade fisica universal declarada pela API, a miniSNN usa nomes como
`spikes_per_neuron_per_time_unit`, e nao Hz.

## 5. Bursts

O threshold e:

```text
media_spikes_por_passo + burst_z_threshold * desvio_padrao
```

O valor minimo seguro e 1 spike. Passos consecutivos acima do threshold sao
agrupados; grupos menores que `min_burst_steps` sao descartados. Sao registrados
contagem, duracao, tamanho, fracao e intervalo medio entre bursts. Essa e uma
heuristica diagnostica configuravel.

## 6. Distribuicao por neuronio

O raster fornece contagens por neuronio, ativos/inativos, extremos, Gini,
entropia e participacao dos 1%, 5% e 10% mais ativos.

- Gini alto sugere concentracao em poucos neuronios.
- Entropia normalizada alta sugere distribuicao mais uniforme.
- `neuron_dead_fraction` significa apenas neuronios sem spikes na execucao.

Casos sem spikes, com um neuronio ou com divisao por zero retornam zero ou `NA`
conforme a metrica tenha ou nao significado.

## 7. EXC e INH

Os tipos sao lidos do raster. Quando o raster esta vazio, a regra registrada no
cenario e no `summary.txt` confirma a divisao: os ultimos N neuronios sao INH.
Se isso nao puder ser confirmado, as metricas ficam `NA` e o relatorio avisa.

## 8. Modo full

`metrics_windows.csv` divide a execucao em janelas de `time_bin_steps` e registra
spikes, neuronios ativos, media, variancia, Fano, EXC e INH.

ISI e calculado apenas para neuronios com pelo menos `isi_min_spikes`. Neuronios
hiperativos sao definidos por:

```text
spikes > media dos neuronios + 3 * desvio padrao
```

Correlacoes usam spikes em bins. A selecao e deterministica pela seed, respeita
`correlation_sample_size`, `neuron_sample_limit` e `sample_stride`, e nunca cria
uma matriz completa sem limite.

## 9. Classificacao de regime

As classes sao `silent`, `sparse`, `sustained`, `intermittent`, `bursting`,
`hyperactive`, `mixed` e `undetermined`.

- `silent`: nenhum spike.
- `sparse`: menos de 10% dos passos ativos e atividade media inferior a 5% da populacao.
- `sustained`: pelo menos 25% dos passos ativos e atividade no ultimo quarto.
- `intermittent`: atividade tardia, fracao entre 5% e 75% e silencio continuo de pelo menos 10% da execucao (minimo 5 passos).
- `bursting`: pelo menos metade dos spikes pertence a bursts detectados.
- `hyperactive`: media de pelo menos 50% ou pico de pelo menos 80% da populacao.
- `mixed`: mais de um sinal forte.
- `undetermined`: dados insuficientes ou nenhum threshold dominante.

`diagnostic_confidence` e uma confianca heuristica, nao estatistica. Os sinais
usados ficam em `diagnostic_reasons`.

## 10. Stability score

O score e a media, limitada a `[0, 1]`, de quatro componentes tambem gravados:

```text
silence_component = fracao de passos ativos
explosion_component = 1 - min(1, fracao_do_pico / 0.5)
persistence_component = 1 quando ha atividade no ultimo quarto, senao 0
distribution_component = entropia normalizada dos spikes por neuronio
```

Uma rede completamente silenciosa recebe zero. O indice penaliza silencio,
picos populacionais extremos, falta de persistencia e concentracao excessiva.
Ele e um indice heuristico de diagnostico, nao uma medida biologica universal.

## 11. Proveniencia e desempenho

`run_manifest.txt` registra versao, compilador, arquitetura, config, nomes
solicitado/real, seed, modelo, topologia, `dt`, passos, nivel e arquivos. A
analise acrescenta Python, pandas e matplotlib. Informacoes indisponiveis ficam
`NA` e nao interrompem a simulacao.

Tempos reais de simulacao e analise sao separados da duracao simulada. Tambem
sao registrados throughput e tamanhos dos CSVs/resultados.

## 12. Compatibilidade e limites

Runs antigas continuam analisaveis se possuirem `population.csv`. Raster ou CSV
individual ausente reduz as metricas disponiveis, mas nao invalida o restante.
`compare_runs.py` reutiliza `metrics.csv` completo e deriva apenas o que faltar.

O raster e lido em chunks. O modo `basic` evita correlacoes O(N^2); o modo
`full` usa amostragem. A analise de potencial de todos os neuronios nao e feita,
pois a execucao atual registra serie temporal completa apenas do neuronio
detalhado. Nenhuma interpretacao gerada estabelece causalidade biologica.
