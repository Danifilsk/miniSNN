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
WORLDS_KERNEL_DIR = worlds/kernel

.PHONY: all help clean clean-core clean-studio clean-tests core core-lib core-headless core-test core-tests core-studio studio studio-path core-evolution worlds-kernel-lib worlds-kernel worlds-kernel-test test-k0-a-sanitize audit-k0-a test-k0-b-entities test-k0-b-commands test-k0-b-events test-k0-b-determinism test-k0-b-sanitize demo-k0-b audit-k0-b test-k0-c-random test-k0-c-hash test-k0-c-observability test-k0-c-determinism test-k0-c-optimization-determinism test-k0-c-sanitize demo-k0-c audit-k0-c demo-k0-d test-k0-d-config test-k0-d-artifacts test-k0-d-determinism test-k0-d-optimization-determinism test-k0-d-corruption test-k0-d-stress test-k0-d-long-run test-k0-d-posix-smoke test-k0-d-sanitize test-k0-external-consumer audit-k0-d audit-k0 clean-worlds-kernel test test-architecture test-docs test-analyzer audit-d1-build-products audit-d1-api test-d1-determinism test-d1-corruption test-d1-lifecycle-stress test-d1-optimization-determinism test-d1-posix-headless test-d1-portability test-d1-long-run test-d1-symbols benchmark-d1 scenario-d1-b audit-d1-b audit-d1 test-version test-api-baseline test-api-baseline-regressions release-core release-headless release-studio release-all test-release-build test-external-consumer package-release test-release-integrity test-release-packages test-studio-source-contracts test-studio-runtime-layout audit-d1-c-automated audit-d1-c test-working-memory test-associative-memory test-sequence-prediction test-c6-checkpoints test-c6-integration test-c6-long test-agent-io test-sensor-encoder test-action-decoder test-agent-cycle test-agent-cycle-checkpoint test-c7-integration test-c7-evolution test-c7-long-run test-c7 scenario-working-memory scenario-associative-memory scenario-sequence-prediction scenario-sequence-prediction-context scenario-c6-suite scenario-sensor-encoding scenario-action-decoding scenario-agent-cycle scenario-agent-cycle-checkpoint scenario-c7-integrated-audit check-c6 check-c7

all: test

help:
	@echo Comandos do monorepo miniSNN:
	@echo   mingw32-make core-lib         - compila somente libminisnn_core.a
	@echo   mingw32-make core             - compila biblioteca e apps headless
	@echo   mingw32-make core-headless    - compila runners headless
	@echo   mingw32-make core-test         - executa testes sem compilar Studio
	@echo   mingw32-make core-studio      - compila o miniSNN Studio, sem abrir
	@echo   mingw32-make studio           - compila quando necessario e abre o Studio
	@echo   mingw32-make studio-path      - mostra o caminho do executavel do Studio
	@echo   Abrir miniSNN Studio.cmd      - launcher clicavel para o uso normal
	@echo   mingw32-make core-evolution   - compila o runner de neuroevolucao
	@echo   mingw32-make worlds-kernel-lib - compila somente a biblioteca Worlds Kernel
	@echo   mingw32-make worlds-kernel     - compila a biblioteca e o demo Worlds Kernel
	@echo   mingw32-make worlds-kernel-test - executa os testes Worlds Kernel
	@echo   mingw32-make test-k0-a-sanitize - executa a regressao ASan/UBSan K0-A
	@echo   mingw32-make audit-k0-a        - audita a fundacao K0-A do Worlds Kernel
	@echo   mingw32-make test-k0-b-entities - valida IDs e registro K0-B
	@echo   mingw32-make test-k0-b-commands - valida fila e ordenacao K0-B
	@echo   mingw32-make test-k0-b-events  - valida eventos e conflitos K0-B
	@echo   mingw32-make test-k0-b-determinism - valida determinismo K0-B
	@echo   mingw32-make test-k0-b-sanitize - executa ASan/UBSan K0-B
	@echo   mingw32-make demo-k0-b         - executa o demo K0-B
	@echo   mingw32-make audit-k0-b        - audita entidades, comandos e eventos K0-B
	@echo   mingw32-make test-k0-c-random  - valida PRNG e atomicidade K0-C
	@echo   mingw32-make test-k0-c-hash    - valida hash canonico K0-C
	@echo   mingw32-make test-k0-c-observability - valida diagnosticos K0-C
	@echo   mingw32-make test-k0-c-determinism - valida determinismo K0-C
	@echo   mingw32-make test-k0-c-optimization-determinism - compara O0/O2 K0-C
	@echo   mingw32-make test-k0-c-sanitize - executa ASan/UBSan K0-C
	@echo   mingw32-make demo-k0-c         - executa o demo de PRNG/hash K0-C
	@echo   mingw32-make audit-k0-c        - audita PRNG, hash e observabilidade K0-C
	@echo   mingw32-make demo-k0-d         - executa o cenario integrado Worlds K0-D
	@echo   mingw32-make test-k0-d-config  - valida configuracao INI K0-D
	@echo   mingw32-make test-k0-d-artifacts - valida artefatos e atomicidade K0-D
	@echo   mingw32-make test-k0-d-determinism - valida processos e texto equivalente K0-D
	@echo   mingw32-make test-k0-d-optimization-determinism - compara K0-D -O0/-O2
	@echo   mingw32-make test-k0-d-corruption - valida corrupcao do parser K0-D
	@echo   mingw32-make test-k0-d-stress  - executa stress deterministico K0-D
	@echo   mingw32-make test-k0-d-long-run - executa long run K0-D
	@echo   mingw32-make test-k0-d-posix-smoke - compila o app K0-D em POSIX quando disponivel
	@echo   mingw32-make test-k0-d-sanitize - executa sanitizers K0-D quando disponivel
	@echo   mingw32-make test-k0-external-consumer - valida consumidor externo Worlds
	@echo   mingw32-make audit-k0-d        - audita entrega integrada K0-D
	@echo   mingw32-make audit-k0          - fecha a fundacao deterministica K0
	@echo   mingw32-make clean-worlds-kernel - limpa somente outputs Worlds Kernel
	@echo   mingw32-make audit-d1-build-products - audita separacao dos produtos
	@echo   mingw32-make audit-d1-api     - audita API publica candidata
	@echo   mingw32-make test-d1-determinism - valida matriz deterministica D1-B
	@echo   mingw32-make test-d1-corruption - valida corrupcao e parsing D1-B
	@echo   mingw32-make test-d1-lifecycle-stress - valida lifecycle e alocacao D1-B
	@echo   mingw32-make test-d1-optimization-determinism - compara O0/O2 D1-B
	@echo   mingw32-make test-d1-posix-headless - smoke estrito POSIX do Core headless, quando disponivel
	@echo   mingw32-make test-d1-portability - valida os contratos dos harnesses D1-B
	@echo   mingw32-make test-d1-long-run - executa o perfil long run D1-B
	@echo   mingw32-make test-d1-symbols - audita simbolos e ABI candidato
	@echo   mingw32-make benchmark-d1 - mede desempenho local D1-B
	@echo   mingw32-make scenario-d1-b - gera resultados e HTML D1-B
	@echo   mingw32-make audit-d1-b - executa a auditoria agregada D1-B
	@echo   mingw32-make test-version - valida a versao publica candidata
	@echo   mingw32-make test-api-baseline - valida o baseline semantico de API v1.0-rc
	@echo   mingw32-make test-api-baseline-regressions - valida invariancia semantica do baseline
	@echo   mingw32-make release-all - compila Core, Studio e ferramentas de release
	@echo   mingw32-make test-external-consumer - compila o consumidor externo limpo
	@echo   mingw32-make package-release - gera ZIPs locais de avaliacao
	@echo   mingw32-make test-release-integrity - adultera copias temporarias dos pacotes D1-C
	@echo   mingw32-make test-release-packages - valida integridade e smoke dos ZIPs
	@echo   mingw32-make test-studio-source-contracts - valida contratos Win32 do Studio
	@echo   mingw32-make test-studio-runtime-layout - valida os layouts repository/package do Studio
	@echo   mingw32-make audit-d1-c - valida gates automaticos e o checklist manual D1-C
	@echo   mingw32-make audit-d1   - executa a auditoria final pre-Worlds D1-A/B/C
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
	@echo   mingw32-make test-sensor-encoder - valida codificacao sensor-neural C7.2
	@echo   mingw32-make scenario-sensor-encoding - executa o demo C7.2
	@echo   mingw32-make test-action-decoder - valida decodificacao neural-acao C7.3
	@echo   mingw32-make scenario-action-decoding - executa o demo C7.3
	@echo   mingw32-make test-agent-cycle  - valida o ciclo cerebro-agente C7.4
	@echo   mingw32-make test-agent-cycle-checkpoint - valida resume/replay C7.5-A
	@echo   mingw32-make scenario-agent-cycle - executa o demo C7.4
	@echo   mingw32-make scenario-agent-cycle-checkpoint - executa o demo C7.5-A
	@echo   mingw32-make test-c7-integration - valida a cadeia integrada C7.5-B
	@echo   mingw32-make test-c7-evolution - valida a ponte C7 com evolucao abstrata
	@echo   mingw32-make test-c7-long-run - executa 100000 passos por modelo C7
	@echo   mingw32-make test-c7          - executa a matriz completa C7
	@echo   mingw32-make scenario-c7-integrated-audit - gera a auditoria C7.5-B
	@echo   mingw32-make check-c7          - verifica C7.1-C7.5-B e fecha C7
	@echo   mingw32-make test             - testes do Core e arquitetura do monorepo
	@echo   mingw32-make test-architecture - valida isolamento e estrutura M1
	@echo   mingw32-make test-docs         - valida documentacao do Core e monorepo
	@echo   mingw32-make test-analyzer     - executa analise estatica do Core
	@echo   mingw32-make target-do-core - encaminha targets legados ao Core

core:
	$(MAKE) -C $(CORE_DIR) core

core-lib:
	$(MAKE) -C $(CORE_DIR) core-lib

core-headless:
	$(MAKE) -C $(CORE_DIR) headless

core-test:
	$(MAKE) -C $(CORE_DIR) core-test

core-tests:
	$(MAKE) -C $(CORE_DIR) core-test

core-studio:
	$(MAKE) -C $(CORE_DIR) studio-build

studio:
	call "Abrir miniSNN Studio.cmd"

studio-path:
	@echo build/studio/bin/minisnn_studio.exe

core-evolution:
	$(MAKE) -C $(CORE_DIR) evolution-build

worlds-kernel-lib:
	$(MAKE) -C $(WORLDS_KERNEL_DIR) lib

worlds-kernel:
	$(MAKE) -C $(WORLDS_KERNEL_DIR) all

worlds-kernel-test:
	$(MAKE) -C $(WORLDS_KERNEL_DIR) test

test-k0-a-sanitize:
	$(MAKE) -C $(WORLDS_KERNEL_DIR) sanitize

audit-k0-a:
	$(MAKE) -C $(WORLDS_KERNEL_DIR) audit-k0-a

test-k0-b-entities test-k0-b-commands test-k0-b-events test-k0-b-determinism test-k0-b-sanitize demo-k0-b audit-k0-b test-k0-c-random test-k0-c-hash test-k0-c-observability test-k0-c-determinism test-k0-c-optimization-determinism test-k0-c-sanitize demo-k0-c audit-k0-c demo-k0-d test-k0-d-config test-k0-d-artifacts test-k0-d-determinism test-k0-d-optimization-determinism test-k0-d-corruption test-k0-d-stress test-k0-d-long-run test-k0-d-posix-smoke test-k0-d-sanitize test-k0-external-consumer audit-k0-d:
	$(MAKE) -C $(WORLDS_KERNEL_DIR) $@

audit-k0: test-architecture test-docs
	$(MAKE) -C $(WORLDS_KERNEL_DIR) audit-k0

test-docs test-analyzer:
	$(MAKE) -C $(CORE_DIR) $@

clean-worlds-kernel:
	$(MAKE) -C $(WORLDS_KERNEL_DIR) clean

audit-d1-build-products:
	$(MAKE) -C $(CORE_DIR) audit-d1-build-products

audit-d1-api:
	$(MAKE) -C $(CORE_DIR) audit-d1-api

test-version test-api-baseline test-api-baseline-regressions release-core release-headless release-studio release-all test-release-build test-external-consumer package-release test-release-integrity test-release-packages test-studio-source-contracts test-studio-runtime-layout audit-d1-c-automated audit-d1-c audit-d1:
	$(MAKE) -C $(CORE_DIR) $@

test-d1-determinism test-d1-corruption test-d1-lifecycle-stress test-d1-optimization-determinism test-d1-posix-headless test-d1-portability test-d1-long-run test-d1-symbols benchmark-d1 scenario-d1-b audit-d1-b:
	$(MAKE) -C $(CORE_DIR) $@

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

test-sensor-encoder:
	$(MAKE) -C $(CORE_DIR) test-sensor-encoder

test-action-decoder:
	$(MAKE) -C $(CORE_DIR) test-action-decoder

test-agent-cycle:
	$(MAKE) -C $(CORE_DIR) test-agent-cycle

test-agent-cycle-checkpoint:
	$(MAKE) -C $(CORE_DIR) test-agent-cycle-checkpoint

test-c7-integration:
	$(MAKE) -C $(CORE_DIR) test-c7-integration

test-c7-evolution:
	$(MAKE) -C $(CORE_DIR) test-c7-evolution

test-c7-long-run:
	$(MAKE) -C $(CORE_DIR) test-c7-long-run

test-c7:
	$(MAKE) -C $(CORE_DIR) test-c7

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

scenario-sensor-encoding:
	$(MAKE) -C $(CORE_DIR) scenario-sensor-encoding

scenario-action-decoding:
	$(MAKE) -C $(CORE_DIR) scenario-action-decoding

scenario-agent-cycle:
	$(MAKE) -C $(CORE_DIR) scenario-agent-cycle

scenario-agent-cycle-checkpoint:
	$(MAKE) -C $(CORE_DIR) scenario-agent-cycle-checkpoint

scenario-c7-integrated-audit:
	$(MAKE) -C $(CORE_DIR) scenario-c7-integrated-audit

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

clean-core:
	$(MAKE) -C $(CORE_DIR) clean-core

clean-studio:
	$(MAKE) -C $(CORE_DIR) clean-studio

clean-tests:
	$(MAKE) -C $(CORE_DIR) clean-tests

# Targets historicos, como check-c4 e scenario-random, continuam delegados ao Core.
%:
	$(MAKE) -C $(CORE_DIR) $@
