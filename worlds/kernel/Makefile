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
TEST_EXE := $(TEST_DIR)/test_worlds_kernel.exe
K0B_ENTITY_TEST := $(TEST_DIR)/test_k0_b_entities.exe
K0B_COMMAND_TEST := $(TEST_DIR)/test_k0_b_commands.exe
K0B_EVENT_TEST := $(TEST_DIR)/test_k0_b_events.exe
K0B_DETERMINISM_TEST := $(TEST_DIR)/test_k0_b_determinism.exe
PUBLIC_HEADERS := $(wildcard $(PUBLIC_INCLUDE_DIR)/*.h)
SANITIZER_SCRIPT := tests/test_k0_a_sanitize.py

.PHONY: all lib demo demo-k0-b test test-k0-b-entities test-k0-b-commands test-k0-b-events test-k0-b-determinism test-k0-b-sanitize sanitize audit-k0-a audit-k0-b clean

all: lib demo demo-k0-b

help:
	@echo Worlds Kernel targets:
	@echo   make lib          - compila a biblioteca estatica do Kernel
	@echo   make demo         - compila o demo headless de ticks
	@echo   make demo-k0-b    - compila e executa o demo de entidades e comandos
	@echo   make test         - executa testes do Kernel
	@echo   make test-k0-b-entities - valida Entity IDs e registro
	@echo   make test-k0-b-commands - valida fila e ordenacao canonica
	@echo   make test-k0-b-events - valida eventos, conflitos e atomicidade
	@echo   make test-k0-b-determinism - valida resolucao deterministica
	@echo   make test-k0-b-sanitize - executa regressao ASan/UBSan K0-B
	@echo   make sanitize     - executa a regressao ASan/UBSan do Kernel
	@echo   make audit-k0-a   - valida arquitetura, headers, simbolos e demo
	@echo   make audit-k0-b   - valida K0-A e K0-B de forma integrada
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

test: $(LIB) $(TEST_EXE) $(K0B_ENTITY_TEST) $(K0B_COMMAND_TEST) $(K0B_EVENT_TEST) $(K0B_DETERMINISM_TEST)
	$(PYTHON) tests/test_public_headers.py "$(CC)" "$(CFLAGS)" "$(PUBLIC_INCLUDE_DIR)" "$(TEST_DIR)"
	$(TEST_EXE)
	$(K0B_ENTITY_TEST)
	$(K0B_COMMAND_TEST)
	$(K0B_EVENT_TEST)
	$(K0B_DETERMINISM_TEST)

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

sanitize: | $(TEST_DIR)
	$(PYTHON) $(SANITIZER_SCRIPT) --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --test "tests/test_worlds_kernel.c" --output-dir "$(TEST_DIR)"

audit-k0-a: lib demo test
	$(PYTHON) scripts/check_k0_a.py --root . --library "$(LIB)" --demo "$(DEMO)" --test "$(TEST_EXE)"

test-k0-b-sanitize:
	$(PYTHON) tests/test_k0_b_sanitize.py --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --output-dir "$(TEST_DIR)" --demo "app/k0_entity_command_demo.c" --tests "tests/test_k0_b_entities.c" "tests/test_k0_b_commands.c" "tests/test_k0_b_events.c" "tests/test_k0_b_determinism.c"

audit-k0-b: audit-k0-a test-k0-b-entities test-k0-b-commands test-k0-b-events test-k0-b-determinism test-k0-b-sanitize demo-k0-b
	$(PYTHON) scripts/check_k0_b.py --root . --library "$(LIB)" --demo "$(K0B_DEMO)" --entity-test "$(K0B_ENTITY_TEST)" --command-test "$(K0B_COMMAND_TEST)" --event-test "$(K0B_EVENT_TEST)" --determinism-test "$(K0B_DETERMINISM_TEST)"

clean:
	@if exist "$(BUILD_ROOT)" rmdir /S /Q "$(BUILD_ROOT)"
