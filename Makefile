# Prefer the per-user Windows runtime when it exists; callers may still pass
# PYTHON=... explicitly for another interpreter.
ifeq ($(origin PYTHON), undefined)
WINDOWS_LOCALAPPDATA := $(subst \,/,$(LOCALAPPDATA))
WINDOWS_PYTHON_CANDIDATES := $(wildcard $(WINDOWS_LOCALAPPDATA)/Python/*/python.exe)
WINDOWS_PYTHON_CORE_CANDIDATES := $(foreach candidate,$(WINDOWS_PYTHON_CANDIDATES),$(if $(findstring /pythoncore-,$(candidate)),$(candidate)))
ifneq ($(strip $(WINDOWS_PYTHON_CANDIDATES)),)
PYTHON := $(if $(WINDOWS_PYTHON_CORE_CANDIDATES),$(firstword $(WINDOWS_PYTHON_CORE_CANDIDATES)),$(firstword $(WINDOWS_PYTHON_CANDIDATES)))
else
PYTHON := python
endif
endif
CORE_DIR = core

.PHONY: all help clean core core-tests core-studio core-evolution test test-architecture test-working-memory test-associative-memory test-sequence-prediction test-c6-checkpoints test-c6-integration test-c6-long test-agent-io scenario-working-memory scenario-associative-memory scenario-sequence-prediction scenario-sequence-prediction-context scenario-c6-suite check-c6 check-c7

all: test

help:
	@echo Comandos do monorepo miniSNN:
	@echo   mingw32-make core             - executa o build padrao do miniSNN Core
	@echo   mingw32-make core-tests       - executa os testes do Core
	@echo   mingw32-make core-studio      - compila o miniSNN Studio
	@echo   mingw32-make core-evolution   - compila o runner de neuroevolucao
	@echo   mingw32-make test-working-memory - valida o protocolo temporal C6.1
	@echo   mingw32-make test-associative-memory - valida o protocolo associativo C6.2
	@echo   mingw32-make test-sequence-prediction - valida o protocolo temporal C6.3
	@echo   mingw32-make scenario-working-memory - executa o demonstrador C6.1
	@echo   mingw32-make scenario-associative-memory - executa o demonstrador C6.2
	@echo   mingw32-make scenario-sequence-prediction - executa o demonstrador C6.3
	@echo   mingw32-make scenario-sequence-prediction-context - executa o demonstrador contextual C6.3
	@echo   mingw32-make scenario-c6-suite - executa a suite integrada C6
	@echo   mingw32-make check-c6         - verifica C6.1-C6.4 (cognicao e memoria)
	@echo   mingw32-make test-agent-io     - valida contratos de I/O C7.1
	@echo   mingw32-make check-c7          - verifica contratos de I/O C7.1
	@echo   mingw32-make test             - testes do Core e arquitetura do monorepo
	@echo   mingw32-make test-architecture - valida isolamento e estrutura M1
	@echo   mingw32-make <target-do-core> - encaminha targets legados ao Core

core:
	$(MAKE) -C $(CORE_DIR) all

core-tests:
	$(MAKE) -C $(CORE_DIR) test

core-studio:
	$(MAKE) -C $(CORE_DIR) studio-build

core-evolution:
	$(MAKE) -C $(CORE_DIR) evolution-build

test-working-memory:
	$(MAKE) -C $(CORE_DIR) test-working-memory

test-associative-memory:
	$(MAKE) -C $(CORE_DIR) test-associative-memory

test-sequence-prediction:
	$(MAKE) -C $(CORE_DIR) test-sequence-prediction

test-c6-checkpoints:
	$(MAKE) -C $(CORE_DIR) test-c6-checkpoints

test-c6-integration:
	$(MAKE) -C $(CORE_DIR) test-c6-integration

test-c6-long:
	$(MAKE) -C $(CORE_DIR) test-c6-long

test-agent-io:
	$(MAKE) -C $(CORE_DIR) test-agent-io

scenario-working-memory:
	$(MAKE) -C $(CORE_DIR) scenario-working-memory

scenario-associative-memory:
	$(MAKE) -C $(CORE_DIR) scenario-associative-memory

scenario-sequence-prediction:
	$(MAKE) -C $(CORE_DIR) scenario-sequence-prediction

scenario-sequence-prediction-context:
	$(MAKE) -C $(CORE_DIR) scenario-sequence-prediction-context

scenario-c6-suite:
	$(MAKE) -C $(CORE_DIR) scenario-c6-suite

check-c6:
	$(MAKE) -C $(CORE_DIR) check-c6

check-c7:
	$(MAKE) -C $(CORE_DIR) check-c7

test: core-tests test-architecture

test-architecture:
	$(PYTHON) test_architecture.py

clean:
	$(MAKE) -C $(CORE_DIR) clean
	@if exist build rmdir /S /Q build

# Targets historicos, como check-c4 e scenario-random, continuam delegados ao Core.
%:
	$(MAKE) -C $(CORE_DIR) $@
