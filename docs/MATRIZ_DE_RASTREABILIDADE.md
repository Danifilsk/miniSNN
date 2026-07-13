# Matriz de rastreabilidade

| Feature | Configuração | Implementação | Teste | Saída observável | Documentação | Status |
|---|---|---|---|---|---|---|
| LIF | `dt`, `tau`, `v_*`, `resistance` | `src/neuron.c`, `src/network.c` | `test_LIF`, `test_topology` | tensão e spike | [Auditoria](AUDITORIA_DO_CORE_V02.md) | Implementado; casos discretos numericamente validados |
| API pública | `MiniSNNConfig` | `include/minisnn.h`, `src/minisnn.c` | `test_minisnn_api` | getters e retorno das funções | [API Reference](../API_REFERENCE.md) | Implementado |
| Topologias | `topology` e opções | `app/scenario_runner.c` | `test_runner_topologies`, golden | assinatura, pares, contagem e CSVs | [Auditoria](AUDITORIA_DO_CORE_V02.md) | Caminho do runner validado estruturalmente |
| Self-connections | `allow_self_connections` | funções `_ex` e runner | `test_minisnn_api`, `test_scenario_runner` | contagem de conexões | [Compatibilidade](COMPATIBILIDADE.md) | Implementado |
| INH -> INH | `allow_inh_to_inh` | `app/scenario_runner.c` | parser/runner e experimentos | métricas EXC/INH | [Cenários](GUIA_DE_CENARIOS.md) | Implementado; efeito científico experimental |
| Cenários | arquivos INI | `scenario_config`, `scenario_runner` | `test_scenario_config`, `test_scenario_runner` | pasta da run | [Cenários](GUIA_DE_CENARIOS.md) | Implementado |
| Studio | controles Win32 | `app/minisnn_studio.c` | compilação e smoke test manual | interface e status | [Studio](GUIA_DO_STUDIO.md) | Implementado no Windows |
| Neurônio detalhado | `record_neuron` | runner e `plot_neuron.py` | `test_plot_neuron`, runner | `neuron_<id>.csv/png` | [Studio](GUIA_DO_STUDIO.md) | Implementado |
| Comparação | pastas de runs | `compare_runs.py` | `test_compare_runs` | CSV, relatório e PNGs | [Métricas](GUIA_DE_METRICAS.md) | Implementado |
| Histórico | `[output]` | runner e comparador | testes de runner/comparação | `index.csv` | [Resultados](ORGANIZACAO_DE_RESULTADOS.md) | Implementado |
| Diagnóstico | `[diagnostics]` | runner, `analyze_run.py`, `metrics_common.py` | `test_metrics_common`, `test_analyze_run` | métricas, relatório e PNGs | [Diagnóstico](GUIA_DE_DIAGNOSTICO.md) | Fórmulas validadas; classificação heurística |
| STDP C1 | `[plasticity]` | `src/plasticity.c`, API e runner | `test_plasticity`, parser, runner, reprodutibilidade e long | pesos, histórico, métricas, relatório e PNG | [Plasticidade](GUIA_DE_PLASTICIDADE.md) | Implementado para origens EXC; experimental |
| Reprodutibilidade | `seed` | `scenario_runner` | `test_reproducibility`, golden | assinatura, spikes e raster | [Auditoria](AUDITORIA_DO_CORE_V02.md) | Validado no mesmo binário |
| Robustez | limites e falhas | parser, runner, scripts | memory stress, scripts e long run | erros controlados | [Cobertura](COBERTURA_DE_TESTES.md) | Automatizado; sanitizer indisponível |
| Documentação | estrutura em `docs/` | Markdown e `check_docs.py` | `test_docs` | relatório do validador | [Índice](INDICE_DA_DOCUMENTACAO.md) | Implementado |

Esta matriz não substitui revisão científica ou validação biológica. A validação
manual do Studio permanece separada.
