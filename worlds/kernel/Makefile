CC ?= gcc
AR ?= ar
PYTHON ?= python
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Wformat=2 -Wstrict-prototypes

REPOSITORY_ROOT := ../..
BUILD_ROOT := $(REPOSITORY_ROOT)/build/worlds/kernel
LIB_DIR := $(BUILD_ROOT)/lib
BIN_DIR := $(BUILD_ROOT)/bin
OBJ_DIR := $(BUILD_ROOT)/obj
TEST_DIR := $(BUILD_ROOT)/tests
PUBLIC_INCLUDE_DIR := include
LIB := $(LIB_DIR)/libminisnn_worlds_kernel.a
LIB_OBJECT := $(OBJ_DIR)/minisnn_worlds_kernel.o
DEMO := $(BIN_DIR)/k0_tick_demo.exe
K0B_DEMO := $(BIN_DIR)/k0_entity_command_demo.exe
K0C_DEMO := $(BIN_DIR)/k0_random_hash_demo.exe
K0D_RUNNER := $(BIN_DIR)/k0_scenario_runner.exe
TEST_EXE := $(TEST_DIR)/test_worlds_kernel.exe
K0B_ENTITY_TEST := $(TEST_DIR)/test_k0_b_entities.exe
K0B_COMMAND_TEST := $(TEST_DIR)/test_k0_b_commands.exe
K0B_EVENT_TEST := $(TEST_DIR)/test_k0_b_events.exe
K0B_DETERMINISM_TEST := $(TEST_DIR)/test_k0_b_determinism.exe
K0C_RANDOM_TEST := $(TEST_DIR)/test_k0_c_random.exe
K0C_HASH_TEST := $(TEST_DIR)/test_k0_c_hash.exe
K0C_OBSERVABILITY_TEST := $(TEST_DIR)/test_k0_c_observability.exe
K0C_DETERMINISM_TEST := $(TEST_DIR)/test_k0_c_determinism.exe
K0D_CONFIG_TEST := $(TEST_DIR)/test_k0_d_config.exe
K0D_ARTIFACT_TEST := $(TEST_DIR)/test_k0_d_artifacts.exe
K0D_ARTIFACT_VALIDATOR := $(TEST_DIR)/k0_d_artifact_validator.exe
K0_EXTERNAL_CONSUMER_TEST := $(TEST_DIR)/k0_external_consumer.exe
K1A_DEMO := $(BIN_DIR)/k1_transform_demo.exe
K1A_SPACE_TEST := $(TEST_DIR)/test_k1_a_space.exe
K1A_STRESS_TEST := $(TEST_DIR)/test_k1_a_stress.exe
K1A_OPTIMIZATION_SCRIPT := tests/test_k1_a_optimization_determinism.py
K1A_SANITIZER_SCRIPT := tests/test_k1_a_sanitize.py
K1A_POSIX_SMOKE_SCRIPT := tests/test_k1_a_posix_smoke.py
PUBLIC_HEADERS := $(wildcard $(PUBLIC_INCLUDE_DIR)/*.h)
SANITIZER_SCRIPT := tests/test_k0_a_sanitize.py
K0C_SANITIZER_SCRIPT := tests/test_k0_c_sanitize.py
K0C_OPTIMIZATION_SCRIPT := tests/test_k0_c_optimization_determinism.py
K0D_DETERMINISM_SCRIPT := tests/test_k0_d_determinism.py
K0D_CORRUPTION_SCRIPT := tests/test_k0_d_corruption.py
K0D_STRESS_SCRIPT := tests/test_k0_d_stress.py
K0D_LONG_RUN_SCRIPT := tests/test_k0_d_long_run.py
K0D_OPTIMIZATION_SCRIPT := tests/test_k0_d_optimization_determinism.py
K0D_SANITIZER_SCRIPT := tests/test_k0_d_sanitize.py
K0D_ARTIFACT_CORRUPTION_SCRIPT := tests/test_k0_d_artifact_corruption.py
K0D_POSIX_SMOKE_SCRIPT := tests/test_k0_d_posix_smoke.py
K0D_APP_SOURCES := app/k0_scenario_config.c app/k0_scenario_engine.c app/k0_scenario_artifacts.c

.PHONY: all lib demo demo-k0-b demo-k0-c demo-k0-d demo-k1-a test test-k1-a-scalar test-k1-a-space test-k1-a-transform test-k1-a-determinism test-k1-a-optimization-determinism test-k1-a-stress test-k1-a-sanitize test-k1-a-posix-smoke audit-k1-a test-k0-b-entities test-k0-b-commands test-k0-b-events test-k0-b-determinism test-k0-b-sanitize test-k0-c-random test-k0-c-hash test-k0-c-observability test-k0-c-determinism test-k0-c-optimization-determinism test-k0-c-sanitize test-k0-d-config test-k0-d-artifacts test-k0-d-determinism test-k0-d-optimization-determinism test-k0-d-corruption test-k0-d-stress test-k0-d-long-run test-k0-d-posix-smoke test-k0-d-sanitize test-k0-external-consumer sanitize audit-k0-a audit-k0-b audit-k0-c audit-k0-d audit-k0 clean

all: lib demo demo-k0-b demo-k0-c $(K0D_RUNNER) $(K1A_DEMO)

help:
	@echo Worlds Kernel targets:
	@echo   make lib          - compila a biblioteca estatica do Kernel
	@echo   make demo         - compila o demo headless de ticks
	@echo   make demo-k0-b    - compila e executa o demo de entidades e comandos
	@echo   make demo-k0-c    - compila e executa o demo de PRNG e hash
	@echo   make demo-k0-d    - executa o cenario integrado e produz artefatos tecnicos
	@echo   make test         - executa testes do Kernel
	@echo   make test-k0-b-entities - valida Entity IDs e registro
	@echo   make test-k0-b-commands - valida fila e ordenacao canonica
	@echo   make test-k0-b-events - valida eventos, conflitos e atomicidade
	@echo   make test-k0-b-determinism - valida resolucao deterministica
	@echo   make test-k0-b-sanitize - executa regressao ASan/UBSan K0-B
	@echo   make test-k0-c-random - valida PRNG, streams, limites e atomicidade
	@echo   make test-k0-c-hash - valida hash canonico e vetores de estado
	@echo   make test-k0-c-observability - valida diagnosticos, trace e inspecao
	@echo   make test-k0-c-determinism - valida matriz deterministica K0-C
	@echo   make test-k0-c-optimization-determinism - compara trajetoria -O0/-O2
	@echo   make test-k0-c-sanitize - executa ASan/UBSan K0-C
	@echo   make test-k0-d-config - valida parser e assinatura normalizada
	@echo   make test-k0-d-artifacts - valida CSV, manifesto, report e atomicidade
	@echo   make test-k0-d-determinism - compara processos e configuracoes equivalentes
	@echo   make test-k0-d-optimization-determinism - compara cenario -O0/-O2
	@echo   make test-k0-d-corruption - valida corpus de configuracoes invalidas
	@echo   make test-k0-d-stress - executa workload de stress e overwrite atomico
	@echo   make test-k0-d-long-run - executa 1000000 ticks reproduziveis
	@echo   make test-k0-d-posix-smoke - compila o app K0-D em alvo POSIX quando disponivel
	@echo   make test-k0-d-sanitize - executa ASan/UBSan do runner quando disponivel
	@echo   make test-k0-external-consumer - valida consumo externo por header e biblioteca
	@echo   make sanitize     - executa a regressao ASan/UBSan do Kernel
	@echo   make audit-k0-a   - valida arquitetura, headers, simbolos e demo
	@echo   make audit-k0-b   - valida K0-A e K0-B de forma integrada
	@echo   make audit-k0-c   - valida K0-A, K0-B e K0-C de forma integrada
	@echo   make audit-k0-d   - valida cenario, artefatos, stress e consumer K0-D
	@echo   make audit-k0     - fecha a fundacao deterministica K0
	@echo   make clean        - limpa somente build/worlds/kernel

lib: $(LIB)

demo: $(DEMO)

$(OBJ_DIR) $(LIB_DIR) $(BIN_DIR) $(TEST_DIR):
	@if not exist "$@" mkdir "$@"

$(LIB_OBJECT): src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) -c $< -o $@

$(LIB): $(LIB_OBJECT) | $(LIB_DIR)
	$(AR) rcs $@ $<

$(DEMO): app/k0_tick_demo.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) $< $(LIB) -o $@

$(K0B_DEMO): app/k0_entity_command_demo.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) $< $(LIB) -o $@

$(K0C_DEMO): app/k0_random_hash_demo.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) $< $(LIB) -o $@

$(K1A_DEMO): app/k1_transform_demo.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) $< $(LIB) -o $@

$(K0D_RUNNER): app/k0_scenario_runner.c $(K0D_APP_SOURCES) $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) -Iapp $< $(K0D_APP_SOURCES) $(LIB) -o $@

$(TEST_EXE): tests/test_worlds_kernel.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(K0B_ENTITY_TEST): tests/test_k0_b_entities.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(K0B_COMMAND_TEST): tests/test_k0_b_commands.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(K0B_EVENT_TEST): tests/test_k0_b_events.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(K0B_DETERMINISM_TEST): tests/test_k0_b_determinism.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(K0C_RANDOM_TEST): tests/test_k0_c_random.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(K0C_HASH_TEST): tests/test_k0_c_hash.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(K0C_OBSERVABILITY_TEST): tests/test_k0_c_observability.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(K0C_DETERMINISM_TEST): tests/test_k0_c_determinism.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(K0D_CONFIG_TEST): tests/test_k0_d_config.c app/k0_scenario_config.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) -Iapp $< app/k0_scenario_config.c -o $@

$(K0D_ARTIFACT_TEST): tests/test_k0_d_artifacts.c $(K0D_APP_SOURCES) $(LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) -Iapp $< $(K0D_APP_SOURCES) $(LIB) -o $@

$(K0D_ARTIFACT_VALIDATOR): tests/k0_d_artifact_validator.c $(K0D_APP_SOURCES) $(LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) -Iapp $< $(K0D_APP_SOURCES) $(LIB) -o $@

$(K1A_SPACE_TEST): tests/test_k1_a_space.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(K1A_STRESS_TEST): tests/test_k1_a_stress.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -O2 -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(K0_EXTERNAL_CONSUMER_TEST): tests/k0_external_consumer.c $(LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) $< $(LIB) -o $@

test: $(LIB) $(K0D_RUNNER) $(K1A_DEMO) $(K1A_SPACE_TEST) $(K1A_STRESS_TEST) $(TEST_EXE) $(K0B_ENTITY_TEST) $(K0B_COMMAND_TEST) $(K0B_EVENT_TEST) $(K0B_DETERMINISM_TEST) $(K0C_RANDOM_TEST) $(K0C_HASH_TEST) $(K0C_OBSERVABILITY_TEST) $(K0C_DETERMINISM_TEST) $(K0D_CONFIG_TEST) $(K0D_ARTIFACT_TEST) $(K0_EXTERNAL_CONSUMER_TEST)
	$(PYTHON) tests/test_public_headers.py "$(CC)" "$(CFLAGS)" "$(PUBLIC_INCLUDE_DIR)" "$(TEST_DIR)"
	$(TEST_EXE)
	$(K0B_ENTITY_TEST)
	$(K0B_COMMAND_TEST)
	$(K0B_EVENT_TEST)
	$(K0B_DETERMINISM_TEST)
	$(K0C_RANDOM_TEST)
	$(K0C_HASH_TEST)
	$(K0C_OBSERVABILITY_TEST)
	$(K0C_DETERMINISM_TEST)
	$(K0D_CONFIG_TEST)
	$(K0D_ARTIFACT_TEST)
	$(K0_EXTERNAL_CONSUMER_TEST)
	$(K1A_SPACE_TEST)
	$(K1A_STRESS_TEST)

test-k0-b-entities: $(K0B_ENTITY_TEST)
	$(K0B_ENTITY_TEST)

test-k0-b-commands: $(K0B_COMMAND_TEST)
	$(K0B_COMMAND_TEST)

test-k0-b-events: $(K0B_EVENT_TEST)
	$(K0B_EVENT_TEST)

test-k0-b-determinism: $(K0B_DETERMINISM_TEST)
	$(K0B_DETERMINISM_TEST)

demo-k0-b: $(K0B_DEMO)
	$(K0B_DEMO)

demo-k0-c: $(K0C_DEMO)
	$(K0C_DEMO)

demo-k0-d: $(K0D_RUNNER)
	$(K0D_RUNNER) --config configs/k0_integrated_demo.ini --output $(BUILD_ROOT)/demo/k0_d --overwrite

demo-k1-a: $(K1A_DEMO)
	@if not exist "$(BUILD_ROOT)/demo/k1_a" mkdir "$(BUILD_ROOT)/demo/k1_a"
	$(K1A_DEMO) configs/k1_transform_demo.ini $(BUILD_ROOT)/demo/k1_a

test-k1-a-scalar test-k1-a-space test-k1-a-transform test-k1-a-determinism: $(K1A_SPACE_TEST)
	$(K1A_SPACE_TEST)

test-k1-a-stress: $(K1A_STRESS_TEST)
	$(K1A_STRESS_TEST)

test-k1-a-optimization-determinism: | $(TEST_DIR)
	$(PYTHON) $(K1A_OPTIMIZATION_SCRIPT) --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --test "tests/test_k1_a_space.c" --output-dir "$(BUILD_ROOT)/audit/k1_a"

test-k1-a-sanitize:
	$(PYTHON) $(K1A_SANITIZER_SCRIPT) --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --test "tests/test_k1_a_space.c" --output-dir "$(TEST_DIR)"

test-k1-a-posix-smoke:
	$(PYTHON) $(K1A_POSIX_SMOKE_SCRIPT) --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --test "tests/test_k1_a_space.c" --output-dir "$(TEST_DIR)"

test-k0-c-random: $(K0C_RANDOM_TEST)
	$(K0C_RANDOM_TEST)

test-k0-c-hash: $(K0C_HASH_TEST)
	$(K0C_HASH_TEST)

test-k0-c-observability: $(K0C_OBSERVABILITY_TEST)
	$(K0C_OBSERVABILITY_TEST)

test-k0-c-determinism: $(K0C_DETERMINISM_TEST)
	$(K0C_DETERMINISM_TEST)

test-k0-c-optimization-determinism: | $(TEST_DIR)
	$(PYTHON) $(K0C_OPTIMIZATION_SCRIPT) --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --runner "tests/k0_c_optimization_runner.c" --output-dir "$(BUILD_ROOT)/audit"

test-k0-c-sanitize:
	$(PYTHON) $(K0C_SANITIZER_SCRIPT) --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --output-dir "$(TEST_DIR)" --demo "app/k0_random_hash_demo.c" --tests "tests/test_k0_c_random.c" "tests/test_k0_c_hash.c" "tests/test_k0_c_observability.c" "tests/test_k0_c_determinism.c"

test-k0-d-config: $(K0D_CONFIG_TEST)
	$(K0D_CONFIG_TEST)

test-k0-d-artifacts: $(K0D_ARTIFACT_TEST) $(K0D_ARTIFACT_VALIDATOR) $(K0D_RUNNER)
	$(K0D_ARTIFACT_TEST)
	$(PYTHON) $(K0D_ARTIFACT_CORRUPTION_SCRIPT) --runner "$(K0D_RUNNER)" --validator "$(K0D_ARTIFACT_VALIDATOR)" --config "configs/k0_integrated_demo.ini" --output-dir "$(BUILD_ROOT)/tests/k0_d_artifact_corruption"

test-k0-d-determinism: $(K0D_RUNNER)
	$(PYTHON) $(K0D_DETERMINISM_SCRIPT) --runner "$(K0D_RUNNER)" --config "configs/k0_integrated_demo.ini" --output-dir "$(BUILD_ROOT)/tests/k0_d_determinism" --repository-root "$(REPOSITORY_ROOT)"

test-k0-d-optimization-determinism:
	$(PYTHON) $(K0D_OPTIMIZATION_SCRIPT) --compiler "$(CC)" --kernel-root "." --config "configs/k0_integrated_demo.ini" --output-dir "$(BUILD_ROOT)/audit/k0_d"

test-k0-d-corruption: $(K0D_RUNNER)
	$(PYTHON) $(K0D_CORRUPTION_SCRIPT) --runner "$(K0D_RUNNER)" --config "configs/k0_integrated_demo.ini" --output-dir "$(BUILD_ROOT)/tests/k0_d_corruption"

test-k0-d-stress: $(K0D_RUNNER)
	$(PYTHON) $(K0D_STRESS_SCRIPT) --runner "$(K0D_RUNNER)" --config "configs/k0_stress.ini" --output-dir "$(BUILD_ROOT)/tests/k0_d_stress"

test-k0-d-long-run: $(K0D_RUNNER)
	$(PYTHON) $(K0D_LONG_RUN_SCRIPT) --runner "$(K0D_RUNNER)" --config "configs/k0_long_run.ini" --output-dir "$(BUILD_ROOT)/tests/k0_d_long_run"

test-k0-d-posix-smoke:
	$(PYTHON) $(K0D_POSIX_SMOKE_SCRIPT) --compiler "$(CC)" --kernel-root "." --config "configs/k0_integrated_demo.ini" --output-dir "$(BUILD_ROOT)/tests/k0_d_posix_smoke"

test-k0-d-sanitize:
	$(PYTHON) $(K0D_SANITIZER_SCRIPT) --compiler "$(CC)" --kernel-root "." --config "configs/k0_integrated_demo.ini" --output-dir "$(BUILD_ROOT)/tests/k0_d_sanitize"

test-k0-external-consumer: $(K0_EXTERNAL_CONSUMER_TEST)
	$(K0_EXTERNAL_CONSUMER_TEST)
	$(K1A_SPACE_TEST)
	$(K1A_STRESS_TEST)

sanitize: | $(TEST_DIR)
	$(PYTHON) $(SANITIZER_SCRIPT) --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --test "tests/test_worlds_kernel.c" --output-dir "$(TEST_DIR)"

audit-k0-a: lib demo test
	$(PYTHON) scripts/check_k0_a.py --root . --library "$(LIB)" --demo "$(DEMO)" --test "$(TEST_EXE)"

test-k0-b-sanitize:
	$(PYTHON) tests/test_k0_b_sanitize.py --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --output-dir "$(TEST_DIR)" --demo "app/k0_entity_command_demo.c" --tests "tests/test_k0_b_entities.c" "tests/test_k0_b_commands.c" "tests/test_k0_b_events.c" "tests/test_k0_b_determinism.c"

audit-k0-b: audit-k0-a test-k0-b-entities test-k0-b-commands test-k0-b-events test-k0-b-determinism test-k0-b-sanitize demo-k0-b
	$(PYTHON) scripts/check_k0_b.py --root . --library "$(LIB)" --demo "$(K0B_DEMO)" --entity-test "$(K0B_ENTITY_TEST)" --command-test "$(K0B_COMMAND_TEST)" --event-test "$(K0B_EVENT_TEST)" --determinism-test "$(K0B_DETERMINISM_TEST)"

audit-k0-c: audit-k0-b test-k0-c-random test-k0-c-hash test-k0-c-observability test-k0-c-determinism test-k0-c-optimization-determinism test-k0-c-sanitize demo-k0-c
	$(PYTHON) scripts/check_k0_c.py --root . --library "$(LIB)" --demo "$(K0C_DEMO)" --random-test "$(K0C_RANDOM_TEST)" --hash-test "$(K0C_HASH_TEST)" --observability-test "$(K0C_OBSERVABILITY_TEST)" --determinism-test "$(K0C_DETERMINISM_TEST)"

audit-k0-d: $(K0D_ARTIFACT_VALIDATOR) audit-k0-c demo-k0-d test-k0-d-config test-k0-d-artifacts test-k0-d-determinism test-k0-d-optimization-determinism test-k0-d-corruption test-k0-d-stress test-k0-d-long-run test-k0-d-posix-smoke test-k0-d-sanitize test-k0-external-consumer
	$(PYTHON) scripts/check_k0_d.py --root . --library "$(LIB)" --runner "$(K0D_RUNNER)" --validator "$(K0D_ARTIFACT_VALIDATOR)"

audit-k0: audit-k0-d
	$(PYTHON) scripts/check_k0.py --root .

audit-k1-a: audit-k0 test-k1-a-scalar test-k1-a-space test-k1-a-transform test-k1-a-determinism test-k1-a-optimization-determinism test-k1-a-stress test-k1-a-sanitize test-k1-a-posix-smoke demo-k1-a
	$(PYTHON) scripts/check_k1_a.py --root . --library "$(LIB)" --demo "$(K1A_DEMO)" --space-test "$(K1A_SPACE_TEST)" --stress-test "$(K1A_STRESS_TEST)"
	@echo K1-A Worlds Kernel fixed-point space and transform validation OK

clean:
	@if exist "$(BUILD_ROOT)" rmdir /S /Q "$(BUILD_ROOT)"
