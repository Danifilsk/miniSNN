CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -fanalyzer
INCLUDES = -Iinclude -Isrc -Iapp
BUILD_DIR = build

STRUCTURE_SOURCES = src/structure.c src/structural_plasticity.c
API_SOURCES = src/minisnn.c src/neuron.c src/neuron_model.c src/network.c src/plasticity.c src/reward.c src/homeostasis.c $(STRUCTURE_SOURCES)
APP_SOURCES = app/scenario_config.c
SCENARIO_RUNNER_SOURCES = app/scenario_config.c app/scenario_runtime.c app/scenario_runner.c app/working_memory.c
EVOLUTION_SOURCES = src/evolution.c
EVOLUTION_RUNNER_SOURCES = app/evolution_config.c app/evolution_runner.c $(SCENARIO_RUNNER_SOURCES) $(EVOLUTION_SOURCES)
CORE_SOURCES = src/neuron.c src/neuron_model.c src/network.c src/plasticity.c src/reward.c src/homeostasis.c $(STRUCTURE_SOURCES) src/topology.c src/stimulus.c src/recorder.c
EXPERIMENT_SOURCES = src/neuron.c src/neuron_model.c src/network.c src/plasticity.c src/reward.c src/homeostasis.c $(STRUCTURE_SOURCES) src/stimulus.c src/recorder.c
SCENARIO ?= configs/random_balanced.ini
# Prefer the per-user Windows runtime when it exists; callers may still pass
# PYTHON=... explicitly for another interpreter.
ifeq ($(origin PYTHON), undefined)
WINDOWS_LOCALAPPDATA := $(subst \,/,$(LOCALAPPDATA))
WINDOWS_PYTHON_CANDIDATES := $(wildcard $(WINDOWS_LOCALAPPDATA)/Python/*/python.exe)
WINDOWS_PYTHON_CORE_CANDIDATES := $(foreach candidate,$(WINDOWS_PYTHON_CANDIDATES),$(if $(findstring /pythoncore-,$(candidate)),$(candidate)))
ifneq ($(strip $(WINDOWS_PYTHON_CANDIDATES)),)
PYTHON := $(if $(WINDOWS_PYTHON_CORE_CANDIDATES),$(firstword $(WINDOWS_PYTHON_CORE_CANDIDATES)),$(firstword $(WINDOWS_PYTHON_CANDIDATES)))
else
PYTHON := python
endif
endif
ANALYZER_CFLAGS = -std=c11 -Wall -Wextra -pedantic -fanalyzer -Wformat=2 -Wshadow -Wnull-dereference

.PHONY: all help clean test test-api test-core test-lif test-neuron-model test-adex test-hodgkin-huxley test-model-checkpoints test-model-error-propagation test-model-provenance test-adex-long test-hh-long test-models-long test-plasticity test-plasticity-long test-homeostasis test-homeostasis-long test-reward test-reward-long test-evolution test-structure test-structural-plasticity test-structure-resume test-structure-long test-evolution-runner test-evolution-resume test-evolution-long test-plot-evolution test-plot-topology test-evolution-report test-structural-html evolution-build evolution-weight-demo evolution-homeostasis-demo evolution-plasticity-demo evolution-structure-demo structural-pruning-demo structural-growth-demo evolution-structure-learning-demo plot-evolution plot-topology report-evolution report-evolution-history report-structural-events report-best-topology test-scenario test-runner test-runner-topologies test-working-memory test-reproducibility test-regression test-memory test-long test-analyzer test-sanitize benchmark-v02 benchmark-c1 benchmark-c15 benchmark-c2 benchmark-c3 benchmark-c4 benchmark-c5 check-v02 check-c1 check-c15 check-c2 check-c3 check-c4 check-c5-interface check-c5 check-c6 compare-neuron-models test-compare-neuron-models \
	test-plot-neuron test-plot-plasticity test-compare-runs test-diagnostics test-run-reports test-history-report test-docs \
	test-plot-homeostasis plot-homeostasis test-plot-reward plot-reward \
	api-examples api-single api-chain api-exc-inh \
	demo ei-balance inhibition-fine inh-to-inh sparse-ei scenario \
	scenario-random scenario-small-world scenario-working-memory scenario-feedforward \
	scenario-stdp-ltp scenario-stdp-ltd scenario-stdp-mixed plot-stdp-ltp \
	scenario-homeostasis-silence scenario-homeostasis-explosion scenario-homeostasis-stdp \
	scenario-reward-positive scenario-punishment-negative scenario-reward-delayed scenario-reward-mixed \
	report-metrics report-weights report-all report-history \
	studio-build studio

all: test

help:
	@echo Comandos disponiveis:
	@echo   make ou make test      - compila e executa todos os testes
	@echo   make test-api          - teste da API publica
	@echo   make test-core         - teste do nucleo/topologias/estimulos/recorders
	@echo   make test-lif          - validacao numerica do LIF discreto
	@echo   make test-neuron-model - validacao da interface interna de modelos neuronais
	@echo   make test-adex         - validacao numerica do AdEx
	@echo   make test-hodgkin-huxley - validacao numerica do Hodgkin-Huxley
	@echo   make test-models-long  - execucao prolongada dos modelos C5
	@echo   make check-c5          - fechamento automatico dos modelos C5
	@echo   make benchmark-c5      - benchmark comparativo local dos modelos C5
	@echo   make test-plasticity   - validacao numerica do STDP por traces
	@echo   make test-plasticity-long - 10000 passos com STDP e verificacao de limites
	@echo   make test-homeostasis   - validacao numerica da homeostase
	@echo   make test-homeostasis-long - 20000 passos com todos os mecanismos
	@echo   make test-reward       - validacao numerica do R-STDP
	@echo   make test-reward-long  - execucao prolongada com reward e homeostase
	@echo   make test-evolution    - validacao numerica do motor neuroevolutivo
	@echo   make test-structure    - validacao da evolucao estrutural e patches atomicos
	@echo   make test-structural-plasticity - validacao de poda e crescimento durante a vida
	@echo   make test-structure-resume - retomada exata do genoma estrutural
	@echo   make test-structure-long - execucao C4 prolongada com resume
	@echo   make test-evolution-runner - blueprint, avaliacao, outputs e heranca darwiniana
	@echo   make test-evolution-resume - retomada exata a partir de checkpoint
	@echo   make test-evolution-long - execucao evolutiva prolongada separada
	@echo   make evolution-build   - compila o runner de neuroevolucao
	@echo   make evolution-weight-demo - evolui pesos para uma contagem-alvo
	@echo   make evolution-homeostasis-demo - evolui parametros homeostaticos
	@echo   make evolution-plasticity-demo - evolui parametros com STDP durante a vida
	@echo   make evolution-structure-demo - evolui arestas e delays para uma contagem-alvo
	@echo   make structural-pruning-demo - executa o demonstrador de poda estrutural
	@echo   make structural-growth-demo - executa o demonstrador de crescimento estrutural
	@echo   make evolution-structure-learning-demo - combina evolucao, STDP e crescimento
	@echo   make test-plot-evolution - valida o panorama PNG evolutivo
	@echo   make test-plot-topology - valida o panorama PNG da topologia adaptativa
	@echo   make test-evolution-report - valida o relatorio HTML evolutivo
	@echo   make test-structural-html - valida relatorios HTML de eventos e topologia estrutural
	@echo   make plot-evolution RUN=results/evolution/experimento - gera panorama evolutivo
	@echo   make plot-topology RUN=results/scenarios/execucao - gera panorama estrutural
	@echo   make report-evolution RUN=results/evolution/experimento - gera relatorio HTML
	@echo   make report-evolution-history - gera historico HTML evolutivo
	@echo   make report-structural-events EXPERIMENT=results/evolution/experimento - gera eventos estruturais HTML
	@echo   make report-best-topology EXPERIMENT=results/evolution/experimento - gera topologia herdavel HTML
	@echo   make test-scenario     - teste do parser de cenarios
	@echo   make test-runner       - teste do executor compartilhado de cenarios
	@echo   make test-working-memory - validacao do protocolo temporal C6.1
	@echo   make test-runner-topologies - validacao estrutural das topologias do runner
	@echo   make test-reproducibility - determinismo de topologia e CSVs por seed
	@echo   make test-regression    - golden pequeno e resultados historicos conhecidos
	@echo   make test-memory       - estresse finito de ciclo de vida, conexoes e delays
	@echo   make test-long         - 50000 passos com verificacao de estado finito
	@echo   make test-analyzer     - recompila a suite com fanalyzer e warnings adicionais
	@echo   make test-sanitize     - executa ASan/UBSan ou informa suporte ausente
	@echo   make benchmark-v02     - benchmark local controlado do Core v0.2
	@echo   make benchmark-c1      - mede STDP off, on e custo do historico
	@echo   make benchmark-c15     - compara cinco modos de homeostase
	@echo   make benchmark-c2      - compara custos de R-STDP e reward
	@echo   make benchmark-c3      - mede custos locais da neuroevolucao
	@echo   make benchmark-c4      - mede custos locais da topologia adaptativa
	@echo   make check-v02         - verifica prontidao automatica sem alterar Git
	@echo   make check-c1          - verifica o fechamento automatico do Bloco C1
	@echo   make check-c15         - verifica o fechamento automatico do Bloco C1.5
	@echo   make check-c2          - verifica o fechamento automatico do Bloco C2
	@echo   make check-c3          - verifica o fechamento automatico do Bloco C3
	@echo   make check-c4          - verifica o fechamento automatico do Bloco C4
	@echo   make check-c6          - verifica memoria de trabalho temporal C6.1
	@echo   make test-plot-neuron  - teste Python do grafico de neuronio
	@echo   make test-plot-plasticity - teste Python do grafico STDP
	@echo   make test-plot-homeostasis - teste Python do panorama homeostatico
	@echo   make plot-homeostasis RUN=results/scenarios/run - gera panorama homeostatico
	@echo   make test-plot-reward - teste Python do panorama de recompensa
	@echo   make plot-reward RUN=results/scenarios/run - gera panorama de recompensa
	@echo   make test-compare-runs - teste Python da comparacao de execucoes
	@echo   make test-diagnostics  - teste Python dos diagnosticos basic/full
	@echo   make test-run-reports  - valida relatorios HTML de metricas e pesos
	@echo   make test-history-report - valida geracao, seguranca e compatibilidade do historico HTML
	@echo   make test-docs         - valida links e referencias da documentacao
	@echo   make api-examples      - executa os exemplos publicos da API
	@echo   make demo              - executa o demo interno
	@echo   make scenario          - executa um cenario .ini com SCENARIO=configs/arquivo.ini
	@echo   make scenario-random   - executa configs/random.ini
	@echo   make scenario-small-world - executa configs/small_world.ini
	@echo   make scenario-working-memory - executa configs/working_memory_demo.ini
	@echo   make scenario-adex-regular - executa o demo AdEx regular
	@echo   make scenario-adex-adapting - executa o demo AdEx com adaptacao
	@echo   make scenario-hh-action-potential - executa o demo HH de potencial de acao
	@echo   make scenario-hh-repetitive - executa o demo HH repetitivo
	@echo   make compare-neuron-models - compara LIF, AdEx e Hodgkin-Huxley
	@echo   make test-compare-neuron-models - valida o comparador de modelos
	@echo   make scenario-feedforward - executa configs/feedforward.ini
	@echo   make scenario-stdp-ltp  - executa o demonstrador STDP de LTP
	@echo   make scenario-stdp-ltd  - executa o demonstrador STDP de LTD
	@echo   make scenario-stdp-mixed - executa o demonstrador STDP misto
	@echo   make scenario-homeostasis-silence - recuperacao de baixa atividade
	@echo   make scenario-homeostasis-explosion - controle de atividade excessiva
	@echo   make scenario-homeostasis-stdp - STDP combinado com scaling
	@echo   make scenario-reward-positive - R-STDP com recompensa positiva
	@echo   make scenario-punishment-negative - R-STDP com punicao
	@echo   make scenario-reward-delayed - R-STDP com reward atrasado
	@echo   make scenario-reward-mixed - R-STDP com sinais mistos
	@echo   make plot-stdp-ltp     - gera o panorama STDP do demonstrador LTP
	@echo   make report-metrics RUN=results/scenarios/run - gera metrics_report.html
	@echo   make report-weights RUN=results/scenarios/run - gera weights_report.html
	@echo   make report-all RUN=results/scenarios/run - gera os dois relatorios HTML
	@echo   make report-history    - gera o historico HTML local das execucoes
	@echo   make studio-build      - compila a interface grafica miniSNN Studio
	@echo   make studio            - compila e abre a interface grafica miniSNN Studio
	@echo   make ei-balance        - executa o experimento EXC vs EXC/INH
	@echo   make inhibition-fine   - executa a varredura fina de inibicao
	@echo   make inh-to-inh        - executa a visualizacao INH para INH
	@echo   make sparse-ei         - executa a analise EXC/INH esparsa
	@echo   make clean             - remove arquivos recriaveis de build e resultados locais

$(BUILD_DIR):
	@if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)

$(BUILD_DIR)/test_minisnn_api.exe: tests/test_minisnn_api.c $(API_SOURCES) include/minisnn.h include/minisnn_types.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_minisnn_api.c $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_topology.exe: tests/test_topology.c $(CORE_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_topology.c $(CORE_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_LIF.exe: tests/test_LIF.c src/neuron.c src/neuron.h src/config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_LIF.c src/neuron.c $(INCLUDES) -o $@

$(BUILD_DIR)/test_neuron_model.exe: tests/test_neuron_model.c $(API_SOURCES) include/minisnn_types.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_neuron_model.c $(API_SOURCES) $(INCLUDES) -o $@

test-neuron-model: $(BUILD_DIR)/test_neuron_model.exe
	$(BUILD_DIR)/test_neuron_model.exe

$(BUILD_DIR)/test_adex.exe: tests/test_adex.c src/neuron.c src/neuron_model.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_adex.c src/neuron.c src/neuron_model.c $(INCLUDES) -o $@

test-adex: $(BUILD_DIR)/test_adex.exe
	$(BUILD_DIR)/test_adex.exe

$(BUILD_DIR)/test_hodgkin_huxley.exe: tests/test_hodgkin_huxley.c src/neuron.c src/neuron_model.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_hodgkin_huxley.c src/neuron.c src/neuron_model.c $(INCLUDES) -o $@

test-hodgkin-huxley: $(BUILD_DIR)/test_hodgkin_huxley.exe
	$(BUILD_DIR)/test_hodgkin_huxley.exe

$(BUILD_DIR)/test_model_error_propagation.exe: tests/test_model_error_propagation.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DMINISNN_TESTING tests/test_model_error_propagation.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) $(INCLUDES) -o $@

test-model-error-propagation: $(BUILD_DIR)/test_model_error_propagation.exe
	$(BUILD_DIR)/test_model_error_propagation.exe

$(BUILD_DIR)/test_model_provenance.exe: tests/test_model_provenance.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_model_provenance.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) $(INCLUDES) -o $@

test-model-provenance: $(BUILD_DIR)/test_model_provenance.exe
	$(BUILD_DIR)/test_model_provenance.exe

$(BUILD_DIR)/test_models_long.exe: tests/test_models_long.c $(API_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_models_long.c $(API_SOURCES) $(INCLUDES) -o $@

test-adex-long: $(BUILD_DIR)/test_models_long.exe
	$(BUILD_DIR)/test_models_long.exe

test-hh-long: $(BUILD_DIR)/test_models_long.exe
	$(BUILD_DIR)/test_models_long.exe

test-models-long: $(BUILD_DIR)/test_models_long.exe
	$(BUILD_DIR)/test_models_long.exe

check-c5-interface: test-neuron-model
	$(PYTHON) scripts/check_c5_interface.py
	@echo C5 neuron model interface validation OK

check-c5: check-c5-interface test-model-checkpoints test-adex test-hodgkin-huxley test-model-error-propagation test-model-provenance test-models-long test-compare-neuron-models
	@echo C5 advanced neuron models validation OK

check-c6: test-working-memory
	$(PYTHON) scripts/check_c6.py

$(BUILD_DIR)/test_plasticity.exe: tests/test_plasticity.c $(API_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_plasticity.c $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_plasticity_long.exe: tests/test_plasticity_long.c $(API_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_plasticity_long.c $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_homeostasis.exe: tests/test_homeostasis.c $(API_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_homeostasis.c $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_homeostasis_runner.exe: tests/test_homeostasis_runner.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_homeostasis_runner.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_homeostasis_long.exe: tests/test_homeostasis_long.c $(API_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_homeostasis_long.c $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_reward.exe: tests/test_reward.c $(API_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_reward.c $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_reward_long.exe: tests/test_reward_long.c $(API_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_reward_long.c $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_evolution.exe: tests/test_evolution.c $(EVOLUTION_SOURCES) src/evolution.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_evolution.c $(EVOLUTION_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_structure.exe: tests/test_structure.c $(API_SOURCES) $(EVOLUTION_SOURCES) src/structure.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_structure.c $(API_SOURCES) $(EVOLUTION_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_structural_plasticity.exe: tests/test_structural_plasticity.c $(API_SOURCES) $(EVOLUTION_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_structural_plasticity.c $(API_SOURCES) $(EVOLUTION_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_structure_resume.exe: tests/test_structure_resume.c $(EVOLUTION_RUNNER_SOURCES) $(API_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DEVOLUTION_RUNNER_NO_MAIN tests/test_structure_resume.c $(EVOLUTION_RUNNER_SOURCES) $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_structure_long.exe: tests/test_structure_long.c $(EVOLUTION_RUNNER_SOURCES) $(API_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DEVOLUTION_RUNNER_NO_MAIN tests/test_structure_long.c $(EVOLUTION_RUNNER_SOURCES) $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_evolution_runner.exe: tests/test_evolution_runner.c $(EVOLUTION_RUNNER_SOURCES) $(API_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DEVOLUTION_RUNNER_NO_MAIN tests/test_evolution_runner.c $(EVOLUTION_RUNNER_SOURCES) $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_evolution_long.exe: tests/test_evolution_long.c $(EVOLUTION_RUNNER_SOURCES) $(API_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DEVOLUTION_RUNNER_NO_MAIN tests/test_evolution_long.c $(EVOLUTION_RUNNER_SOURCES) $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/evolution_runner.exe: $(EVOLUTION_RUNNER_SOURCES) $(API_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(EVOLUTION_RUNNER_SOURCES) $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_scenario_config.exe: tests/test_scenario_config.c $(APP_SOURCES) src/neuron_model.c src/neuron.c app/scenario_config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_scenario_config.c $(APP_SOURCES) src/neuron_model.c src/neuron.c $(INCLUDES) -o $@

$(BUILD_DIR)/test_scenario_runner.exe: tests/test_scenario_runner.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) app/scenario_runner.h app/scenario_config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_scenario_runner.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_working_memory.exe: tests/test_working_memory.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) app/working_memory.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DMINISNN_TESTING tests/test_working_memory.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_runner_topologies.exe: tests/test_runner_topologies.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) app/scenario_runner.h app/scenario_config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_runner_topologies.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_reproducibility.exe: tests/test_reproducibility.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) app/scenario_runner.h app/scenario_config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_reproducibility.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_memory_stress.exe: tests/test_memory_stress.c $(API_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_memory_stress.c $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_long_run.exe: tests/test_long_run.c $(API_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_long_run.c $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/benchmark_core.exe: tests/benchmark_core.c $(API_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/benchmark_core.c $(API_SOURCES) $(INCLUDES) -o $@

test-api: $(BUILD_DIR)/test_minisnn_api.exe
	$(BUILD_DIR)/test_minisnn_api.exe

test-core: $(BUILD_DIR)/test_topology.exe
	$(BUILD_DIR)/test_topology.exe

test-lif: $(BUILD_DIR)/test_LIF.exe
	$(BUILD_DIR)/test_LIF.exe
	@if exist lif.csv del /Q lif.csv

test-plasticity: $(BUILD_DIR)/test_plasticity.exe
	$(BUILD_DIR)/test_plasticity.exe

test-plasticity-long: $(BUILD_DIR)/test_plasticity_long.exe
	$(BUILD_DIR)/test_plasticity_long.exe

test-homeostasis: $(BUILD_DIR)/test_homeostasis.exe $(BUILD_DIR)/test_homeostasis_runner.exe
	$(BUILD_DIR)/test_homeostasis.exe
	$(BUILD_DIR)/test_homeostasis_runner.exe

test-homeostasis-long: $(BUILD_DIR)/test_homeostasis_long.exe
	$(BUILD_DIR)/test_homeostasis_long.exe

test-reward: $(BUILD_DIR)/test_reward.exe
	$(BUILD_DIR)/test_reward.exe

test-reward-long: $(BUILD_DIR)/test_reward_long.exe
	$(BUILD_DIR)/test_reward_long.exe

test-evolution: $(BUILD_DIR)/test_evolution.exe
	$(BUILD_DIR)/test_evolution.exe

test-structure: $(BUILD_DIR)/test_structure.exe
	$(BUILD_DIR)/test_structure.exe

test-structural-plasticity: $(BUILD_DIR)/test_structural_plasticity.exe
	$(BUILD_DIR)/test_structural_plasticity.exe

test-structure-resume: $(BUILD_DIR)/test_structure_resume.exe
	$(BUILD_DIR)/test_structure_resume.exe

test-structure-long: $(BUILD_DIR)/test_structure_long.exe
	$(BUILD_DIR)/test_structure_long.exe

test-evolution-runner: $(BUILD_DIR)/test_evolution_runner.exe
	$(BUILD_DIR)/test_evolution_runner.exe

test-evolution-resume: test-evolution-runner

test-evolution-long: $(BUILD_DIR)/test_evolution_long.exe
	$(BUILD_DIR)/test_evolution_long.exe

evolution-build: $(BUILD_DIR)/evolution_runner.exe

evolution-weight-demo: $(BUILD_DIR)/evolution_runner.exe
	$(BUILD_DIR)/evolution_runner.exe configs/evolution_weight_target_demo.ini

evolution-homeostasis-demo: $(BUILD_DIR)/evolution_runner.exe
	$(BUILD_DIR)/evolution_runner.exe configs/evolution_homeostasis_demo.ini

evolution-plasticity-demo: $(BUILD_DIR)/evolution_runner.exe
	$(BUILD_DIR)/evolution_runner.exe configs/evolution_plasticity_demo.ini

evolution-structure-demo: $(BUILD_DIR)/evolution_runner.exe
	$(BUILD_DIR)/evolution_runner.exe configs/evolution_structure_target_demo.ini
	$(PYTHON) scripts/plot_evolution.py results/evolution/evolution_structure_target_demo
	$(PYTHON) scripts/generate_evolution_report.py results/evolution/evolution_structure_target_demo

structural-pruning-demo: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/structural_pruning_demo.ini
	$(PYTHON) scripts/plot_topology.py results/scenarios/structural_pruning_demo

structural-growth-demo: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/structural_growth_demo.ini
	$(PYTHON) scripts/plot_topology.py results/scenarios/structural_growth_demo

evolution-structure-learning-demo: $(BUILD_DIR)/evolution_runner.exe
	$(BUILD_DIR)/evolution_runner.exe configs/evolution_structure_learning_demo.ini
	$(PYTHON) scripts/plot_evolution.py results/evolution/evolution_structure_learning_demo
	$(PYTHON) scripts/generate_evolution_report.py results/evolution/evolution_structure_learning_demo

test-plot-evolution: | $(BUILD_DIR)
	$(PYTHON) tests/test_plot_evolution.py

test-plot-topology: | $(BUILD_DIR)
	$(PYTHON) tests/test_plot_topology.py

test-evolution-report: | $(BUILD_DIR)
	$(PYTHON) tests/test_evolution_report.py

test-structural-html: | $(BUILD_DIR)
	$(PYTHON) tests/test_structural_html.py

plot-evolution:
	@if "$(RUN)"=="" (echo Erro: informe RUN=results/evolution/nome_do_experimento & exit /B 1)
	$(PYTHON) scripts/plot_evolution.py "$(RUN)"

plot-topology:
	@if "$(RUN)"=="" (echo Erro: informe RUN=results/scenarios/nome_da_execucao & exit /B 1)
	$(PYTHON) scripts/plot_topology.py "$(RUN)"

report-evolution:
	@if "$(RUN)"=="" (echo Erro: informe RUN=results/evolution/nome_do_experimento & exit /B 1)
	$(PYTHON) scripts/generate_evolution_report.py "$(RUN)"

report-evolution-history:
	$(PYTHON) scripts/generate_evolution_report.py results/evolution --history

report-structural-events:
	@if "$(EXPERIMENT)"=="" (echo Erro: informe EXPERIMENT=results/evolution/nome_do_experimento & exit /B 1)
	$(PYTHON) scripts/generate_evolution_report.py "$(EXPERIMENT)" --structural-events

report-best-topology:
	@if "$(EXPERIMENT)"=="" (echo Erro: informe EXPERIMENT=results/evolution/nome_do_experimento & exit /B 1)
	$(PYTHON) scripts/generate_evolution_report.py "$(EXPERIMENT)" --best-topology

test-scenario: $(BUILD_DIR)/test_scenario_config.exe
	$(BUILD_DIR)/test_scenario_config.exe

test-runner: $(BUILD_DIR)/test_scenario_runner.exe
	$(BUILD_DIR)/test_scenario_runner.exe

test-working-memory: $(BUILD_DIR)/test_working_memory.exe
	$(BUILD_DIR)/test_working_memory.exe

test-runner-topologies: $(BUILD_DIR)/test_runner_topologies.exe
	$(BUILD_DIR)/test_runner_topologies.exe

test-reproducibility: $(BUILD_DIR)/test_reproducibility.exe
	$(BUILD_DIR)/test_reproducibility.exe

test-regression: $(BUILD_DIR)/minisnn_runner.exe | $(BUILD_DIR)
	$(PYTHON) tests/test_regression_baseline.py

test-memory: $(BUILD_DIR)/test_memory_stress.exe
	$(BUILD_DIR)/test_memory_stress.exe

test-long: $(BUILD_DIR)/test_long_run.exe
	$(BUILD_DIR)/test_long_run.exe

test-analyzer:
	$(MAKE) -B test CFLAGS="$(ANALYZER_CFLAGS)"

test-sanitize: | $(BUILD_DIR)
	$(PYTHON) scripts/run_sanitizers.py

benchmark-v02: $(BUILD_DIR)/benchmark_core.exe $(BUILD_DIR)/minisnn_runner.exe | $(BUILD_DIR)
	$(PYTHON) scripts/run_benchmarks.py

benchmark-c1: $(BUILD_DIR)/minisnn_runner.exe | $(BUILD_DIR)
	$(PYTHON) scripts/run_benchmarks_c1.py

benchmark-c15: $(BUILD_DIR)/minisnn_runner.exe | $(BUILD_DIR)
	$(PYTHON) scripts/run_benchmarks_c15.py

benchmark-c2: $(BUILD_DIR)/minisnn_runner.exe | $(BUILD_DIR)
	$(PYTHON) scripts/run_benchmarks_c2.py

benchmark-c3: $(BUILD_DIR)/minisnn_runner.exe $(BUILD_DIR)/evolution_runner.exe | $(BUILD_DIR)
	$(PYTHON) scripts/run_benchmarks_c3.py

benchmark-c4: $(BUILD_DIR)/minisnn_runner.exe $(BUILD_DIR)/evolution_runner.exe | $(BUILD_DIR)
	$(PYTHON) scripts/run_benchmarks_c4.py

check-c3: $(BUILD_DIR)/minisnn_runner.exe $(BUILD_DIR)/evolution_runner.exe | $(BUILD_DIR)
	$(PYTHON) scripts/check_c3.py

check-c4: $(BUILD_DIR)/minisnn_runner.exe $(BUILD_DIR)/evolution_runner.exe | $(BUILD_DIR)
	$(PYTHON) scripts/check_c4.py

check-v02: | $(BUILD_DIR)
	$(PYTHON) scripts/check_release_v02.py

check-c1: $(BUILD_DIR)/minisnn_runner.exe | $(BUILD_DIR)
	$(PYTHON) scripts/check_c1.py

check-c15: $(BUILD_DIR)/minisnn_runner.exe | $(BUILD_DIR)
	$(PYTHON) scripts/check_c15.py

check-c2: $(BUILD_DIR)/minisnn_runner.exe | $(BUILD_DIR)
	$(PYTHON) scripts/check_c2.py

test-plot-neuron: | $(BUILD_DIR)
	$(PYTHON) tests/test_plot_neuron.py

test-plot-plasticity: | $(BUILD_DIR)
	$(PYTHON) tests/test_plot_plasticity.py

test-plot-homeostasis: | $(BUILD_DIR)
	$(PYTHON) tests/test_plot_homeostasis.py

test-plot-reward: | $(BUILD_DIR)
	$(PYTHON) tests/test_plot_reward.py

plot-homeostasis:
	@if "$(RUN)"=="" (echo Erro: informe RUN=results/scenarios/nome_da_execucao & exit /B 1)
	$(PYTHON) scripts/plot_homeostasis.py "$(RUN)"

plot-reward:
	@if "$(RUN)"=="" (echo Erro: informe RUN=results/scenarios/nome_da_execucao & exit /B 1)
	$(PYTHON) scripts/plot_reward.py "$(RUN)"

test-compare-runs: | $(BUILD_DIR)
	$(PYTHON) tests/test_compare_runs.py

test-diagnostics: | $(BUILD_DIR)
	$(PYTHON) tests/test_metrics_common.py
	$(PYTHON) tests/test_analyze_run.py
	$(PYTHON) tests/test_script_robustness.py

test-run-reports: | $(BUILD_DIR)
	$(PYTHON) tests/test_run_reports.py

test-history-report: | $(BUILD_DIR)
	$(PYTHON) tests/test_history_report.py

test-docs: | $(BUILD_DIR)
	$(PYTHON) tests/test_docs.py

test: test-api test-core test-lif test-neuron-model test-adex test-hodgkin-huxley test-model-error-propagation test-plasticity test-homeostasis test-reward test-structure test-structural-plasticity test-scenario test-runner test-runner-topologies test-working-memory test-reproducibility test-memory

$(BUILD_DIR)/example_api_single_neuron.exe: examples/api/example_api_single_neuron.c $(API_SOURCES) include/minisnn.h include/minisnn_types.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) examples/api/example_api_single_neuron.c $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/example_api_chain.exe: examples/api/example_api_chain.c $(API_SOURCES) include/minisnn.h include/minisnn_types.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) examples/api/example_api_chain.c $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/example_api_exc_inh.exe: examples/api/example_api_exc_inh.c $(API_SOURCES) include/minisnn.h include/minisnn_types.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) examples/api/example_api_exc_inh.c $(API_SOURCES) $(INCLUDES) -o $@

api-single: $(BUILD_DIR)/example_api_single_neuron.exe
	$(BUILD_DIR)/example_api_single_neuron.exe

api-chain: $(BUILD_DIR)/example_api_chain.exe
	$(BUILD_DIR)/example_api_chain.exe

api-exc-inh: $(BUILD_DIR)/example_api_exc_inh.exe
	$(BUILD_DIR)/example_api_exc_inh.exe

api-examples: api-single api-chain api-exc-inh

$(BUILD_DIR)/demo_main.exe: examples/internal/demo_main.c $(CORE_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) examples/internal/demo_main.c $(CORE_SOURCES) $(INCLUDES) -o $@

demo: $(BUILD_DIR)/demo_main.exe
	$(BUILD_DIR)/demo_main.exe

$(BUILD_DIR)/minisnn_runner.exe: app/minisnn_runner.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) include/minisnn.h include/minisnn_types.h app/scenario_config.h app/scenario_runner.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) app/minisnn_runner.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) $(INCLUDES) -o $@

scenario: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe $(SCENARIO)

scenario-random: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/random.ini

scenario-small-world: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/small_world.ini

scenario-working-memory: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/working_memory_demo.ini

scenario-adex-regular: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/adex_regular_spiking_demo.ini

scenario-adex-adapting: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/adex_adapting_spiking_demo.ini

scenario-hh-action-potential: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/hh_action_potential_demo.ini

scenario-hh-repetitive: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/hh_repetitive_firing_demo.ini

compare-neuron-models: $(BUILD_DIR)/minisnn_runner.exe
	$(PYTHON) scripts/compare_neuron_models.py

test-compare-neuron-models:
	$(PYTHON) tests/test_compare_neuron_models.py

test-model-checkpoints: test-neuron-model test-model-provenance test-evolution-resume test-structure-resume
	@echo C5 model checkpoint validation OK

benchmark-c5: $(BUILD_DIR)/minisnn_runner.exe
	$(PYTHON) scripts/run_benchmarks_c5.py

scenario-feedforward: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/feedforward.ini

scenario-stdp-ltp: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/stdp_ltp_demo.ini

scenario-stdp-ltd: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/stdp_ltd_demo.ini

scenario-stdp-mixed: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/stdp_mixed_demo.ini

scenario-homeostasis-silence: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/homeostasis_silence_recovery_demo.ini
	$(PYTHON) scripts/plot_homeostasis.py results/scenarios/homeostasis_silence_recovery_demo

scenario-homeostasis-explosion: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/homeostasis_explosion_control_demo.ini
	$(PYTHON) scripts/plot_homeostasis.py results/scenarios/homeostasis_explosion_control_demo

scenario-homeostasis-stdp: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/homeostasis_stdp_scaling_demo.ini
	$(PYTHON) scripts/plot_homeostasis.py results/scenarios/homeostasis_stdp_scaling_demo

scenario-reward-positive: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/reward_positive_demo.ini
	$(PYTHON) scripts/generate_run_reports.py results/scenarios/reward_positive_demo --reward
	$(PYTHON) scripts/plot_reward.py results/scenarios/reward_positive_demo

scenario-punishment-negative: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/punishment_negative_demo.ini
	$(PYTHON) scripts/generate_run_reports.py results/scenarios/punishment_negative_demo --reward
	$(PYTHON) scripts/plot_reward.py results/scenarios/punishment_negative_demo

scenario-reward-delayed: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/reward_delayed_demo.ini
	$(PYTHON) scripts/generate_run_reports.py results/scenarios/reward_delayed_demo --reward
	$(PYTHON) scripts/plot_reward.py results/scenarios/reward_delayed_demo

scenario-reward-mixed: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/reward_mixed_demo.ini
	$(PYTHON) scripts/generate_run_reports.py results/scenarios/reward_mixed_demo --reward
	$(PYTHON) scripts/plot_reward.py results/scenarios/reward_mixed_demo

plot-stdp-ltp: scenario-stdp-ltp
	$(PYTHON) scripts/plot_plasticity.py results/scenarios/stdp_ltp_demo

report-metrics:
	@if "$(RUN)"=="" (echo Erro: informe RUN=results/scenarios/nome_da_execucao & exit /B 1)
	$(PYTHON) scripts/generate_run_reports.py "$(RUN)" --metrics

report-weights:
	@if "$(RUN)"=="" (echo Erro: informe RUN=results/scenarios/nome_da_execucao & exit /B 1)
	$(PYTHON) scripts/generate_run_reports.py "$(RUN)" --weights

report-all:
	@if "$(RUN)"=="" (echo Erro: informe RUN=results/scenarios/nome_da_execucao & exit /B 1)
	$(PYTHON) scripts/generate_run_reports.py "$(RUN)" --all

report-history:
	$(PYTHON) scripts/generate_history_report.py results/scenarios

$(BUILD_DIR)/minisnn_studio.exe: app/minisnn_studio.c app/evolution_config.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) include/minisnn.h include/minisnn_types.h app/scenario_config.h app/scenario_runner.h app/evolution_config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) app/minisnn_studio.c app/evolution_config.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) $(INCLUDES) -o $@ -mwindows -lcomdlg32 -lshell32 -lgdi32 -luser32 -lole32

studio-build: $(BUILD_DIR)/minisnn_studio.exe

studio: $(BUILD_DIR)/minisnn_studio.exe
	$(BUILD_DIR)/minisnn_studio.exe

$(BUILD_DIR)/example_ei_balance.exe: experiments/example_ei_balance.c $(EXPERIMENT_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) experiments/example_ei_balance.c $(EXPERIMENT_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/example_inhibition_fine_sweep.exe: experiments/example_inhibition_fine_sweep.c $(EXPERIMENT_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) experiments/example_inhibition_fine_sweep.c $(EXPERIMENT_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/example_inh_to_inh_visual.exe: experiments/example_inh_to_inh_visual.c $(EXPERIMENT_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) experiments/example_inh_to_inh_visual.c $(EXPERIMENT_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/example_sparse_ei_balance_analysis.exe: experiments/example_sparse_ei_balance_analysis.c $(EXPERIMENT_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) experiments/example_sparse_ei_balance_analysis.c $(EXPERIMENT_SOURCES) $(INCLUDES) -o $@

ei-balance: $(BUILD_DIR)/example_ei_balance.exe
	$(BUILD_DIR)/example_ei_balance.exe

inhibition-fine: $(BUILD_DIR)/example_inhibition_fine_sweep.exe
	$(BUILD_DIR)/example_inhibition_fine_sweep.exe

inh-to-inh: $(BUILD_DIR)/example_inh_to_inh_visual.exe
	$(BUILD_DIR)/example_inh_to_inh_visual.exe

sparse-ei: $(BUILD_DIR)/example_sparse_ei_balance_analysis.exe
	$(BUILD_DIR)/example_sparse_ei_balance_analysis.exe

clean:
	@if exist $(BUILD_DIR)\*.exe del /Q $(BUILD_DIR)\*.exe
	@if exist $(BUILD_DIR)\*.o del /Q $(BUILD_DIR)\*.o
	@if exist $(BUILD_DIR)\*.obj del /Q $(BUILD_DIR)\*.obj
	@if exist $(BUILD_DIR)\*.out del /Q $(BUILD_DIR)\*.out
	@if exist $(BUILD_DIR)\*.dll del /Q $(BUILD_DIR)\*.dll
	@if exist $(BUILD_DIR)\*.pdb del /Q $(BUILD_DIR)\*.pdb
	@if exist $(BUILD_DIR)\*.ilk del /Q $(BUILD_DIR)\*.ilk
	@if exist results\api\*.csv del /Q results\api\*.csv
	@if exist results\api\*.png del /Q results\api\*.png
	@if exist results\internal_demo\*.csv del /Q results\internal_demo\*.csv
	@if exist results\internal_demo\*.png del /Q results\internal_demo\*.png
	@if exist results\scenarios for /D %%D in (results\scenarios\*) do rmdir /S /Q "%%D"
	@if exist results\scenarios\*.csv del /Q results\scenarios\*.csv
	@if exist results\scenarios\*.png del /Q results\scenarios\*.png
	@if exist results\scenarios\history.html del /Q results\scenarios\history.html
	@if exist results\comparisons for /D %%D in (results\comparisons\*) do rmdir /S /Q "%%D"
	@if exist results\comparisons\*.csv del /Q results\comparisons\*.csv
	@if exist results\model_comparison\*.csv del /Q results\model_comparison\*.csv
	@if exist results\model_comparison\*.txt del /Q results\model_comparison\*.txt
	@if exist results\model_comparison\*.html del /Q results\model_comparison\*.html
	@if exist results\model_comparison\*.png del /Q results\model_comparison\*.png
	@if exist results\benchmarks\*.csv del /Q results\benchmarks\*.csv
	@if exist results\benchmarks\*.txt del /Q results\benchmarks\*.txt
