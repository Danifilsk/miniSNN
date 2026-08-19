CC ?= gcc
PYTHON ?= python
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Wformat=2 -Wstrict-prototypes

REPOSITORY_ROOT := ../../..
CORE_ROOT := $(REPOSITORY_ROOT)/core
KERNEL_ROOT := ../../kernel
DOMAIN_ROOT := ../../domain
BRIDGE_ROOT := ../../brain_bridge
BUILD_ROOT := $(REPOSITORY_ROOT)/build/worlds/scenarios/wf0_fish
BIN_DIR := $(BUILD_ROOT)/bin
TEST_DIR := $(BUILD_ROOT)/tests
RESULT_DIR := $(BUILD_ROOT)/results/wf0_fish_demo
CORE_LIB := $(REPOSITORY_ROOT)/build/core/lib/libminisnn_core.a
KERNEL_LIB := $(REPOSITORY_ROOT)/build/worlds/kernel/lib/libminisnn_worlds_kernel.a
DOMAIN_LIB := $(REPOSITORY_ROOT)/build/worlds/domain/lib/libminisnn_worlds_domain.a
BRIDGE_SOURCES := $(BRIDGE_ROOT)/src/minisnn_worlds_brain_bridge.c
INCLUDES := -Iinclude -I$(CORE_ROOT)/include -I$(KERNEL_ROOT)/include -I$(DOMAIN_ROOT)/include -I$(BRIDGE_ROOT)/include
SOURCES := src/wf0_fish.c src/wf0_fish_observation.c
DEMO := $(BIN_DIR)/wf0_fish_demo.exe
TEST := $(TEST_DIR)/test_wf0_fish.exe
CHECKER := scripts/check_wf0_fish.py
DETERMINISM := scripts/test_wf0_determinism.py
OPTIMIZATION := scripts/test_wf0_optimization_determinism.py
SANITIZER := scripts/test_wf0_sanitize.py
POSIX_SMOKE := scripts/test_wf0_posix_smoke.py

.PHONY: all help test-wf0 test-wf0-neural-causality test-wf0-neural-continuity test-wf0-determinism \
        test-wf0-optimization-determinism test-wf0-sanitize test-wf0-posix-smoke \
        demo-wf0 check-wf0 audit-wf0 clean

all: $(DEMO) $(TEST)

help:
	@echo WF0 Fish targets:
	@echo   make test-wf0                         - valida causalidade neural, energia e isolamento
	@echo   make test-wf0-neural-causality        - alias para a prova causal do cerebro
	@echo   make test-wf0-neural-continuity       - prova retencao neural entre ticks e reset manual
	@echo   make test-wf0-determinism             - compara dois demos byte a byte
	@echo   make test-wf0-optimization-determinism - compara resultados O0 e O2
	@echo   make test-wf0-sanitize                - executa ASan/UBSan quando disponivel
	@echo   make test-wf0-posix-smoke             - compila e roda smoke POSIX quando disponivel
	@echo   make demo-wf0                         - gera resumo e trace do organismo
	@echo   make check-wf0                        - valida artefatos WF0
	@echo   make audit-wf0                        - executa a auditoria focada WF0

$(BIN_DIR) $(TEST_DIR) $(RESULT_DIR):
	@if not exist "$@" mkdir "$@"

$(CORE_LIB):
	$(MAKE) -C $(CORE_ROOT) core-lib

$(KERNEL_LIB):
	$(MAKE) -C $(KERNEL_ROOT) lib

$(DOMAIN_LIB): $(KERNEL_LIB)
	$(MAKE) -C $(DOMAIN_ROOT) lib

$(DEMO): app/wf0_fish_demo.c $(SOURCES) $(BRIDGE_SOURCES) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SOURCES) $(BRIDGE_SOURCES) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) -o $@

$(TEST): tests/test_wf0_fish.c $(SOURCES) $(BRIDGE_SOURCES) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SOURCES) $(BRIDGE_SOURCES) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) -o $@

test-wf0: $(TEST)
	$(TEST)

test-wf0-neural-causality: test-wf0

test-wf0-neural-continuity: test-wf0

demo-wf0: $(DEMO) | $(RESULT_DIR)
	@if exist "$(RESULT_DIR)" rmdir /S /Q "$(RESULT_DIR)"
	@if not exist "$(RESULT_DIR)" mkdir "$(RESULT_DIR)"
	$(DEMO) "$(RESULT_DIR)"

check-wf0: demo-wf0
	$(PYTHON) -B $(CHECKER) --directory "$(RESULT_DIR)"

test-wf0-determinism: $(DEMO)
	$(PYTHON) -B $(DETERMINISM) --demo "$(DEMO)"

test-wf0-optimization-determinism: $(CORE_LIB)
	$(PYTHON) -B $(OPTIMIZATION) --compiler "$(CC)" --scenario-root "."

test-wf0-sanitize:
	$(PYTHON) -B $(SANITIZER) --compiler "$(CC)" --scenario-root "."

test-wf0-posix-smoke:
	$(PYTHON) -B $(POSIX_SMOKE) --scenario-root "."

audit-wf0: test-wf0 check-wf0 test-wf0-determinism test-wf0-optimization-determinism test-wf0-sanitize test-wf0-posix-smoke
	@echo WF0 Fish audit OK

clean:
	@if exist "$(BUILD_ROOT)" rmdir /S /Q "$(BUILD_ROOT)"
