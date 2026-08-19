CC = gcc
AR ?= ar
CFLAGS = -std=c11 -Wall -Wextra -pedantic -fanalyzer
INCLUDES = -Iinclude -Isrc -Iapp
PUBLIC_INCLUDES = -Iinclude
APP_INCLUDES = -Iinclude -Iapp
BUILD_ROOT = ../build
CORE_LIB_DIR = $(BUILD_ROOT)/core/lib
CORE_OBJ_DIR = $(BUILD_ROOT)/core/obj
CORE_BIN_DIR = $(BUILD_ROOT)/core/bin
TOOLS_BIN_DIR = $(BUILD_ROOT)/tools/bin
TEST_BIN_DIR = $(BUILD_ROOT)/tests/bin
STUDIO_BIN_DIR = $(BUILD_ROOT)/studio/bin
STUDIO_OBJ_DIR = $(BUILD_ROOT)/studio/obj
# Historical app rules below use BUILD_DIR for headless executable products.
BUILD_DIR = $(TOOLS_BIN_DIR)
CORE_LIB = $(CORE_LIB_DIR)/libminisnn_core.a
CORE_LIBRARY_SOURCES = $(sort $(wildcard src/*.c))
CORE_OBJECTS = $(patsubst src/%.c,$(CORE_OBJ_DIR)/%.o,$(CORE_LIBRARY_SOURCES))
RELEASE_ROOT = $(BUILD_ROOT)/release
RELEASE_CORE_LIB_DIR = $(RELEASE_ROOT)/core/lib
RELEASE_CORE_INCLUDE_DIR = $(RELEASE_ROOT)/core/include
RELEASE_CORE_OBJ_DIR = $(RELEASE_ROOT)/core/obj
RELEASE_TOOLS_BIN_DIR = $(RELEASE_ROOT)/tools/bin
RELEASE_STUDIO_BIN_DIR = $(RELEASE_ROOT)/studio/bin
RELEASE_STUDIO_OBJ_DIR = $(RELEASE_ROOT)/studio/obj
RELEASE_CORE_LIB = $(RELEASE_CORE_LIB_DIR)/libminisnn_core.a
RELEASE_CORE_OBJECTS = $(patsubst src/%.c,$(RELEASE_CORE_OBJ_DIR)/%.o,$(CORE_LIBRARY_SOURCES))
RELEASE_CFLAGS = -std=c11 -O2 -DNDEBUG -Wall -Wextra -pedantic

STRUCTURE_SOURCES = src/structure.c src/structural_plasticity.c
API_SOURCES = src/minisnn.c src/neuron.c src/neuron_model.c src/network.c src/plasticity.c src/reward.c src/homeostasis.c src/agent_io.c src/sensor_encoder.c src/action_decoder.c src/agent_cycle.c src/agent_cycle_checkpoint.c $(STRUCTURE_SOURCES)
TESTING_API_SOURCES = $(API_SOURCES)
APP_SOURCES = app/scenario_config.c
APP_FILESYSTEM_SOURCES = app/app_filesystem.c
SCENARIO_RUNNER_SOURCES = $(APP_FILESYSTEM_SOURCES) app/scenario_config.c app/scenario_runtime.c app/scenario_runner.c app/working_memory.c app/associative_memory.c app/sequence_prediction.c
C7_AUDIT_SOURCES = $(APP_FILESYSTEM_SOURCES) app/c7_audit_common.c
C6_SUITE_SOURCES = app/c6_suite.c $(SCENARIO_RUNNER_SOURCES)
D1_B_AUDIT_SOURCES = app/d1_b_robustness_config.c $(C7_AUDIT_SOURCES)
EVOLUTION_SOURCES = src/evolution.c
EVOLUTION_RUNNER_SOURCES = app/evolution_config.c app/evolution_runner.c $(SCENARIO_RUNNER_SOURCES)
CORE_SOURCES = src/neuron.c src/neuron_model.c src/network.c src/plasticity.c src/reward.c src/homeostasis.c src/agent_io.c $(STRUCTURE_SOURCES) src/topology.c src/stimulus.c src/recorder.c
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
ANALYZER_CFLAGS = -std=c11 -Wall -Wextra -pedantic -fanalyzer -Wformat=2 -Wstrict-prototypes -Wshadow -Wconversion -Wnull-dereference

TEST_BINARIES = $(patsubst tests/%.c,$(TEST_BIN_DIR)/%.exe,$(wildcard tests/*.c))

.PHONY: all core core-lib headless core-test clean-core clean-studio clean-tests help clean test test-runtime test-api test-core test-topology-factory test-lif test-neuron-model test-adex test-hodgkin-huxley test-model-checkpoints test-model-error-propagation test-model-provenance test-adex-long test-hh-long test-models-long test-plasticity test-plasticity-long test-homeostasis test-homeostasis-long test-reward test-reward-long test-evolution test-structure test-structural-plasticity test-structure-resume test-structure-long test-evolution-runner test-evolution-resume test-evolution-long test-plot-evolution test-plot-topology test-evolution-report test-structural-html evolution-build evolution-weight-demo evolution-homeostasis-demo evolution-plasticity-demo evolution-structure-demo structural-pruning-demo structural-growth-demo evolution-structure-learning-demo plot-evolution plot-topology report-evolution report-evolution-history report-structural-events report-best-topology test-scenario test-runner test-runner-topologies test-topology-factory test-working-memory test-associative-memory test-sequence-prediction test-c6-checkpoints test-c6-integration test-c6-long test-agent-io test-sensor-encoder test-action-decoder test-agent-cycle test-reproducibility test-regression test-memory test-long test-analyzer test-sanitize test-d1-determinism test-d1-corruption test-d1-lifecycle-stress test-d1-optimization-determinism test-d1-posix-headless test-d1-portability test-d1-long-run test-d1-symbols benchmark-d1 scenario-d1-b audit-d1-b audit-d1 benchmark-v02 benchmark-c1 benchmark-c15 benchmark-c2 benchmark-c3 benchmark-c4 benchmark-c5 check-v02 check-c1 check-c15 check-c2 check-c3 check-c4 check-c5-interface check-c5 check-c6 check-c7 compare-neuron-models test-compare-neuron-models test-version test-api-baseline test-api-baseline-regressions release-headers release-core release-headless release-studio release-all test-release-build test-external-consumer package-release test-release-integrity test-release-packages test-studio-source-contracts test-studio-product-launcher test-studio-runtime-layout audit-d1-c-automated audit-d1-c \
	test-plot-neuron test-plot-plasticity test-compare-runs test-diagnostics test-run-reports test-history-report test-docs test-agent-cycle-checkpoint \
	test-plot-homeostasis plot-homeostasis test-plot-reward plot-reward \
	test-c7-integration test-c7-evolution test-c7-long-run test-c7 \
	api-examples api-single api-chain api-exc-inh \
	demo ei-balance inhibition-fine inh-to-inh sparse-ei scenario \
	scenario-random scenario-small-world scenario-working-memory scenario-associative-memory scenario-sequence-prediction scenario-sequence-prediction-context scenario-c6-suite scenario-sensor-encoding scenario-action-decoding scenario-agent-cycle-checkpoint scenario-feedforward \
	scenario-c7-integrated-audit \
	scenario-stdp-ltp scenario-stdp-ltd scenario-stdp-mixed plot-stdp-ltp \
	scenario-homeostasis-silence scenario-homeostasis-explosion scenario-homeostasis-stdp \
	scenario-reward-positive scenario-punishment-negative scenario-reward-delayed scenario-reward-mixed \
	report-metrics report-weights report-all report-history \
	studio-build studio audit-d1-build-products audit-d1-api

all: core

core: core-lib headless

core-lib: $(CORE_LIB)

headless: $(BUILD_DIR)/minisnn_runner.exe $(BUILD_DIR)/evolution_runner.exe \
	$(BUILD_DIR)/c6_suite_runner.exe $(BUILD_DIR)/c7_integrated_audit.exe

core-test: test

core-headless: headless

core-studio: studio-build

core-evolution: evolution-build

help:
	@echo Comandos disponiveis:
	@echo   make core-lib          - compila somente a biblioteca estatica do Core
	@echo   make core              - compila biblioteca e produtos headless, sem Studio
	@echo   make core-headless     - compila somente runners headless
	@echo   make core-test         - executa testes sem compilar Studio
	@echo   make core-studio       - compila explicitamente o miniSNN Studio
	@echo   make core-evolution    - compila o runner de neuroevolucao
	@echo   make clean-core        - limpa biblioteca e objetos do Core
	@echo   make clean-studio      - limpa somente o produto Studio
	@echo   make clean-tests       - limpa somente executaveis de teste
	@echo   make audit-d1-build-products - valida a separacao dos produtos D1-A
	@echo   make audit-d1-api      - valida API publica e arquitetura D1-A
	@echo   make test-version       - valida a identidade publica da biblioteca
	@echo   make test-api-baseline  - compara a superficie publica semantica com o baseline RC
	@echo   make test-api-baseline-regressions - valida invariancia semantica do baseline
	@echo   make release-all        - compila Core, ferramentas e Studio de release
	@echo   make test-external-consumer - valida consumo externo do pacote Core
	@echo   make package-release    - gera os ZIPs locais de avaliacao
	@echo   make test-release-integrity - adultera copias temporarias dos pacotes D1-C
	@echo   make test-release-packages - valida integridade e smoke dos ZIPs de release
	@echo   make test-studio-source-contracts - valida contratos Win32 do Studio
	@echo   make test-studio-product-launcher - valida a resolucao do produto adjacente ao Studio
	@echo   make test-studio-runtime-layout - valida layouts repository/package do Studio
	@echo   make audit-d1-c         - valida gates automaticos e o checklist manual D1-C
	@echo   make audit-d1           - executa a auditoria final D1-A/B/C
	@echo   make ou make test      - compila e executa todos os testes
	@echo   make test-api          - teste da API publica
	@echo   make test-core         - teste do nucleo/topologias/estimulos/recorders
	@echo   make test-topology-factory - valida a fabrica publica de topologias
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
	@echo   make test-associative-memory - validacao do protocolo associativo C6.2
	@echo   make test-sequence-prediction - validacao do protocolo temporal C6.3
	@echo   make test-c6-checkpoints - persistencia sinaptica C6.2/C6.3
	@echo   make test-c6-integration - contratos integrados C6.1-C6.3
	@echo   make test-c6-long        - robustez moderada dos protocolos C6
	@echo   make test-agent-io       - valida contratos genericos de I/O C7.1
	@echo   make test-sensor-encoder - valida codificacao sensor-neural C7.2
	@echo   make scenario-sensor-encoding - executa o demo reproduzivel C7.2
	@echo   make test-action-decoder - valida decodificacao neural-acao C7.3
	@echo   make scenario-action-decoding - executa o demo reproduzivel C7.3
	@echo   make test-agent-cycle    - valida orquestracao generica C7.4
	@echo   make test-agent-cycle-checkpoint - valida persistencia e replay C7.5-A
	@echo   make test-c7-integration - valida a cadeia integrada C7.5-B
	@echo   make test-c7-evolution  - valida a ponte C7 com a evolucao abstrata
	@echo   make test-c7-long-run   - executa 100000 passos por modelo C7
	@echo   make test-c7            - executa a matriz completa C7 sem duplicacao no teste geral
	@echo   make scenario-c7-integrated-audit - gera a auditoria reproduzivel C7.5-B
	@echo   make scenario-agent-cycle - executa o demo reproduzivel C7.4
	@echo   make scenario-agent-cycle-checkpoint - executa o demo C7.5-A
	@echo   make test-runner-topologies - validacao estrutural das topologias do runner
	@echo   make test-reproducibility - determinismo de topologia e CSVs por seed
	@echo   make test-regression    - golden pequeno e resultados historicos conhecidos
	@echo   make test-memory       - estresse finito de ciclo de vida, conexoes e delays
	@echo   make test-long         - 50000 passos com verificacao de estado finito
	@echo   make test-d1-determinism - matriz deterministica D1-B por modelo e modo
	@echo   make test-d1-corruption - parser e arquivos corrompidos D1-B
	@echo   make test-d1-lifecycle-stress - lifecycle, falha de alocacao e contadores D1-B
	@echo   make test-d1-optimization-determinism - compara os mesmos traces em O0 e O2
	@echo   make test-d1-posix-headless - smoke estrito do Core headless em POSIX, quando disponivel
	@echo   make test-d1-portability - contratos dos harnesses portaveis D1-B
	@echo   make test-d1-long-run - executa os orcamentos completos da auditoria D1-B
	@echo   make test-d1-symbols - audita simbolos publicos e a garantia ABI real
	@echo   make benchmark-d1    - mede passos, C7 e checkpoints em tres repeticoes
	@echo   make scenario-d1-b   - gera todos os artefatos e relatorio local D1-B
	@echo   make audit-d1-b      - executa a auditoria agregada D1-B
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
	@echo   make check-c6          - verifica os protocolos C6.1-C6.4
	@echo   make check-c7          - verifica C7.1-C7.5-B e fecha a interface cerebro-agente
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
	@echo   make scenario-associative-memory - executa configs/associative_memory_demo.ini
	@echo   make scenario-sequence-prediction - executa configs/sequence_prediction_demo.ini
	@echo   make scenario-sequence-prediction-context - executa configs/sequence_prediction_context_demo.ini
	@echo   make scenario-c6-suite   - executa a suite integrada C6
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

$(CORE_LIB_DIR) $(CORE_OBJ_DIR) $(CORE_BIN_DIR) $(TOOLS_BIN_DIR) $(TEST_BIN_DIR) $(STUDIO_BIN_DIR) $(STUDIO_OBJ_DIR) $(RELEASE_CORE_LIB_DIR) $(RELEASE_CORE_INCLUDE_DIR) $(RELEASE_CORE_OBJ_DIR) $(RELEASE_TOOLS_BIN_DIR) $(RELEASE_STUDIO_BIN_DIR) $(RELEASE_STUDIO_OBJ_DIR):
	@if not exist "$@" mkdir "$@"

$(TEST_BINARIES): | $(TEST_BIN_DIR)

$(CORE_OBJ_DIR)/%.o: src/%.c | $(CORE_OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(CORE_LIB): $(CORE_OBJECTS) | $(CORE_LIB_DIR)
	$(AR) rcs $@ $(CORE_OBJECTS)

$(RELEASE_CORE_OBJ_DIR)/%.o: src/%.c | $(RELEASE_CORE_OBJ_DIR)
	$(CC) $(RELEASE_CFLAGS) $(INCLUDES) -c $< -o $@

$(RELEASE_CORE_LIB): $(RELEASE_CORE_OBJECTS) | $(RELEASE_CORE_LIB_DIR)
	$(AR) rcs $@ $(RELEASE_CORE_OBJECTS)

release-headers: | $(RELEASE_CORE_INCLUDE_DIR)
	copy /Y include\*.h "$(RELEASE_CORE_INCLUDE_DIR)\" >nul

release-core: $(RELEASE_CORE_LIB) release-headers

$(RELEASE_TOOLS_BIN_DIR)/minisnn_runner.exe: app/minisnn_runner.c $(SCENARIO_RUNNER_SOURCES) $(RELEASE_CORE_LIB) | $(RELEASE_TOOLS_BIN_DIR)
	$(CC) $(RELEASE_CFLAGS) app/minisnn_runner.c $(SCENARIO_RUNNER_SOURCES) $(RELEASE_CORE_LIB) $(INCLUDES) -o $@

$(RELEASE_TOOLS_BIN_DIR)/evolution_runner.exe: $(EVOLUTION_RUNNER_SOURCES) $(RELEASE_CORE_LIB) | $(RELEASE_TOOLS_BIN_DIR)
	$(CC) $(RELEASE_CFLAGS) $(EVOLUTION_RUNNER_SOURCES) $(RELEASE_CORE_LIB) $(INCLUDES) -o $@

$(RELEASE_TOOLS_BIN_DIR)/c6_suite_runner.exe: app/c6_suite_runner.c $(C6_SUITE_SOURCES) $(RELEASE_CORE_LIB) | $(RELEASE_TOOLS_BIN_DIR)
	$(CC) $(RELEASE_CFLAGS) app/c6_suite_runner.c $(C6_SUITE_SOURCES) $(RELEASE_CORE_LIB) $(INCLUDES) -o $@

$(RELEASE_TOOLS_BIN_DIR)/c7_integrated_audit.exe: app/c7_integrated_audit.c app/c7_integrated_audit_config.c $(C7_AUDIT_SOURCES) $(RELEASE_CORE_LIB) | $(RELEASE_TOOLS_BIN_DIR)
	$(CC) $(RELEASE_CFLAGS) app/c7_integrated_audit.c app/c7_integrated_audit_config.c $(C7_AUDIT_SOURCES) $(RELEASE_CORE_LIB) $(INCLUDES) -o $@

release-headless: $(RELEASE_TOOLS_BIN_DIR)/minisnn_runner.exe $(RELEASE_TOOLS_BIN_DIR)/evolution_runner.exe $(RELEASE_TOOLS_BIN_DIR)/c6_suite_runner.exe $(RELEASE_TOOLS_BIN_DIR)/c7_integrated_audit.exe

$(RELEASE_STUDIO_OBJ_DIR)/minisnn_studio.o: studio/minisnn_studio.c include/minisnn.h include/minisnn_version.h | $(RELEASE_STUDIO_OBJ_DIR)
	$(CC) $(RELEASE_CFLAGS) $(APP_INCLUDES) -Istudio -c $< -o $@

$(RELEASE_STUDIO_BIN_DIR)/minisnn_studio.exe: $(RELEASE_STUDIO_OBJ_DIR)/minisnn_studio.o studio/studio_runtime_layout.c studio/studio_runtime_layout.h studio/studio_product_launcher.c studio/studio_product_launcher.h app/evolution_config.c $(SCENARIO_RUNNER_SOURCES) $(RELEASE_CORE_LIB) | $(RELEASE_STUDIO_BIN_DIR)
	$(CC) $(RELEASE_CFLAGS) $(RELEASE_STUDIO_OBJ_DIR)/minisnn_studio.o studio/studio_runtime_layout.c studio/studio_product_launcher.c app/evolution_config.c $(SCENARIO_RUNNER_SOURCES) $(RELEASE_CORE_LIB) $(APP_INCLUDES) -Istudio -o $@ -mwindows -lcomdlg32 -lshell32 -lgdi32 -luser32 -lole32

release-studio: $(RELEASE_STUDIO_BIN_DIR)/minisnn_studio.exe

release-all: release-core release-headless release-studio

$(TEST_BIN_DIR)/test_minisnn_api.exe: tests/test_minisnn_api.c $(CORE_LIB) include/minisnn.h include/minisnn_types.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_minisnn_api.c $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_version.exe: tests/test_version.c $(CORE_LIB) include/minisnn_version.h | $(TEST_BIN_DIR)
	$(CC) $(CFLAGS) tests/test_version.c $(CORE_LIB) $(INCLUDES) -o $@

test-version: $(TEST_BIN_DIR)/test_version.exe
	$(TEST_BIN_DIR)/test_version.exe

test-api-baseline: test-api-baseline-regressions
	$(PYTHON) scripts/check_api_baseline.py

test-api-baseline-regressions:
	$(PYTHON) tests/test_api_baseline_regressions.py

test-release-build: release-all
	$(PYTHON) scripts/check_release_build.py

test-external-consumer: release-core
	$(PYTHON) scripts/test_external_consumer.py

package-release: release-all
	$(PYTHON) scripts/build_release_packages.py

test-release-integrity:
	$(PYTHON) tests/test_d1_c_release_integrity.py

test-release-packages: package-release test-release-integrity
	$(PYTHON) scripts/check_release_packages.py

$(TEST_BIN_DIR)/test_studio_runtime_layout.exe: tests/test_studio_runtime_layout.c studio/studio_runtime_layout.c studio/studio_runtime_layout.h | $(TEST_BIN_DIR)
	$(CC) $(CFLAGS) tests/test_studio_runtime_layout.c studio/studio_runtime_layout.c -Istudio -o $@

test-studio-source-contracts:
	$(PYTHON) tests/test_studio_source_contracts.py

$(TEST_BIN_DIR)/test_studio_product_launcher.exe: tests/test_studio_product_launcher.c studio/studio_product_launcher.c studio/studio_product_launcher.h | $(TEST_BIN_DIR)
	$(CC) $(CFLAGS) tests/test_studio_product_launcher.c studio/studio_product_launcher.c -Istudio -o $@

test-studio-product-launcher: $(TEST_BIN_DIR)/test_studio_product_launcher.exe
	$(TEST_BIN_DIR)/test_studio_product_launcher.exe

test-studio-runtime-layout: test-studio-source-contracts test-studio-product-launcher $(TEST_BIN_DIR)/test_studio_runtime_layout.exe
	$(TEST_BIN_DIR)/test_studio_runtime_layout.exe

audit-d1-c-automated: audit-d1-api audit-d1-b test-version test-api-baseline test-release-build test-external-consumer test-studio-runtime-layout package-release test-release-packages
	$(PYTHON) scripts/check_d1_c.py --automated

audit-d1-c: audit-d1-c-automated
	$(PYTHON) scripts/check_d1_c.py

audit-d1: audit-d1-api audit-d1-b audit-d1-c
	$(PYTHON) scripts/check_d1_c.py --d1

$(TEST_BIN_DIR)/test_agent_io.exe: tests/test_agent_io.c $(CORE_LIB) include/minisnn_agent_io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_agent_io.c $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_sensor_encoder.exe: tests/test_sensor_encoder.c $(CORE_LIB) include/minisnn_sensor_encoder.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_sensor_encoder.c $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_action_decoder.exe: tests/test_action_decoder.c $(CORE_LIB) include/minisnn_action_decoder.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_action_decoder.c $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_agent_cycle.exe: tests/test_agent_cycle.c $(CORE_LIB) include/minisnn_agent_cycle.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DMINISNN_TESTING tests/test_agent_cycle.c $(TESTING_API_SOURCES) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_agent_cycle_checkpoint.exe: tests/test_agent_cycle_checkpoint.c $(APP_FILESYSTEM_SOURCES) $(CORE_LIB) include/minisnn_agent_cycle.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DMINISNN_TESTING tests/test_agent_cycle_checkpoint.c $(APP_FILESYSTEM_SOURCES) $(TESTING_API_SOURCES) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_c7_integration.exe: tests/test_c7_integration.c $(C7_AUDIT_SOURCES) $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DMINISNN_TESTING tests/test_c7_integration.c $(C7_AUDIT_SOURCES) $(TESTING_API_SOURCES) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_c7_evolution.exe: tests/test_c7_evolution.c $(C7_AUDIT_SOURCES) $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_c7_evolution.c $(C7_AUDIT_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_c7_long_run.exe: tests/test_c7_long_run.c $(C7_AUDIT_SOURCES) $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_c7_long_run.c $(C7_AUDIT_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_d1_determinism.exe: tests/test_d1_determinism.c $(SCENARIO_RUNNER_SOURCES) $(C7_AUDIT_SOURCES) $(CORE_LIB) | $(TEST_BIN_DIR)
	$(CC) $(CFLAGS) tests/test_d1_determinism.c $(sort $(SCENARIO_RUNNER_SOURCES) $(C7_AUDIT_SOURCES)) $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_d1_corruption.exe: tests/test_d1_corruption.c app/scenario_config.c $(APP_FILESYSTEM_SOURCES) $(CORE_LIB) | $(TEST_BIN_DIR)
	$(CC) $(CFLAGS) tests/test_d1_corruption.c app/scenario_config.c $(APP_FILESYSTEM_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_d1_lifecycle_stress.exe: tests/test_d1_lifecycle_stress.c tests/test_allocation.c $(C7_AUDIT_SOURCES) $(TESTING_API_SOURCES) | $(TEST_BIN_DIR)
	$(CC) $(CFLAGS) -DMINISNN_TESTING -include tests/test_allocation.h tests/test_d1_lifecycle_stress.c tests/test_allocation.c $(C7_AUDIT_SOURCES) $(TESTING_API_SOURCES) $(INCLUDES) -o $@

$(BUILD_DIR)/agent_cycle_demo.exe: app/agent_cycle_demo.c app/agent_cycle_demo_config.c app/agent_cycle_demo_config.h $(APP_FILESYSTEM_SOURCES) $(CORE_LIB) include/minisnn_agent_cycle.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) app/agent_cycle_demo.c app/agent_cycle_demo_config.c $(APP_FILESYSTEM_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(BUILD_DIR)/agent_cycle_checkpoint_demo.exe: app/agent_cycle_checkpoint_demo.c app/agent_cycle_checkpoint_demo_config.c app/agent_cycle_checkpoint_demo_config.h app/agent_cycle_demo_config.c app/agent_cycle_demo_config.h $(APP_FILESYSTEM_SOURCES) $(CORE_LIB) include/minisnn_agent_cycle.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) app/agent_cycle_checkpoint_demo.c app/agent_cycle_checkpoint_demo_config.c app/agent_cycle_demo_config.c $(APP_FILESYSTEM_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(BUILD_DIR)/c7_integrated_audit.exe: app/c7_integrated_audit.c app/c7_integrated_audit_config.c app/c7_integrated_audit_config.h $(C7_AUDIT_SOURCES) $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) app/c7_integrated_audit.c app/c7_integrated_audit_config.c $(C7_AUDIT_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(BUILD_DIR)/d1_b_robustness_audit.exe: app/d1_b_robustness_audit.c app/d1_b_robustness_config.h $(D1_B_AUDIT_SOURCES) $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) app/d1_b_robustness_audit.c $(D1_B_AUDIT_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(BUILD_DIR)/d1_b_benchmark.exe: app/d1_b_benchmark.c $(C7_AUDIT_SOURCES) $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) app/d1_b_benchmark.c $(C7_AUDIT_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(BUILD_DIR)/sensor_encoding_demo.exe: app/sensor_encoding_demo.c app/sensor_encoding_demo_config.c app/sensor_encoding_demo_config.h $(APP_FILESYSTEM_SOURCES) $(CORE_LIB) include/minisnn_sensor_encoder.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) app/sensor_encoding_demo.c app/sensor_encoding_demo_config.c $(APP_FILESYSTEM_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(BUILD_DIR)/action_decoding_demo.exe: app/action_decoding_demo.c app/action_decoding_demo_config.c app/action_decoding_demo_config.h $(APP_FILESYSTEM_SOURCES) $(CORE_LIB) include/minisnn_action_decoder.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) app/action_decoding_demo.c app/action_decoding_demo_config.c $(APP_FILESYSTEM_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_topology.exe: tests/test_topology.c $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_topology.c $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_topology_factory.exe: tests/test_topology_factory.c $(CORE_LIB) include/minisnn.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_topology_factory.c $(CORE_LIB) $(INCLUDES) -o $@

test-topology-factory: $(TEST_BIN_DIR)/test_topology_factory.exe
	$(TEST_BIN_DIR)/test_topology_factory.exe
$(TEST_BIN_DIR)/test_LIF.exe: tests/test_LIF.c src/neuron.c src/neuron.h src/config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_LIF.c src/neuron.c $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_neuron_model.exe: tests/test_neuron_model.c $(CORE_LIB) include/minisnn_types.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_neuron_model.c $(CORE_LIB) $(INCLUDES) -o $@

test-neuron-model: $(TEST_BIN_DIR)/test_neuron_model.exe
	$(TEST_BIN_DIR)/test_neuron_model.exe

$(TEST_BIN_DIR)/test_adex.exe: tests/test_adex.c src/neuron.c src/neuron_model.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_adex.c src/neuron.c src/neuron_model.c $(INCLUDES) -o $@

test-adex: $(TEST_BIN_DIR)/test_adex.exe
	$(TEST_BIN_DIR)/test_adex.exe

$(TEST_BIN_DIR)/test_hodgkin_huxley.exe: tests/test_hodgkin_huxley.c src/neuron.c src/neuron_model.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_hodgkin_huxley.c src/neuron.c src/neuron_model.c $(INCLUDES) -o $@

test-hodgkin-huxley: $(TEST_BIN_DIR)/test_hodgkin_huxley.exe
	$(TEST_BIN_DIR)/test_hodgkin_huxley.exe

$(TEST_BIN_DIR)/test_model_error_propagation.exe: tests/test_model_error_propagation.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DMINISNN_TESTING tests/test_model_error_propagation.c $(SCENARIO_RUNNER_SOURCES) $(TESTING_API_SOURCES) $(INCLUDES) -o $@

test-model-error-propagation: $(TEST_BIN_DIR)/test_model_error_propagation.exe
	$(TEST_BIN_DIR)/test_model_error_propagation.exe

$(TEST_BIN_DIR)/test_model_provenance.exe: tests/test_model_provenance.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_model_provenance.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

test-model-provenance: $(TEST_BIN_DIR)/test_model_provenance.exe
	$(TEST_BIN_DIR)/test_model_provenance.exe

$(TEST_BIN_DIR)/test_models_long.exe: tests/test_models_long.c $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_models_long.c $(CORE_LIB) $(INCLUDES) -o $@

test-adex-long: $(TEST_BIN_DIR)/test_models_long.exe
	$(TEST_BIN_DIR)/test_models_long.exe

test-hh-long: $(TEST_BIN_DIR)/test_models_long.exe
	$(TEST_BIN_DIR)/test_models_long.exe

test-models-long: $(TEST_BIN_DIR)/test_models_long.exe
	$(TEST_BIN_DIR)/test_models_long.exe

check-c5-interface: test-neuron-model
	$(PYTHON) scripts/check_c5_interface.py
	@echo C5 neuron model interface validation OK

check-c5: check-c5-interface test-model-checkpoints test-adex test-hodgkin-huxley test-model-error-propagation test-model-provenance test-models-long test-compare-neuron-models
	@echo C5 advanced neuron models validation OK

check-c6: test-runtime test-working-memory test-associative-memory test-sequence-prediction test-c6-checkpoints test-c6-integration test-c6-long
	$(PYTHON) scripts/check_c6.py
	@if exist build rmdir /S /Q build

check-c7: test-runtime test-agent-io test-sensor-encoder test-action-decoder test-agent-cycle test-agent-cycle-checkpoint test-c7-integration test-c7-evolution test-c7-long-run scenario-c7-integrated-audit
	$(PYTHON) scripts/check_c7.py
	@if exist build rmdir /S /Q build

audit-d1-build-products:
	$(PYTHON) scripts/check_build_products.py

audit-d1-api: audit-d1-build-products core-lib headless
	$(PYTHON) scripts/check_public_api.py

$(TEST_BIN_DIR)/test_plasticity.exe: tests/test_plasticity.c $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_plasticity.c $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_plasticity_long.exe: tests/test_plasticity_long.c $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_plasticity_long.c $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_homeostasis.exe: tests/test_homeostasis.c $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_homeostasis.c $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_homeostasis_runner.exe: tests/test_homeostasis_runner.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_homeostasis_runner.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_homeostasis_long.exe: tests/test_homeostasis_long.c $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_homeostasis_long.c $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_reward.exe: tests/test_reward.c $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_reward.c $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_reward_long.exe: tests/test_reward_long.c $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_reward_long.c $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_evolution.exe: tests/test_evolution.c $(CORE_LIB) include/minisnn_evolution_legacy.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_evolution.c $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_structure.exe: tests/test_structure.c $(CORE_LIB) $(CORE_LIB) src/structure.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_structure.c $(CORE_LIB) $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_structural_plasticity.exe: tests/test_structural_plasticity.c $(CORE_LIB) $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_structural_plasticity.c $(CORE_LIB) $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_structure_resume.exe: tests/test_structure_resume.c $(EVOLUTION_RUNNER_SOURCES) $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DEVOLUTION_RUNNER_NO_MAIN tests/test_structure_resume.c $(EVOLUTION_RUNNER_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_structure_long.exe: tests/test_structure_long.c $(EVOLUTION_RUNNER_SOURCES) $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DEVOLUTION_RUNNER_NO_MAIN tests/test_structure_long.c $(EVOLUTION_RUNNER_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_evolution_runner.exe: tests/test_evolution_runner.c $(EVOLUTION_RUNNER_SOURCES) $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DEVOLUTION_RUNNER_NO_MAIN tests/test_evolution_runner.c $(EVOLUTION_RUNNER_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_evolution_long.exe: tests/test_evolution_long.c $(EVOLUTION_RUNNER_SOURCES) $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DEVOLUTION_RUNNER_NO_MAIN tests/test_evolution_long.c $(EVOLUTION_RUNNER_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(BUILD_DIR)/evolution_runner.exe: $(EVOLUTION_RUNNER_SOURCES) $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(EVOLUTION_RUNNER_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_scenario_config.exe: tests/test_scenario_config.c $(APP_SOURCES) $(CORE_LIB) app/scenario_config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_scenario_config.c $(APP_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_scenario_runner.exe: tests/test_scenario_runner.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) app/scenario_runner.h app/scenario_config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_scenario_runner.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_working_memory.exe: tests/test_working_memory.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) app/working_memory.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DMINISNN_TESTING tests/test_working_memory.c $(SCENARIO_RUNNER_SOURCES) $(TESTING_API_SOURCES) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_associative_memory.exe: tests/test_associative_memory.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) app/associative_memory.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DMINISNN_TESTING tests/test_associative_memory.c $(SCENARIO_RUNNER_SOURCES) $(TESTING_API_SOURCES) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_sequence_prediction.exe: tests/test_sequence_prediction.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) app/sequence_prediction.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DMINISNN_TESTING tests/test_sequence_prediction.c $(SCENARIO_RUNNER_SOURCES) $(TESTING_API_SOURCES) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_c6_checkpoints.exe: tests/test_c6_checkpoints.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) app/associative_memory.h app/sequence_prediction.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DMINISNN_TESTING tests/test_c6_checkpoints.c $(SCENARIO_RUNNER_SOURCES) $(TESTING_API_SOURCES) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_c6_integration.exe: tests/test_c6_integration.c $(C6_SUITE_SOURCES) $(CORE_LIB) app/c6_suite.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DMINISNN_TESTING tests/test_c6_integration.c $(C6_SUITE_SOURCES) $(TESTING_API_SOURCES) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_c6_long.exe: tests/test_c6_long.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_c6_long.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(BUILD_DIR)/c6_suite_runner.exe: app/c6_suite_runner.c $(C6_SUITE_SOURCES) $(CORE_LIB) app/c6_suite.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) app/c6_suite_runner.c $(C6_SUITE_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_runner_topologies.exe: tests/test_runner_topologies.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) app/scenario_runner.h app/scenario_config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_runner_topologies.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_reproducibility.exe: tests/test_reproducibility.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) app/scenario_runner.h app/scenario_config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_reproducibility.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_memory_stress.exe: tests/test_memory_stress.c $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_memory_stress.c $(CORE_LIB) $(INCLUDES) -o $@

$(TEST_BIN_DIR)/test_long_run.exe: tests/test_long_run.c $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_long_run.c $(CORE_LIB) $(INCLUDES) -o $@

$(BUILD_DIR)/benchmark_core.exe: tests/benchmark_core.c $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/benchmark_core.c $(CORE_LIB) $(INCLUDES) -o $@

test-api: $(TEST_BIN_DIR)/test_minisnn_api.exe
	$(TEST_BIN_DIR)/test_minisnn_api.exe

test-core: $(TEST_BIN_DIR)/test_topology.exe
	$(TEST_BIN_DIR)/test_topology.exe

test-lif: $(TEST_BIN_DIR)/test_LIF.exe
	$(TEST_BIN_DIR)/test_LIF.exe
	@if exist lif.csv del /Q lif.csv

test-plasticity: $(TEST_BIN_DIR)/test_plasticity.exe
	$(TEST_BIN_DIR)/test_plasticity.exe

test-plasticity-long: $(TEST_BIN_DIR)/test_plasticity_long.exe
	$(TEST_BIN_DIR)/test_plasticity_long.exe

test-homeostasis: $(TEST_BIN_DIR)/test_homeostasis.exe $(TEST_BIN_DIR)/test_homeostasis_runner.exe
	$(TEST_BIN_DIR)/test_homeostasis.exe
	$(TEST_BIN_DIR)/test_homeostasis_runner.exe

test-homeostasis-long: $(TEST_BIN_DIR)/test_homeostasis_long.exe
	$(TEST_BIN_DIR)/test_homeostasis_long.exe

test-reward: $(TEST_BIN_DIR)/test_reward.exe
	$(TEST_BIN_DIR)/test_reward.exe

test-reward-long: $(TEST_BIN_DIR)/test_reward_long.exe
	$(TEST_BIN_DIR)/test_reward_long.exe

test-evolution: $(TEST_BIN_DIR)/test_evolution.exe
	$(TEST_BIN_DIR)/test_evolution.exe

test-structure: $(TEST_BIN_DIR)/test_structure.exe
	$(TEST_BIN_DIR)/test_structure.exe

test-structural-plasticity: $(TEST_BIN_DIR)/test_structural_plasticity.exe
	$(TEST_BIN_DIR)/test_structural_plasticity.exe

test-structure-resume: $(TEST_BIN_DIR)/test_structure_resume.exe
	$(TEST_BIN_DIR)/test_structure_resume.exe

test-structure-long: $(TEST_BIN_DIR)/test_structure_long.exe
	$(TEST_BIN_DIR)/test_structure_long.exe

test-evolution-runner: $(TEST_BIN_DIR)/test_evolution_runner.exe
	$(TEST_BIN_DIR)/test_evolution_runner.exe

test-evolution-resume: test-evolution-runner

test-evolution-long: $(TEST_BIN_DIR)/test_evolution_long.exe
	$(TEST_BIN_DIR)/test_evolution_long.exe

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

test-scenario: $(TEST_BIN_DIR)/test_scenario_config.exe
	$(TEST_BIN_DIR)/test_scenario_config.exe

test-runner: $(TEST_BIN_DIR)/test_scenario_runner.exe
	$(TEST_BIN_DIR)/test_scenario_runner.exe

test-working-memory: $(TEST_BIN_DIR)/test_working_memory.exe
	$(TEST_BIN_DIR)/test_working_memory.exe

test-associative-memory: $(TEST_BIN_DIR)/test_associative_memory.exe
	$(TEST_BIN_DIR)/test_associative_memory.exe

test-sequence-prediction: $(TEST_BIN_DIR)/test_sequence_prediction.exe
	$(TEST_BIN_DIR)/test_sequence_prediction.exe

test-c6-checkpoints: $(TEST_BIN_DIR)/test_c6_checkpoints.exe
	$(TEST_BIN_DIR)/test_c6_checkpoints.exe

test-c6-integration: $(TEST_BIN_DIR)/test_c6_integration.exe
	$(TEST_BIN_DIR)/test_c6_integration.exe

test-c6-long: $(TEST_BIN_DIR)/test_c6_long.exe
	$(TEST_BIN_DIR)/test_c6_long.exe

test-agent-io: $(TEST_BIN_DIR)/test_agent_io.exe
	$(TEST_BIN_DIR)/test_agent_io.exe

test-sensor-encoder: $(TEST_BIN_DIR)/test_sensor_encoder.exe $(BUILD_DIR)/sensor_encoding_demo.exe
	$(TEST_BIN_DIR)/test_sensor_encoder.exe
	$(PYTHON) tests/test_sensor_encoding_demo.py $(BUILD_DIR)/sensor_encoding_demo.exe

test-action-decoder: $(TEST_BIN_DIR)/test_action_decoder.exe $(BUILD_DIR)/action_decoding_demo.exe
	$(TEST_BIN_DIR)/test_action_decoder.exe
	$(PYTHON) tests/test_action_decoding_demo.py $(BUILD_DIR)/action_decoding_demo.exe

test-agent-cycle: $(TEST_BIN_DIR)/test_agent_cycle.exe $(BUILD_DIR)/agent_cycle_demo.exe
	$(TEST_BIN_DIR)/test_agent_cycle.exe
	$(PYTHON) tests/test_agent_cycle_demo.py $(BUILD_DIR)/agent_cycle_demo.exe

test-agent-cycle-checkpoint: $(TEST_BIN_DIR)/test_agent_cycle_checkpoint.exe $(BUILD_DIR)/agent_cycle_checkpoint_demo.exe
	$(TEST_BIN_DIR)/test_agent_cycle_checkpoint.exe
	$(PYTHON) tests/test_agent_cycle_checkpoint_demo.py $(BUILD_DIR)/agent_cycle_checkpoint_demo.exe

test-c7-integration: $(TEST_BIN_DIR)/test_c7_integration.exe
	$(TEST_BIN_DIR)/test_c7_integration.exe

test-c7-evolution: $(TEST_BIN_DIR)/test_c7_evolution.exe
	$(TEST_BIN_DIR)/test_c7_evolution.exe

test-c7-long-run: $(TEST_BIN_DIR)/test_c7_long_run.exe
	$(TEST_BIN_DIR)/test_c7_long_run.exe

test-c7: test-runtime test-agent-io test-sensor-encoder test-action-decoder test-agent-cycle test-agent-cycle-checkpoint test-c7-integration test-c7-evolution test-c7-long-run check-c7
	@echo C7 complete test matrix OK

scenario-sensor-encoding: $(BUILD_DIR)/sensor_encoding_demo.exe
	$(BUILD_DIR)/sensor_encoding_demo.exe configs/sensor_encoding_demo.ini

scenario-action-decoding: $(BUILD_DIR)/action_decoding_demo.exe
	$(BUILD_DIR)/action_decoding_demo.exe configs/action_decoding_demo.ini

scenario-agent-cycle: $(BUILD_DIR)/agent_cycle_demo.exe
	$(BUILD_DIR)/agent_cycle_demo.exe configs/agent_cycle_demo.ini

scenario-agent-cycle-checkpoint: $(BUILD_DIR)/agent_cycle_checkpoint_demo.exe
	$(BUILD_DIR)/agent_cycle_checkpoint_demo.exe configs/agent_cycle_checkpoint_demo.ini

scenario-c7-integrated-audit: $(BUILD_DIR)/c7_integrated_audit.exe
	$(BUILD_DIR)/c7_integrated_audit.exe configs/c7_integrated_audit.ini
	$(PYTHON) scripts/check_c7_integrated_audit.py $(BUILD_DIR)/c7_integrated_audit.exe

test-runner-topologies: $(TEST_BIN_DIR)/test_runner_topologies.exe
	$(TEST_BIN_DIR)/test_runner_topologies.exe

test-reproducibility: $(TEST_BIN_DIR)/test_reproducibility.exe
	$(TEST_BIN_DIR)/test_reproducibility.exe

test-regression: $(BUILD_DIR)/minisnn_runner.exe | $(BUILD_DIR)
	$(PYTHON) tests/test_regression_baseline.py

test-memory: $(TEST_BIN_DIR)/test_memory_stress.exe
	$(TEST_BIN_DIR)/test_memory_stress.exe

test-long: $(TEST_BIN_DIR)/test_long_run.exe
	$(TEST_BIN_DIR)/test_long_run.exe

test-d1-lifecycle-stress: $(TEST_BIN_DIR)/test_d1_lifecycle_stress.exe
	$(PYTHON) scripts/check_d1_lifecycle_stress.py $(TEST_BIN_DIR)/test_d1_lifecycle_stress.exe

test-d1-determinism: test-reproducibility test-structure-resume test-evolution-resume test-c6-integration test-agent-cycle-checkpoint $(TEST_BIN_DIR)/test_d1_determinism.exe
	$(PYTHON) scripts/check_d1_determinism.py $(TEST_BIN_DIR)/test_d1_determinism.exe

test-d1-corruption: $(TEST_BIN_DIR)/test_d1_corruption.exe
	$(PYTHON) scripts/check_d1_corruption.py $(TEST_BIN_DIR)/test_d1_corruption.exe

test-d1-optimization-determinism: | $(BUILD_DIR)
	$(PYTHON) scripts/check_d1_optimization_determinism.py

test-d1-posix-headless: | $(BUILD_DIR)
	$(PYTHON) scripts/check_d1_posix_headless.py

test-d1-portability: test-d1-determinism | $(BUILD_DIR)
	$(PYTHON) tests/test_d1_portability.py

test-d1-long-run: test-reward-long test-evolution-long test-structure-long test-c6-long test-c7-long-run $(BUILD_DIR)/d1_b_robustness_audit.exe
	$(BUILD_DIR)/d1_b_robustness_audit.exe configs/d1_b_robustness_audit.ini

test-d1-symbols: core-lib | $(BUILD_DIR)
	$(PYTHON) scripts/check_core_symbols.py

benchmark-d1: $(BUILD_DIR)/d1_b_benchmark.exe
	$(BUILD_DIR)/d1_b_benchmark.exe
	$(PYTHON) scripts/check_d1_benchmark.py

scenario-d1-b: test-d1-determinism test-d1-corruption test-d1-lifecycle-stress test-d1-optimization-determinism test-d1-posix-headless test-d1-portability test-d1-symbols benchmark-d1 test-d1-long-run
	$(PYTHON) scripts/run_portable_sanitizers.py
	$(PYTHON) scripts/generate_d1_b_report.py
	$(PYTHON) scripts/check_d1_b.py
	$(PYTHON) scripts/generate_d1_b_report.py
	@if exist build rmdir /S /Q build

audit-d1-b: scenario-d1-b
	$(PYTHON) scripts/check_d1_b.py
	$(PYTHON) scripts/generate_d1_b_report.py

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

test-runtime:
	@if not exist build mkdir build

test: test-runtime test-api test-core test-topology-factory test-lif test-neuron-model test-adex test-hodgkin-huxley test-model-error-propagation test-plasticity test-homeostasis test-reward test-structure test-structural-plasticity test-scenario test-runner test-runner-topologies test-topology-factory test-working-memory test-associative-memory test-sequence-prediction test-c6-checkpoints test-c6-integration test-agent-io test-sensor-encoder test-action-decoder test-agent-cycle test-agent-cycle-checkpoint test-reproducibility test-memory test-studio-runtime-layout
	@if exist build rmdir /S /Q build

$(BUILD_DIR)/example_api_single_neuron.exe: examples/api/example_api_single_neuron.c $(CORE_LIB) include/minisnn.h include/minisnn_types.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) examples/api/example_api_single_neuron.c $(CORE_LIB) $(INCLUDES) -o $@

$(BUILD_DIR)/example_api_chain.exe: examples/api/example_api_chain.c $(CORE_LIB) include/minisnn.h include/minisnn_types.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) examples/api/example_api_chain.c $(CORE_LIB) $(INCLUDES) -o $@

$(BUILD_DIR)/example_api_exc_inh.exe: examples/api/example_api_exc_inh.c $(CORE_LIB) include/minisnn.h include/minisnn_types.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) examples/api/example_api_exc_inh.c $(CORE_LIB) $(INCLUDES) -o $@

api-single: $(BUILD_DIR)/example_api_single_neuron.exe
	$(BUILD_DIR)/example_api_single_neuron.exe

api-chain: $(BUILD_DIR)/example_api_chain.exe
	$(BUILD_DIR)/example_api_chain.exe

api-exc-inh: $(BUILD_DIR)/example_api_exc_inh.exe
	$(BUILD_DIR)/example_api_exc_inh.exe

api-examples: api-single api-chain api-exc-inh

$(BUILD_DIR)/demo_main.exe: examples/internal/demo_main.c $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) examples/internal/demo_main.c $(CORE_LIB) $(INCLUDES) -o $@

demo: $(BUILD_DIR)/demo_main.exe
	$(BUILD_DIR)/demo_main.exe

$(BUILD_DIR)/minisnn_runner.exe: app/minisnn_runner.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) include/minisnn.h include/minisnn_types.h app/scenario_config.h app/scenario_runner.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) app/minisnn_runner.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) $(INCLUDES) -o $@

scenario: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe $(SCENARIO)

scenario-random: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/random.ini

scenario-small-world: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/small_world.ini

scenario-working-memory: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/working_memory_demo.ini

scenario-associative-memory: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/associative_memory_demo.ini

scenario-sequence-prediction: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/sequence_prediction_demo.ini

scenario-sequence-prediction-context: $(BUILD_DIR)/minisnn_runner.exe
	$(BUILD_DIR)/minisnn_runner.exe configs/sequence_prediction_context_demo.ini

scenario-c6-suite: $(BUILD_DIR)/c6_suite_runner.exe
	$(BUILD_DIR)/c6_suite_runner.exe

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

$(STUDIO_OBJ_DIR)/minisnn_studio.o: studio/minisnn_studio.c include/minisnn.h include/minisnn_types.h app/scenario_config.h app/scenario_runner.h app/evolution_config.h | $(STUDIO_OBJ_DIR)
	$(CC) $(CFLAGS) $(APP_INCLUDES) -Istudio -c $< -o $@

$(STUDIO_BIN_DIR)/minisnn_studio.exe: $(STUDIO_OBJ_DIR)/minisnn_studio.o studio/studio_runtime_layout.c studio/studio_runtime_layout.h studio/studio_product_launcher.c studio/studio_product_launcher.h app/evolution_config.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) | $(STUDIO_BIN_DIR)
	$(CC) $(CFLAGS) $(STUDIO_OBJ_DIR)/minisnn_studio.o studio/studio_runtime_layout.c studio/studio_product_launcher.c app/evolution_config.c $(SCENARIO_RUNNER_SOURCES) $(CORE_LIB) $(APP_INCLUDES) -Istudio -o $@ -mwindows -lcomdlg32 -lshell32 -lgdi32 -luser32 -lole32

studio-build: $(STUDIO_BIN_DIR)/minisnn_studio.exe

studio: $(STUDIO_BIN_DIR)/minisnn_studio.exe
	$(STUDIO_BIN_DIR)/minisnn_studio.exe

$(BUILD_DIR)/example_ei_balance.exe: experiments/example_ei_balance.c $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) experiments/example_ei_balance.c $(CORE_LIB) $(INCLUDES) -o $@

$(BUILD_DIR)/example_inhibition_fine_sweep.exe: experiments/example_inhibition_fine_sweep.c $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) experiments/example_inhibition_fine_sweep.c $(CORE_LIB) $(INCLUDES) -o $@

$(BUILD_DIR)/example_inh_to_inh_visual.exe: experiments/example_inh_to_inh_visual.c $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) experiments/example_inh_to_inh_visual.c $(CORE_LIB) $(INCLUDES) -o $@

$(BUILD_DIR)/example_sparse_ei_balance_analysis.exe: experiments/example_sparse_ei_balance_analysis.c $(CORE_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) experiments/example_sparse_ei_balance_analysis.c $(CORE_LIB) $(INCLUDES) -o $@

ei-balance: $(BUILD_DIR)/example_ei_balance.exe
	$(BUILD_DIR)/example_ei_balance.exe

inhibition-fine: $(BUILD_DIR)/example_inhibition_fine_sweep.exe
	$(BUILD_DIR)/example_inhibition_fine_sweep.exe

inh-to-inh: $(BUILD_DIR)/example_inh_to_inh_visual.exe
	$(BUILD_DIR)/example_inh_to_inh_visual.exe

sparse-ei: $(BUILD_DIR)/example_sparse_ei_balance_analysis.exe
	$(BUILD_DIR)/example_sparse_ei_balance_analysis.exe

clean-core:
	@if exist "$(BUILD_ROOT)/core" rmdir /S /Q "$(BUILD_ROOT)/core"

clean-studio:
	@if exist "$(BUILD_ROOT)/studio" rmdir /S /Q "$(BUILD_ROOT)/studio"

clean-tests:
	@if exist "$(BUILD_ROOT)/tests" rmdir /S /Q "$(BUILD_ROOT)/tests"

clean: clean-core clean-studio clean-tests
	@if exist "$(BUILD_ROOT)/tools" rmdir /S /Q "$(BUILD_ROOT)/tools"
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
