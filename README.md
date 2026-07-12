# miniSNN

miniSNN e uma biblioteca em C para redes neurais pulsadas simplificadas,
baseada em neuronios LIF (Leaky Integrate-and-Fire).

O projeto tem finalidade exploratoria e educacional. Ele nao e uma simulacao
biologicamente completa do cerebro. A miniSNN usa neuronios LIF, pesos
excitarorios/inibitorios, delays sinapticos e corrente sinaptica com decaimento.
Tambem possui uma API publica em `minisnn.h`.

## Recursos Atuais

- Neuronios LIF configuraveis por rede.
- Sinapses excitatorias e inibitorias.
- Delays sinapticos.
- Estimulos externos.
- Topologias e experimentos.
- Recorders CSV.
- API publica MiniSNN.
- Testes automatizados em C.

## Estrutura Atual Do Projeto

```text
include/                   API publica
src/                       nucleo interno
tests/                     testes em C
examples/api/              exemplos usando apenas minisnn.h
examples/internal/         demo usando nucleo interno
experiments/               experimentos cientificos em C
scripts/                   scripts Python ativos
scripts/legacy/            scripts antigos mantidos para referencia
configs/                   cenarios configuraveis em formato INI
results/api/               resultados dos exemplos publicos
results/internal_demo/     saida do demo interno
results/experiments/       resultados dos experimentos
results/scenarios/         resultados locais dos cenarios
results/comparisons/       comparacoes locais de execucoes
```

## Compilacao rapida com Makefile

O Makefile e o caminho recomendado. No ambiente Windows atual do projeto, o
comando disponivel e `mingw32-make`. O comando `make` so deve ser usado se ele
existir no PATH do usuario.

`mingw32-make` sem argumentos executa os testes.

```powershell
mingw32-make test
mingw32-make api-examples
mingw32-make demo
mingw32-make ei-balance
mingw32-make clean
```

## Cenarios configuraveis

Edite um arquivo em `configs/` e execute:

```powershell
mingw32-make scenario SCENARIO=configs/random_balanced.ini
```

Topologias disponiveis nos cenarios:

```text
chain, ring, all_to_all, random, random_balanced, small_world, feedforward
```

Atalhos uteis:

```powershell
mingw32-make scenario-random
mingw32-make scenario-small-world
mingw32-make scenario-feedforward
```

Os resultados ficam em:

```text
results/scenarios/<run_name>/
```

Quando `auto_unique_run = true`, o runner cria uma pasta unica se o `run_name`
ja existir. O historico local de execucoes fica em
`results/scenarios/index.csv`.

Cada cenario tambem grava `neuron_<id>.csv` para o neuronio detalhado. O grafico
individual pode ser gerado com:

```powershell
python scripts/plot_neuron.py results/scenarios/<run_name> <neuron_id>
```

Para comparar duas ou mais execucoes:

```powershell
python scripts/compare_runs.py results/scenarios/random_demo results/scenarios/small_world_demo --out-name random_vs_small_world
```

As comparacoes ficam em `results/comparisons/<comparison_name>/`. Se o nome ja
existir, `scripts/compare_runs.py` cria uma pasta unica por padrao e registra
`results/comparisons/index.csv`.

## miniSNN Studio

Para abrir a interface grafica no Windows:

```powershell
mingw32-make studio
```

O executavel gerado fica em:

```text
build/minisnn_studio.exe
```

A interface permite criar, carregar, salvar e executar cenarios sem editar
arquivos C. O botao `OPCOES`, ao lado da topologia, abre as configuracoes de
conectividade, seed, delays e opcoes especificas de `small_world` e
`feedforward`. A opcao de auto-conexao funciona em topologias que consideram
`source -> target` como candidato: `all_to_all`, `random`, `random_balanced` e
`small_world`. Em `chain`, `ring` e `feedforward`, ela fica desabilitada.

O Studio evita sobrescrita automaticamente ao rodar simulacoes. Use `ABRIR
RESULTADOS` para abrir `results/`, `ABRIR ULTIMA` para abrir a pasta real da
ultima execucao e `ABRIR HISTORICO` para abrir `results/scenarios/index.csv`.

## Diagnostico da rede

Os cenarios aceitam diagnostico `off`, `basic` ou `full`. Para analisar uma run:

```powershell
python scripts/analyze_run.py results/scenarios/random_demo --level basic
python scripts/analyze_run.py results/scenarios/random_demo --level full
```

O resultado inclui `metrics.csv`, relatorio em portugues e graficos. Consulte
[docs/GUIA_DE_DIAGNOSTICO.md](docs/GUIA_DE_DIAGNOSTICO.md) para formulas,
thresholds, custo e limitacoes das heuristicas.

Na API publica, `minisnn_connect()` e `minisnn_connect_delayed()` preservam o
comportamento antigo e rejeitam auto-conexoes. Para permitir `source == target`
explicitamente, use `minisnn_connect_delayed_ex()`.

Para gerar graficos, o Studio procura Python automaticamente usando
`MINISNN_PYTHON`, a instalacao local do Windows, `py.exe` ou `python.exe`.
Antes de aceitar um interpretador, ele valida `pandas` e `matplotlib`. A
interface normal nao exige terminal; a deteccao e automatica. Evite Python de
jogos, caches ou runtimes internos.

## Documentacao

- `docs/MANUAL_DE_USO.md`: instalacao, compilacao, estrutura e fluxo basico.
- `docs/GUIA_DE_EXPERIMENTOS.md`: como configurar, executar e interpretar os experimentos.
- `docs/GUIA_DE_CENARIOS.md`: como executar simulacoes editando arquivos `.ini`.
- `docs/GUIA_DO_STUDIO.md`: como usar a interface grafica inicial.
- `docs/GUIA_DE_METRICAS.md`: metricas geradas na comparacao de execucoes.
- `docs/GUIA_DE_DIAGNOSTICO.md`: niveis, formulas, classificacao e limitacoes.
- `docs/ORGANIZACAO_DE_RESULTADOS.md`: nomes unicos, historicos e limpeza segura.
- `API_REFERENCE.md`: referencia da API publica em `include/minisnn.h`.

## Compilacao manual com GCC (referencia)

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -fanalyzer examples/api/example_api_single_neuron.c src/minisnn.c src/neuron.c src/network.c -Iinclude -Isrc -o build/example_api_single_neuron.exe
.\build\example_api_single_neuron.exe

gcc -std=c11 -Wall -Wextra -pedantic -fanalyzer examples/api/example_api_chain.c src/minisnn.c src/neuron.c src/network.c -Iinclude -Isrc -o build/example_api_chain.exe
.\build\example_api_chain.exe

gcc -std=c11 -Wall -Wextra -pedantic -fanalyzer examples/api/example_api_exc_inh.c src/minisnn.c src/neuron.c src/network.c -Iinclude -Isrc -o build/example_api_exc_inh.exe
.\build\example_api_exc_inh.exe

gcc -std=c11 -Wall -Wextra -pedantic -fanalyzer tests/test_minisnn_api.c src/minisnn.c src/neuron.c src/network.c -Iinclude -Isrc -o build/test_minisnn_api.exe
.\build\test_minisnn_api.exe

gcc -std=c11 -Wall -Wextra -pedantic -fanalyzer experiments/example_ei_balance.c src/neuron.c src/network.c src/stimulus.c src/recorder.c -Iinclude -Isrc -o build/example_ei_balance.exe
.\build\example_ei_balance.exe

gcc -std=c11 -Wall -Wextra -pedantic -fanalyzer examples/internal/demo_main.c src/neuron.c src/network.c src/topology.c src/stimulus.c src/recorder.c -Iinclude -Isrc -o build/demo_main.exe
.\build\demo_main.exe

& "C:\Users\danif\AppData\Local\Python\pythoncore-3.14-64\python.exe" scripts/plot_ei_balance.py
```

## Exemplo Minimo De API

```c
#include <stdio.h>
#include "minisnn.h"

int main(void)
{
    MiniSNNConfig config = minisnn_default_config();
    config.neuron_count = 1;

    MiniSNN *brain = minisnn_create_with_config(&config);
    if (brain == NULL) {
        return 1;
    }

    minisnn_set_input(brain, 0, 20.0);

    for (int t = 0; t < 1000; t++) {
        int spikes = minisnn_step(brain);

        if (spikes < 0) {
            minisnn_destroy(&brain);
            return 1;
        }
    }

    minisnn_destroy(&brain);
    return 0;
}
```

## Limitacoes Atuais

- Nao ha plasticidade/STDP ainda.
- Nao ha periodo refratario explicito.
- Nao ha ruido ou heterogeneidade por neuronio.
- Pesos e unidades ainda sao parametros de simulacao.
- O projeto ainda passara por revisao, limpeza e polimento.
