CC ?= gcc
AR ?= ar
PYTHON ?= python
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Wformat=2 -Wstrict-prototypes

REPOSITORY_ROOT := ../..
CORE_ROOT := $(REPOSITORY_ROOT)/core
DOMAIN_ROOT := ../domain
KERNEL_ROOT := ../kernel
BUILD_ROOT := $(REPOSITORY_ROOT)/build/worlds/brain_bridge
LIB_DIR := $(BUILD_ROOT)/lib
BIN_DIR := $(BUILD_ROOT)/bin
TEST_DIR := $(BUILD_ROOT)/tests
RESULT_DIR := $(BUILD_ROOT)/results/wb0_demo
LIB := $(LIB_DIR)/libminisnn_worlds_brain_bridge.a
SOURCE := src/minisnn_worlds_brain_bridge.c
OBJECT := $(BUILD_ROOT)/obj/minisnn_worlds_brain_bridge.o
CORE_LIB := $(REPOSITORY_ROOT)/build/core/lib/libminisnn_core.a
DOMAIN_LIB := $(REPOSITORY_ROOT)/build/worlds/domain/lib/libminisnn_worlds_domain.a
KERNEL_LIB := $(REPOSITORY_ROOT)/build/worlds/kernel/lib/libminisnn_worlds_kernel.a
KERNEL_SOURCES := $(KERNEL_ROOT)/src/minisnn_worlds_kernel.c $(KERNEL_ROOT)/src/minisnn_worlds_kernel_snapshot.c $(KERNEL_ROOT)/src/minisnn_worlds_kernel_command_log.c
TEST := $(TEST_DIR)/test_wb0_brain_bridge.exe
RETRY_TEST := $(TEST_DIR)/test_wb0_failure_retry.exe
DEMO := $(BIN_DIR)/wb0_brain_bridge_demo.exe
CHECKER := scripts/check_wb0.py
HEADER_TEST := tests/test_wb0_public_headers.py
DETERMINISM_TEST := tests/test_wb0_determinism.py
OPTIMIZATION_TEST := tests/test_wb0_optimization_determinism.py
SANITIZER_CLASSIFICATION_TEST := tests/test_wb0_sanitizer_classification.py
SANITIZER_TEST := tests/test_wb0_sanitize.py
POSIX_TEST := tests/test_wb0_posix_smoke.py

.PHONY: all lib test clean \
        test-wb0-encoder test-wb0-decoder test-wb0-binding test-wb0-core-integration \
        test-wb0-cache test-wb0-failure-retry test-wb0-multi-organism test-wb0-invariants \
        test-wb0-determinism test-wb0-optimization-determinism \
        test-wb0-sanitizer-classification test-wb0-sanitize test-wb0-posix-smoke \
        demo-wb0 check-wb0 audit-wb0

all: lib $(DEMO)

$(BUILD_ROOT)/obj $(LIB_DIR) $(BIN_DIR) $(TEST_DIR):
	@if not exist "$@" mkdir "$@"

$(CORE_LIB):
	$(MAKE) -C $(CORE_ROOT) core-lib

$(KERNEL_LIB):
	$(MAKE) -C $(KERNEL_ROOT) lib

$(DOMAIN_LIB): $(KERNEL_LIB)
	$(MAKE) -C $(DOMAIN_ROOT) lib

$(OBJECT): $(SOURCE) include/minisnn_worlds_brain_bridge.h | $(BUILD_ROOT)/obj
	$(CC) $(CFLAGS) -Iinclude -I$(CORE_ROOT)/include -I$(DOMAIN_ROOT)/include -I$(KERNEL_ROOT)/include -c $< -o $@

$(LIB): $(OBJECT) | $(LIB_DIR)
	$(AR) rcs $@ $^

$(TEST): tests/test_wb0_brain_bridge.c $(SOURCE) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) -Iinclude -I$(CORE_ROOT)/include -I$(DOMAIN_ROOT)/include -I$(KERNEL_ROOT)/include $< $(SOURCE) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) -o $@

$(RETRY_TEST): tests/test_wb0_failure_retry.c $(SOURCE) $(CORE_LIB) $(DOMAIN_ROOT)/src/minisnn_worlds_domain.c $(KERNEL_SOURCES) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -Iinclude -I$(CORE_ROOT)/include -I$(DOMAIN_ROOT)/include -I$(KERNEL_ROOT)/include $< $(SOURCE) $(DOMAIN_ROOT)/src/minisnn_worlds_domain.c $(KERNEL_SOURCES) $(CORE_LIB) -o $@

$(DEMO): app/wb0_brain_bridge_demo.c $(SOURCE) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -Iinclude -I$(CORE_ROOT)/include -I$(DOMAIN_ROOT)/include -I$(KERNEL_ROOT)/include $< $(SOURCE) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) -o $@

test-wb0-encoder test-wb0-decoder test-wb0-binding test-wb0-core-integration test-wb0-cache test-wb0-multi-organism test-wb0-invariants: $(TEST)
	$(TEST)

test-wb0-failure-retry: $(RETRY_TEST)
	$(RETRY_TEST)

test: test-wb0-encoder test-wb0-decoder test-wb0-binding test-wb0-core-integration test-wb0-cache test-wb0-failure-retry test-wb0-multi-organism test-wb0-invariants
	$(PYTHON) -B $(HEADER_TEST) "$(CC)" "$(CFLAGS)" "include" "$(CORE_ROOT)/include" "$(DOMAIN_ROOT)/include" "$(KERNEL_ROOT)/include" "$(TEST_DIR)"

demo-wb0: $(DEMO)
	@if exist "$(RESULT_DIR)" rmdir /S /Q "$(RESULT_DIR)"
	@if not exist "$(RESULT_DIR)" mkdir "$(RESULT_DIR)"
	$(DEMO) "$(RESULT_DIR)"

check-wb0: demo-wb0
	$(PYTHON) -B $(CHECKER) --directory "$(RESULT_DIR)"

test-wb0-determinism: $(DEMO)
	$(PYTHON) -B $(DETERMINISM_TEST) --demo "$(DEMO)"

test-wb0-optimization-determinism: $(CORE_LIB)
	$(PYTHON) -B $(OPTIMIZATION_TEST) --compiler "$(CC)" --bridge-root "."

test-wb0-sanitizer-classification:
	$(PYTHON) -B $(SANITIZER_CLASSIFICATION_TEST) --bridge-root .

test-wb0-sanitize: test-wb0-sanitizer-classification
	$(PYTHON) -B $(SANITIZER_TEST) --compiler "$(CC)" --bridge-root .

test-wb0-posix-smoke:
	$(PYTHON) -B $(POSIX_TEST) --bridge-root .

audit-wb0: test check-wb0 test-wb0-determinism test-wb0-optimization-determinism test-wb0-sanitizer-classification test-wb0-sanitize test-wb0-posix-smoke
	@echo WB0 Brain Bridge audit OK

clean:
	@if exist "$(BUILD_ROOT)" rmdir /S /Q "$(BUILD_ROOT)"