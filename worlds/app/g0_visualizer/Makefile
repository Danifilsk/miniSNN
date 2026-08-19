CC ?= gcc
PYTHON ?= python
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Wformat=2 -Wstrict-prototypes

REPOSITORY_ROOT := ../../..
CORE_ROOT := $(REPOSITORY_ROOT)/core
KERNEL_ROOT := ../../kernel
DOMAIN_ROOT := ../../domain
BRIDGE_ROOT := ../../brain_bridge
TERRAIN_ROOT := ../../terrain
WF0_ROOT := ../../scenarios/wf0_fish
BUILD_ROOT := $(REPOSITORY_ROOT)/build/worlds/app/g0_visualizer
BIN_DIR := $(BUILD_ROOT)/bin
PRODUCT_BIN_DIR := $(REPOSITORY_ROOT)/build/studio/bin
TEST_DIR := $(BUILD_ROOT)/tests
RESULT_DIR := $(BUILD_ROOT)/results/g0_visualizer_demo
G0_D_RESULT_DIR := $(BUILD_ROOT)/results/g0_d_demo

CORE_LIB := $(REPOSITORY_ROOT)/build/core/lib/libminisnn_core.a
KERNEL_LIB := $(REPOSITORY_ROOT)/build/worlds/kernel/lib/libminisnn_worlds_kernel.a
DOMAIN_LIB := $(REPOSITORY_ROOT)/build/worlds/domain/lib/libminisnn_worlds_domain.a
TERRAIN_LIB := $(REPOSITORY_ROOT)/build/worlds/terrain/lib/libminisnn_worlds_terrain.a
BRIDGE_SOURCES := $(BRIDGE_ROOT)/src/minisnn_worlds_brain_bridge.c $(BRIDGE_ROOT)/src/minisnn_worlds_trainable_brain.c
WF0_SOURCES := $(WF0_ROOT)/src/wf0_fish.c $(WF0_ROOT)/src/wf0_fish_observation.c
SOURCES := src/g0_visualizer_assets.c src/g0_visualizer_camera.c src/g0_app_settings.c src/g0_event_log.c src/g0_world_config.c src/g0_world_document.c src/g0_visualizer_runtime.c src/g0_visualizer_ui_layout.c
WINDOW_SOURCES := app/g0_visualizer_sandbox_win32.c $(SOURCES) $(WF0_SOURCES) $(BRIDGE_SOURCES)
INCLUDES := -Iinclude -I$(CORE_ROOT)/include -I$(KERNEL_ROOT)/include -I$(DOMAIN_ROOT)/include -I$(BRIDGE_ROOT)/include -I$(TERRAIN_ROOT)/include -I$(WF0_ROOT)/include

HEADLESS_DEMO := $(BIN_DIR)/g0_visualizer_headless_demo.exe
SANDBOX_DEMO := $(BIN_DIR)/g0_visualizer_sandbox_demo.exe
G0_D_DEMO := $(BIN_DIR)/g0_visualizer_product_demo.exe
WINDOW_DEMO := $(BIN_DIR)/g0_visualizer.exe
WORLDS_PRODUCT := $(PRODUCT_BIN_DIR)/minisnn_worlds.exe
TEST := $(TEST_DIR)/test_g0_visualizer.exe
SANDBOX_TEST := $(TEST_DIR)/test_g0_sandbox.exe
G0_D_TEST := $(TEST_DIR)/test_g0_d.exe

.PHONY: all help test-g0-a test-g0-a-determinism test-g0-a-optimization-determinism \
        test-g0-a-sanitize test-g0-a-posix-smoke demo-g0-a check-g0-a audit-g0-a test-g0-b test-g0-b-runtime test-g0-b-facing test-g0-b-determinism test-g0-b-optimization-determinism test-g0-b-sanitize test-g0-b-posix-smoke demo-g0-b check-g0-b audit-g0-b test-g0-c test-g0-c-editor test-g0-c-camera test-g0-c-runtime test-g0-c-determinism test-g0-c-sanitize test-g0-c-posix-smoke demo-g0-c check-g0-c audit-g0-c test-g0-d test-g0-d-world-save-load test-g0-d-log test-g0-d-reset test-g0-d-menu-settings test-g0-d-settings test-g0-d-h test-g0-d-i test-g0-d-settings-layout test-g0-d-ui-resize test-g0-d-new-world-cancel test-g0-d-header-layout test-g0-d-determinism test-g0-d-sanitize test-g0-d-posix-smoke demo-g0-d check-g0-d audit-g0-d worlds-app clean

all: $(HEADLESS_DEMO) $(WINDOW_DEMO) $(TEST)

help:
	@echo G0-A visualizer targets:
	@echo   make test-g0-a                         - valida runtime, assets, camadas e bloqueio
	@echo   make test-g0-a-determinism             - compara dois demos headless
	@echo   make test-g0-a-optimization-determinism - compara resultados O0 e O2
	@echo   make test-g0-a-sanitize                - executa ASan/UBSan quando disponivel
	@echo   make test-g0-a-posix-smoke             - smoke POSIX quando disponivel
	@echo   make demo-g0-a                         - gera trace e resumo do visualizador
	@echo   make check-g0-a                        - valida os artefatos deterministas G0-A
	@echo   make audit-g0-a                        - executa a auditoria enxuta G0-A
	@echo   make test-g0-b                         - valida tempo real, layout e orientacao G0-B
	@echo   make test-g0-b-runtime                 - valida scheduler pausavel G0-B
	@echo   make test-g0-b-facing                  - valida facing somente apos movimento aplicado
	@echo   make audit-g0-b                        - executa a auditoria enxuta G0-B
	@echo   make test-g0-c                         - valida sandbox, editor, camera e pacing
	@echo   make audit-g0-c                        - executa a auditoria enxuta G0-C
	@echo   make worlds-app                        - gera build/studio/bin/minisnn_worlds.exe
	@echo G0-D product shell targets:
	@echo   make test-g0-d                         - valida persistencia, reset, log e shell
	@echo   make test-g0-d-settings                - valida configuracao visual e persistencia V1/V2
	@echo   make test-g0-d-h                       - valida Presentation Mode e resize transacional
	@echo   make test-g0-d-settings-layout         - valida geometria do painel Settings
	@echo   make test-g0-d-i                       - valida startup publicado e tipografia pixel
	@echo   make demo-g0-d                         - gera resumo e CSV de eventos
	@echo   make audit-g0-d                        - executa auditoria focada G0-D

$(BIN_DIR) $(TEST_DIR) $(RESULT_DIR) $(G0_D_RESULT_DIR) $(PRODUCT_BIN_DIR):
	@if not exist "$@" mkdir "$@"

$(CORE_LIB):
	$(MAKE) -C $(CORE_ROOT) core-lib

$(KERNEL_LIB):
	$(MAKE) -C $(KERNEL_ROOT) lib

$(DOMAIN_LIB): $(KERNEL_LIB)
	$(MAKE) -C $(DOMAIN_ROOT) lib

$(TERRAIN_LIB): $(KERNEL_LIB)
	$(MAKE) -C $(TERRAIN_ROOT) lib

$(HEADLESS_DEMO): app/g0_visualizer_headless_demo.c $(SOURCES) $(WF0_SOURCES) $(BRIDGE_SOURCES) $(TERRAIN_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SOURCES) $(WF0_SOURCES) $(BRIDGE_SOURCES) $(TERRAIN_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) -o $@

$(SANDBOX_DEMO): app/g0_visualizer_sandbox_demo.c $(SOURCES) $(WF0_SOURCES) $(BRIDGE_SOURCES) $(TERRAIN_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SOURCES) $(WF0_SOURCES) $(BRIDGE_SOURCES) $(TERRAIN_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) -o $@


$(G0_D_DEMO): app/g0_visualizer_product_demo.c $(SOURCES) $(WF0_SOURCES) $(BRIDGE_SOURCES) $(TERRAIN_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SOURCES) $(WF0_SOURCES) $(BRIDGE_SOURCES) $(TERRAIN_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) -o $@
$(WINDOW_DEMO): $(WINDOW_SOURCES) $(TERRAIN_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -mwindows $(WINDOW_SOURCES) $(TERRAIN_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) -lole32 -lwindowscodecs -lmsimg32 -lgdi32 -luser32 -o $@

$(WORLDS_PRODUCT): $(WINDOW_SOURCES) $(TERRAIN_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) | $(PRODUCT_BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -mwindows $(WINDOW_SOURCES) $(TERRAIN_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) -lole32 -lwindowscodecs -lmsimg32 -lgdi32 -luser32 -o $@

worlds-app: $(WORLDS_PRODUCT)

$(SANDBOX_TEST): tests/test_g0_sandbox.c $(SOURCES) $(WF0_SOURCES) $(BRIDGE_SOURCES) $(TERRAIN_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SOURCES) $(WF0_SOURCES) $(BRIDGE_SOURCES) $(TERRAIN_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) -o $@


$(G0_D_TEST): tests/test_g0_d.c $(SOURCES) $(WF0_SOURCES) $(BRIDGE_SOURCES) $(TERRAIN_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SOURCES) $(WF0_SOURCES) $(BRIDGE_SOURCES) $(TERRAIN_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) -o $@
$(TEST): tests/test_g0_visualizer.c $(SOURCES) $(WF0_SOURCES) $(BRIDGE_SOURCES) $(TERRAIN_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(SOURCES) $(WF0_SOURCES) $(BRIDGE_SOURCES) $(TERRAIN_LIB) $(DOMAIN_LIB) $(KERNEL_LIB) $(CORE_LIB) -o $@

test-g0-a: $(TEST)
	$(TEST) "$(REPOSITORY_ROOT)"
	$(PYTHON) -B tests/test_g0_visualizer_source_contracts.py --source "app/g0_visualizer_win32.c"

test-g0-b: $(TEST)
	$(TEST) "$(REPOSITORY_ROOT)"
	$(PYTHON) -B tests/test_g0_b_source_contracts.py --window-source "app/g0_visualizer_win32.c" --runtime-source "src/g0_visualizer_runtime.c" --runtime-header "include/g0_visualizer_runtime.h"

test-g0-b-runtime: test-g0-b

test-g0-b-facing: test-g0-b
demo-g0-a: $(HEADLESS_DEMO) | $(RESULT_DIR)
	@if exist "$(RESULT_DIR)" rmdir /S /Q "$(RESULT_DIR)"
	@if not exist "$(RESULT_DIR)" mkdir "$(RESULT_DIR)"
	$(HEADLESS_DEMO) "$(RESULT_DIR)" "$(REPOSITORY_ROOT)"

check-g0-a: demo-g0-a
	$(PYTHON) -B scripts/check_g0_a.py --directory "$(RESULT_DIR)"

test-g0-a-determinism: $(HEADLESS_DEMO)
	$(PYTHON) -B scripts/test_g0_a_determinism.py --demo "$(HEADLESS_DEMO)" --repository-root "$(REPOSITORY_ROOT)"

test-g0-a-optimization-determinism: $(CORE_LIB) $(KERNEL_LIB) $(DOMAIN_LIB) $(TERRAIN_LIB)
	$(PYTHON) -B scripts/test_g0_a_optimization_determinism.py --compiler "$(CC)" --visualizer-root "."

test-g0-a-sanitize:
	$(PYTHON) -B scripts/test_g0_a_sanitize.py --compiler "$(CC)" --visualizer-root "."

test-g0-a-posix-smoke:
	$(PYTHON) -B scripts/test_g0_a_posix_smoke.py --visualizer-root "."

audit-g0-a: test-g0-a test-g0-a-determinism test-g0-a-optimization-determinism test-g0-a-sanitize test-g0-a-posix-smoke check-g0-a
	@echo G0-A visualizer audit OK

test-g0-b-determinism: test-g0-a-determinism

test-g0-b-optimization-determinism: test-g0-a-optimization-determinism

test-g0-b-sanitize: test-g0-a-sanitize

test-g0-b-posix-smoke: test-g0-a-posix-smoke

demo-g0-b: demo-g0-a

check-g0-b: test-g0-b check-g0-a

audit-g0-b: test-g0-b test-g0-b-determinism test-g0-b-optimization-determinism test-g0-b-sanitize test-g0-b-posix-smoke check-g0-b
	@echo G0-B visualizer audit OK
test-g0-c: $(SANDBOX_TEST)
	$(SANDBOX_TEST)
	$(PYTHON) -B tests/test_g0_c_source_contracts.py --window-source "app/g0_visualizer_sandbox_win32.c" --runtime-source "src/g0_visualizer_runtime.c" --runtime-header "include/g0_visualizer_runtime.h"

test-g0-c-editor: test-g0-c

test-g0-c-camera: test-g0-c

test-g0-c-runtime: test-g0-c

test-g0-c-determinism: test-g0-c

test-g0-c-sanitize:
	$(PYTHON) -B scripts/test_g0_c_sanitize.py --compiler "$(CC)" --visualizer-root "."

test-g0-c-posix-smoke:
	$(PYTHON) -B scripts/test_g0_c_posix_smoke.py --visualizer-root "."

demo-g0-c: $(SANDBOX_DEMO) | $(RESULT_DIR)
	if not exist "$(RESULT_DIR)" mkdir "$(RESULT_DIR)"
	$(SANDBOX_DEMO) "$(RESULT_DIR)/g0_c_summary.txt"

check-g0-c: test-g0-c
	@echo G0-C interactive sandbox validation OK

audit-g0-c: test-g0-c test-g0-c-sanitize test-g0-c-posix-smoke test-g0-a test-g0-a-determinism test-g0-a-optimization-determinism test-g0-a-sanitize test-g0-a-posix-smoke check-g0-c
	@echo G0-C interactive sandbox audit OK
test-g0-d: $(G0_D_TEST)
	$(G0_D_TEST)
	$(PYTHON) -B tests/test_g0_d_source_contracts.py --window-source "app/g0_visualizer_sandbox_win32.c" --document-source "src/g0_world_document.c" --log-source "src/g0_event_log.c"

test-g0-d-world-save-load: test-g0-d

test-g0-d-log: test-g0-d

test-g0-d-reset: test-g0-d

test-g0-d-menu-settings: test-g0-d

test-g0-d-settings: $(G0_D_TEST)
	$(G0_D_TEST)

test-g0-d-h: test-g0-d-settings
	$(PYTHON) -B tests/test_g0_d_h_source_contracts.py --window-source "app/g0_visualizer_sandbox_win32.c" --settings-source "src/g0_app_settings.c" --document-source "src/g0_world_document.c" --config-source "src/g0_world_config.c" --runtime-source "src/g0_visualizer_runtime.c" --layout-source "src/g0_visualizer_ui_layout.c"
	@echo G0-D-H presentation and resize validation OK

test-g0-d-settings-layout: test-g0-d-h

# Startup, asset discovery, font, and Settings geometry contracts for G0-D-I.
test-g0-d-i: test-g0-d-settings-layout
	$(PYTHON) -B tests/test_g0_d_i_source_contracts.py --window-source "app/g0_visualizer_sandbox_win32.c" --layout-source "src/g0_visualizer_ui_layout.c"
	@echo G0-D-I startup and Settings validation OK

test-g0-d-ui-resize: test-g0-d-h

test-g0-d-new-world-cancel: test-g0-d-h

test-g0-d-header-layout: test-g0-d-h

test-g0-d-determinism: $(G0_D_DEMO)
	$(PYTHON) -B scripts/test_g0_d_determinism.py --demo "$(G0_D_DEMO)"

test-g0-d-sanitize:
	$(PYTHON) -B scripts/test_g0_d_sanitize.py --compiler "$(CC)" --visualizer-root "."

test-g0-d-posix-smoke:
	$(PYTHON) -B scripts/test_g0_d_posix_smoke.py --visualizer-root "."

demo-g0-d: $(G0_D_DEMO) | $(G0_D_RESULT_DIR)
	if not exist "$(G0_D_RESULT_DIR)" mkdir "$(G0_D_RESULT_DIR)"
	$(G0_D_DEMO) "$(G0_D_RESULT_DIR)/g0_d_summary.txt"

check-g0-d: test-g0-d test-g0-d-settings test-g0-d-h test-g0-d-i test-g0-d-settings-layout test-g0-d-ui-resize test-g0-d-new-world-cancel test-g0-d-header-layout test-g0-d-determinism
	@echo G0-D product shell validation OK

audit-g0-d: test-g0-d test-g0-d-settings test-g0-d-h test-g0-d-i test-g0-d-settings-layout test-g0-d-ui-resize test-g0-d-new-world-cancel test-g0-d-header-layout test-g0-d-determinism test-g0-d-sanitize test-g0-d-posix-smoke check-g0-d
	@echo G0-D product shell audit OK
clean:
	@if exist "$(BUILD_ROOT)" rmdir /S /Q "$(BUILD_ROOT)"
	@if exist "$(WORLDS_PRODUCT)" del /Q "$(WORLDS_PRODUCT)"