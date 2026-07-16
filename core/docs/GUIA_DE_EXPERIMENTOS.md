# Guia de experimentos da miniSNN

[Voltar ao índice da documentação](INDICE_DA_DOCUMENTACAO.md)

Os programas desta pasta são **EXPERIMENTAIS**. Eles geram evidência
reproduzível, mas não estabelecem causalidade biológica por si só.

## Regra principal

**Nao comece modificando `src/`.**

Para testar hipoteses, modifique ou copie um arquivo em `experiments/`.
Primeiro rode a versao original e registre o resultado base. Depois altere uma
variavel por vez.

## Onde os parametros vivem

Existem dois lugares importantes:

```text
src/config.h
experiments/*.c
```

`src/config.h` define parametros compartilhados pelo nucleo e por varios
experimentos.

Parametros principais de `src/config.h`:

| Parametro | O que faz |
| --- | --- |
| `DT` | Passo de tempo usado na integracao LIF |
| `TAU` | Constante de tempo do neuronio LIF |
| `V_REST` | Potencial de repouso |
| `V_RESET` | Potencial apos spike |
| `V_THRESH` | Limiar de disparo |
| `R` | Resistencia usada na equacao LIF |
| `I_EXT` | Corrente externa padrao aplicada em estimulos |
| `W_EXC` | Peso excitatorio padrao |
| `W_INH` | Peso inibitorio padrao |
| `SYN_DECAY` | Decaimento da corrente sinaptica |
| `MAX_SYNAPTIC_DELAY` | Maior delay sinaptico aceito |

Aviso importante:

Alterar `src/config.h` pode afetar experimentos que usam os valores globais.
Mas varios experimentos possuem pesos ou configuracoes locais e nao obedecem
diretamente a `W_INH`.

Exemplos reais:

- `example_ei_balance.c` usa `I_EXT`, `W_EXC` e `W_INH` de `config.h`.
- `example_inh_to_inh_visual.c` usa `TEST_W_INH` definido no proprio arquivo.
- `example_inhibition_sweep.c`, `example_inhibition_transition.c` e
  `example_inhibition_fine_sweep.c` usam listas locais de pesos inibitorios.
- `example_sparse_ei_balance_analysis.c` e
  `example_sparse_ei_robustness.c` usam `TEST_W_INH`, probabilidades e seeds
  definidos no proprio arquivo.

## Experimentos principais suportados pelo Makefile

| Comando | Arquivo | Pergunta cientifica | Parametros mais importantes | Saidas |
| --- | --- | --- | --- | --- |
| `mingw32-make ei-balance` | `experiments/example_ei_balance.c` | Uma sinapse inibitoria muda o alvo em relacao a excitacao pura? | `EXPERIMENT_STEPS`, `I_EXT`, `W_EXC`, `W_INH` | `results/experiments/ei_balance/` |
| `mingw32-make inhibition-fine` | `experiments/example_inhibition_fine_sweep.c` | Onde ocorre a transicao entre avalanche e supressao? | `POPULATION_SIZE`, `INHIBITORY_COUNT`, `EXPERIMENT_STEPS`, lista de pesos, `SUSTAINED_ACTIVITY_STEP` | `results/experiments/inhibition/` |
| `mingw32-make inh-to-inh` | `experiments/example_inh_to_inh_visual.c` | O que muda ao remover conexoes INH -> INH? | `TEST_W_INH`, `POPULATION_SIZE`, `INHIBITORY_COUNT`, `INPUT_SOURCE_COUNT`, `EXPERIMENT_STEPS` | `results/experiments/inh_to_inh/` |
| `mingw32-make sparse-ei` | `experiments/example_sparse_ei_balance_analysis.c` | Como EXC e INH se comportam em redes esparsas aleatorias? | `TEST_W_INH`, `probabilities[]`, `seeds[]`, `LAST_WINDOW_START` | `results/experiments/sparse_ei/` |

## Receita 1 - Alterar um peso inibitorio

Arquivo:

```text
experiments/example_inhibition_fine_sweep.c
```

Esse experimento usa uma rede all-to-all. Os parametros principais sao:

- `POPULATION_SIZE`: tamanho da rede.
- `INHIBITORY_COUNT`: numero de neuronios inibitorios.
- `EXPERIMENT_STEPS`: duracao da simulacao.
- Lista de pesos inibitorios em `main`.
- `SUSTAINED_ACTIVITY_STEP`: inicio da regiao tardia usada para classificar
  atividade sustentada.

Procedimento:

1. Execute:

   ```powershell
   mingw32-make inhibition-fine
   ```

2. Abra:

   ```text
   results/experiments/inhibition/inhibition_fine_sweep.csv
   ```

3. Copie o arquivo para uma pasta de comparacao antes de alterar algo.
4. Altere somente a lista de pesos inibitorios.
5. Execute novamente.
6. Compare `spikes_totais`, `pico_spikes`, `ultimo_timestep_ativo` e atividade
   sustentada.

Alterar `W_INH` em `src/config.h` nao e a forma correta de mudar esse sweep,
porque ele usa pesos locais.

## Receita 2 - Comparar presenca e ausencia de INH -> INH

Arquivo:

```text
experiments/example_inh_to_inh_visual.c
```

Parametros principais:

- `TEST_W_INH`: peso inibitorio testado.
- `POPULATION_SIZE` e `INHIBITORY_COUNT`: composicao da rede.
- `INPUT_SOURCE_COUNT`: quantos neuronios recebem estimulo.
- `EXPERIMENT_STEPS`: duracao da simulacao.

Esse arquivo compara duas arquiteturas:

- `all_to_all`: EXC conecta a todos, INH conecta a EXC e INH.
- `no_inh_to_inh`: EXC conecta a todos, INH conecta somente a EXC.

Execute:

```powershell
mingw32-make inh-to-inh

python scripts/plot_inh_to_inh_comparison.py
```

Arquivos gerados:

```text
all_to_all_-400_spikes.csv
all_to_all_-400_metrics.csv
no_inh_to_inh_-400_spikes.csv
no_inh_to_inh_-400_metrics.csv
all_to_all_-400_raster.png
no_inh_to_inh_-400_raster.png
inh_to_inh_population_activity.png
```

Esses graficos descrevem o regime testado com os parametros atuais. Eles nao
provam uma conclusao universal sobre todas as redes EXC/INH.

## Receita 3 - Mudar densidade e seeds em rede esparsa

Arquivo:

```text
experiments/example_sparse_ei_balance_analysis.c
```

Parametros principais:

- `TEST_W_INH`: peso inibitorio local.
- `probabilities[]`: densidades de conexao.
- `seeds[]`: repeticoes aleatorias.
- `PROBABILITY_COUNT`: deve coincidir com o tamanho de `probabilities[]`.
- `SEED_COUNT`: deve coincidir com o tamanho de `seeds[]`.
- `LAST_WINDOW_START`: inicio da janela tardia analisada.

Aviso forte:

**Nao aumente `PROBABILITY_COUNT` ou `SEED_COUNT` sem tambem ajustar o vetor
correspondente.**

Arquiteturas comparadas:

- `random_all_connections`: EXC conecta a EXC e INH; INH conecta a EXC e INH.
- `random_no_inh_to_inh`: EXC conecta a EXC e INH; INH conecta apenas a EXC.

Nao trate uma arquitetura como sempre melhor, mais estavel ou mais biologica.
Leia os resultados como descricao do conjunto atual de parametros.

Arquivos principais:

- `sparse_ei_balance_runs.csv`: uma linha por execucao.
- `sparse_ei_balance_summary.csv`: medias por arquitetura e probabilidade.

## Outros experimentos disponiveis

| Arquivo | Objetivo | Pasta de resultado | Alvo no Makefile | Como compilar quando nao houver alvo |
| --- | --- | --- | --- | --- |
| `example_population_balance.c` | Comparar populacao EXC-only com EXC/INH | `results/experiments/population_balance/` | Nao | Compilacao manual |
| `example_inhibition_sweep.c` | Varredura ampla de pesos inibitorios | `results/experiments/inhibition/` | Nao | Compilacao manual |
| `example_inhibition_transition.c` | Varredura intermediaria da transicao inibitoria | `results/experiments/inhibition/` | Nao | Compilacao manual |
| `example_inhibition_regimes.c` | Observar regimes temporais especificos | `results/experiments/inhibition_regimes/` | Nao | Compilacao manual |
| `example_inh_to_inh_comparison.c` | Comparar metricas com e sem INH -> INH | `results/experiments/inh_to_inh/` | Nao | Compilacao manual |
| `example_sparse_ei_robustness.c` | Medir robustez em redes esparsas | `results/experiments/sparse_ei/` | Nao | Compilacao manual |

Padrao de compilacao manual:

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -fanalyzer experiments/example_inhibition_sweep.c src/neuron.c src/network.c src/stimulus.c src/recorder.c -Iinclude -Isrc -o build/example_inhibition_sweep.exe
.\build\NOME_DO_EXECUTAVEL.exe
```

Use esse padrao apenas para arquivos que ja existem em `experiments/`.

## Como ler os CSVs

### Dados de neuronio

Cabecalho:

```text
tempo,V,spike,corrente_externa,corrente_sinaptica
```

Colunas:

- `tempo`: timestep.
- `V`: potencial de membrana apos o update.
- `spike`: `1` se o neuronio disparou naquele timestep, senao `0`.
- `corrente_externa`: corrente externa aplicada naquele timestep.
- `corrente_sinaptica`: corrente sinaptica usada pelo LIF naquele timestep.

### Dados de populacao

Cabecalho:

```text
tempo,spikes_total,spikes_exc,spikes_inh,mean_potential,mean_syn_current
```

Colunas:

- `spikes_total`: total de spikes no timestep.
- `spikes_exc`: spikes de neuronios excitatorios.
- `spikes_inh`: spikes de neuronios inibitorios.
- `mean_potential`: media dos potenciais.
- `mean_syn_current`: media da corrente sinaptica usada no timestep.

### Raster

Cabecalho:

```text
tempo,neuronio,tipo
```

Cada linha representa um spike de um neuronio em um timestep. `tipo` indica
`EXC` ou `INH`.

### Sweeps e resultados esparsos

Esses arquivos possuem cabecalhos descritivos. Leia campos como
`spikes_totais`, `pico_spikes`, `ultimo_timestep_ativo` e
`neuronios_ativos` junto com a configuracao usada no experimento.

## Graficos Python

Para o experimento EI balance:

```powershell
python scripts/plot_ei_balance.py
```

Esse script espera CSVs em `results/experiments/ei_balance/` e produz:

```text
ei_compare_voltage.png
ei_compare_syn_current.png
ei_compare_population_spikes.png
```

Para a comparacao INH -> INH:

```powershell
python scripts/plot_inh_to_inh_comparison.py
```

Esse script espera CSVs em `results/experiments/inh_to_inh/` e produz:

```text
all_to_all_-400_raster.png
no_inh_to_inh_-400_raster.png
inh_to_inh_population_activity.png
```

Os scripts usam `plt.show()`. Feche a janela do grafico para o terminal
encerrar.

## Checklist antes de alterar um experimento

```text
[ ] Rodei a versao original.
[ ] Salvei ou copiei os resultados-base.
[ ] Sei qual arquivo controla o parametro que quero mudar.
[ ] Vou alterar uma variavel por vez.
[ ] Mantive contagens e vetores consistentes.
[ ] Sei onde o novo CSV sera salvo.
[ ] Vou registrar os parametros usados junto com o resultado.
[ ] Nao alterei src/ para testar uma hipotese simples.
```
