CC ?= gcc
AR ?= ar
PYTHON ?= python
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Wformat=2 -Wstrict-prototypes

REPOSITORY_ROOT := ../..
KERNEL_ROOT := ../kernel
BUILD_ROOT := $(REPOSITORY_ROOT)/build/worlds/domain
LIB_DIR := $(BUILD_ROOT)/lib
BIN_DIR := $(BUILD_ROOT)/bin
OBJ_DIR := $(BUILD_ROOT)/obj
TEST_DIR := $(BUILD_ROOT)/tests
RESULT_DIR := $(BUILD_ROOT)/results/wd0_demo
WD1_RESULT_DIR := $(BUILD_ROOT)/results/wd1_demo
PUBLIC_INCLUDE_DIR := include
KERNEL_INCLUDE_DIR := $(KERNEL_ROOT)/include
KERNEL_LIB := $(REPOSITORY_ROOT)/build/worlds/kernel/lib/libminisnn_worlds_kernel.a
DOMAIN_LIB := $(LIB_DIR)/libminisnn_worlds_domain.a
DOMAIN_OBJECT := $(OBJ_DIR)/minisnn_worlds_domain.o
DOMAIN_SOURCE := src/minisnn_worlds_domain.c
WD1_FILE_ADAPTER_SOURCE := app/wd1_domain_snapshot_file.c
DEMO := $(BIN_DIR)/wd0_domain_demo.exe
DOMAIN_TEST := $(TEST_DIR)/test_wd0_domain.exe
ACTIONS_TEST := $(TEST_DIR)/test_wd0_actions.exe
PERCEPTION_TEST := $(TEST_DIR)/test_wd0_perception.exe
INVARIANTS_TEST := $(TEST_DIR)/test_wd0_invariants.exe
ATOMICITY_TEST := $(TEST_DIR)/test_wd0_atomicity.exe
STRESS_TEST := $(TEST_DIR)/test_wd0_stress.exe
WD1_SNAPSHOT_TEST := $(TEST_DIR)/test_wd1_snapshot.exe
WD1_FILE_TEST := $(TEST_DIR)/test_wd1_file_roundtrip.exe
WD1_DEMO := $(BIN_DIR)/wd1_domain_persistence_demo.exe

.PHONY: all lib test clean demo-wd0 check-wd0 audit-wd0 \
        test-wd0-domain test-wd0-actions test-wd0-perception test-wd0-invariants test-wd0-atomicity \
        test-wd0-stress test-wd0-determinism test-wd0-optimization-determinism \
        test-wd0-sanitizer-classification test-wd0-sanitize test-wd0-posix-smoke \
        test-wd1-snapshot test-wd1-restore test-wd1-corruption test-wd1-file-roundtrip \
        test-wd1-determinism test-wd1-optimization-determinism test-wd1-sanitizer-classification \
        test-wd1-sanitize test-wd1-posix-smoke test-wd1-golden-portability demo-wd1 check-wd1 audit-wd1

all: lib $(DEMO)

lib: $(DOMAIN_LIB)

help:
	@echo Worlds Domain WD0/WD1 targets:
	@echo   make test-wd0-domain                    - valida registros semanticos, energia e hash
	@echo   make test-wd0-actions                   - valida WAIT, MOVE, EAT, competicao e metabolismo
	@echo   make test-wd0-perception                - valida Manhattan e desempate por EntityId
	@echo   make test-wd0-invariants                - valida corrupcoes test-only
	@echo   make test-wd0-atomicity                 - valida rollback Domain-Kernel e retry
	@echo   make test-wd0-stress                    - executa 256 organismos, 512 alimentos, 200 ticks
	@echo   make demo-wd0                           - gera artefatos deterministas WD0
	@echo   make check-wd0                          - valida conteudo real dos artefatos
	@echo   make audit-wd0                          - executa a auditoria completa WD0
	@echo   make test-wd1-snapshot                - valida captura, restore, corrupcao e binding Kernel
	@echo   make test-wd1-file-roundtrip          - valida save/load atomico do snapshot Domain
	@echo   make demo-wd1                         - gera o demo deterministico de persistencia WD1
	@echo   make check-wd1                        - valida o artefato canonico WD1
	@echo   make test-wd1-golden-portability      - valida golden textual LF e rejeicao semantica
	@echo   make audit-wd1                        - executa a auditoria enxuta WD1

$(OBJ_DIR) $(LIB_DIR) $(BIN_DIR) $(TEST_DIR) $(RESULT_DIR) $(WD1_RESULT_DIR):
	@if not exist "$@" mkdir "$@"

$(KERNEL_LIB):
	$(MAKE) -C $(KERNEL_ROOT) lib

$(DOMAIN_OBJECT): $(DOMAIN_SOURCE) $(wildcard $(PUBLIC_INCLUDE_DIR)/*.h) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) -I$(KERNEL_INCLUDE_DIR) -c $< -o $@

$(DOMAIN_LIB): $(DOMAIN_OBJECT) | $(LIB_DIR)
	$(AR) rcs $@ $^

$(DEMO): app/wd0_domain_demo.c $(DOMAIN_LIB) $(KERNEL_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) -I$(KERNEL_INCLUDE_DIR) $< $(DOMAIN_LIB) $(KERNEL_LIB) -o $@

$(DOMAIN_TEST): tests/test_wd0_domain.c $(DOMAIN_SOURCE) $(KERNEL_LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_DOMAIN_TESTING -I$(PUBLIC_INCLUDE_DIR) -I$(KERNEL_INCLUDE_DIR) $< $(DOMAIN_SOURCE) $(KERNEL_LIB) -o $@

$(ACTIONS_TEST): tests/test_wd0_actions.c $(DOMAIN_SOURCE) $(KERNEL_LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_DOMAIN_TESTING -I$(PUBLIC_INCLUDE_DIR) -I$(KERNEL_INCLUDE_DIR) $< $(DOMAIN_SOURCE) $(KERNEL_LIB) -o $@

$(PERCEPTION_TEST): tests/test_wd0_perception.c $(DOMAIN_SOURCE) $(KERNEL_LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_DOMAIN_TESTING -I$(PUBLIC_INCLUDE_DIR) -I$(KERNEL_INCLUDE_DIR) $< $(DOMAIN_SOURCE) $(KERNEL_LIB) -o $@

$(INVARIANTS_TEST): tests/test_wd0_invariants.c $(DOMAIN_SOURCE) $(KERNEL_LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_DOMAIN_TESTING -I$(PUBLIC_INCLUDE_DIR) -I$(KERNEL_INCLUDE_DIR) $< $(DOMAIN_SOURCE) $(KERNEL_LIB) -o $@

$(ATOMICITY_TEST): tests/test_wd0_atomicity.c $(DOMAIN_SOURCE) $(wildcard $(KERNEL_ROOT)/src/*.c) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_DOMAIN_TESTING -DMINISNN_WORLDS_KERNEL_TESTING -I$(PUBLIC_INCLUDE_DIR) -I$(KERNEL_INCLUDE_DIR) $< $(DOMAIN_SOURCE) $(KERNEL_ROOT)/src/minisnn_worlds_kernel.c $(KERNEL_ROOT)/src/minisnn_worlds_kernel_snapshot.c $(KERNEL_ROOT)/src/minisnn_worlds_kernel_command_log.c -o $@

$(STRESS_TEST): tests/test_wd0_stress.c app/wd0_test_controller.c $(DOMAIN_SOURCE) $(KERNEL_LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) -O2 -DMINISNN_WORLDS_DOMAIN_TESTING -I$(PUBLIC_INCLUDE_DIR) -I$(KERNEL_INCLUDE_DIR) -Iapp $< app/wd0_test_controller.c $(DOMAIN_SOURCE) $(KERNEL_LIB) -o $@
$(WD1_SNAPSHOT_TEST): tests/test_wd1_snapshot.c $(DOMAIN_SOURCE) $(KERNEL_LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_DOMAIN_TESTING -I$(PUBLIC_INCLUDE_DIR) -I$(KERNEL_INCLUDE_DIR) -Itests $< $(DOMAIN_SOURCE) $(KERNEL_LIB) -o $@

$(WD1_FILE_TEST): tests/test_wd1_file_roundtrip.c $(WD1_FILE_ADAPTER_SOURCE) $(DOMAIN_SOURCE) $(KERNEL_LIB) | $(TEST_DIR)
	$(CC) $(CFLAGS) -DMINISNN_WORLDS_DOMAIN_TESTING -I$(PUBLIC_INCLUDE_DIR) -I$(KERNEL_INCLUDE_DIR) -Itests -Iapp $< $(WD1_FILE_ADAPTER_SOURCE) $(DOMAIN_SOURCE) $(KERNEL_LIB) -o $@

$(WD1_DEMO): app/wd1_domain_persistence_demo.c $(WD1_FILE_ADAPTER_SOURCE) $(DOMAIN_SOURCE) $(KERNEL_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(PUBLIC_INCLUDE_DIR) -I$(KERNEL_INCLUDE_DIR) -Iapp $< $(WD1_FILE_ADAPTER_SOURCE) $(DOMAIN_SOURCE) $(KERNEL_LIB) -o $@

test-wd0-domain: $(DOMAIN_TEST)
	$(DOMAIN_TEST)

test-wd0-actions: $(ACTIONS_TEST)
	$(ACTIONS_TEST)

test-wd0-perception: $(PERCEPTION_TEST)
	$(PERCEPTION_TEST)

test-wd0-invariants: $(INVARIANTS_TEST)
	$(INVARIANTS_TEST)

test-wd0-atomicity: $(ATOMICITY_TEST)
	$(ATOMICITY_TEST)

test-wd0-stress: $(STRESS_TEST)
	$(STRESS_TEST)

test-wd1-snapshot: $(WD1_SNAPSHOT_TEST)
	$(WD1_SNAPSHOT_TEST)

test-wd1-restore: test-wd1-snapshot

test-wd1-corruption: test-wd1-snapshot

test-wd1-file-roundtrip: $(WD1_FILE_TEST)
	$(WD1_FILE_TEST) $(TEST_DIR)/wd1_snapshot_roundtrip.bin

demo-wd1: $(WD1_DEMO) | $(WD1_RESULT_DIR)
	$(WD1_DEMO) $(WD1_RESULT_DIR)

check-wd1: demo-wd1
	$(PYTHON) scripts/check_wd1.py --domain-root . --directory $(WD1_RESULT_DIR)

test-wd1-golden-portability: $(WD1_DEMO)
	$(PYTHON) tests/test_wd1_golden_portability.py --python "$(PYTHON)" --checker scripts/check_wd1.py --demo $(WD1_DEMO) --domain-root .

test-wd1-determinism: $(WD1_DEMO)
	$(PYTHON) tests/test_wd1_determinism.py --demo $(WD1_DEMO)

test-wd1-optimization-determinism:
	$(PYTHON) tests/test_wd1_optimization_determinism.py --compiler "$(CC)" --domain-root .

test-wd1-sanitizer-classification:
	$(PYTHON) tests/test_wd1_sanitizer_classification.py --domain-root .

test-wd1-sanitize:
	$(PYTHON) tests/test_wd1_sanitize.py --compiler "$(CC)" --domain-root .

test-wd1-posix-smoke:
	$(PYTHON) tests/test_wd1_posix_smoke.py --domain-root .

audit-wd1: test check-wd1 test-wd1-golden-portability test-wd1-determinism test-wd1-optimization-determinism test-wd1-sanitizer-classification test-wd1-sanitize test-wd1-posix-smoke
	@echo WD1 Domain persistence audit OK

test: test-wd0-domain test-wd0-actions test-wd0-perception test-wd0-invariants test-wd0-atomicity test-wd0-stress test-wd1-snapshot test-wd1-file-roundtrip
	$(PYTHON) tests/test_wd0_public_headers.py "$(CC)" "$(CFLAGS)" "$(PUBLIC_INCLUDE_DIR)" "$(KERNEL_INCLUDE_DIR)" "$(TEST_DIR)"

demo-wd0: $(DEMO) | $(RESULT_DIR)
	$(DEMO) $(RESULT_DIR)

check-wd0: demo-wd0
	$(PYTHON) scripts/check_wd0.py --domain-root . --directory $(RESULT_DIR)

test-wd0-determinism: $(DEMO)
	$(PYTHON) tests/test_wd0_determinism.py --demo $(DEMO)

test-wd0-optimization-determinism:
	$(PYTHON) tests/test_wd0_optimization_determinism.py --compiler "$(CC)" --domain-root .

test-wd0-sanitizer-classification:
	$(PYTHON) tests/test_wd0_sanitizer_classification.py --domain-root .

test-wd0-sanitize:
	$(PYTHON) tests/test_wd0_sanitize.py --compiler "$(CC)" --domain-root .

test-wd0-posix-smoke:
	$(PYTHON) tests/test_wd0_posix_smoke.py --domain-root .

audit-wd0: test check-wd0 test-wd0-determinism test-wd0-optimization-determinism test-wd0-sanitizer-classification test-wd0-sanitize test-wd0-posix-smoke
	@echo WD0 Domain audit OK

clean:
	@if exist "$(BUILD_ROOT)" rmdir /s /q "$(BUILD_ROOT)"