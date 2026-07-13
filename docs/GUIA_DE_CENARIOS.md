# Guia de cenários da miniSNN

[Voltar ao índice da documentação](INDICE_DA_DOCUMENTACAO.md)

## 1. O que sao cenarios

Cenarios sao arquivos `.ini` em `configs/` que descrevem uma simulacao sem
exigir edicao de codigo C.

Com um cenario, voce escolhe:

- nome da execucao;
- topologia;
- numero de neuronios;
- fracao de neuronios inibitorios;
- pesos EXC/INH;
- entrada externa;
- parametros LIF principais;
- duracao da simulacao;
- neuronio monitorado.

O runner fica em `app/minisnn_runner.c` e usa a API publica da miniSNN.

## 2. Como editar um `.ini`

Abra um arquivo em `configs/`, por exemplo:

```text
configs/random_balanced.ini
```

Edite uma chave por vez e salve. Comentarios podem comecar com `#` ou `;`.
Espacos antes e depois de `=` sao aceitos.

Exemplo:

```ini
[network]
topology = random_balanced
neurons = 20
connection_probability = 0.25
```

Se uma chave for desconhecida, duplicada ou tiver valor invalido, o runner
mostra erro com numero de linha.

## 3. Todas as chaves disponiveis

| Chave | Descricao |
| --- | --- |
| `run_name` | Nome solicitado; a pasta efetiva usa `actual_run_name` |
| `topology` | Topologia da rede |
| `neurons` | Numero total de neuronios |
| `inhibitory_fraction` | Fracao de neuronios inibitorios |
| `connection_probability` | Probabilidade de conexao em topologias probabilisticas |
| `seed` | Semente deterministica da topologia aleatoria |
| `delay` | Delay de cada conexao |
| `max_synaptic_delay` | Maior delay permitido na rede |
| `allow_self_connections` | Permite auto-conexoes em topologias aplicaveis |
| `allow_inh_to_inh` | Permite conexoes de neuronios INH para neuronios INH |
| `small_world_neighbors` | Numero par de vizinhos locais em `small_world` |
| `small_world_rewire_probability` | Probabilidade de reconexao em `small_world` |
| `feedforward_layers` | Numero de camadas em `feedforward` |
| `excitatory_weight` | Peso de saida de neuronios EXC |
| `inhibitory_weight` | Peso de saida de neuronios INH |
| `source_count` | Quantos neuronios iniciais recebem entrada |
| `input_current` | Corrente aplicada aos neuronios-fonte |
| `steps` | Numero de timesteps |
| `dt` | Passo de tempo do LIF |
| `tau` | Constante de tempo do LIF |
| `v_rest` | Potencial de repouso |
| `v_reset` | Potencial de reset |
| `v_threshold` | Limiar de spike |
| `resistance` | Resistencia do LIF |
| `synaptic_decay` | Decaimento da corrente sinaptica |
| `record_neuron` | Neuronio detalhado gravado em CSV individual |
| `auto_unique_run` | Evita sobrescrever uma pasta existente de `run_name` |
| `history_enabled` | Registra execucoes em `results/scenarios/index.csv` |
| `level` | Diagnostico `off`, `basic` ou `full` |
| `time_bin_steps` | Tamanho das janelas do modo full |
| `burst_z_threshold` | Multiplicador do desvio para detectar bursts |
| `min_burst_steps` | Duracao minima de burst |
| `isi_min_spikes` | Minimo de spikes para ISI |
| `correlation_sample_size` | Limite de neuronios na correlacao |
| `neuron_sample_limit` | Limite da amostra de neuronios |
| `sample_stride` | Passo deterministico da amostra |

Configs antigas sem `[diagnostics]` usam `off`. Arquivos salvos pelo Studio
registram a secao, e o padrao de um novo cenario e `basic`.

## 4. Valores validos

Regras principais:

- `run_name`: 1 a 48 caracteres, usando apenas letras, numeros, `_` e `-`.
- `topology`: `chain`, `ring`, `all_to_all`, `random`, `random_balanced`,
  `small_world` ou `feedforward`.
- `neurons`: entre 1 e 1000.
- `steps`: maior que zero.
- `source_count`: entre 1 e `neurons`.
- `record_neuron`: entre 0 e `neurons - 1`.
- `inhibitory_fraction`: entre 0.0 e 1.0.
- `connection_probability`: entre 0.0 e 1.0.
- `allow_self_connections` e `allow_inh_to_inh`: `true` ou `false`.
- `allow_self_connections` se aplica a `all_to_all`, `random`,
  `random_balanced` e `small_world`. Em `chain`, `ring` e `feedforward`, nao ha
  candidato natural de self-loop.
- `delay`: entre 1 e `max_synaptic_delay`.
- `max_synaptic_delay`: maior que zero.
- `small_world_neighbors`: numero par maior que zero e menor que `neurons`,
  usado em `small_world`.
- `small_world_rewire_probability`: entre 0.0 e 1.0, usado em `small_world`.
- `feedforward_layers`: entre 2 e `neurons`, usado em `feedforward`.
- `excitatory_weight`: maior que zero.
- `inhibitory_weight`: menor que zero.
- `dt`, `tau`, `resistance` e `synaptic_decay`: positivos.
- `auto_unique_run` e `history_enabled`: `true` ou `false`.

## 5. Topologias suportadas

### `chain`

Cria uma cadeia:

```text
0 -> 1 -> 2 -> ... -> N-1
```

### `ring`

Cria uma cadeia fechada:

```text
0 -> 1 -> 2 -> ... -> N-1 -> 0
```

### `all_to_all`

Cada neuronio conecta a todos os outros neuronios. Com
`allow_self_connections = true`, tambem inclui `source == target`.

### `random`

Percorre todos os pares `source -> target` e cria uma conexao quando o gerador
deterministico sorteia valor menor que `connection_probability`. Self-loops
entram no sorteio somente com `allow_self_connections = true`.

### `random_balanced`

Percorre todos os pares `source -> target` e cria a conexao
quando o gerador deterministico sorteia valor menor que
`connection_probability`. Self-loops entram no sorteio somente com
`allow_self_connections = true`.

A mesma `seed` e o mesmo arquivo produzem a mesma topologia no mesmo codigo.

### `small_world`

Cria uma vizinhanca local em anel usando `small_world_neighbors` e depois tenta
reconectar alvos com probabilidade `small_world_rewire_probability`. As
conexoes locais nao criam self-loop; durante a reconexao, `source == target` so
pode ser escolhido com `allow_self_connections = true`.

### `feedforward`

Divide os neuronios em `feedforward_layers` camadas e cria conexoes apenas de
uma camada para a proxima, usando `connection_probability`.

## 6. Como executar `chain.ini`

```powershell
mingw32-make scenario SCENARIO=configs/chain.ini
```

Saida:

```text
results/scenarios/chain_demo/
```

Arquivos principais:

```text
config_used.ini
summary.txt
population.csv
raster.csv
neuron_0.csv
```

## 7. Como executar `random_balanced.ini`

```powershell
mingw32-make scenario SCENARIO=configs/random_balanced.ini
```

Saida:

```text
results/scenarios/random_balanced_demo/
```

Esse e o cenario padrao do alvo `scenario`.

## 7.1. Saida e historico

A secao opcional `[output]` controla organizacao dos resultados:

```ini
[output]
auto_unique_run = false
history_enabled = true
```

Com `auto_unique_run = false`, o runner usa exatamente `run_name`, preservando
o comportamento antigo. Com `auto_unique_run = true`, se a pasta ja existir, a
saida real vira algo como:

```text
results/scenarios/random_demo_20260708_153012/
```

Cada execucao concluida com `history_enabled = true` adiciona uma linha em:

```text
results/scenarios/index.csv
```

No Studio, nome unico fica ativo automaticamente para evitar sobrescrita
acidental.

## 7.2. Diagnóstico

A seção opcional `[diagnostics]` controla profundidade e amostragem:

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

`off` preserva o fluxo histórico; `basic` gera métricas essenciais; `full`
adiciona análises mais caras e amostradas. Consulte o
[Guia de diagnóstico](GUIA_DE_DIAGNOSTICO.md).

### Atalhos de topologia

```powershell
mingw32-make scenario-random
mingw32-make scenario-small-world
mingw32-make scenario-feedforward
```

## 8. Como mudar parametros sem editar C

### Mudar peso INH

No arquivo `.ini`, edite:

```ini
[weights]
inhibitory_weight = -500.0
```

### Mudar densidade

Para `random`, `random_balanced` e `feedforward`, edite:

```ini
[network]
connection_probability = 0.50
```

### Mudar conexoes INH -> INH

```ini
[connectivity]
allow_inh_to_inh = false
```

### Permitir auto-conexoes

```ini
[connectivity]
allow_self_connections = true
```

Essa opcao cria self-loops reais apenas em `all_to_all`, `random`,
`random_balanced` e na etapa de reconexao de `small_world`.

### Mudar small-world

```ini
[topology_options]
small_world_neighbors = 6
small_world_rewire_probability = 0.20
```

### Mudar feedforward

```ini
[topology_options]
feedforward_layers = 4
```

### Mudar seed

```ini
[network]
seed = 2
```

### Mudar numero de neuronios

```ini
[network]
neurons = 50
inhibitory_fraction = 0.20
```

### Mudar duracao

```ini
[simulation]
steps = 2000
```

## 9. Como gerar graficos

Depois de executar um cenario:

```powershell
python scripts/plot_scenario.py results/scenarios/random_balanced_demo
```

O script cria na mesma pasta:

```text
population_activity.png
mean_state.png
raster.png
```

### Grafico de neuronio individual

Todo cenario tambem grava o neuronio escolhido em `record_neuron`:

```text
results/scenarios/<actual_run_name>/neuron_<id>.csv
```

O arquivo possui:

```text
tempo,V,spike,corrente_externa,corrente_sinaptica
```

Significado basico:

- `tempo`: timestep da simulacao.
- `V`: potencial de membrana do neuronio.
- `spike`: 1 quando houve spike, 0 caso contrario.
- `corrente_externa`: entrada externa aplicada naquele timestep.
- `corrente_sinaptica`: corrente sinaptica usada no LIF naquele timestep.

Para gerar o PNG individual:

```powershell
python scripts/plot_neuron.py results/scenarios/<actual_run_name> <neuron_id>
```

Saida:

```text
neuron_<id>_detail.png
```

## 10. Aviso sobre sobrescrita

O runner grava arquivos em modo `"w"`. Se voce executar novamente o mesmo
`run_name`, os resultados anteriores daquela pasta serao substituidos.

Antes de testar uma mudanca importante, copie a pasta do cenario ou use outro
`run_name`.

## 11. Como comparar dois testes

Use nomes diferentes:

```ini
[run]
run_name = random_p025_seed1
```

Depois rode outro arquivo ou outra versao com:

```ini
[run]
run_name = random_p050_seed1
```

Assim os resultados ficam em pastas separadas:

```text
results/scenarios/random_p025_seed1/
results/scenarios/random_p050_seed1/
```

Compare `summary.txt`, `population.csv`, `raster.csv` e os PNGs gerados por
`scripts/plot_scenario.py`.

## 12. Como comparar execucoes

Depois de rodar dois ou mais cenarios, use:

```powershell
python scripts/compare_runs.py results/scenarios/random_demo results/scenarios/small_world_demo --out-name random_vs_small_world
```

Saida:

```text
results/comparisons/random_vs_small_world/
```

Se a pasta de comparacao ja existir, o script cria uma pasta unica com
timestamp. Use `--overwrite` somente quando quiser reutilizar explicitamente a
pasta existente.

Arquivos gerados:

- `comparison_summary.csv`: metricas por execucao.
- `comparison_report.txt`: relatorio legivel com rankings e avisos.
- `comparison_metrics.png`: comparacao de metricas principais.
- `comparison_activity_overlay.png`: atividade populacional sobreposta.

O historico local de comparacoes fica em `results/comparisons/index.csv`.

Veja tambem:

```text
docs/GUIA_DE_METRICAS.md
```

## 13. Seção `[plasticity]`

As chaves disponíveis são `enabled`, `rule`, `a_plus`, `a_minus`, `tau_plus`,
`tau_minus`, `trace_increment`, `weight_min`, `weight_max`, `record_weights`,
`record_history`, `record_interval_steps` e `record_connection_limit`.

`enabled` e os dois campos `record_*` são booleanos. A única regra aceita no C1
é `stdp_pair_trace`. Amplitudes devem ser finitas e não negativas; taus e
incremento, positivos; `weight_min >= 0`; `weight_max > weight_min`; intervalo
e limite, inteiros positivos. Sem a seção, STDP fica desligado.

Exemplos prontos: `configs/stdp_ltp_demo.ini`, `configs/stdp_ltd_demo.ini` e
`configs/stdp_mixed_demo.ini`. Veja o [Guia de plasticidade](GUIA_DE_PLASTICIDADE.md).
