# Guia de Cenarios da miniSNN

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
| `run_name` | Nome da execucao e da pasta em `results/scenarios/` |
| `topology` | Topologia da rede |
| `neurons` | Numero total de neuronios |
| `inhibitory_fraction` | Fracao de neuronios inibitorios |
| `connection_probability` | Probabilidade de conexao em `random_balanced` |
| `seed` | Semente deterministica da topologia aleatoria |
| `delay` | Delay de cada conexao |
| `max_synaptic_delay` | Maior delay permitido na rede |
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
| `record_neuron` | Neuronio gravado em CSV individual |

## 4. Valores validos

Regras principais:

- `run_name`: 1 a 48 caracteres, usando apenas letras, numeros, `_` e `-`.
- `topology`: `chain`, `ring`, `all_to_all` ou `random_balanced`.
- `neurons`: entre 1 e 1000.
- `steps`: maior que zero.
- `source_count`: entre 1 e `neurons`.
- `record_neuron`: entre 0 e `neurons - 1`.
- `inhibitory_fraction`: entre 0.0 e 1.0.
- `connection_probability`: entre 0.0 e 1.0.
- `delay`: entre 1 e `max_synaptic_delay`.
- `max_synaptic_delay`: maior que zero.
- `excitatory_weight`: maior que zero.
- `inhibitory_weight`: menor que zero.
- `dt`, `tau`, `resistance` e `synaptic_decay`: positivos.

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

Cada neuronio conecta a todos os outros neuronios, sem auto-conexao.

### `random_balanced`

Percorre todos os pares `source -> target`, sem auto-conexao, e cria a conexao
quando o gerador deterministico sorteia valor menor que
`connection_probability`.

A mesma `seed` e o mesmo arquivo produzem a mesma topologia no mesmo codigo.

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

## 8. Como mudar parametros sem editar C

### Mudar peso INH

No arquivo `.ini`, edite:

```ini
[weights]
inhibitory_weight = -500.0
```

### Mudar densidade

Para `random_balanced`, edite:

```ini
[network]
connection_probability = 0.50
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
& "C:\Users\danif\AppData\Local\Python\pythoncore-3.14-64\python.exe" scripts/plot_scenario.py results/scenarios/random_balanced_demo
```

O script cria na mesma pasta:

```text
population_activity.png
mean_state.png
raster.png
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
