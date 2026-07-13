# Mapa do projeto

Este documento responde: “qual arquivo cuida de quê?”.

## Arquivos e pastas principais

| Arquivo ou pasta | Responsabilidade | Quem utiliza |
|---|---|---|
| `include/minisnn.h` | Contrato da API pública e tipo opaco `MiniSNN`. | Exemplos, runner e aplicações externas. |
| `include/minisnn_types.h` | Enum público EXC/INH. | API e núcleo. |
| `src/minisnn.c` | Adapta a API pública ao módulo interno de rede. | Programas que ligam a biblioteca. |
| `src/neuron.c` e `src/neuron.h` | Parâmetros e atualização do neurônio LIF. | Rede e testes do LIF. |
| `src/network.c` e `src/network.h` | Ciclo de vida, atualização, conexões, correntes e delays. | Fachada pública, núcleo e experimentos. |
| `src/connection.h` | Estruturas internas das conexões. | Rede e topologias internas. |
| `src/topology.c` e `src/topology.h` | Topologias internas antigas: chain, ring, all-to-all e random. | Demo e testes internos. |
| `src/stimulus.c` e `src/stimulus.h` | Pulsos e agenda de estímulos. | Demo, testes e experimentos internos. |
| `src/recorder.c` e `src/recorder.h` | Recorders individuais e populacionais. | Demo, testes e experimentos internos. |
| `app/scenario_config.c` e `app/scenario_config.h` | Defaults, parser, validação e gravação dos INIs. | Runner, Studio e testes. |
| `app/scenario_runner.c` e `app/scenario_runner.h` | Tipos, topologias, simulação, CSVs, métricas básicas e histórico. | Runner e Studio. |
| `app/minisnn_runner.c` | Programa de terminal para um cenário. | Usuário de linha de comando. |
| `app/minisnn_studio.c` | Interface Win32 para cenários, gráficos, comparação e diagnóstico. | Usuário do Studio. |
| `configs/` | Cenários de exemplo reproduzíveis. | Runner, Studio e testes manuais. |
| `scripts/analyze_run.py` | Diagnóstico `basic/full`, relatório e gráficos. | Terminal e Studio. |
| `scripts/metrics_common.py` | Fórmulas compartilhadas de métricas. | Analisador e comparador. |
| `scripts/compare_runs.py` | Compara runs novas ou antigas. | Terminal e Studio. |
| `scripts/plot_scenario.py` | Gráficos gerais de uma execução. | Terminal e Studio. |
| `scripts/plot_neuron.py` | Gráfico do neurônio detalhado. | Terminal e Studio. |
| `tests/test_minisnn_api.c` | Contrato e ciclo de vida da API pública. | `test-api`. |
| `tests/test_topology.c` | Núcleo, topologias internas, estímulos e recorders. | `test-core`. |
| `tests/test_LIF.c` | Smoke test básico do LIF; não é validação matemática completa. | `test-lif`. |
| `tests/test_scenario_config.c` | Parser, defaults, erros e compatibilidade de INI. | `test-scenario`. |
| `tests/test_scenario_runner.c` | Integração do runner e seus arquivos. | `test-runner`. |
| `src/plasticity.c` e `src/plasticity.h` | Matemática, traces, índice de entradas e estatísticas STDP. | Interno ao Core. |
| `tests/test_plasticity.c` | Valores exatos, ordem temporal, clamps, traces e autoconexões. | `test-plasticity`. |
| `tests/test_plasticity_long.c` | Finitude e limites em execução prolongada. | `test-plasticity-long`. |
| `scripts/plot_plasticity.py` | Panorama de pesos, deltas, trajetórias e métricas. | `test-plot-plasticity`. |
| `tests/test_analyze_run.py` | Casos sintéticos do diagnóstico. | `test-diagnostics`. |
| `tests/test_compare_runs.py` | Comparação, fallback e nomes únicos. | `test-compare-runs`. |
| `tests/test_plot_neuron.py` | Geração do gráfico individual. | `test-plot-neuron`. |
| `examples/api/` | Uso somente por `minisnn.h`. | Novos usuários da API. |
| `experiments/` | Experimentos exploratórios, não API pública. | Pesquisa e reprodução histórica. |
| `results/` | Saídas locais e resultados preservados. | Scripts e análise humana. |

## Fluxo de uma execução

1. O usuário escolhe um INI em `configs/` ou preenche o Studio.
2. `scenario_config` aplica defaults, lê chaves e valida valores.
3. `scenario_runner` cria uma `MiniSNNConfig` e uma rede pela API pública.
4. O runner define tipos e cria conexões com `minisnn_connect_delayed_ex()`.
5. Em cada passo, aplica entrada, chama `minisnn_step()` e consulta o estado.
6. Se STDP estiver ativo, a Core atualiza pesos após transmitir os spikes atuais.
7. CSVs, resumo, configuração e manifesto são gravados na pasta real da run.
8. `analyze_run.py` pode calcular diagnóstico posterior.
9. `compare_runs.py` pode reunir duas ou mais runs.
10. O Studio abre os mesmos artefatos sem duplicar o motor de simulação.

O leitor não precisa conhecer ponteiros ou Win32 para usar esse fluxo; esses
detalhes ficam encapsulados nos módulos correspondentes.
