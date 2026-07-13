CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -fanalyzer
INCLUDES = -Iinclude -Isrc -Iapp
BUILD_DIR = build

API_SOURCES = src/minisnn.c src/neuron.c src/network.c src/plasticity.c
APP_SOURCES = app/scenario_config.c
SCENARIO_RUNNER_SOURCES = app/scenario_config.c app/scenario_runner.c
CORE_SOURCES = src/neuron.c src/network.c src/plasticity.c src/topology.c src/stimulus.c src/recorder.c
EXPERIMENT_SOURCES = src/neuron.c src/network.c src/plasticity.c src/stimulus.c src/recorder.c
SCENARIO ?= configs/random_balanced.ini
PYTHON ?= python
ANALYZER_CFLAGS = -std=c11 -Wall -Wextra -pedantic -fanalyzer -Wformat=2 -Wshadow -Wnull-dereference

.PHONY: all help clean test test-api test-core test-lif test-plasticity test-plasticity-long test-scenario test-runner test-runner-topologies test-reproducibility test-regression test-memory test-long test-analyzer test-sanitize benchmark-v02 benchmark-c1 check-v02 check-c1 \
	test-plot-neuron test-plot-plasticity test-compare-runs test-diagnostics test-docs \
	api-examples api-single api-chain api-exc-inh \
	demo ei-balance inhibition-fine inh-to-inh sparse-ei scenario \
	scenario-random scenario-small-world scenario-feedforward \
	scenario-stdp-ltp scenario-stdp-ltd scenario-stdp-mixed plot-stdp-ltp \
	studio-build studio

all: test

help:
	@echo Comandos disponiveis:
	@echo   make ou make test      - compila e executa todos os testes
	@echo   make test-api          - teste da API publica
	@echo   make test-core         - teste do nucleo/topologias/estimulos/recorders
	@echo   make test-lif          - validacao numerica do LIF discreto
	@echo   make test-plasticity   - validacao numerica do STDP por traces
	@echo   make test-plasticity-long - 10000 passos com STDP e verificacao de limites
	@echo   make test-scenario     - teste do parser de cenarios
	@echo   make test-runner       - teste do executor compartilhado de cenarios
	@echo   make test-runner-topologies - validacao estrutural das topologias do runner
	@echo   make test-reproducibility - determinismo de topologia e CSVs por seed
	@echo   make test-regression    - golden pequeno e resultados historicos conhecidos
	@echo   make test-memory       - estresse finito de ciclo de vida, conexoes e delays
	@echo   make test-long         - 50000 passos com verificacao de estado finito
	@echo   make test-analyzer     - recompila a suite com fanalyzer e warnings adicionais
	@echo   make test-sanitize     - executa ASan/UBSan ou informa suporte ausente
	@echo   make benchmark-v02     - benchmark local controlado do Core v0.2
	@echo   make benchmark-c1      - mede STDP off, on e custo do historico
	@echo   make check-v02         - verifica prontidao automatica sem alterar Git
	@echo   make check-c1          - verifica o fechamento automatico do Bloco C1
	@echo   make test-plot-neuron  - teste Python do grafico de neuronio
	@echo   make test-plot-plasticity - teste Python do grafico STDP
	@echo   make test-compare-runs - teste Python da comparacao de execucoes
	@echo   make test-diagnostics  - teste Python dos diagnosticos basic/full
	@echo   make test-docs         - valida links e referencias da documentacao
	@echo   make api-examples      - executa os exemplos publicos da API
	@echo   make demo              - executa o demo interno
	@echo   make scenario          - executa um cenario .ini com SCENARIO=configs/arquivo.ini
	@echo   make scenario-random   - executa configs/random.ini
	@echo   make scenario-small-world - executa configs/small_world.ini
	@echo   make scenario-feedforward - executa configs/feedforward.ini
	@echo   make scenario-stdp-ltp  - executa o demonstrador STDP de LTP
	@echo   make scenario-stdp-ltd  - executa o demonstrador STDP de LTD
	@echo   make scenario-stdp-mixed - executa o demonstrador STDP misto
	@echo   make plot-stdp-ltp     - gera o panorama STDP do demonstrador LTP
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

$(BUILD_DIR)/test_plasticity.exe: tests/test_plasticity.c src/neuron.c src/network.c src/plasticity.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_plasticity.c src/neuron.c src/network.c src/plasticity.c $(INCLUDES) -o $@

$(BUILD_DIR)/test_plasticity_long.exe: tests/test_plasticity_long.c $(API_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_plasticity_long.c $(API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_scenario_config.exe: tests/test_scenario_config.c $(APP_SOURCES) app/scenario_config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_scenario_config.c $(APP_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_scenario_runner.exe: tests/test_scenario_runner.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) app/scenario_runner.h app/scenario_config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_scenario_runner.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) $(INCLUDES) -o $@

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

test-scenario: $(BUILD_DIR)/test_scenario_config.exe
	$(BUILD_DIR)/test_scenario_config.exe

test-runner: $(BUILD_DIR)/test_scenario_runner.exe
	$(BUILD_DIR)/test_scenario_runner.exe

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

check-v02: | $(BUILD_DIR)
	$(PYTHON) scripts/check_release_v02.py

check-c1: $(BUILD_DIR)/minisnn_runner.exe | $(BUILD_DIR)
	$(PYTHON) scripts/check_c1.py

test-plot-neuron: | $(BUILD_DIR)
	$(PYTHON) tests/test_plot_neuron.py

test-plot-plasticity: | $(BUILD_DIR)
	$(PYTHON) tests/test_plot_plasticity.py

test-compare-runs: | $(BUILD_DIR)
	$(PYTHON) tests/test_compare_runs.py

test-diagnostics: | $(BUILD_DIR)
	$(PYTHON) tests/test_metrics_common.py
	$(PYTHON) tests/test_analyze_run.py
	$(PYTHON) tests/test_script_robustness.py

test-docs: | $(BUILD_DIR)
	$(PYTHON) tests/test_docs.py

test: test-api test-core test-lif test-plasticity test-scenario test-runner test-runner-topologies test-reproducibility test-memory

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

scenario-feedforward: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/feedforward.ini

scenario-stdp-ltp: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/stdp_ltp_demo.ini

scenario-stdp-ltd: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/stdp_ltd_demo.ini

scenario-stdp-mixed: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/stdp_mixed_demo.ini

plot-stdp-ltp: scenario-stdp-ltp
	$(PYTHON) scripts/plot_plasticity.py results/scenarios/stdp_ltp_demo

$(BUILD_DIR)/minisnn_studio.exe: app/minisnn_studio.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) include/minisnn.h include/minisnn_types.h app/scenario_config.h app/scenario_runner.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) app/minisnn_studio.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) $(INCLUDES) -o $@ -mwindows -lcomdlg32 -lshell32 -lgdi32 -luser32 -lole32

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
	@if exist results\comparisons for /D %%D in (results\comparisons\*) do rmdir /S /Q "%%D"
	@if exist results\comparisons\*.csv del /Q results\comparisons\*.csv
	@if exist results\benchmarks\*.csv del /Q results\benchmarks\*.csv
	@if exist results\benchmarks\*.txt del /Q results\benchmarks\*.txt
