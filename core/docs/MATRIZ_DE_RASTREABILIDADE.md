# Matriz de rastreabilidade

| Feature | Configuração | Implementação | Teste | Saída observável | Documentação | Status |
|---|---|---|---|---|---|---|
| LIF | `dt`, `tau`, `v_*`, `resistance` | `src/neuron.c`, `src/network.c` | `test_LIF`, `test_topology` | tensão e spike | [Auditoria](AUDITORIA_DO_CORE_V02.md) | Implementado; casos discretos numericamente validados |
| Interface de modelo neuronal C5 | `neuron_model` e `[neuron] model` | `src/neuron_model.c`, `src/network.c`, parser e API | `test_neuron_model`, `test_scenario_config` | `config_used.ini`, resumo e manifesto | [Modelos](GUIA_DE_MODELOS_NEURONAIS.md) | LIF preservado; modelos adicionais fora de escopo |
| API pública | `MiniSNNConfig` | `include/minisnn.h`, `src/minisnn.c` | `test_minisnn_api` | getters e retorno das funções | [API Reference](../API_REFERENCE.md) | Implementado |
| Topologias | `topology` e opções | `app/scenario_runner.c` | `test_runner_topologies`, golden | assinatura, pares, contagem e CSVs | [Auditoria](AUDITORIA_DO_CORE_V02.md) | Caminho do runner validado estruturalmente |
| Self-connections | `allow_self_connections` | funções `_ex` e runner | `test_minisnn_api`, `test_scenario_runner` | contagem de conexões | [Compatibilidade](COMPATIBILIDADE.md) | Implementado |
| INH -> INH | `allow_inh_to_inh` | `app/scenario_runner.c` | parser/runner e experimentos | métricas EXC/INH | [Cenários](GUIA_DE_CENARIOS.md) | Implementado; efeito científico experimental |
| Cenários | arquivos INI | `scenario_config`, `scenario_runner` | `test_scenario_config`, `test_scenario_runner` | pasta da run | [Cenários](GUIA_DE_CENARIOS.md) | Implementado |
| Studio | controles Win32 | `app/minisnn_studio.c` | compilação e smoke test manual | interface e status | [Studio](GUIA_DO_STUDIO.md) | Implementado no Windows |
| Neurônio detalhado | `record_neuron` | runner e `plot_neuron.py` | `test_plot_neuron`, runner | `neuron_<id>.csv/png` | [Studio](GUIA_DO_STUDIO.md) | Implementado |
| Comparação | pastas de runs | `compare_runs.py` | `test_compare_runs` | CSV, relatório e PNGs | [Métricas](GUIA_DE_METRICAS.md) | Implementado |
| Histórico | `[output]` | runner, `generate_history_report.py` e Studio | testes de runner, HTML e comparação | `index.csv` append-only e `history.html` | [Resultados](ORGANIZACAO_DE_RESULTADOS.md) | Implementado; HTML é apresentação local |
| Diagnóstico | `[diagnostics]` | runner, `analyze_run.py`, `metrics_common.py` | `test_metrics_common`, `test_analyze_run` | métricas, relatório e PNGs | [Diagnóstico](GUIA_DE_DIAGNOSTICO.md) | Fórmulas validadas; classificação heurística |
| STDP C1 | `[plasticity]` | `src/plasticity.c`, API e runner | `test_plasticity`, parser, runner, reprodutibilidade e long | pesos, histórico, métricas, relatório e PNG | [Plasticidade](GUIA_DE_PLASTICIDADE.md) | Implementado para origens EXC; experimental |
| Relatórios HTML C1.1 | pasta de run | `generate_run_reports.py`, `html_report_common.py` e Studio | `test_run_reports`, diagnóstico, plot STDP e docs | `metrics_report.html`, `weights_report.html` e links para CSV | [Diagnóstico](GUIA_DE_DIAGNOSTICO.md) e [Plasticidade](GUIA_DE_PLASTICIDADE.md) | Apresentação local; não altera métricas ou dinâmica |
| Reprodutibilidade | `seed` | `scenario_runner` | `test_reproducibility`, golden | assinatura, spikes e raster | [Auditoria](AUDITORIA_DO_CORE_V02.md) | Validado no mesmo binário |
| Robustez | limites e falhas | parser, runner, scripts | memory stress, scripts e long run | erros controlados | [Cobertura](COBERTURA_DE_TESTES.md) | Automatizado; sanitizer indisponível |
| Documentação | estrutura em `docs/` | Markdown e `check_docs.py` | `test_docs` | relatório do validador | [Índice](INDICE_DA_DOCUMENTACAO.md) | Implementado |
| Rate trace | `src/homeostasis.c` | fórmula exponencial exata | `test_homeostasis` | `homeostasis_neurons.csv` | [Homeostase](GUIA_DE_HOMEOSTASE.md) | Implementado |
| Threshold adaptativo | `src/homeostasis.c`, `src/network.c` | direção, clamp e ordem | `test_homeostasis` | `threshold_history.csv` | [Homeostase](GUIA_DE_HOMEOSTASE.md) | Implementado |
| Scaling EXC | `src/homeostasis.c` | soma, proporção, zero e STDP | `test_homeostasis` | `homeostasis_metrics.csv` | [Homeostase](GUIA_DE_HOMEOSTASE.md) | Implementado |
| Ganho INH | `src/homeostasis.c`, `src/network.c` | fórmula e transmissão futura | `test_homeostasis` | `homeostasis_history.csv` | [Homeostase](GUIA_DE_HOMEOSTASE.md) | Implementado |
| Fechamento C1.5 | `scripts/check_c15.py` | arquivos, demos, regressão | `check-c15` | saída do checker | [Homeostase](GUIA_DE_HOMEOSTASE.md) | Implementado |
| R-STDP C2 | `[plasticity]`, `[reward]`, `[reward_events]` | `src/reward.c`, plasticidade, rede, API e runner | `test_reward`, parser, runner, reprodutibilidade e long | elegibilidade, eventos, pesos, CSV, TXT, HTML e PNG | [Recompensa](GUIA_DE_RECOMPENSA.md) | Implementado para origens EXC; experimental |
| Fechamento C2 | `scripts/check_c2.py` | arquivos, demos, regressões C1/C1.5 e documentação | `check-c2` | saída do checker | [Recompensa](GUIA_DE_RECOMPENSA.md) | Automatizado; Studio permanece manual |
| Neuroevolução C3 | `[evolution]`, `[genome]`, `[fitness]` | `src/evolution.c`, `app/evolution_config.c`, `app/evolution_runner.c` | `test-evolution`, runner, resume, long, PNG e HTML | CSVs, checkpoint, best run, PNG, HTML e histórico | [Neuroevolução](GUIA_DE_NEUROEVOLUCAO.md) | Implementado com topologia fixa; experimental |
| Fechamento C3 | `scripts/check_c3.py` | motor, runner, demos, regressões e documentação | `check-c3` | saída do checker | [Neuroevolução](GUIA_DE_NEUROEVOLUCAO.md) | Automação prevista; validação manual do Studio separada |

Esta matriz não substitui revisão científica ou validação biológica. A validação
manual do Studio permanece separada.

| Requisito | Config/API | Implementação | Testes | Evidência | Guia | Estado |
|---|---|---|---|---|---|---|
| Genoma estrutural C4 | `genome_mode = structural_connections`, `[structure]` | `src/structure.c`, `app/evolution_runner.c` | `test-structure`, `test-structure-resume`, `test-structure-long` | `structures.csv`, `best_topology.csv`, checkpoint estrutural | [Topologia adaptativa](GUIA_DE_TOPOLOGIA_ADAPTATIVA.md) | Implementado; experimental |
| Plasticidade estrutural | `[structural_plasticity]` | `src/structural_plasticity.c`, `app/scenario_runtime.c` | `test-structural-plasticity`, demos | eventos, métricas, topologias inicial/final | [Topologia adaptativa](GUIA_DE_TOPOLOGIA_ADAPTATIVA.md) | Implementado; não herdável |
| Observabilidade C4 | APIs, runner e scripts | relatórios de cenário/evolução | PNG/HTML/checker C4 | `topology_report.html`, `topology_overview.png` | [Topologia adaptativa](GUIA_DE_TOPOLOGIA_ADAPTATIVA.md) | Automático; Studio manual pendente |
