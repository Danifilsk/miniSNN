# Organização de resultados da miniSNN

[Voltar ao índice da documentação](INDICE_DA_DOCUMENTACAO.md)

Este guia explica onde a miniSNN grava resultados locais e como evitar
sobrescrita acidental.

## 1. Execucoes de cenarios

Cada cenario grava seus arquivos em:

```text
results/scenarios/<actual_run_name>/
```

Arquivos principais:

- `config_used.ini`
- `summary.txt`
- `population.csv`
- `raster.csv`
- `neuron_<id>.csv`

## 2. Comparacoes

Comparacoes ficam em:

```text
results/comparisons/<comparison_name>/
```

Arquivos principais:

- `comparison_summary.csv`
- `comparison_report.txt`
- `comparison_metrics.png`
- `comparison_activity_overlay.png`

## 3. `auto_unique_run`

Em arquivos `.ini`, a secao opcional `[output]` aceita:

```ini
[output]
auto_unique_run = false
history_enabled = true
```

Com `auto_unique_run = false`, o runner usa exatamente `run_name`. Esse e o
comportamento historico e preserva compatibilidade com comandos e testes
antigos.

Com `auto_unique_run = true`, se `results/scenarios/<run_name>/` ja existir, o
runner cria uma pasta com timestamp:

```text
<run_name>_YYYYMMDD_HHMMSS
```

Se ainda houver colisao, um sufixo numerico e adicionado.

## 4. `run_name` versus `actual_run_name`

`run_name` e o nome pedido no cenario.

`actual_run_name` e o nome real da pasta criada. Eles sao iguais quando nao ha
colisao ou quando `auto_unique_run = false`.

## 5. Historico de execucoes

Quando `history_enabled = true`, cada execucao concluida adiciona uma linha em:

```text
results/scenarios/index.csv
```

Esse arquivo e append-only e local. Ele registra `timestamp`, `run_name`,
`actual_run_name`, caminho, topologia, passos, seed, conexoes, spikes e status.

## 6. Historico de comparacoes

`scripts/compare_runs.py` tambem evita sobrescrita por padrao. Ao repetir:

```powershell
python scripts/compare_runs.py A B --out-name teste
```

se `results/comparisons/teste/` ja existir, o script cria uma pasta unica com
timestamp.

O historico local fica em:

```text
results/comparisons/index.csv
```

## 7. Studio

O Studio sempre ativa nome unico ao rodar simulacoes. Assim, rodar duas vezes o
mesmo cenario nao apaga a primeira saida.

Botoes uteis:

- `ABRIR RESULTADOS`: abre `results/`.
- `ABRIR ULTIMA`: abre a pasta real da ultima execucao.
- `ABRIR HISTORICO`: abre `results/scenarios/index.csv`.
- `COMPARAR EXECUCOES`: compara pastas em `results/scenarios/`.
- `ABRIR COMPARACAO`: abre a ultima comparacao gerada.

## 8. Limpeza segura

Arquivos em `results/scenarios/` e `results/comparisons/` sao resultados locais
recriaveis, mas podem ser importantes para analise. Antes de limpar, mova ou
copie execucoes importantes para uma pasta de arquivo.

`results/archive/` e `results/experiments/` nao devem ser apagados por limpeza
normal.

## 9. Arquivos de diagnostico

Runs novas tambem podem conter `run_manifest.txt`, `metrics.csv`,
`metrics_report.txt`, `metrics_report.html` e graficos `diagnostics_*.png`. No modo full aparecem
`metrics_neurons.csv` e `metrics_windows.csv`. Esses arquivos pertencem a mesma
pasta real da execucao e seguem a mesma politica de nome unico.

Runs com registro de pesos também podem conter `weights_report.html`. Os dois
HTMLs são apresentações locais e autossuficientes; `metrics.csv`,
`weights_final.csv`, `plasticity_metrics.csv` e `weight_history.csv` continuam
como dados brutos. Links relativos nos relatórios permitem abrir esses arquivos
sem depender de internet. Runs antigas podem receber o relatório depois com
`scripts/generate_run_reports.py`; STDP desligado normalmente não produz
`weights_final.csv` nem relatório de pesos.

## 10. O que nao deve ser commitado

Por padrao, o Git ignora resultados locais de cenarios e comparacoes:

```text
results/scenarios/*
results/comparisons/*
```

Os `.gitkeep` dessas pastas continuam versionaveis para preservar a estrutura.

## Arquivos de homeostase

Runs ativas podem incluir `homeostasis_metrics.csv`, `homeostasis_history.csv`,
`threshold_history.csv`, `homeostasis_neurons.csv`, `homeostasis_report.txt`,
`homeostasis_report.html` e `homeostasis_overview.png`. Eles permanecem na pasta
da run. Benchmark C1.5 recriável fica em `results/benchmarks/` e continua
ignorado pelo Git conforme a política local.
