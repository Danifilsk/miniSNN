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
SOURCES := src/minisnn_worlds_brain_bridge.c src/minisnn_worlds_trainable_brain.c
OBJECTS := $(BUILD_ROOT)/obj/minisnn_worlds_brain_bridge.o $(BUILD_ROOT)/obj/minisnn_worlds_trainable_brain.o
CORE_LIB := $(REPOSITORY_ROOT)/build/core/lib/libminisnn_core.a
DOMAIN_LIB := $(REPOSITORY_ROOT)/build/worlds/domain/lib/libminisnn_worlds_domain.a
KERNEL_LIB := $(REPOSITORY_ROOT)/build/worlds/kernel/lib/libminisnn_worlds_kernel.a
KERNEL_SOURCES := $(KERNEL_ROOT)/src/minisnn_worlds_kernel.c $(KERNEL_ROOT)/src/minisnn_worlds_kernel_snapshot.c $(KERNEL_ROOT)/src/minisnn_worlds_kernel_command_log.c
TEST := $(TEST_DIR)/test_wb0_brain_bridge.exe
WB1_TEST := $(TEST_DIR)/test_wb1_trainable_brain.exe
WB1_DEMO := $(BIN_DIR)/wb1_trainable_brain_demo.exe
WB1_CHECKER := scripts/check_wb1.py
WB1_DETERMINISM_TEST := tests/test_wb1_determinism.py
WB1_OPTIMIZATION_TEST := tests/test_wb1_optimization_determinism.py
WB1_SANITIZER_TEST := tests/test_wb1_sanitize.py
WB1_POSIX_TEST := tests/test_wb1_posix_smoke.py
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
        demo-wb0 check-wb0 audit-wb0 test-wb1-bridge test-wb1-brain-config test-wb1-brain-persistence test-wb1-reward test-wb1-episode-reset test-wb1-determinism test-wb1-optimization-determinism test-wb1-sanitize test-wb1-posix-smoke demo-wb1 check-wb1 audit-wb1

all: lib $(DEMO)

$(BUILD_ROOT)/obj $(LIB_DIR) $(BIN_DIR) $(TEST_DIR):
	@if not exist "$@" mkdir "$@"

$(CORE_LIB):
	$(MAKE) -C $(CORE_ROOT) core-lib

$(KERNEL_LIB):
	$(MAKE) -C $(KERNEL_ROOT) lib

$(DOMAIN_LIB): $(KERNEL_LIB)
	$(MAKE) -C $(DOMAIN_ROOT) lib

$(BUILD_ROOT)/obj/minisnn_worlds_brain_bridge.o: src/minisnn_worlds_brain_bridge.c include/minisnn_worlds_brain_bridge.h | $(BUILD_ROOT)/obj
	$(CC) $(CFLAGS) -Iinclude -I$(CORE_ROOT)/include -I$(DOMAIN_ROOT)/include -I$(KERNEL_ROOT)/include -c $< -o $@

$(BUILD_ROOT)/obj/minisnn_worlds_trainable_brain.o: src/minisnn_worlds_trainable_brain.c include/minisnn_worlds_brain_bridge.h | $(BUILD_ROOT)/obj
	$(CC) $(CFLAGS) -Iinclude -I$(CORE_ROOT)/include -I$(DOMAIN_ROOT)/include -I$(KERNEL_ROOT)/include -c $< -o $@

$(LIB): $(OBJECTS) | $(LIB_DIR)
	$(AR) rcs $@ $^

$(TEST): tests/test_wb0_brain_bridge.c $(SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) -Iinclude -I$(CORE_ROOT)/include -I$(DOMAIN_ROOT)/include -I$(KERNEL_ROOT)/include $< $(SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) -o $@

$(RETRY_TEST): tests/test_wb0_failure_retry.c $(SOURCES) $(CORE_LIB) $(DOMAIN_ROOT)/src/minisnn_worlds_domain.c $(KERNEL_SOURCES) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING -Iinclude -I$(CORE_ROOT)/include -I$(DOMAIN_ROOT)/include -I$(KERNEL_ROOT)/include $< $(SOURCES) $(DOMAIN_ROOT)/src/minisnn_worlds_domain.c $(KERNEL_SOURCES) $(CORE_LIB) -o $@

$(DEMO): app/wb0_brain_bridge_demo.c $(SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -Iinclude -I$(CORE_ROOT)/include -I$(DOMAIN_ROOT)/include -I$(KERNEL_ROOT)/include $< $(SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) -o $@

test-wb0-encoder test-wb0-decoder test-wb0-binding test-wb0-core-integration test-wb0-cache test-wb0-multi-organism test-wb0-invariants: $(TEST)
	$(TEST)

$(WB1_TEST): tests/test_wb1_trainable_brain.c $(SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_TESTING -Iinclude -I$(CORE_ROOT)/include -I$(DOMAIN_ROOT)/include -I$(KERNEL_ROOT)/include $< $(SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) -o $@

$(WB1_DEMO): app/wb1_trainable_brain_demo.c $(SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -Iinclude -I$(CORE_ROOT)/include -I$(DOMAIN_ROOT)/include -I$(KERNEL_ROOT)/include $< $(SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) -o $@

test-wb1-bridge test-wb1-brain-config test-wb1-brain-persistence test-wb1-reward test-wb1-episode-reset: $(WB1_TEST)
	@cd "$(TEST_DIR)" && test_wb1_trainable_brain.exe

test-wb1-determinism: $(WB1_TEST)
	@cd "$(TEST_DIR)" && test_wb1_trainable_brain.exe

test-wb1-optimization-determinism:
	$(PYTHON) -B $(WB1_OPTIMIZATION_TEST) --compiler "$(CC)" --bridge-root "."

test-wb1-sanitize:
	$(PYTHON) -B $(WB1_SANITIZER_TEST) --compiler "$(CC)" --bridge-root "."

test-wb1-posix-smoke:
	$(PYTHON) -B $(WB1_POSIX_TEST) --bridge-root "."

demo-wb1: $(WB1_DEMO)
	@if not exist "$(BUILD_ROOT)/results/wb1_demo" mkdir "$(BUILD_ROOT)/results/wb1_demo"
	@cd "$(BUILD_ROOT)/results" && ..\bin\wb1_trainable_brain_demo.exe "wb1_demo"

check-wb1: demo-wb1
	$(PYTHON) -B $(WB1_CHECKER) --directory "$(BUILD_ROOT)/results/wb1_demo"

audit-wb1: test-wb1-bridge test-wb1-brain-config test-wb1-brain-persistence test-wb1-reward test-wb1-episode-reset test-wb1-determinism test-wb1-optimization-determinism test-wb1-sanitize test-wb1-posix-smoke check-wb1
	@echo WB1 Trainable Brain Bridge audit OK
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