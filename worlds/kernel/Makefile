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
SNAPSHOT_OBJECT := $(OBJ_DIR)/minisnn_worlds_kernel_snapshot.o
RESTORE_OBJECT := $(OBJ_DIR)/minisnn_worlds_kernel_restore.o
COMMAND_LOG_OBJECT := $(OBJ_DIR)/minisnn_worlds_kernel_command_log.o
LIB_OBJECTS := $(LIB_OBJECT) $(SNAPSHOT_OBJECT) $(RESTORE_OBJECT) $(COMMAND_LOG_OBJECT)
KERNEL_SOURCES := src/minisnn_worlds_kernel.c src/minisnn_worlds_kernel_snapshot.c src/minisnn_worlds_kernel_restore.c src/minisnn_worlds_kernel_command_log.c
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
KERNEL_COMMAND_BATCH_TEST := $(TEST_DIR)/test_kernel_command_batch.exe
K1A_DEMO := $(BIN_DIR)/k1_transform_demo.exe
K1A_SPACE_TEST := $(TEST_DIR)/test_k1_a_space.exe
K1A_STRESS_TEST := $(TEST_DIR)/test_k1_a_stress.exe
K1A_OPTIMIZATION_SCRIPT := tests/test_k1_a_optimization_determinism.py
K1A_SANITIZER_SCRIPT := tests/test_k1_a_sanitize.py
K1A_POSIX_SMOKE_SCRIPT := tests/test_k1_a_posix_smoke.py
K1B1_DEMO := $(BIN_DIR)/k1_occupancy_demo.exe
K1B1_OCCUPANCY_TEST := $(TEST_DIR)/test_k1_b1_occupancy.exe
K1B1_STRESS_TEST := $(TEST_DIR)/test_k1_b1_stress.exe
K1B1_OPTIMIZATION_SCRIPT := tests/test_k1_b1_optimization_determinism.py
K1B1_SANITIZER_SCRIPT := tests/test_k1_b1_sanitize.py
K1B1_SANITIZER_CLASSIFICATION_SCRIPT := tests/test_k1_b1_sanitizer_classification.py
K1B1_POSIX_SMOKE_SCRIPT := tests/test_k1_b1_posix_smoke.py
K1B2_DEMO := $(BIN_DIR)/k1_movement_demo.exe
K1B2_MOVEMENT_TEST := $(TEST_DIR)/test_k1_b2_movement.exe
K1B2_STRESS_TEST := $(TEST_DIR)/test_k1_b2_stress.exe
K1B2_OPTIMIZATION_SCRIPT := tests/test_k1_b2_optimization_determinism.py
K1B2_SANITIZER_SCRIPT := tests/test_k1_b2_sanitize.py
K1B2_SANITIZER_CLASSIFICATION_SCRIPT := tests/test_k1_b2_sanitizer_classification.py
K1B2_POSIX_SMOKE_SCRIPT := tests/test_k1_b2_posix_smoke.py
K1C1_SPATIAL_TEST := $(TEST_DIR)/test_k1_c1_spatial_links.exe
K1C1_OPTIMIZATION_SCRIPT := tests/test_k1_c1_optimization_determinism.py
K1C1_SANITIZER_SCRIPT := tests/test_k1_c1_sanitize.py
K1C1_SANITIZER_CLASSIFICATION_SCRIPT := tests/test_k1_c1_sanitizer_classification.py
K1C1_POSIX_SMOKE_SCRIPT := tests/test_k1_c1_posix_smoke.py
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

.PHONY: all lib demo demo-k0-b demo-k0-c demo-k0-d demo-k1-a demo-k1-b1 demo-k1-b2 test test-k1-a-scalar test-k1-a-space test-k1-a-transform test-k1-a-determinism test-k1-a-optimization-determinism test-k1-a-stress test-k1-a-sanitize test-k1-a-posix-smoke audit-k1-a audit-k1-b1 audit-k1-b2 test-k1-b1-occupancy test-k1-b1-overlap test-k1-b1-conflicts test-k1-b1-determinism test-k1-b1-optimization-determinism test-k1-b1-stress test-k1-b1-sanitizer-classification test-k1-b1-sanitize test-k1-b1-posix-smoke test-k1-b2-movement test-k1-b2-determinism test-k1-b2-optimization-determinism test-k1-b2-stress test-k1-b2-sanitizer-classification test-k1-b2-sanitize test-k1-b2-posix-smoke test-k1-c1-spatial-links test-k1-c1-hash test-k1-c1-determinism test-k1-c1-optimization-determinism test-k1-c1-sanitizer-classification test-k1-c1-sanitize test-k1-c1-posix-smoke audit-k1-c1 test-k0-b-entities test-k0-b-commands test-k0-b-events test-k0-b-determinism test-k0-b-sanitize test-k0-c-random test-k0-c-hash test-k0-c-observability test-k0-c-determinism test-k0-c-optimization-determinism test-k0-c-sanitize test-k0-d-config test-k0-d-artifacts test-k0-d-determinism test-k0-d-optimization-determinism test-k0-d-corruption test-k0-d-stress test-k0-d-long-run test-k0-d-posix-smoke test-k0-d-sanitize test-k0-external-consumer test-kernel-command-batch sanitize audit-k0-a audit-k0-b audit-k0-c audit-k0-d audit-k0 clean

all: lib demo demo-k0-b demo-k0-c $(K0D_RUNNER) $(K1A_DEMO) $(K1B1_DEMO) $(K1B2_DEMO)

help:
	@echo   make test-kernel-command-batch       - valida rollback provisorio de comandos
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
	@echo   make demo-k1-a    - executa o demo espacial K1-A
	@echo   make audit-k1-a   - audita a fundacao espacial K1-A
	@echo   make test-k1-b1-occupancy - valida AABB, mascaras e ciclo de vida K1-B1
	@echo   make test-k1-b1-overlap - valida contato e sobreposicao AABB K1-B1
	@echo   make test-k1-b1-conflicts - valida conflitos e related_entity K1-B1
	@echo   make test-k1-b1-determinism - valida determinismo K1-B1
	@echo   make test-k1-b1-optimization-determinism - compara K1-B1 em -O0/-O2
	@echo   make test-k1-b1-stress - executa stress deterministico K1-B1
	@echo   make test-k1-b1-sanitizer-classification - valida classificacao sanitizer K1-B1
	@echo   make test-k1-b1-sanitize - executa ASan/UBSan K1-B1 quando disponivel
	@echo   make test-k1-b1-posix-smoke - executa smoke POSIX K1-B1 quando disponivel
	@echo   make demo-k1-b1  - executa o demo de ocupacao e barreiras K1-B1
	@echo   make audit-k1-b1 - audita K0, K1-A e K1-B1
	@echo   make test-k1-b2-movement - valida deslocamento atomico K1-B2
	@echo   make test-k1-b2-determinism - repete a trajetoria K1-B2
	@echo   make test-k1-b2-optimization-determinism - compara K1-B2 em -O0/-O2
	@echo   make test-k1-b2-stress - executa stress de movimentos e conflitos K1-B2
	@echo   make test-k1-b2-sanitizer-classification - valida classificacao sanitizer K1-B2
	@echo   make test-k1-b2-sanitize - executa ASan/UBSan K1-B2 quando disponivel
	@echo   make test-k1-b2-posix-smoke - executa smoke POSIX K1-B2 quando disponivel
	@echo   make demo-k1-b2 - executa o demo de movimento K1-B2
	@echo   make audit-k1-b2 - audita K0, K1-A, K1-B1 e K1-B2
	@echo   make test-k1-c1-spatial-links - valida floresta, lifecycle e atomicidade K1-C1
	@echo   make test-k1-c1-hash - valida state hash V5 K1-C1
	@echo   make test-k1-c1-determinism - repete a trajetoria K1-C1
	@echo   make test-k1-c1-optimization-determinism - compara K1-C1 em -O0/-O2
	@echo   make test-k1-c1-sanitizer-classification - valida classificacao sanitizer K1-C1
	@echo   make test-k1-c1-sanitize - executa ASan/UBSan K1-C1 quando disponivel
	@echo   make test-k1-c1-posix-smoke - executa smoke POSIX K1-C1 quando disponivel
	@echo   make audit-k1-c1 - audita links espaciais e V5 K1-C1
	@echo   make test-k1-c2-subtree-movement - valida translacao rigida e eventos C2
	@echo   make audit-k1-c2 - audita translacao de subarvore e V5 C2
	@echo   make test-k1-c3-ordering - valida a matriz canonica de ordenacao C3
	@echo   make test-k1-c3-invariants - valida invariantes planejadas e oficiais C3
	@echo   make test-k1-c3-limits - valida limites de IDs, contadores e capacidades C3
	@echo   make test-k1-c3-failure-atomicity - valida atomicidade sob falha interna C3
	@echo   make test-k1-c3-long-run - executa churn deterministico C3
	@echo   make test-k1-c3-hash - valida V5 com auditoria de invariantes C3
	@echo   make test-k1-c3-determinism - repete o long run C3
	@echo   make test-k1-c3-optimization-determinism - compara C3 em -O0/-O2
	@echo   make test-k1-c3-sanitizer-classification - valida classificacao sanitizer C3
	@echo   make test-k1-c3-sanitize - executa ASan/UBSan C3 quando disponivel
	@echo   make test-k1-c3-posix-smoke - executa smoke POSIX C3 quando disponivel
	@echo   make audit-k1-c3 - audita ordering e hardening C3
	@echo   make audit-k0-d   - valida cenario, artefatos, stress e consumer K0-D
	@echo   make audit-k0     - fecha a fundacao deterministica K0
	@echo   make test-k2-a-snapshot - valida o snapshot canonico V1
	@echo   make test-k2-a-long-run - compara snapshots em ticks 0/100/500/1000
	@echo   make audit-k2-a - audita K1 e o snapshot canonico K2-A
	@echo   make demo-k2-b - executa save/load e restore do snapshot V1
	@echo   make check-k2-b - valida artefatos do demo de persistencia V1
	@echo   make audit-k2-b - audita importacao, restore e save/load K2-B
	@echo   make demo-k2-d - executa A/B/C de persistencia e replay
	@echo   make test-k2-d-persistence - valida 1000 ticks e checkpoints multiplos
	@echo   make check-k2-d - valida golden, binding de estado e artefatos
	@echo   make audit-k2-d - fecha K2-A/B/C/D
	@echo   make audit-k2 - executa a auditoria autoritativa completa de K2
	@echo   make clean        - limpa somente build/worlds/kernel

lib: $(LIB)

demo: $(DEMO)

$(OBJ_DIR) $(LIB_DIR) $(BIN_DIR) $(TEST_DIR):
	@if not exist "$@" mkdir "$@"

$(LIB_OBJECT): src/minisnn_worlds_kernel.c src/minisnn_worlds_kernel_internal.h $(PUBLIC_HEADERS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) -c $< -o $@

$(SNAPSHOT_OBJECT): src/minisnn_worlds_kernel_snapshot.c src/minisnn_worlds_kernel_internal.h $(PUBLIC_HEADERS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) -c $< -o $@

$(RESTORE_OBJECT): src/minisnn_worlds_kernel_restore.c src/minisnn_worlds_kernel_internal.h $(PUBLIC_HEADERS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) -c $< -o $@
$(COMMAND_LOG_OBJECT): src/minisnn_worlds_kernel_command_log.c src/minisnn_worlds_kernel_internal.h $(PUBLIC_HEADERS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) -c $< -o $@

$(LIB): $(LIB_OBJECTS) | $(LIB_DIR)
	$(AR) rcs $@ $^

$(DEMO): app/k0_tick_demo.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) $< $(LIB) -o $@

$(K0B_DEMO): app/k0_entity_command_demo.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) $< $(LIB) -o $@

$(K0C_DEMO): app/k0_random_hash_demo.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) $< $(LIB) -o $@

$(K1A_DEMO): app/k1_transform_demo.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) $< $(LIB) -o $@

$(K1B1_DEMO): app/k1_occupancy_demo.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) $< $(LIB) -o $@

$(K1B2_DEMO): app/k1_movement_demo.c $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) $< $(LIB) -o $@

$(K0D_RUNNER): app/k0_scenario_runner.c $(K0D_APP_SOURCES) $(LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) -Iapp $< $(K0D_APP_SOURCES) $(LIB) -o $@

$(TEST_EXE): tests/test_worlds_kernel.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(KERNEL_COMMAND_BATCH_TEST): tests/test_kernel_command_batch.c $(KERNEL_SOURCES) $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c src/minisnn_worlds_kernel_snapshot.c src/minisnn_worlds_kernel_command_log.c -o $@

test-kernel-command-batch: $(KERNEL_COMMAND_BATCH_TEST)
	$(KERNEL_COMMAND_BATCH_TEST)

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

$(K1B1_OCCUPANCY_TEST): tests/test_k1_b1_occupancy.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(K1B1_STRESS_TEST): tests/test_k1_b1_stress.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -O2 -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(K1B2_MOVEMENT_TEST): tests/test_k1_b2_movement.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(K1B2_STRESS_TEST): tests/test_k1_b2_stress.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -O2 -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(K1C1_SPATIAL_TEST): tests/test_k1_c1_spatial_links.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(K0_EXTERNAL_CONSUMER_TEST): tests/k0_external_consumer.c $(LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) $< $(LIB) -o $@

test: $(LIB) $(K0D_RUNNER) $(K1A_DEMO) $(K1B1_DEMO) $(K1B2_DEMO) $(K1A_SPACE_TEST) $(K1A_STRESS_TEST) $(K1B1_OCCUPANCY_TEST) $(K1B1_STRESS_TEST) $(K1B2_MOVEMENT_TEST) $(K1B2_STRESS_TEST) $(K1C1_SPATIAL_TEST) $(TEST_EXE) $(K0B_ENTITY_TEST) $(K0B_COMMAND_TEST) $(K0B_EVENT_TEST) $(K0B_DETERMINISM_TEST) $(K0C_RANDOM_TEST) $(K0C_HASH_TEST) $(K0C_OBSERVABILITY_TEST) $(K0C_DETERMINISM_TEST) $(K0D_CONFIG_TEST) $(K0D_ARTIFACT_TEST) $(K0_EXTERNAL_CONSUMER_TEST) $(K2B_RESTORE_TEST) $(K2B_CHECKPOINT_TEST) $(K2B_FILE_TEST)
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
	$(K1B1_OCCUPANCY_TEST)
	$(K1B1_STRESS_TEST)
	$(K1B2_MOVEMENT_TEST)
	$(K1B2_STRESS_TEST)
	$(K1C1_SPATIAL_TEST)
	$(K2A_SNAPSHOT_TEST)
	$(K2A_LONG_RUN_TEST)
	$(K2B_RESTORE_TEST)
	$(K2B_CHECKPOINT_TEST)
	$(K2B_FILE_TEST)
	$(K2C_COMMAND_LOG_TEST)
	$(K2C_LONG_RUN_TEST)
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
	$(PYTHON) -B $(K1A_SANITIZER_SCRIPT) --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --test "tests/test_k1_a_space.c" --output-dir "$(TEST_DIR)"

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

demo-k1-b1: $(K1B1_DEMO)
	@if not exist "$(BUILD_ROOT)/demo/k1_b1" mkdir "$(BUILD_ROOT)/demo/k1_b1"
	$(K1B1_DEMO) configs/k1_occupancy_demo.ini $(BUILD_ROOT)/demo/k1_b1

test-k1-b1-occupancy test-k1-b1-overlap test-k1-b1-conflicts test-k1-b1-determinism: $(K1B1_OCCUPANCY_TEST)
	$(K1B1_OCCUPANCY_TEST)

test-k1-b1-stress: $(K1B1_STRESS_TEST)
	$(K1B1_STRESS_TEST)

test-k1-b1-optimization-determinism: | $(TEST_DIR)
	$(PYTHON) $(K1B1_OPTIMIZATION_SCRIPT) --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --test "tests/test_k1_b1_occupancy.c" --output-dir "$(BUILD_ROOT)/audit/k1_b1"

test-k1-b1-sanitizer-classification:
	$(PYTHON) -B $(K1B1_SANITIZER_CLASSIFICATION_SCRIPT)

test-k1-b1-sanitize: test-k1-b1-sanitizer-classification
	$(PYTHON) -B $(K1B1_SANITIZER_SCRIPT) --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --occupancy-test "tests/test_k1_b1_occupancy.c" --stress-test "tests/test_k1_b1_stress.c" --demo "app/k1_occupancy_demo.c" --config "configs/k1_occupancy_demo.ini" --output-dir "$(TEST_DIR)"

test-k1-b1-posix-smoke:
	$(PYTHON) $(K1B1_POSIX_SMOKE_SCRIPT) --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --test "tests/test_k1_b1_occupancy.c" --output-dir "$(TEST_DIR)"

audit-k1-b1: audit-k1-a test-k1-b1-occupancy test-k1-b1-overlap test-k1-b1-conflicts test-k1-b1-determinism test-k1-b1-optimization-determinism test-k1-b1-stress test-k1-b1-sanitizer-classification test-k1-b1-sanitize test-k1-b1-posix-smoke demo-k1-b1
	$(PYTHON) scripts/check_k1_b1.py --root . --library "$(LIB)" --demo "$(K1B1_DEMO)" --occupancy-test "$(K1B1_OCCUPANCY_TEST)" --stress-test "$(K1B1_STRESS_TEST)"
	@echo K1-B1 Worlds Kernel occupancy and generic barrier validation OK

demo-k1-b2: $(K1B2_DEMO)
	@if not exist "$(BUILD_ROOT)/demo/k1_b2" mkdir "$(BUILD_ROOT)/demo/k1_b2"
	$(K1B2_DEMO) configs/k1_movement_demo.ini $(BUILD_ROOT)/demo/k1_b2

test-k1-b2-movement test-k1-b2-determinism: $(K1B2_MOVEMENT_TEST)
	$(K1B2_MOVEMENT_TEST)

test-k1-b2-stress: $(K1B2_STRESS_TEST)
	$(K1B2_STRESS_TEST)

test-k1-b2-optimization-determinism: | $(TEST_DIR)
	$(PYTHON) $(K1B2_OPTIMIZATION_SCRIPT) --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --test "tests/test_k1_b2_movement.c" --output-dir "$(BUILD_ROOT)/audit/k1_b2"

test-k1-b2-sanitizer-classification:
	$(PYTHON) -B $(K1B2_SANITIZER_CLASSIFICATION_SCRIPT)

test-k1-b2-sanitize: test-k1-b2-sanitizer-classification
	$(PYTHON) -B $(K1B2_SANITIZER_SCRIPT) --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --movement-test "tests/test_k1_b2_movement.c" --stress-test "tests/test_k1_b2_stress.c" --demo "app/k1_movement_demo.c" --config "configs/k1_movement_demo.ini" --output-dir "$(TEST_DIR)"

test-k1-b2-posix-smoke:
	$(PYTHON) $(K1B2_POSIX_SMOKE_SCRIPT) --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --test "tests/test_k1_b2_movement.c" --output-dir "$(TEST_DIR)"

audit-k1-b2: audit-k1-b1 test-k1-b2-movement test-k1-b2-determinism test-k1-b2-optimization-determinism test-k1-b2-stress test-k1-b2-sanitizer-classification test-k1-b2-sanitize test-k1-b2-posix-smoke demo-k1-b2
	$(PYTHON) scripts/check_k1_b2.py --root . --library "$(LIB)" --demo "$(K1B2_DEMO)" --movement-test "$(K1B2_MOVEMENT_TEST)" --stress-test "$(K1B2_STRESS_TEST)"
	@echo K1-B2 Worlds Kernel atomic movement validation OK

test-k1-c1-spatial-links test-k1-c1-hash test-k1-c1-determinism: $(K1C1_SPATIAL_TEST)
	$(K1C1_SPATIAL_TEST)

test-k1-c1-optimization-determinism: | $(TEST_DIR)
	$(PYTHON) $(K1C1_OPTIMIZATION_SCRIPT) --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --test "tests/test_k1_c1_spatial_links.c" --output-dir "$(BUILD_ROOT)/audit/k1_c1"

test-k1-c1-sanitizer-classification:
	$(PYTHON) -B $(K1C1_SANITIZER_CLASSIFICATION_SCRIPT)

test-k1-c1-sanitize: test-k1-c1-sanitizer-classification
	$(PYTHON) -B $(K1C1_SANITIZER_SCRIPT) --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --test "tests/test_k1_c1_spatial_links.c" --output-dir "$(TEST_DIR)"

test-k1-c1-posix-smoke:
	$(PYTHON) $(K1C1_POSIX_SMOKE_SCRIPT) --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --test "tests/test_k1_c1_spatial_links.c" --output-dir "$(TEST_DIR)"

audit-k1-c1: audit-k1-b2 test-k1-c1-spatial-links test-k1-c1-hash test-k1-c1-determinism test-k1-c1-optimization-determinism test-k1-c1-sanitizer-classification test-k1-c1-sanitize test-k1-c1-posix-smoke
	$(PYTHON) scripts/check_k1_c1.py --root . --library "$(LIB)" --spatial-test "$(K1C1_SPATIAL_TEST)"
	@echo K1-C1 spatial links and state hash V5 validation OK
clean:
	@if exist "$(BUILD_ROOT)" rmdir /S /Q "$(BUILD_ROOT)"

K1C2_SUBTREE_TEST := $(TEST_DIR)/test_k1_c2_subtree_movement.exe
K1C2_OPTIMIZATION_SCRIPT := tests/test_k1_c2_optimization_determinism.py
K1C2_SANITIZER_SCRIPT := tests/test_k1_c2_sanitize.py
K1C2_SANITIZER_CLASSIFICATION_SCRIPT := tests/test_k1_c2_sanitizer_classification.py
K1C2_POSIX_SMOKE_SCRIPT := tests/test_k1_c2_posix_smoke.py

.PHONY: test-k1-c2-subtree-movement test-k1-c2-events test-k1-c2-hash test-k1-c2-determinism test-k1-c2-optimization-determinism test-k1-c2-sanitizer-classification test-k1-c2-sanitize test-k1-c2-posix-smoke audit-k1-c2

$(K1C2_SUBTREE_TEST): tests/test_k1_c2_subtree_movement.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

test-k1-c2-subtree-movement test-k1-c2-events test-k1-c2-hash test-k1-c2-determinism: $(K1C2_SUBTREE_TEST)
	$(K1C2_SUBTREE_TEST)

test-k1-c2-optimization-determinism: | $(TEST_DIR)
	$(PYTHON) $(K1C2_OPTIMIZATION_SCRIPT) --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --test "tests/test_k1_c2_subtree_movement.c" --output-dir "$(BUILD_ROOT)/audit/k1_c2"

test-k1-c2-sanitizer-classification:
	$(PYTHON) -B $(K1C2_SANITIZER_CLASSIFICATION_SCRIPT)

test-k1-c2-sanitize: test-k1-c2-sanitizer-classification
	$(PYTHON) -B $(K1C2_SANITIZER_SCRIPT) --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --test "tests/test_k1_c2_subtree_movement.c" --output-dir "$(TEST_DIR)"

test-k1-c2-posix-smoke:
	$(PYTHON) $(K1C2_POSIX_SMOKE_SCRIPT) --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --test "tests/test_k1_c2_subtree_movement.c" --output-dir "$(TEST_DIR)"

audit-k1-c2: audit-k1-c1 test-k1-c2-subtree-movement test-k1-c2-events test-k1-c2-hash test-k1-c2-determinism test-k1-c2-optimization-determinism test-k1-c2-sanitizer-classification test-k1-c2-sanitize test-k1-c2-posix-smoke
	$(PYTHON) scripts/check_k1_c2.py --root . --library "$(LIB)" --subtree-test "$(K1C2_SUBTREE_TEST)"
	@echo K1-C2 subtree translation and state hash V5 validation OK
K1C3_ORDERING_TEST := $(TEST_DIR)/test_k1_c3_ordering.exe
K1C3_INVARIANTS_TEST := $(TEST_DIR)/test_k1_c3_invariants.exe
K1C3_LIMITS_TEST := $(TEST_DIR)/test_k1_c3_limits.exe
K1C3_LONG_RUN_TEST := $(TEST_DIR)/test_k1_c3_long_run.exe
K1C3_OPTIMIZATION_SCRIPT := tests/test_k1_c3_optimization_determinism.py
K1C3_SANITIZER_SCRIPT := tests/test_k1_c3_sanitize.py
K1C3_SANITIZER_CLASSIFICATION_SCRIPT := tests/test_k1_c3_sanitizer_classification.py
K1C3_POSIX_SMOKE_SCRIPT := tests/test_k1_c3_posix_smoke.py

.PHONY: test-k1-c3-ordering test-k1-c3-invariants test-k1-c3-limits test-k1-c3-failure-atomicity test-k1-c3-long-run test-k1-c3-hash test-k1-c3-determinism test-k1-c3-optimization-determinism test-k1-c3-sanitizer-classification test-k1-c3-sanitize test-k1-c3-posix-smoke audit-k1-c3

$(K1C3_ORDERING_TEST): tests/test_k1_c3_ordering.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -DMINISNN_WORLDS_KERNEL_C3_INVARIANT_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(K1C3_INVARIANTS_TEST): tests/test_k1_c3_invariants.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -DMINISNN_WORLDS_KERNEL_C3_INVARIANT_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(K1C3_LIMITS_TEST): tests/test_k1_c3_limits.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -DMINISNN_WORLDS_KERNEL_C3_INVARIANT_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

$(K1C3_LONG_RUN_TEST): tests/test_k1_c3_long_run.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -DMINISNN_WORLDS_KERNEL_C3_INVARIANT_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

test-k1-c3-ordering: $(K1C3_ORDERING_TEST)
	$(K1C3_ORDERING_TEST)

test-k1-c3-invariants: $(K1C3_INVARIANTS_TEST)
	$(K1C3_INVARIANTS_TEST)

test-k1-c3-limits test-k1-c3-failure-atomicity: $(K1C3_LIMITS_TEST)
	$(K1C3_LIMITS_TEST)

test-k1-c3-long-run test-k1-c3-determinism: $(K1C3_LONG_RUN_TEST)
	$(K1C3_LONG_RUN_TEST)

test-k1-c3-hash: $(K1C3_INVARIANTS_TEST)
	$(K1C3_INVARIANTS_TEST)

test-k1-c3-optimization-determinism: | $(TEST_DIR)
	$(PYTHON) $(K1C3_OPTIMIZATION_SCRIPT) --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --test "tests/test_k1_c3_long_run.c" --output-dir "$(BUILD_ROOT)/audit/k1_c3"

test-k1-c3-sanitizer-classification:
	$(PYTHON) -B $(K1C3_SANITIZER_CLASSIFICATION_SCRIPT)

test-k1-c3-sanitize: test-k1-c3-sanitizer-classification
	$(PYTHON) -B $(K1C3_SANITIZER_SCRIPT) --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --ordering-test "tests/test_k1_c3_ordering.c" --invariants-test "tests/test_k1_c3_invariants.c" --limits-test "tests/test_k1_c3_limits.c" --long-run-test "tests/test_k1_c3_long_run.c" --output-dir "$(TEST_DIR)"

test-k1-c3-posix-smoke:
	$(PYTHON) $(K1C3_POSIX_SMOKE_SCRIPT) --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --test "tests/test_k1_c3_ordering.c" --output-dir "$(TEST_DIR)"

audit-k1-c3: audit-k1-c2 test-k1-c3-ordering test-k1-c3-invariants test-k1-c3-limits test-k1-c3-failure-atomicity test-k1-c3-long-run test-k1-c3-hash test-k1-c3-determinism test-k1-c3-optimization-determinism test-k1-c3-sanitizer-classification test-k1-c3-sanitize test-k1-c3-posix-smoke
	$(PYTHON) scripts/check_k1_c3.py --root . --library "$(LIB)" --ordering-test "$(K1C3_ORDERING_TEST)" --invariants-test "$(K1C3_INVARIANTS_TEST)" --limits-test "$(K1C3_LIMITS_TEST)" --long-run-test "$(K1C3_LONG_RUN_TEST)"
	@echo K1-C3 ordering, hardening and state hash V5 validation OK

# K1-C4 integration: parser/configuration, canonical artifacts and final spatial stress.
K1C4_DEMO := $(BIN_DIR)/k1_spatial_links_demo.exe
K1C4_STRESS_TEST := $(TEST_DIR)/test_k1_c4_stress.exe
K1C4_CHECKER := scripts/check_k1_c4.py
K1C4_DEMO_TEST := tests/test_k1_c4_demo.py
K1C4_GOLDEN := tests/golden/k1_c4_integrated_hash.txt
K1C4_CONFIG_TEST := $(TEST_DIR)/test_k1_c4_config.exe
K1C4_OPTIMIZATION_SCRIPT := tests/test_k1_c4_optimization_determinism.py
K1C4_SANITIZER_SCRIPT := tests/test_k1_c4_sanitize.py
K1C4_SANITIZER_CLASSIFICATION_SCRIPT := tests/test_k1_c4_sanitizer_classification.py
K1C4_POSIX_SMOKE_SCRIPT := tests/test_k1_c4_posix_smoke.py

.PHONY: demo-k1-c4 check-k1-c4 test-k1-c4-config test-k1-c4-demo test-k1-c4-determinism test-k1-c4-stress test-k1-c4-optimization-determinism test-k1-c4-sanitizer-classification test-k1-c4-sanitize test-k1-c4-posix-smoke audit-k1-c4 audit-k1-c audit-k1

$(K1C4_DEMO): app/k1_spatial_links_demo.c app/k1_c4_config.c app/k1_c4_config.h src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -DMINISNN_WORLDS_KERNEL_C3_INVARIANT_TESTING -I$(PUBLIC_INCLUDE_DIR) -Iapp app/k1_spatial_links_demo.c app/k1_c4_config.c src/minisnn_worlds_kernel.c -o $@

$(K1C4_STRESS_TEST): tests/test_k1_c4_stress.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -O2 -DMINISNN_WORLDS_KERNEL_TESTING -DMINISNN_WORLDS_KERNEL_C3_INVARIANT_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

demo-k1-c4: $(K1C4_DEMO)
	@if exist "$(BUILD_ROOT)/demo/k1_c4" rmdir /S /Q "$(BUILD_ROOT)/demo/k1_c4"
	@if not exist "$(BUILD_ROOT)/demo/k1_c4" mkdir "$(BUILD_ROOT)/demo/k1_c4"
	$(K1C4_DEMO) configs/k1_spatial_links_demo.ini $(BUILD_ROOT)/demo/k1_c4
	$(PYTHON) $(K1C4_CHECKER) --directory "$(BUILD_ROOT)/demo/k1_c4" --golden "$(K1C4_GOLDEN)"

$(K1C4_CONFIG_TEST): tests/test_k1_c4_config.c app/k1_c4_config.c app/k1_c4_config.h $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) -Iapp $< app/k1_c4_config.c -o $@

check-k1-c4: demo-k1-c4

test-k1-c4-config: $(K1C4_CONFIG_TEST)
	$(K1C4_CONFIG_TEST)

test-k1-c4-demo: $(K1C4_DEMO)
	$(PYTHON) -B $(K1C4_DEMO_TEST) --demo "$(K1C4_DEMO)" --config "configs/k1_spatial_links_demo.ini" --checker "$(K1C4_CHECKER)" --golden "$(K1C4_GOLDEN)"

test-k1-c4-determinism: test-k1-c4-demo

test-k1-c4-stress: $(K1C4_STRESS_TEST)
	$(K1C4_STRESS_TEST)

test-k1-c4-optimization-determinism:
	$(PYTHON) -B $(K1C4_OPTIMIZATION_SCRIPT) --compiler "$(CC)" --kernel-root "." --config "configs/k1_spatial_links_demo.ini" --golden "$(K1C4_GOLDEN)"

test-k1-c4-sanitizer-classification:
	$(PYTHON) -B $(K1C4_SANITIZER_CLASSIFICATION_SCRIPT)

test-k1-c4-sanitize: test-k1-c4-sanitizer-classification
	$(PYTHON) -B $(K1C4_SANITIZER_SCRIPT) --compiler "$(CC)" --kernel-root "." --config "configs/k1_spatial_links_demo.ini" --golden "$(K1C4_GOLDEN)"

test-k1-c4-posix-smoke:
	$(PYTHON) -B $(K1C4_POSIX_SMOKE_SCRIPT) --kernel-root "." --config "configs/k1_spatial_links_demo.ini" --golden "$(K1C4_GOLDEN)"

audit-k1-c4: audit-k1-c3 check-k1-c4 test-k1-c4-config test-k1-c4-demo test-k1-c4-determinism test-k1-c4-stress test-k1-c4-optimization-determinism test-k1-c4-sanitizer-classification test-k1-c4-sanitize test-k1-c4-posix-smoke
	@echo K1-C4 integrated spatial links closure validation OK

audit-k1-c: audit-k1-c4
	@echo K1-C spatial links integration validation OK

audit-k1: audit-k1-c
	@echo K1 spatial foundation validation OK
# K2-A: canonical, memory-only snapshot V1. K2-B will own validation and restore.
K2A_SNAPSHOT_TEST := $(TEST_DIR)/test_k2_a_snapshot.exe
K2A_LONG_RUN_TEST := $(TEST_DIR)/test_k2_a_long_run.exe
K2A_GOLDEN := tests/golden/k2_a_snapshot_v1.txt
K2A_OPTIMIZATION_SCRIPT := tests/test_k2_a_optimization_determinism.py
K2A_SANITIZER_SCRIPT := tests/test_k2_a_sanitize.py
K2A_SANITIZER_CLASSIFICATION_SCRIPT := tests/test_k2_a_sanitizer_classification.py
K2A_POSIX_SMOKE_SCRIPT := tests/test_k2_a_posix_smoke.py
K2A_CHECKER := scripts/check_k2_a.py

.PHONY: test-k2-a-snapshot test-k2-a-format test-k2-a-canonical test-k2-a-failure-atomicity test-k2-a-long-run test-k2-a-determinism test-k2-a-optimization-determinism test-k2-a-sanitizer-classification test-k2-a-sanitize test-k2-a-posix-smoke audit-k2-a

$(K2A_SNAPSHOT_TEST): tests/test_k2_a_snapshot.c $(KERNEL_SOURCES) src/minisnn_worlds_kernel_internal.h $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< $(KERNEL_SOURCES) -o $@

$(K2A_LONG_RUN_TEST): tests/test_k2_a_long_run.c $(KERNEL_SOURCES) src/minisnn_worlds_kernel_internal.h $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< $(KERNEL_SOURCES) -o $@

test: $(K2A_SNAPSHOT_TEST) $(K2A_LONG_RUN_TEST)

test-k2-a-snapshot test-k2-a-format test-k2-a-canonical test-k2-a-failure-atomicity: $(K2A_SNAPSHOT_TEST)
	$(K2A_SNAPSHOT_TEST)

test-k2-a-long-run test-k2-a-determinism: $(K2A_LONG_RUN_TEST)
	$(K2A_LONG_RUN_TEST)

test-k2-a-optimization-determinism:
	$(PYTHON) -B $(K2A_OPTIMIZATION_SCRIPT) --compiler "$(CC)" --kernel-root "."

test-k2-a-sanitizer-classification:
	$(PYTHON) -B $(K2A_SANITIZER_CLASSIFICATION_SCRIPT)

test-k2-a-sanitize: test-k2-a-sanitizer-classification
	$(PYTHON) -B $(K2A_SANITIZER_SCRIPT) --compiler "$(CC)" --kernel-root "."

test-k2-a-posix-smoke:
	$(PYTHON) -B $(K2A_POSIX_SMOKE_SCRIPT) --kernel-root "."

audit-k2-a: audit-k1 test-k2-a-snapshot test-k2-a-format test-k2-a-canonical test-k2-a-failure-atomicity test-k2-a-long-run test-k2-a-determinism test-k2-a-optimization-determinism test-k2-a-sanitizer-classification test-k2-a-sanitize test-k2-a-posix-smoke
	$(PYTHON) -B $(K2A_CHECKER) --root . --snapshot-test "$(K2A_SNAPSHOT_TEST)" --golden "$(K2A_GOLDEN)"
	@echo K2-A canonical snapshot validation OK
# K2-B: strict V1 import, transactional restore, and app-layer binary persistence.
K2B_RESTORE_TEST := $(TEST_DIR)/test_k2_b_restore.exe
K2B_CHECKPOINT_TEST := $(TEST_DIR)/test_k2_b_checkpoints.exe
K2B_FILE_TEST := $(TEST_DIR)/test_k2_b_file_roundtrip.exe
K2B_DEMO := $(BIN_DIR)/k2_snapshot_restore_demo.exe
K2B_CHECKER := scripts/check_k2_b.py
K2B_GOLDEN := tests/golden/k2_b_restore_v1.txt
K2B_OPTIMIZATION_SCRIPT := tests/test_k2_b_optimization_determinism.py
K2B_SANITIZER_SCRIPT := tests/test_k2_b_sanitize.py
K2B_SANITIZER_CLASSIFICATION_SCRIPT := tests/test_k2_b_sanitizer_classification.py
K2B_POSIX_SMOKE_SCRIPT := tests/test_k2_b_posix_smoke.py

.PHONY: test-k2-b-import test-k2-b-restore test-k2-b-malformed test-k2-b-truncation test-k2-b-failure-atomicity test-k2-b-roundtrip test-k2-b-continuation test-k2-b-checkpoints test-k2-b-file-roundtrip test-k2-b-determinism test-k2-b-optimization-determinism test-k2-b-sanitizer-classification test-k2-b-sanitize test-k2-b-posix-smoke demo-k2-b check-k2-b audit-k2-b

$(K2B_RESTORE_TEST): tests/test_k2_b_restore.c tests/test_k2_a_snapshot.c $(KERNEL_SOURCES) src/minisnn_worlds_kernel_internal.h $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< $(KERNEL_SOURCES) -o $@

$(K2B_CHECKPOINT_TEST): tests/test_k2_b_checkpoints.c tests/test_k2_a_long_run.c $(KERNEL_SOURCES) src/minisnn_worlds_kernel_internal.h $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< $(KERNEL_SOURCES) -o $@

$(K2B_FILE_TEST): tests/test_k2_b_file_roundtrip.c tests/test_k2_a_snapshot.c app/k2_snapshot_file.c app/k2_snapshot_file.h $(KERNEL_SOURCES) src/minisnn_worlds_kernel_internal.h $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) -Iapp $< app/k2_snapshot_file.c $(KERNEL_SOURCES) -o $@

$(K2B_DEMO): app/k2_snapshot_restore_demo.c app/k2_snapshot_file.c app/k2_snapshot_file.h $(KERNEL_SOURCES) src/minisnn_worlds_kernel_internal.h $(PUBLIC_HEADERS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) -Iapp $< app/k2_snapshot_file.c $(KERNEL_SOURCES) -o $@

test-k2-b-import test-k2-b-restore test-k2-b-malformed test-k2-b-truncation test-k2-b-failure-atomicity test-k2-b-roundtrip test-k2-b-continuation: $(K2B_RESTORE_TEST)
	$(K2B_RESTORE_TEST)

test-k2-b-checkpoints: $(K2B_CHECKPOINT_TEST)
	$(K2B_CHECKPOINT_TEST)

test-k2-b-file-roundtrip: $(K2B_FILE_TEST)
	$(K2B_FILE_TEST)

test-k2-b-determinism: test-k2-b-continuation test-k2-b-checkpoints

test-k2-b-optimization-determinism:
	$(PYTHON) -B $(K2B_OPTIMIZATION_SCRIPT) --compiler "$(CC)" --kernel-root "."

test-k2-b-sanitizer-classification:
	$(PYTHON) -B $(K2B_SANITIZER_CLASSIFICATION_SCRIPT)

test-k2-b-sanitize: test-k2-b-sanitizer-classification
	$(PYTHON) -B $(K2B_SANITIZER_SCRIPT) --compiler "$(CC)" --kernel-root "."

test-k2-b-posix-smoke:
	$(PYTHON) -B $(K2B_POSIX_SMOKE_SCRIPT) --kernel-root "."

demo-k2-b: $(K2B_DEMO)
	@if exist "$(BUILD_ROOT)/demo/k2_b" rmdir /S /Q "$(BUILD_ROOT)/demo/k2_b"
	@if not exist "$(BUILD_ROOT)/demo/k2_b" mkdir "$(BUILD_ROOT)/demo/k2_b"
	$(K2B_DEMO) $(BUILD_ROOT)/demo/k2_b

check-k2-b: demo-k2-b
	$(PYTHON) -B $(K2B_CHECKER) --directory "$(BUILD_ROOT)/demo/k2_b" --golden "$(K2B_GOLDEN)"

test: $(K2B_RESTORE_TEST) $(K2B_CHECKPOINT_TEST) $(K2B_FILE_TEST)

audit-k2-b: audit-k2-a test-k2-b-import test-k2-b-malformed test-k2-b-truncation test-k2-b-failure-atomicity test-k2-b-roundtrip test-k2-b-continuation test-k2-b-checkpoints test-k2-b-file-roundtrip test-k2-b-determinism test-k2-b-optimization-determinism test-k2-b-sanitizer-classification test-k2-b-sanitize test-k2-b-posix-smoke check-k2-b
	@echo K2-B snapshot restore and persistence validation OK
# K2-D: final state-bound replay, persistence closure and frozen K2 formats.
K2D_STATE_BINDING_TEST := $(TEST_DIR)/test_k2_d_state_binding.exe
K2D_NEGATIVE_TEST := $(TEST_DIR)/test_k2_d_negative.exe
K2D_PERSISTENCE_TEST := $(TEST_DIR)/test_k2_d_persistence.exe
K2D_DEMO := $(BIN_DIR)/k2_persistence_replay_demo.exe
K2D_CHECKER := scripts/check_k2_d.py
K2D_GOLDEN := tests/golden/k2_d_persistence_replay_v1.txt
K2D_OPTIMIZATION_SCRIPT := tests/test_k2_d_optimization_determinism.py
K2D_SANITIZER_SCRIPT := tests/test_k2_d_sanitize.py
K2D_SANITIZER_CLASSIFICATION_SCRIPT := tests/test_k2_d_sanitizer_classification.py
K2D_POSIX_SMOKE_SCRIPT := tests/test_k2_d_posix_smoke.py

.PHONY: test-k2-d-state-binding test-k2-d-negative test-k2-d-persistence test-k2-d-long-run test-k2-d-determinism test-k2-d-optimization-determinism test-k2-d-sanitizer-classification test-k2-d-sanitize test-k2-d-posix-smoke test-k2-d-integrated test-k2-d-multi-checkpoint test-k2-d-malformed test-k2-d-failure-atomicity demo-k2-d check-k2-d audit-k2-d audit-k2

$(K2D_STATE_BINDING_TEST): tests/test_k2_d_state_binding.c tests/test_k2_c_command_log.c app/k2_command_log_file.c app/k2_command_log_file.h $(KERNEL_SOURCES) src/minisnn_worlds_kernel_internal.h $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) -Iapp $< app/k2_command_log_file.c $(KERNEL_SOURCES) -o $@

$(K2D_NEGATIVE_TEST): tests/test_k2_d_negative.c tests/test_k2_c_command_log.c app/k2_command_log_file.c app/k2_command_log_file.h $(KERNEL_SOURCES) src/minisnn_worlds_kernel_internal.h $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) -Iapp $< app/k2_command_log_file.c $(KERNEL_SOURCES) -o $@

$(K2D_PERSISTENCE_TEST): tests/test_k2_d_persistence.c tests/test_k2_c_command_log.c app/k2_snapshot_file.c app/k2_snapshot_file.h app/k2_command_log_file.c app/k2_command_log_file.h $(KERNEL_SOURCES) src/minisnn_worlds_kernel_internal.h $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) -Iapp $< app/k2_snapshot_file.c app/k2_command_log_file.c $(KERNEL_SOURCES) -o $@

$(K2D_DEMO): app/k2_persistence_replay_demo.c app/k2_snapshot_file.c app/k2_snapshot_file.h app/k2_command_log_file.c app/k2_command_log_file.h $(KERNEL_SOURCES) src/minisnn_worlds_kernel_internal.h $(PUBLIC_HEADERS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) -Iapp $< app/k2_snapshot_file.c app/k2_command_log_file.c $(KERNEL_SOURCES) -o $@

test: $(K2D_STATE_BINDING_TEST) $(K2D_NEGATIVE_TEST) $(K2D_PERSISTENCE_TEST)

test-k2-d-state-binding: $(K2D_STATE_BINDING_TEST)
	$(K2D_STATE_BINDING_TEST)

test-k2-d-negative: $(K2D_NEGATIVE_TEST)
	$(K2D_NEGATIVE_TEST)

test-k2-d-persistence test-k2-d-long-run: $(K2D_PERSISTENCE_TEST)
	$(K2D_PERSISTENCE_TEST)

test-k2-d-determinism: test-k2-d-persistence

test-k2-d-integrated: check-k2-d

test-k2-d-multi-checkpoint: test-k2-d-persistence

test-k2-d-malformed test-k2-d-failure-atomicity: test-k2-d-negative

test-k2-d-optimization-determinism:
	$(PYTHON) -B $(K2D_OPTIMIZATION_SCRIPT) --compiler "$(CC)" --kernel-root "."

test-k2-d-sanitizer-classification:
	$(PYTHON) -B $(K2D_SANITIZER_CLASSIFICATION_SCRIPT)

test-k2-d-sanitize: test-k2-d-sanitizer-classification
	$(PYTHON) -B $(K2D_SANITIZER_SCRIPT) --compiler "$(CC)" --kernel-root "."

test-k2-d-posix-smoke:
	$(PYTHON) -B $(K2D_POSIX_SMOKE_SCRIPT) --kernel-root "."

demo-k2-d: $(K2D_DEMO)
	@if exist "$(BUILD_ROOT)/demo/k2_d" rmdir /S /Q "$(BUILD_ROOT)/demo/k2_d"
	@if not exist "$(BUILD_ROOT)/demo/k2_d" mkdir "$(BUILD_ROOT)/demo/k2_d"
	$(K2D_DEMO) $(BUILD_ROOT)/demo/k2_d

check-k2-d: demo-k2-d test-k2-d-state-binding test-k2-d-negative test-k2-d-persistence
	$(PYTHON) -B $(K2D_CHECKER) --root . --directory "$(BUILD_ROOT)/demo/k2_d" --golden "$(K2D_GOLDEN)"

audit-k2-d: audit-k2-c test-k2-d-state-binding test-k2-d-negative test-k2-d-persistence test-k2-d-determinism test-k2-d-optimization-determinism test-k2-d-sanitizer-classification test-k2-d-sanitize test-k2-d-posix-smoke check-k2-d
	@echo K2-D final persistence and replay validation OK

audit-k2: audit-k2-d
	@echo K2 persistence foundation validation OK
# K2-C: canonical external command log V1, deterministic replay, and K2-A/B continuation.
K2C_COMMAND_LOG_TEST := $(TEST_DIR)/test_k2_c_command_log.exe
K2C_LONG_RUN_TEST := $(TEST_DIR)/test_k2_c_long_run.exe
K2C_DEMO := $(BIN_DIR)/k2_command_replay_demo.exe
K2C_CHECKER := scripts/check_k2_c.py
K2C_GOLDEN := tests/golden/k2_c_command_replay_v1.txt
K2C_OPTIMIZATION_SCRIPT := tests/test_k2_c_optimization_determinism.py
K2C_SANITIZER_SCRIPT := tests/test_k2_c_sanitize.py
K2C_SANITIZER_CLASSIFICATION_SCRIPT := tests/test_k2_c_sanitizer_classification.py
K2C_POSIX_SMOKE_SCRIPT := tests/test_k2_c_posix_smoke.py

.PHONY: test-k2-c-command-log test-k2-c-format test-k2-c-malformed test-k2-c-failure-atomicity test-k2-c-replay test-k2-c-snapshot-replay test-k2-c-long-run test-k2-c-determinism test-k2-c-optimization-determinism test-k2-c-sanitizer-classification test-k2-c-sanitize test-k2-c-posix-smoke demo-k2-c check-k2-c audit-k2-c

$(K2C_COMMAND_LOG_TEST): tests/test_k2_c_command_log.c app/k2_command_log_file.c app/k2_command_log_file.h $(KERNEL_SOURCES) src/minisnn_worlds_kernel_internal.h $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) -Iapp $< app/k2_command_log_file.c $(KERNEL_SOURCES) -o $@

$(K2C_LONG_RUN_TEST): tests/test_k2_c_long_run.c tests/test_k2_c_command_log.c app/k2_command_log_file.c app/k2_command_log_file.h $(KERNEL_SOURCES) src/minisnn_worlds_kernel_internal.h $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) -Iapp $< app/k2_command_log_file.c $(KERNEL_SOURCES) -o $@

$(K2C_DEMO): app/k2_command_replay_demo.c app/k2_snapshot_file.c app/k2_snapshot_file.h app/k2_command_log_file.c app/k2_command_log_file.h $(KERNEL_SOURCES) src/minisnn_worlds_kernel_internal.h $(PUBLIC_HEADERS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) -Iapp $< app/k2_snapshot_file.c app/k2_command_log_file.c $(KERNEL_SOURCES) -o $@

test: $(K2C_COMMAND_LOG_TEST) $(K2C_LONG_RUN_TEST)

test-k2-c-command-log test-k2-c-format test-k2-c-malformed test-k2-c-failure-atomicity test-k2-c-replay test-k2-c-snapshot-replay: $(K2C_COMMAND_LOG_TEST)
	$(K2C_COMMAND_LOG_TEST)

test-k2-c-long-run test-k2-c-determinism: $(K2C_LONG_RUN_TEST)
	$(K2C_LONG_RUN_TEST)

test-k2-c-optimization-determinism:
	$(PYTHON) -B $(K2C_OPTIMIZATION_SCRIPT) --compiler "$(CC)" --kernel-root "."

test-k2-c-sanitizer-classification:
	$(PYTHON) -B $(K2C_SANITIZER_CLASSIFICATION_SCRIPT)

test-k2-c-sanitize: test-k2-c-sanitizer-classification
	$(PYTHON) -B $(K2C_SANITIZER_SCRIPT) --compiler "$(CC)" --kernel-root "."

test-k2-c-posix-smoke:
	$(PYTHON) -B $(K2C_POSIX_SMOKE_SCRIPT) --kernel-root "."

demo-k2-c: $(K2C_DEMO)
	@if exist "$(BUILD_ROOT)/demo/k2_c" rmdir /S /Q "$(BUILD_ROOT)/demo/k2_c"
	@if not exist "$(BUILD_ROOT)/demo/k2_c" mkdir "$(BUILD_ROOT)/demo/k2_c"
	$(K2C_DEMO) $(BUILD_ROOT)/demo/k2_c

check-k2-c: demo-k2-c
	$(PYTHON) -B $(K2C_CHECKER) --directory "$(BUILD_ROOT)/demo/k2_c" --golden "$(K2C_GOLDEN)"

audit-k2-c: audit-k2-b test-k2-c-command-log test-k2-c-format test-k2-c-malformed test-k2-c-failure-atomicity test-k2-c-replay test-k2-c-snapshot-replay test-k2-c-long-run test-k2-c-determinism test-k2-c-optimization-determinism test-k2-c-sanitizer-classification test-k2-c-sanitize test-k2-c-posix-smoke check-k2-c
	@echo K2-C command replay validation OK