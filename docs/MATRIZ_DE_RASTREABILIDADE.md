# Matriz de rastreabilidade

| Feature | Configuração | Implementação | Teste | Saída observável | Documentação | Status |
|---|---|---|---|---|---|---|
| LIF | `dt`, `tau`, `v_*`, `resistance` | `src/neuron.c`, `src/network.c` | `test_LIF`, `test_topology` | tensão, spike e CSV | [Arquitetura](ARQUITETURA_DO_CORE.md) | Implementado; validação numérica ampla planejada |
| API pública | `MiniSNNConfig` | `include/minisnn.h`, `src/minisnn.c` | `test_minisnn_api` | getters e retorno das funções | [API Reference](../API_REFERENCE.md) | Implementado |
| Topologias | `topology` e opções | `app/scenario_runner.c` | `test_scenario_runner` | conexão total e CSVs | [Cenários](GUIA_DE_CENARIOS.md) | Implementado |
| Self-connections | `allow_self_connections` | funções `_ex` e runner | `test_minisnn_api`, `test_scenario_runner` | contagem de conexões | [Compatibilidade](COMPATIBILIDADE.md) | Implementado |
| INH -> INH | `allow_inh_to_inh` | `app/scenario_runner.c` | parser/runner e experimentos | métricas EXC/INH | [Cenários](GUIA_DE_CENARIOS.md) | Implementado; efeito científico experimental |
| Cenários | arquivos INI | `scenario_config`, `scenario_runner` | `test_scenario_config`, `test_scenario_runner` | pasta da run | [Cenários](GUIA_DE_CENARIOS.md) | Implementado |
| Studio | controles Win32 | `app/minisnn_studio.c` | compilação e smoke test manual | interface e status | [Studio](GUIA_DO_STUDIO.md) | Implementado no Windows |
| Neurônio detalhado | `record_neuron` | runner e `plot_neuron.py` | `test_plot_neuron`, runner | `neuron_<id>.csv/png` | [Studio](GUIA_DO_STUDIO.md) | Implementado |
| Comparação | pastas de runs | `compare_runs.py` | `test_compare_runs` | CSV, relatório e PNGs | [Métricas](GUIA_DE_METRICAS.md) | Implementado |
| Histórico | `[output]` | runner e comparador | testes de runner/comparação | `index.csv` | [Resultados](ORGANIZACAO_DE_RESULTADOS.md) | Implementado |
| Diagnóstico | `[diagnostics]` | runner, `analyze_run.py`, `metrics_common.py` | `test_analyze_run` | métricas, relatório e PNGs | [Diagnóstico](GUIA_DE_DIAGNOSTICO.md) | Implementado; classificação heurística |
| Documentação | estrutura em `docs/` | Markdown e `check_docs.py` | `test_docs` | relatório do validador | [Índice](INDICE_DA_DOCUMENTACAO.md) | Implementado |

Esta matriz registra evidência existente, não substitui revisão científica. O
Bloco B deverá fortalecer linhas cujo status menciona validação planejada.
