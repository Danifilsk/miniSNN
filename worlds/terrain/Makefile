CC ?= gcc
AR ?= ar
PYTHON ?= python
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Wformat=2 -Wstrict-prototypes

REPOSITORY_ROOT := ../..
CORE_ROOT := $(REPOSITORY_ROOT)/core
KERNEL_ROOT := ../kernel
DOMAIN_ROOT := ../domain
BRIDGE_ROOT := ../brain_bridge
WF0_ROOT := ../scenarios/wf0_fish
BUILD_ROOT := $(REPOSITORY_ROOT)/build/worlds/terrain
LIB_DIR := $(BUILD_ROOT)/lib
BIN_DIR := $(BUILD_ROOT)/bin
TEST_DIR := $(BUILD_ROOT)/tests
RESULT_DIR := $(BUILD_ROOT)/results/wt0_terrain_demo
TERRAIN_SOURCE := src/minisnn_worlds_terrain.c
KERNEL_TEST_SOURCES := $(KERNEL_ROOT)/src/minisnn_worlds_kernel.c $(KERNEL_ROOT)/src/minisnn_worlds_kernel_snapshot.c $(KERNEL_ROOT)/src/minisnn_worlds_kernel_restore.c $(KERNEL_ROOT)/src/minisnn_worlds_kernel_command_log.c
TERRAIN_LIB := $(LIB_DIR)/libminisnn_worlds_terrain.a
KERNEL_LIB := $(REPOSITORY_ROOT)/build/worlds/kernel/lib/libminisnn_worlds_kernel.a
DOMAIN_LIB := $(REPOSITORY_ROOT)/build/worlds/domain/lib/libminisnn_worlds_domain.a
CORE_LIB := $(REPOSITORY_ROOT)/build/core/lib/libminisnn_core.a
BRIDGE_SOURCE := $(BRIDGE_ROOT)/src/minisnn_worlds_brain_bridge.c
WF0_SOURCES := $(WF0_ROOT)/src/wf0_fish.c $(WF0_ROOT)/src/wf0_fish_observation.c
INCLUDES := -Iinclude -I$(KERNEL_ROOT)/include
FISH_INCLUDES := $(INCLUDES) -I$(DOMAIN_ROOT)/include -I$(BRIDGE_ROOT)/include -I$(WF0_ROOT)/include -I$(CORE_ROOT)/include
DEMO := $(BIN_DIR)/wt0_terrain_demo.exe
TEST := $(TEST_DIR)/test_wt0_terrain.exe
FISH_TEST := $(TEST_DIR)/test_wt0_fish_smoke.exe
MATERIALIZATION_FAILURE_TEST := $(TEST_DIR)/test_wt0_materialization_failure.exe
CATEGORY_MASK_TEST := $(TEST_DIR)/test_wt0_category_mask.exe
CHECKER := scripts/check_wt0.py
DETERMINISM := scripts/test_wt0_determinism.py
OPTIMIZATION := scripts/test_wt0_optimization_determinism.py
SANITIZER := scripts/test_wt0_sanitize.py
POSIX_SMOKE := scripts/test_wt0_posix_smoke.py

.PHONY: all lib test-wt0 test-wt0-materialization-failure test-wt0-category-mask \
        test-wt0-determinism test-wt0-optimization-determinism test-wt0-sanitize \
        test-wt0-posix-smoke demo-wt0 check-wt0 audit-wt0

all: lib $(DEMO) $(TEST) $(FISH_TEST) $(MATERIALIZATION_FAILURE_TEST) $(CATEGORY_MASK_TEST)

$(LIB_DIR) $(BIN_DIR) $(TEST_DIR) $(RESULT_DIR):
	@if not exist "$@" mkdir "$@"

.PHONY: kernel-lib

kernel-lib:
	$(MAKE) -C $(KERNEL_ROOT) lib

$(KERNEL_LIB): kernel-lib

$(DOMAIN_LIB): $(KERNEL_LIB)
	$(MAKE) -C $(DOMAIN_ROOT) lib

$(CORE_LIB):
	$(MAKE) -C $(CORE_ROOT) core-lib

$(TERRAIN_LIB): $(TERRAIN_SOURCE) include/minisnn_worlds_terrain.h | $(LIB_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $(TERRAIN_SOURCE) -o $(BUILD_ROOT)/terrain.o
	$(AR) rcs $@ $(BUILD_ROOT)/terrain.o

lib: $(TERRAIN_LIB)

$(DEMO): app/wt0_terrain_demo.c $(TERRAIN_LIB) kernel-lib | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(TERRAIN_LIB) $(KERNEL_LIB) -o $@

$(TEST): tests/test_wt0_terrain.c $(TERRAIN_LIB) kernel-lib | $(TEST_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(TERRAIN_LIB) $(KERNEL_LIB) -o $@

$(FISH_TEST): tests/test_wt0_fish_smoke.c $(TERRAIN_SOURCE) $(WF0_SOURCES) $(BRIDGE_SOURCE) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) $(FISH_INCLUDES) $< $(TERRAIN_SOURCE) $(WF0_SOURCES) $(BRIDGE_SOURCE) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) -lm -o $@

$(MATERIALIZATION_FAILURE_TEST): tests/test_wt0_materialization_failure.c $(TERRAIN_SOURCE) $(KERNEL_TEST_SOURCES) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_KERNEL_TESTING $(INCLUDES) $< $(TERRAIN_SOURCE) $(KERNEL_TEST_SOURCES) -o $@

$(CATEGORY_MASK_TEST): tests/test_wt0_category_mask.c $(TERRAIN_SOURCE) kernel-lib | $(TEST_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(TERRAIN_SOURCE) $(KERNEL_LIB) -o $@

test-wt0: $(TEST) $(FISH_TEST) $(MATERIALIZATION_FAILURE_TEST) $(CATEGORY_MASK_TEST)
	$(TEST)
	$(FISH_TEST)
	$(MATERIALIZATION_FAILURE_TEST)
	$(CATEGORY_MASK_TEST)

test-wt0-materialization-failure: $(MATERIALIZATION_FAILURE_TEST)
	$(MATERIALIZATION_FAILURE_TEST)

test-wt0-category-mask: $(CATEGORY_MASK_TEST)
	$(CATEGORY_MASK_TEST)

demo-wt0: $(DEMO) | $(RESULT_DIR)
	$(DEMO) "$(RESULT_DIR)"

check-wt0: demo-wt0
	$(PYTHON) -B $(CHECKER) --directory "$(RESULT_DIR)"

test-wt0-determinism: $(DEMO)
	$(PYTHON) -B $(DETERMINISM) --demo "$(DEMO)"

test-wt0-optimization-determinism:
	$(PYTHON) -B $(OPTIMIZATION) --compiler "$(CC)" --terrain-root "."

test-wt0-sanitize:
	$(PYTHON) -B $(SANITIZER) --compiler "$(CC)" --terrain-root "."

test-wt0-posix-smoke:
	$(PYTHON) -B $(POSIX_SMOKE) --terrain-root "."

audit-wt0: test-wt0 test-wt0-determinism test-wt0-optimization-determinism test-wt0-sanitize test-wt0-posix-smoke check-wt0
	@echo WT0 terrain audit OK

