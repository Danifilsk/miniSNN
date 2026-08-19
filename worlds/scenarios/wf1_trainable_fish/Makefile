CC ?= gcc
PYTHON ?= python
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Wformat=2 -Wstrict-prototypes

REPOSITORY_ROOT := ../../..
CORE_ROOT := $(REPOSITORY_ROOT)/core
KERNEL_ROOT := ../../kernel
DOMAIN_ROOT := ../../domain
BRIDGE_ROOT := ../../brain_bridge
BUILD_ROOT := $(REPOSITORY_ROOT)/build/worlds/scenarios/wf1_trainable_fish
BIN_DIR := $(BUILD_ROOT)/bin
TEST_DIR := $(BUILD_ROOT)/tests
RESULT_DIR := $(BUILD_ROOT)/results
A1_RESULT_DIR := $(RESULT_DIR)/wf1_a1
A2_RESULT_DIR := $(RESULT_DIR)/wf1_a2
A3_RESULT_DIR := $(RESULT_DIR)/wf1_a3
A4_RESULT_DIR := $(RESULT_DIR)/wf1_a4
A4_PILOT_RESULT_DIR := $(RESULT_DIR)/wf1_a4_pilot
A5_RESULT_DIR := $(RESULT_DIR)/wf1_a5
CORE_LIB := $(REPOSITORY_ROOT)/build/core/lib/libminisnn_core.a
KERNEL_LIB := $(REPOSITORY_ROOT)/build/worlds/kernel/lib/libminisnn_worlds_kernel.a
DOMAIN_LIB := $(REPOSITORY_ROOT)/build/worlds/domain/lib/libminisnn_worlds_domain.a
INCLUDES := -Iinclude -I$(CORE_ROOT)/include -I$(KERNEL_ROOT)/include -I$(DOMAIN_ROOT)/include -I$(BRIDGE_ROOT)/include
SCENARIO_SOURCES := src/wf1_trainable_fish.c
A1_SOURCES := $(SCENARIO_SOURCES) src/wf1_a1_exploration.c
A2_SOURCES := $(SCENARIO_SOURCES) src/wf1_a2_terminal_ablation.c
A3_SOURCES := $(SCENARIO_SOURCES) src/wf1_a2_terminal_ablation.c src/wf1_a3_reward_probe.c
A4_SOURCES := $(SCENARIO_SOURCES) src/wf1_a2_terminal_ablation.c src/wf1_a4_saturation_probe.c
A4_PILOT_SOURCES := $(SCENARIO_SOURCES) src/wf1_a2_terminal_ablation.c src/wf1_a4_pilot.c
A5_SOURCES := $(SCENARIO_SOURCES) src/wf1_a2_terminal_ablation.c src/wf1_a5_tau_probe.c
BRIDGE_SOURCES := $(BRIDGE_ROOT)/src/minisnn_worlds_brain_bridge.c $(BRIDGE_ROOT)/src/minisnn_worlds_trainable_brain.c
DEMO := $(BIN_DIR)/wf1_trainable_fish_experiment.exe
A1_DEMO := $(BIN_DIR)/wf1_a1_exploration_experiment.exe
A2_DEMO := $(BIN_DIR)/wf1_a2_terminal_ablation_experiment.exe
A3_DEMO := $(BIN_DIR)/wf1_a3_reward_probe_experiment.exe
A4_DEMO := $(BIN_DIR)/wf1_a4_saturation_probe_experiment.exe
A4_PILOT_DEMO := $(BIN_DIR)/wf1_a4_pilot_experiment.exe
A5_DEMO := $(BIN_DIR)/wf1_a5_tau_probe_experiment.exe
TEST := $(TEST_DIR)/test_wf1_trainable_fish.exe
CHECKER := scripts/check_wf1_a.py
A1_CHECKER := scripts/check_wf1_a1.py
DETERMINISM := scripts/test_wf1_a_determinism.py
A1_DETERMINISM := scripts/test_wf1_a1_determinism.py
POLICY_CHECK := scripts/test_wf1_a_no_scripted_policy.py
A1_POLICY_CHECK := scripts/test_wf1_a1_no_scripted_policy.py
POSIX_SMOKE := scripts/test_wf1_a_posix_smoke.py

.PHONY: all test-wf1-a-episode test-wf1-a-terminal-feedback \
        test-wf1-a-evaluation-freeze test-wf1-a-save-load \
        test-wf1-a-no-scripted-policy test-wf1-a-determinism test-wf1-a-posix-smoke \
        experiment-wf1-a demo-wf1-a check-wf1-a audit-wf1-a \
        test-wf1-a1-metrics test-wf1-a1-no-scripted-policy test-wf1-a1-determinism \
        experiment-wf1-a1 demo-wf1-a1 check-wf1-a1 audit-wf1-a1 clean

all: $(DEMO) $(TEST) $(A1_DEMO) $(A2_DEMO) $(A3_DEMO) $(A4_DEMO) $(A4_PILOT_DEMO) $(A5_DEMO)

$(BIN_DIR) $(TEST_DIR) $(RESULT_DIR):
	@if not exist "$@" mkdir "$@"

$(CORE_LIB):
	$(MAKE) -C $(CORE_ROOT) core-lib

$(KERNEL_LIB):
	$(MAKE) -C $(KERNEL_ROOT) lib

$(DOMAIN_LIB): $(KERNEL_LIB)
	$(MAKE) -C $(DOMAIN_ROOT) lib

$(DEMO): app/wf1_trainable_fish_experiment.c $(SCENARIO_SOURCES) $(BRIDGE_SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SCENARIO_SOURCES) $(BRIDGE_SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) -o $@

$(A1_DEMO): app/wf1_a1_exploration_experiment.c $(A1_SOURCES) $(BRIDGE_SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(A1_SOURCES) $(BRIDGE_SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) -o $@

$(A2_DEMO): app/wf1_a2_terminal_ablation_experiment.c $(A2_SOURCES) $(BRIDGE_SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(A2_SOURCES) $(BRIDGE_SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) -o $@

$(A3_DEMO): app/wf1_a3_reward_probe_experiment.c $(A3_SOURCES) $(BRIDGE_SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(A3_SOURCES) $(BRIDGE_SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) -o $@

$(A4_DEMO): app/wf1_a4_saturation_probe_experiment.c $(A4_SOURCES) $(BRIDGE_SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(A4_SOURCES) $(BRIDGE_SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) -o $@

$(A4_PILOT_DEMO): app/wf1_a4_pilot_experiment.c $(A4_PILOT_SOURCES) $(BRIDGE_SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(A4_PILOT_SOURCES) $(BRIDGE_SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) -o $@

$(A5_DEMO): app/wf1_a5_tau_probe_experiment.c $(A5_SOURCES) $(BRIDGE_SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(A5_SOURCES) $(BRIDGE_SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) -o $@
$(TEST): tests/test_wf1_trainable_fish.c $(SCENARIO_SOURCES) $(BRIDGE_SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_TESTING $(INCLUDES) $< $(SCENARIO_SOURCES) $(BRIDGE_SOURCES) $(CORE_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) -o $@

test-wf1-a-episode test-wf1-a-evaluation-freeze test-wf1-a-save-load test-wf1-a1-metrics: $(TEST)
	@cd "$(REPOSITORY_ROOT)" && build\worlds\scenarios\wf1_trainable_fish\tests\test_wf1_trainable_fish.exe

test-wf1-a-terminal-feedback: $(TEST)
	@cd "$(REPOSITORY_ROOT)" && build\worlds\scenarios\wf1_trainable_fish\tests\test_wf1_trainable_fish.exe
	$(MAKE) -C $(BRIDGE_ROOT) test-wb1-bridge

test-wf1-a-no-scripted-policy:
	$(PYTHON) -B $(POLICY_CHECK) --scenario-root .

experiment-wf1-a demo-wf1-a: $(DEMO) | $(RESULT_DIR)
	@for %%F in (summary.txt training.csv evaluation.csv seeds.csv brain_*.wb1) do @if exist "$(RESULT_DIR)\%%F" del /Q "$(RESULT_DIR)\%%F"
	@cd "$(REPOSITORY_ROOT)" && build\worlds\scenarios\wf1_trainable_fish\bin\wf1_trainable_fish_experiment.exe "build/worlds/scenarios/wf1_trainable_fish/results"

check-wf1-a: experiment-wf1-a
	$(PYTHON) -B $(CHECKER) --directory "$(RESULT_DIR)"

test-wf1-a-determinism: $(DEMO)
	$(PYTHON) -B $(DETERMINISM) --demo "$(DEMO)"

test-wf1-a-posix-smoke:
	$(PYTHON) -B $(POSIX_SMOKE) --scenario-root .

test-wf1-a1-no-scripted-policy:
	$(PYTHON) -B $(A1_POLICY_CHECK) --scenario-root .

experiment-wf1-a1 demo-wf1-a1: $(A1_DEMO) | $(RESULT_DIR)
	@if exist "$(A1_RESULT_DIR)" rmdir /S /Q "$(A1_RESULT_DIR)"
	@if not exist "$(A1_RESULT_DIR)" mkdir "$(A1_RESULT_DIR)"
	@cd "$(REPOSITORY_ROOT)" && build\worlds\scenarios\wf1_trainable_fish\bin\wf1_a1_exploration_experiment.exe "build/worlds/scenarios/wf1_trainable_fish/results/wf1_a1"

check-wf1-a1: experiment-wf1-a1
	$(PYTHON) -B $(A1_CHECKER) --directory "$(A1_RESULT_DIR)"

test-wf1-a1-determinism: $(A1_DEMO)
	$(PYTHON) -B $(A1_DETERMINISM) --demo "$(A1_DEMO)"

audit-wf1-a: test-wf1-a-episode test-wf1-a-terminal-feedback test-wf1-a-evaluation-freeze test-wf1-a-save-load test-wf1-a-no-scripted-policy test-wf1-a-determinism test-wf1-a-posix-smoke check-wf1-a
	$(MAKE) -C $(DOMAIN_ROOT) test-wd2-lifecycle
	$(MAKE) -C ../wf0_fish test-wf0
	@echo WF1-A trainable fish audit OK

audit-wf1-a1:
	$(MAKE) audit-wf1-a
	$(MAKE) test-wf1-a1-metrics test-wf1-a1-no-scripted-policy test-wf1-a1-determinism check-wf1-a1
	@echo WF1-A.1 exploration bootstrap audit OK

experiment-wf1-a2 demo-wf1-a2: $(A2_DEMO) | $(RESULT_DIR)
	@if exist "$(A2_RESULT_DIR)" rmdir /S /Q "$(A2_RESULT_DIR)"
	@if not exist "$(A2_RESULT_DIR)" mkdir "$(A2_RESULT_DIR)"
	@cd "$(REPOSITORY_ROOT)" && build\worlds\scenarios\wf1_trainable_fish\bin\wf1_a2_terminal_ablation_experiment.exe "build/worlds/scenarios/wf1_trainable_fish/results/wf1_a2"

check-wf1-a2: experiment-wf1-a2
	$(PYTHON) -B scripts/check_wf1_a2.py --directory "$(A2_RESULT_DIR)"

test-wf1-a2-determinism: $(A2_DEMO)
	$(PYTHON) -B scripts/test_wf1_a2_determinism.py --demo "$(A2_DEMO)"

test-wf1-a2-no-scripted-policy:
	$(PYTHON) -B scripts/test_wf1_a1_no_scripted_policy.py --scenario-root .

audit-wf1-a2: test-wf1-a1-metrics test-wf1-a2-no-scripted-policy test-wf1-a2-determinism check-wf1-a2
	$(MAKE) -C $(BRIDGE_ROOT) test-wb1-bridge
	$(MAKE) -C $(DOMAIN_ROOT) test-wd2-lifecycle
	$(MAKE) -C ../wf0_fish test-wf0
	@echo WF1-A.2 terminal reward ablation audit OK

probe-wf1-a3: $(A3_DEMO) | $(RESULT_DIR)
	@if exist "$(A3_RESULT_DIR)" rmdir /S /Q "$(A3_RESULT_DIR)"
	@if not exist "$(A3_RESULT_DIR)" mkdir "$(A3_RESULT_DIR)"
	@cd "$(REPOSITORY_ROOT)" && build\worlds\scenarios\wf1_trainable_fish\bin\wf1_a3_reward_probe_experiment.exe "build/worlds/scenarios/wf1_trainable_fish/results/wf1_a3"

check-wf1-a3: probe-wf1-a3
	$(PYTHON) -B scripts/check_wf1_a3.py --directory "$(A3_RESULT_DIR)"

test-wf1-a3-determinism: $(A3_DEMO)
	$(PYTHON) -B scripts/test_wf1_a3_determinism.py --demo "$(A3_DEMO)"

test-wf1-a3-no-scripted-policy:
	$(PYTHON) -B scripts/test_wf1_a1_no_scripted_policy.py --scenario-root .

probe-wf1-a4: $(A4_DEMO) | $(RESULT_DIR)
	@if exist "$(A4_RESULT_DIR)" rmdir /S /Q "$(A4_RESULT_DIR)"
	@if not exist "$(A4_RESULT_DIR)" mkdir "$(A4_RESULT_DIR)"
	@cd "$(REPOSITORY_ROOT)" && build\worlds\scenarios\wf1_trainable_fish\bin\wf1_a4_saturation_probe_experiment.exe "build/worlds/scenarios/wf1_trainable_fish/results/wf1_a4"

check-wf1-a4: probe-wf1-a4
	$(PYTHON) -B scripts/check_wf1_a4.py --directory "$(A4_RESULT_DIR)"

test-wf1-a4-determinism: $(A4_DEMO)
	$(PYTHON) -B scripts/test_wf1_a4_determinism.py --demo "$(A4_DEMO)"

pilot-wf1-a4: $(A4_PILOT_DEMO) | $(RESULT_DIR)
	@if exist "$(A4_PILOT_RESULT_DIR)" rmdir /S /Q "$(A4_PILOT_RESULT_DIR)"
	@if not exist "$(A4_PILOT_RESULT_DIR)" mkdir "$(A4_PILOT_RESULT_DIR)"
	@cd "$(REPOSITORY_ROOT)" && build\worlds\scenarios\wf1_trainable_fish\bin\wf1_a4_pilot_experiment.exe "build/worlds/scenarios/wf1_trainable_fish/results/wf1_a4_pilot"

check-wf1-a4-pilot: pilot-wf1-a4
	$(PYTHON) -B scripts/check_wf1_a4_pilot.py --directory "$(A4_PILOT_RESULT_DIR)"

probe-wf1-a5: $(A5_DEMO) | $(RESULT_DIR)
	@if exist "$(A5_RESULT_DIR)" rmdir /S /Q "$(A5_RESULT_DIR)"
	@if not exist "$(A5_RESULT_DIR)" mkdir "$(A5_RESULT_DIR)"
	@cd "$(REPOSITORY_ROOT)" && build\worlds\scenarios\wf1_trainable_fish\bin\wf1_a5_tau_probe_experiment.exe "build/worlds/scenarios/wf1_trainable_fish/results/wf1_a5"

check-wf1-a5: probe-wf1-a5
	$(PYTHON) -B scripts/check_wf1_a5.py --directory "$(A5_RESULT_DIR)"

test-wf1-a5-determinism: $(A5_DEMO)
	$(PYTHON) -B scripts/test_wf1_a5_determinism.py --demo "$(A5_DEMO)"
clean:
	@if exist "$(BUILD_ROOT)" rmdir /S /Q "$(BUILD_ROOT)"
.PHONY: experiment-wf1-a2 demo-wf1-a2 check-wf1-a2 test-wf1-a2-determinism test-wf1-a2-no-scripted-policy audit-wf1-a2
.PHONY: probe-wf1-a3 check-wf1-a3 test-wf1-a3-determinism test-wf1-a3-no-scripted-policy
.PHONY: probe-wf1-a4 check-wf1-a4 test-wf1-a4-determinism
.PHONY: pilot-wf1-a4 check-wf1-a4-pilot
.PHONY: probe-wf1-a5 check-wf1-a5 test-wf1-a5-determinism
