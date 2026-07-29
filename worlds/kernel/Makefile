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
TEST_EXE := $(TEST_DIR)/test_worlds_kernel.exe
PUBLIC_HEADERS := $(wildcard $(PUBLIC_INCLUDE_DIR)/*.h)
SANITIZER_SCRIPT := tests/test_k0_a_sanitize.py

.PHONY: all lib demo test sanitize audit-k0-a clean

all: lib demo

help:
	@echo Worlds Kernel K0-A targets:
	@echo   make lib          - compila a biblioteca estatica do Kernel
	@echo   make demo         - compila o demo headless de ticks
	@echo   make test         - executa testes do Kernel
	@echo   make sanitize     - executa a regressao ASan/UBSan do Kernel
	@echo   make audit-k0-a   - valida arquitetura, headers, simbolos e demo
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

$(TEST_EXE): tests/test_worlds_kernel.c src/minisnn_worlds_kernel.c $(PUBLIC_HEADERS) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) $< src/minisnn_worlds_kernel.c -o $@

test: $(LIB) $(TEST_EXE)
	$(PYTHON) tests/test_public_headers.py "$(CC)" "$(CFLAGS)" "$(PUBLIC_INCLUDE_DIR)" "$(TEST_DIR)"
	$(TEST_EXE)

sanitize: | $(TEST_DIR)
	$(PYTHON) $(SANITIZER_SCRIPT) --compiler "$(CC)" --include "$(PUBLIC_INCLUDE_DIR)" --source "src/minisnn_worlds_kernel.c" --test "tests/test_worlds_kernel.c" --output-dir "$(TEST_DIR)"

audit-k0-a: lib demo test
	$(PYTHON) scripts/check_k0_a.py --root . --library "$(LIB)" --demo "$(DEMO)" --test "$(TEST_EXE)"

clean:
	@if exist "$(BUILD_ROOT)" rmdir /S /Q "$(BUILD_ROOT)"
