CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -fanalyzer
INCLUDES = -Iinclude -Isrc -Iapp
BUILD_DIR = build

API_SOURCES = src/minisnn.c src/neuron.c src/network.c
APP_SOURCES = app/scenario_config.c
SCENARIO_RUNNER_SOURCES = app/scenario_config.c app/scenario_runner.c
CORE_SOURCES = src/neuron.c src/network.c src/topology.c src/stimulus.c src/recorder.c
EXPERIMENT_SOURCES = src/neuron.c src/network.c src/stimulus.c src/recorder.c
SCENARIO ?= configs/random_balanced.ini

.PHONY: all help clean test test-api test-core test-lif test-scenario test-runner \
	api-examples api-single api-chain api-exc-inh \
	demo ei-balance inhibition-fine inh-to-inh sparse-ei scenario studio-build studio

all: test

help:
	@echo Comandos disponiveis:
	@echo   make ou make test      - compila e executa todos os testes
	@echo   make test-api          - teste da API publica
	@echo   make test-core         - teste do nucleo/topologias/estimulos/recorders
	@echo   make test-lif          - teste basico do LIF
	@echo   make test-scenario     - teste do parser de cenarios
	@echo   make test-runner       - teste do executor compartilhado de cenarios
	@echo   make api-examples      - executa os exemplos publicos da API
	@echo   make demo              - executa o demo interno
	@echo   make scenario          - executa um cenario .ini com SCENARIO=configs/arquivo.ini
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

$(BUILD_DIR)/test_scenario_config.exe: tests/test_scenario_config.c $(APP_SOURCES) app/scenario_config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_scenario_config.c $(APP_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/test_scenario_runner.exe: tests/test_scenario_runner.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) app/scenario_runner.h app/scenario_config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_scenario_runner.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) $(INCLUDES) -o $@

test-api: $(BUILD_DIR)/test_minisnn_api.exe
	$(BUILD_DIR)/test_minisnn_api.exe

test-core: $(BUILD_DIR)/test_topology.exe
	$(BUILD_DIR)/test_topology.exe

test-lif: $(BUILD_DIR)/test_LIF.exe
	$(BUILD_DIR)/test_LIF.exe
	@if exist lif.csv del /Q lif.csv

test-scenario: $(BUILD_DIR)/test_scenario_config.exe
	$(BUILD_DIR)/test_scenario_config.exe

test-runner: $(BUILD_DIR)/test_scenario_runner.exe
	$(BUILD_DIR)/test_scenario_runner.exe

test: test-api test-core test-lif test-scenario test-runner

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

$(BUILD_DIR)/minisnn_studio.exe: app/minisnn_studio.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) include/minisnn.h include/minisnn_types.h app/scenario_config.h app/scenario_runner.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) app/minisnn_studio.c $(SCENARIO_RUNNER_SOURCES) $(API_SOURCES) $(INCLUDES) -o $@ -mwindows -lcomdlg32 -lshell32 -lgdi32 -luser32

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
