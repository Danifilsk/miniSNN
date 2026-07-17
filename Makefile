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

.PHONY: all help clean core core-tests core-studio core-evolution test test-architecture

all: test

help:
	@echo Comandos do monorepo miniSNN:
	@echo   mingw32-make core             - executa o build padrao do miniSNN Core
	@echo   mingw32-make core-tests       - executa os testes do Core
	@echo   mingw32-make core-studio      - compila o miniSNN Studio
	@echo   mingw32-make core-evolution   - compila o runner de neuroevolucao
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

test: core-tests test-architecture

test-architecture:
	$(PYTHON) test_architecture.py

clean:
	$(MAKE) -C $(CORE_DIR) clean
	@if exist build rmdir /S /Q build

# Targets historicos, como check-c4 e scenario-random, continuam delegados ao Core.
%:
	$(MAKE) -C $(CORE_DIR) $@
