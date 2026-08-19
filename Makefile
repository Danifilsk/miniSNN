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
WORLDS_DOMAIN_DIR = worlds/domain
WORLDS_BRAIN_BRIDGE_DIR = worlds/brain_bridge
WORLDS_WF0_FISH_DIR = worlds/scenarios/wf0_fish
WORLDS_WF1_TRAINABLE_FISH_DIR = worlds/scenarios/wf1_trainable_fish
WORLDS_TERRAIN_DIR = worlds/terrain
WORLDS_G0_VISUALIZER_DIR = worlds/app/g0_visualizer

.PHONY: all help clean clean-core clean-studio clean-tests core core-lib core-headless core-test core-tests core-studio studio studio-path core-evolution worlds-kernel-lib worlds-kernel worlds-kernel-test test-kernel-command-batch test-k0-a-sanitize audit-k0-a test-k0-b-entities test-k0-b-commands test-k0-b-events test-k0-b-determinism test-k0-b-sanitize demo-k0-b audit-k0-b test-k0-c-random test-k0-c-hash test-k0-c-observability test-k0-c-determinism test-k0-c-optimization-determinism test-k0-c-sanitize demo-k0-c audit-k0-c demo-k0-d test-k0-d-config test-k0-d-artifacts test-k0-d-determinism test-k0-d-optimization-determinism test-k0-d-corruption test-k0-d-stress test-k0-d-long-run test-k0-d-posix-smoke test-k0-d-sanitize test-k0-external-consumer audit-k0-d audit-k0 clean-worlds-kernel test test-architecture test-docs test-analyzer audit-d1-build-products audit-d1-api test-d1-determinism test-d1-corruption test-d1-lifecycle-stress test-d1-optimization-determinism test-d1-posix-headless test-d1-portability test-d1-long-run test-d1-symbols benchmark-d1 scenario-d1-b audit-d1-b audit-d1 test-version test-api-baseline test-api-baseline-regressions release-core release-headless release-studio release-all test-release-build test-external-consumer package-release test-release-integrity test-release-packages test-studio-source-contracts test-studio-runtime-layout audit-d1-c-automated audit-d1-c test-working-memory test-associative-memory test-sequence-prediction test-c6-checkpoints test-c6-integration test-c6-long test-agent-io test-sensor-encoder test-action-decoder test-agent-cycle test-agent-cycle-checkpoint test-c7-integration test-c7-evolution test-c7-long-run test-c7 scenario-working-memory scenario-associative-memory scenario-sequence-prediction scenario-sequence-prediction-context scenario-c6-suite scenario-sensor-encoding scenario-action-decoding scenario-agent-cycle scenario-agent-cycle-checkpoint scenario-c7-integrated-audit check-c6 check-c7 test-k1-a-scalar test-k1-a-space test-k1-a-transform test-k1-a-determinism test-k1-a-optimization-determinism test-k1-a-stress test-k1-a-sanitize test-k1-a-posix-smoke demo-k1-a audit-k1-a test-k1-b1-occupancy test-k1-b1-overlap test-k1-b1-conflicts test-k1-b1-determinism test-k1-b1-optimization-determinism test-k1-b1-stress test-k1-b1-sanitizer-classification test-k1-b1-sanitize test-k1-b1-posix-smoke demo-k1-b1 audit-k1-b1 test-k1-b2-movement test-k1-b2-determinism test-k1-b2-optimization-determinism test-k1-b2-stress test-k1-b2-sanitizer-classification test-k1-b2-sanitize test-k1-b2-posix-smoke demo-k1-b2 audit-k1-b2 test-k1-c1-spatial-links test-k1-c1-hash test-k1-c1-determinism test-k1-c1-optimization-determinism test-k1-c1-sanitizer-classification test-k1-c1-sanitize test-k1-c1-posix-smoke audit-k1-c1

all: test

help:
	@echo Comandos do monorepo miniSNN:
	@echo   mingw32-make core-lib         - compila somente libminisnn_core.a
	@echo   mingw32-make core             - compila biblioteca e apps headless
	@echo   mingw32-make demo-k1-c4       - executa o demo integrado de spatial links
	@echo   mingw32-make audit-k1-c4      - executa a validacao completa do fechamento K1-C4
	@echo   mingw32-make audit-k2-a       - audita snapshot canonico V1 do Worlds Kernel
	@echo   mingw32-make demo-k2-b        - executa save/load e restore do snapshot V1
	@echo   mingw32-make check-k2-b       - valida os artefatos do demo de persistencia V1
	@echo   mingw32-make audit-k2-b       - audita importacao, restore e save/load K2-B
	@echo   mingw32-make demo-k2-c        - executa command log e replay deterministico
	@echo   mingw32-make check-k2-c       - valida os artefatos do replay K2-C
	@echo   mingw32-make demo-k2-d        - executa a prova final A/B/C de persistencia K2-D
	@echo   mingw32-make test-k2-d-persistence - valida 1000 ticks e checkpoints multiplos K2-D
	@echo   mingw32-make check-k2-d       - valida state-binding e persistencia K2-D
	@echo   mingw32-make audit-k2-d       - fecha a auditoria final K2
	@echo   mingw32-make audit-k2         - executa a auditoria autoritativa completa de K2
	@echo   mingw32-make audit-k2-c       - audita o fechamento K2-A/B/C
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
	@echo   mingw32-make test-kernel-command-batch - valida rollback provisorio no Worlds Kernel
	@echo   mingw32-make worlds-domain      - compila o produto Worlds Domain WD0
	@echo   mingw32-make worlds-domain-test - executa os testes do Worlds Domain
	@echo   mingw32-make demo-wd0           - gera o demo semantico deterministico WD0
	@echo   mingw32-make check-wd0          - valida os artefatos semanticos WD0
	@echo   mingw32-make audit-wd0          - executa a auditoria completa do Worlds Domain
	@echo   mingw32-make test-wd1-snapshot    - valida captura, restore, binding e corrupcao WD1
	@echo   mingw32-make test-wd1-file-roundtrip - valida save/load atomico WD1
	@echo   mingw32-make demo-wd1             - gera o demo de persistencia semantica WD1
	@echo   mingw32-make check-wd1            - valida o golden do Domain Snapshot V1
	@echo   mingw32-make test-wd1-golden-portability - valida LF e rejeicao semantica do golden WD1
	@echo   mingw32-make audit-wd1            - executa a auditoria focada de persistencia Domain
	@echo   mingw32-make test-wd2-lifecycle      - valida starvation e terminalidade do Domain
	@echo   mingw32-make test-wd2-persistence    - valida snapshots lifecycle V1/V2
	@echo   mingw32-make demo-wd2                - gera o demo deterministico de morte
	@echo   mingw32-make check-wd2               - valida os artefatos WD2
	@echo   mingw32-make audit-wd2               - executa a auditoria focada WD2
	@echo   mingw32-make worlds-brain-bridge - compila a biblioteca e demo WB0
	@echo   mingw32-make test-wb0-core-integration - valida Bridge com Core real
	@echo   mingw32-make demo-wb0           - gera artefatos deterministas do Brain Bridge
	@echo   mingw32-make check-wb0          - valida conteudo dos artefatos WB0
	@echo   mingw32-make audit-wb0          - executa a auditoria completa WB0
	@echo   mingw32-make demo-wf0           - executa o primeiro organismo neural headless
	@echo   mingw32-make check-wf0          - valida os artefatos deterministas WF0
	@echo   mingw32-make test-wf0-neural-continuity - prova continuidade neural e reset manual
	@echo   mingw32-make audit-wf0          - executa a auditoria focada do Fish neural
	@echo   mingw32-make test-wt0          - valida Terrain WATER/LAND/ROCK
	@echo   mingw32-make test-wt0-materialization-failure - valida atomicidade WT0 sob falha
	@echo   mingw32-make test-wt0-category-mask - valida blocking generico WT0
	@echo   mingw32-make demo-wt0          - gera o demo headless WT0
	@echo   mingw32-make audit-wt0         - executa a auditoria focada WT0
	@echo   mingw32-make worlds            - gera o executavel principal miniSNN Worlds
	@echo   mingw32-make test-worlds-published-product - valida paridade binaria e smoke publicado
	@echo   mingw32-make test-g0-d-settings-layout - valida geometria do painel Settings
	@echo   mingw32-make test-g0-d-i         - valida startup publicado e fonte pixel
	@echo   mingw32-make worlds-app        - gera build/studio/bin/minisnn_worlds.exe
	@echo   mingw32-make test-g0-a         - valida o visualizador Worlds G0-A
	@echo   mingw32-make demo-g0-a         - gera o demo headless do visualizador G0-A
	@echo   mingw32-make audit-g0-a        - executa a auditoria enxuta G0-A
	@echo   mingw32-make test-g0-b         - valida tempo real e orientacao G0-B
	@echo   mingw32-make test-g0-b-runtime - valida scheduler pausavel do Worlds
	@echo   mingw32-make test-g0-b-facing  - valida orientacao por movimento aplicado
	@echo   mingw32-make audit-g0-b        - executa a auditoria enxuta G0-B
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
	@echo   mingw32-make test-k1-a-space   - valida espaco fixed-point e placement K1-A
	@echo   mingw32-make test-k1-a-transform - valida transforms, conflitos e hash K1-A
	@echo   mingw32-make demo-k1-a         - executa o demo espacial K1-A
	@echo   mingw32-make audit-k1-a        - audita a fundacao espacial K1-A
	@echo   mingw32-make test-k1-b1-occupancy - valida AABB, masks e lifecycle K1-B1
	@echo   mingw32-make test-k1-b1-overlap - valida contato e sobreposicao AABB K1-B1
	@echo   mingw32-make test-k1-b1-conflicts - valida conflitos e related_entity K1-B1
	@echo   mingw32-make test-k1-b1-determinism - valida determinismo K1-B1
	@echo   mingw32-make test-k1-b1-optimization-determinism - compara K1-B1 em -O0/-O2
	@echo   mingw32-make test-k1-b1-stress - executa stress deterministico K1-B1
	@echo   mingw32-make test-k1-b1-sanitizer-classification - valida classificacao sanitizer K1-B1
	@echo   mingw32-make test-k1-b1-sanitize - executa ASan/UBSan K1-B1 quando disponivel
	@echo   mingw32-make test-k1-b1-posix-smoke - executa smoke POSIX K1-B1 quando disponivel
	@echo   mingw32-make demo-k1-b1         - executa o demo de ocupacao e barreiras K1-B1
	@echo   mingw32-make audit-k1-b1        - audita K0, K1-A e K1-B1
	@echo   mingw32-make test-k1-b2-movement - valida deslocamento atomico K1-B2
	@echo   mingw32-make test-k1-b2-determinism - repete a trajetoria K1-B2
	@echo   mingw32-make test-k1-b2-optimization-determinism - compara K1-B2 em -O0/-O2
	@echo   mingw32-make test-k1-b2-stress - executa stress K1-B2
	@echo   mingw32-make test-k1-b2-sanitizer-classification - valida classificacao sanitizer K1-B2
	@echo   mingw32-make test-k1-b2-sanitize - executa ASan/UBSan K1-B2 quando disponivel
	@echo   mingw32-make test-k1-b2-posix-smoke - executa smoke POSIX K1-B2 quando disponivel
	@echo   mingw32-make demo-k1-b2         - executa o demo de movimento K1-B2
	@echo   mingw32-make audit-k1-b2        - audita K0, K1-A, K1-B1 e K1-B2
	@echo   mingw32-make test-k1-c1-spatial-links - valida floresta e lifecycle K1-C1
	@echo   mingw32-make test-k1-c1-hash - valida state hash V5 K1-C1
	@echo   mingw32-make test-k1-c1-determinism - repete a trajetoria K1-C1
	@echo   mingw32-make test-k1-c1-optimization-determinism - compara K1-C1 em -O0/-O2
	@echo   mingw32-make test-k1-c1-sanitizer-classification - valida classificacao sanitizer K1-C1
	@echo   mingw32-make test-k1-c1-sanitize - executa ASan/UBSan K1-C1
	@echo   mingw32-make test-k1-c1-posix-smoke - executa smoke POSIX K1-C1
	@echo   mingw32-make audit-k1-c1        - audita spatial links e state hash V5 K1-C1
	@echo   mingw32-make test-k1-c2-subtree-movement - valida translacao rigida C2
	@echo   mingw32-make audit-k1-c2        - audita translacao de subarvore C2
	@echo   mingw32-make test-k1-c3-ordering - valida matriz de ordenacao e conflitos C3
	@echo   mingw32-make test-k1-c3-invariants - valida corrupcao e invariantes C3
	@echo   mingw32-make test-k1-c3-limits - valida limites e capacidades C3
	@echo   mingw32-make test-k1-c3-failure-atomicity - valida atomicidade sob falha C3
	@echo   mingw32-make test-k1-c3-long-run - executa long run deterministico C3
	@echo   mingw32-make test-k1-c3-hash - valida V5 com auditoria C3
	@echo   mingw32-make test-k1-c3-determinism - repete o long run C3
	@echo   mingw32-make test-k1-c3-optimization-determinism - compara C3 em -O0/-O2
	@echo   mingw32-make test-k1-c3-sanitizer-classification - valida classificacao sanitizer C3
	@echo   mingw32-make test-k1-c3-sanitize - executa ASan/UBSan C3 quando disponivel
	@echo   mingw32-make test-k1-c3-posix-smoke - executa smoke POSIX C3 quando disponivel
	@echo   mingw32-make audit-k1-c3        - audita ordering e hardening C3
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
	@echo   mingw32-make test-studio-worlds-launcher - valida o launcher independente do Worlds
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

	@echo   mingw32-make experiment-wf1-a - executa o experimento headless trainable fish
	@echo   mingw32-make check-wf1-a      - valida os artefatos cientificos WF1-A
	@echo   mingw32-make audit-wf1-a      - executa os gates focados WF1-A
	@echo   mingw32-make experiment-wf1-a1 - executa o bootstrap exploratorio WF1-A.1
	@echo   mingw32-make check-wf1-a1     - valida os artefatos cientificos WF1-A.1
	@echo   mingw32-make audit-wf1-a1     - executa os gates focados WF1-A.1
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

test-kernel-command-batch:
	$(MAKE) -C $(WORLDS_KERNEL_DIR) test-kernel-command-batch

worlds-domain-lib:
	$(MAKE) -C $(WORLDS_DOMAIN_DIR) lib

worlds-domain:
	$(MAKE) -C $(WORLDS_DOMAIN_DIR) all

worlds-domain-test:
	$(MAKE) -C $(WORLDS_DOMAIN_DIR) test

test-wd0-domain test-wd0-actions test-wd0-perception test-wd0-invariants test-wd0-atomicity test-wd0-stress test-wd0-determinism test-wd0-optimization-determinism test-wd0-sanitizer-classification test-wd0-sanitize test-wd0-posix-smoke demo-wd0 check-wd0:
	$(MAKE) -C $(WORLDS_DOMAIN_DIR) $@

test-wd1-snapshot test-wd1-restore test-wd1-corruption test-wd1-file-roundtrip test-wd1-golden-portability test-wd1-determinism test-wd1-optimization-determinism test-wd1-sanitizer-classification test-wd1-sanitize test-wd1-posix-smoke demo-wd1 check-wd1:
	$(MAKE) -C $(WORLDS_DOMAIN_DIR) $@

test-wd2-lifecycle test-wd2-starvation test-wd2-dead-actions test-wd2-persistence test-wd2-atomicity test-wd2-determinism test-wd2-optimization-determinism test-wd2-sanitize test-wd2-posix-smoke demo-wd2 check-wd2:
	$(MAKE) -C $(WORLDS_DOMAIN_DIR) $@

audit-wd0: test-architecture test-docs test-analyzer audit-k2 test-kernel-command-batch
	$(MAKE) -C $(WORLDS_DOMAIN_DIR) audit-wd0

audit-wd1: test-architecture test-docs test-analyzer audit-wd0 audit-wb0
	$(MAKE) -C $(WORLDS_DOMAIN_DIR) audit-wd1

audit-wd2: test-architecture test-docs
	$(MAKE) -C $(WORLDS_DOMAIN_DIR) audit-wd2

clean-worlds-domain:
	$(MAKE) -C $(WORLDS_DOMAIN_DIR) clean

worlds-brain-bridge-lib:
	$(MAKE) -C $(WORLDS_BRAIN_BRIDGE_DIR) lib

worlds-brain-bridge:
	$(MAKE) -C $(WORLDS_BRAIN_BRIDGE_DIR) all

worlds-brain-bridge-test:
	$(MAKE) -C $(WORLDS_BRAIN_BRIDGE_DIR) test

test-wb0-encoder test-wb0-decoder test-wb0-binding test-wb0-core-integration test-wb0-cache test-wb0-failure-retry test-wb0-multi-organism test-wb0-invariants test-wb0-determinism test-wb0-optimization-determinism test-wb0-sanitizer-classification test-wb0-sanitize test-wb0-posix-smoke demo-wb0 test-wb1-bridge test-wb1-brain-config test-wb1-brain-persistence test-wb1-reward test-wb1-episode-reset test-wb1-determinism test-wb1-optimization-determinism test-wb1-sanitize test-wb1-posix-smoke demo-wb1:
	$(MAKE) -C $(WORLDS_BRAIN_BRIDGE_DIR) $@

check-wb0:
	$(MAKE) -C $(WORLDS_BRAIN_BRIDGE_DIR) check-wb0

check-wb1:
	$(MAKE) -C $(WORLDS_BRAIN_BRIDGE_DIR) check-wb1

audit-wb0: test-architecture test-docs test-analyzer audit-wd0

audit-wb1: test-architecture
	$(MAKE) -C $(WORLDS_BRAIN_BRIDGE_DIR) audit-wb1

worlds-wf0:
	$(MAKE) -C $(WORLDS_WF0_FISH_DIR) all

test-wf0 test-wf0-neural-causality test-wf0-neural-continuity test-wf0-determinism test-wf0-optimization-determinism test-wf0-sanitize test-wf0-posix-smoke demo-wf0 check-wf0:
	$(MAKE) -C $(WORLDS_WF0_FISH_DIR) $@

audit-wf0: test-architecture
	$(MAKE) -C $(WORLDS_WF0_FISH_DIR) audit-wf0

test-k0-a-sanitize:
	$(MAKE) -C $(WORLDS_KERNEL_DIR) sanitize

audit-k0-a:
	$(MAKE) -C $(WORLDS_KERNEL_DIR) audit-k0-a

test-k0-b-entities test-k0-b-commands test-k0-b-events test-k0-b-determinism test-k0-b-sanitize demo-k0-b audit-k0-b test-k0-c-random test-k0-c-hash test-k0-c-observability test-k0-c-determinism test-k0-c-optimization-determinism test-k0-c-sanitize demo-k0-c audit-k0-c demo-k0-d test-k0-d-config test-k0-d-artifacts test-k0-d-determinism test-k0-d-optimization-determinism test-k0-d-corruption test-k0-d-stress test-k0-d-long-run test-k0-d-posix-smoke test-k0-d-sanitize test-k0-external-consumer audit-k0-d:
	$(MAKE) -C $(WORLDS_KERNEL_DIR) $@

audit-k0: test-architecture test-docs
	$(MAKE) -C $(WORLDS_KERNEL_DIR) audit-k0

test-k1-a-scalar test-k1-a-space test-k1-a-transform test-k1-a-determinism test-k1-a-optimization-determinism test-k1-a-stress test-k1-a-sanitize test-k1-a-posix-smoke demo-k1-a audit-k1-a test-k1-b1-occupancy test-k1-b1-overlap test-k1-b1-conflicts test-k1-b1-determinism test-k1-b1-optimization-determinism test-k1-b1-stress test-k1-b1-sanitizer-classification test-k1-b1-sanitize test-k1-b1-posix-smoke demo-k1-b1 audit-k1-b1 test-k1-b2-movement test-k1-b2-determinism test-k1-b2-optimization-determinism test-k1-b2-stress test-k1-b2-sanitizer-classification test-k1-b2-sanitize test-k1-b2-posix-smoke demo-k1-b2 audit-k1-b2 test-k1-c1-spatial-links test-k1-c1-hash test-k1-c1-determinism test-k1-c1-optimization-determinism test-k1-c1-sanitizer-classification test-k1-c1-sanitize test-k1-c1-posix-smoke:
	$(MAKE) -C $(WORLDS_KERNEL_DIR) $@

test-k1-c2-subtree-movement test-k1-c2-events test-k1-c2-hash test-k1-c2-determinism test-k1-c2-optimization-determinism test-k1-c2-sanitizer-classification test-k1-c2-sanitize test-k1-c2-posix-smoke:
	$(MAKE) -C $(WORLDS_KERNEL_DIR) $@

audit-k1-c2: test-architecture test-docs
	$(MAKE) -C $(WORLDS_KERNEL_DIR) audit-k1-c2
audit-k1-c1: test-architecture test-docs
	$(MAKE) -C $(WORLDS_KERNEL_DIR) audit-k1-c1

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

.PHONY: test-k1-c2-subtree-movement test-k1-c2-events test-k1-c2-hash test-k1-c2-determinism test-k1-c2-optimization-determinism test-k1-c2-sanitizer-classification test-k1-c2-sanitize test-k1-c2-posix-smoke audit-k1-c2

.PHONY: test-k1-c3-ordering test-k1-c3-invariants test-k1-c3-limits test-k1-c3-failure-atomicity test-k1-c3-long-run test-k1-c3-hash test-k1-c3-determinism test-k1-c3-optimization-determinism test-k1-c3-sanitizer-classification test-k1-c3-sanitize test-k1-c3-posix-smoke audit-k1-c3

test-k1-c3-ordering test-k1-c3-invariants test-k1-c3-limits test-k1-c3-failure-atomicity test-k1-c3-long-run test-k1-c3-hash test-k1-c3-determinism test-k1-c3-optimization-determinism test-k1-c3-sanitizer-classification test-k1-c3-sanitize test-k1-c3-posix-smoke:
	$(MAKE) -C $(WORLDS_KERNEL_DIR) $@

test-k1-c4-demo test-k1-c4-config test-k1-c4-determinism test-k1-c4-stress test-k1-c4-optimization-determinism test-k1-c4-sanitizer-classification test-k1-c4-sanitize test-k1-c4-posix-smoke demo-k1-c4 check-k1-c4 audit-k1-c4 audit-k1-c:
	$(MAKE) -C $(WORLDS_KERNEL_DIR) $@

# The Kernel chain reaches audit-k0 through K1-A/B/C. These global gates make
# the root audit-k1 target the authoritative monorepo closure gate.
audit-k1: test-architecture test-docs test-analyzer
	$(MAKE) -C $(WORLDS_KERNEL_DIR) audit-k1
audit-k1-c3: test-architecture test-docs
	$(MAKE) -C $(WORLDS_KERNEL_DIR) audit-k1-c3

.PHONY: test-k2-a-snapshot test-k2-a-format test-k2-a-canonical test-k2-a-failure-atomicity test-k2-a-long-run test-k2-a-determinism test-k2-a-optimization-determinism test-k2-a-sanitizer-classification test-k2-a-sanitize test-k2-a-posix-smoke audit-k2-a

test-k2-a-snapshot test-k2-a-format test-k2-a-canonical test-k2-a-failure-atomicity test-k2-a-long-run test-k2-a-determinism test-k2-a-optimization-determinism test-k2-a-sanitizer-classification test-k2-a-sanitize test-k2-a-posix-smoke:
	$(MAKE) -C $(WORLDS_KERNEL_DIR) $@

audit-k2-a: test-architecture test-docs test-analyzer
	$(MAKE) -C $(WORLDS_KERNEL_DIR) audit-k2-a
.PHONY: test-k2-b-import test-k2-b-restore test-k2-b-malformed test-k2-b-truncation test-k2-b-failure-atomicity test-k2-b-roundtrip test-k2-b-continuation test-k2-b-checkpoints test-k2-b-file-roundtrip test-k2-b-determinism test-k2-b-optimization-determinism test-k2-b-sanitizer-classification test-k2-b-sanitize test-k2-b-posix-smoke demo-k2-b check-k2-b audit-k2-b

test-k2-b-import test-k2-b-restore test-k2-b-malformed test-k2-b-truncation test-k2-b-failure-atomicity test-k2-b-roundtrip test-k2-b-continuation test-k2-b-checkpoints test-k2-b-file-roundtrip test-k2-b-determinism test-k2-b-optimization-determinism test-k2-b-sanitizer-classification test-k2-b-sanitize test-k2-b-posix-smoke demo-k2-b check-k2-b:
	$(MAKE) -C $(WORLDS_KERNEL_DIR) $@

audit-k2-b: test-architecture test-docs test-analyzer
	$(MAKE) -C $(WORLDS_KERNEL_DIR) audit-k2-b
.PHONY: test-k2-c-command-log test-k2-c-format test-k2-c-malformed test-k2-c-failure-atomicity test-k2-c-replay test-k2-c-snapshot-replay test-k2-c-long-run test-k2-c-determinism test-k2-c-optimization-determinism test-k2-c-sanitizer-classification test-k2-c-sanitize test-k2-c-posix-smoke demo-k2-c check-k2-c audit-k2-c

test-k2-c-command-log test-k2-c-format test-k2-c-malformed test-k2-c-failure-atomicity test-k2-c-replay test-k2-c-snapshot-replay test-k2-c-long-run test-k2-c-determinism test-k2-c-optimization-determinism test-k2-c-sanitizer-classification test-k2-c-sanitize test-k2-c-posix-smoke demo-k2-c check-k2-c:
	$(MAKE) -C $(WORLDS_KERNEL_DIR) $@

audit-k2-c: test-architecture test-docs test-analyzer
	$(MAKE) -C $(WORLDS_KERNEL_DIR) audit-k2-c
.PHONY: test-k2-d-state-binding test-k2-d-negative test-k2-d-persistence test-k2-d-long-run test-k2-d-determinism test-k2-d-optimization-determinism test-k2-d-sanitizer-classification test-k2-d-sanitize test-k2-d-posix-smoke test-k2-d-integrated test-k2-d-multi-checkpoint test-k2-d-malformed test-k2-d-failure-atomicity demo-k2-d check-k2-d audit-k2-d audit-k2

test-k2-d-state-binding test-k2-d-negative test-k2-d-persistence test-k2-d-long-run test-k2-d-determinism test-k2-d-optimization-determinism test-k2-d-sanitizer-classification test-k2-d-sanitize test-k2-d-posix-smoke test-k2-d-integrated test-k2-d-multi-checkpoint test-k2-d-malformed test-k2-d-failure-atomicity demo-k2-d check-k2-d:
	$(MAKE) -C $(WORLDS_KERNEL_DIR) $@

audit-k2-d: test-architecture test-docs test-analyzer
	$(MAKE) -C $(WORLDS_KERNEL_DIR) audit-k2-d

audit-k2: test-architecture test-docs test-analyzer
	$(MAKE) -C $(WORLDS_KERNEL_DIR) audit-k2
.PHONY: worlds-domain-lib worlds-domain worlds-domain-test clean-worlds-domain test-wd0-domain test-wd0-actions test-wd0-perception test-wd0-invariants test-wd0-atomicity test-wd0-stress test-wd0-determinism test-wd0-optimization-determinism test-wd0-sanitizer-classification test-wd0-sanitize test-wd0-posix-smoke demo-wd0 check-wd0 audit-wd0
.PHONY: test-wd1-snapshot test-wd1-restore test-wd1-corruption test-wd1-file-roundtrip test-wd1-golden-portability test-wd1-determinism test-wd1-optimization-determinism test-wd1-sanitizer-classification test-wd1-sanitize test-wd1-posix-smoke demo-wd1 check-wd1 audit-wd1

.PHONY: worlds-brain-bridge-lib worlds-brain-bridge worlds-brain-bridge-test clean-worlds-brain-bridge test-wb0-encoder test-wb0-decoder test-wb0-binding test-wb0-core-integration test-wb0-cache test-wb0-failure-retry test-wb0-multi-organism test-wb0-invariants test-wb0-determinism test-wb0-optimization-determinism test-wb0-sanitizer-classification test-wb0-sanitize test-wb0-posix-smoke demo-wb0 check-wb0 check-wb1 audit-wb0 audit-wb1

clean-worlds-brain-bridge:
	$(MAKE) -C $(WORLDS_BRAIN_BRIDGE_DIR) clean
.PHONY: worlds-wf0 test-wf0 test-wf0-neural-causality test-wf0-neural-continuity test-wf0-determinism test-wf0-optimization-determinism test-wf0-sanitize test-wf0-posix-smoke demo-wf0 check-wf0 audit-wf0

.PHONY: worlds-terrain-lib worlds-terrain test-wt0 test-wt0-determinism test-wt0-optimization-determinism test-wt0-sanitize test-wt0-posix-smoke demo-wt0 check-wt0 audit-wt0

worlds-terrain-lib:
	$(MAKE) -C $(WORLDS_TERRAIN_DIR) lib

worlds-terrain:
	$(MAKE) -C $(WORLDS_TERRAIN_DIR) all

test-wt0 test-wt0-materialization-failure test-wt0-category-mask test-wt0-determinism test-wt0-optimization-determinism test-wt0-sanitize test-wt0-posix-smoke demo-wt0 check-wt0:
	$(MAKE) -C $(WORLDS_TERRAIN_DIR) $@

audit-wt0: test-architecture
	$(MAKE) -C $(WORLDS_TERRAIN_DIR) audit-wt0
.PHONY: test-wt0-materialization-failure test-wt0-category-mask
.PHONY: worlds worlds-app publish-worlds publish-worlds-products test-studio-worlds-launcher test-worlds-published-product

worlds publish-worlds: publish-worlds-products

publish-worlds-products: core-studio worlds-app
	copy /Y "build\studio\bin\minisnn_studio.exe" "minisnn_studio.exe" >nul
	copy /Y "build\studio\bin\minisnn_worlds.exe" "minisnn_worlds.exe" >nul

test-studio-worlds-launcher:
	$(MAKE) -C $(CORE_DIR) test-studio-product-launcher

worlds-app:
	$(MAKE) -C $(WORLDS_G0_VISUALIZER_DIR) worlds-app

test-worlds-published-product: worlds
	$(PYTHON) -B worlds/app/g0_visualizer/scripts/test_worlds_published_product.py --repository-root "." --internal-worlds "build/studio/bin/minisnn_worlds.exe" --published-worlds "minisnn_worlds.exe" --internal-studio "build/studio/bin/minisnn_studio.exe" --published-studio "minisnn_studio.exe"
.PHONY: test-g0-a test-g0-a-determinism test-g0-a-optimization-determinism test-g0-a-sanitize test-g0-a-posix-smoke demo-g0-a check-g0-a audit-g0-a test-g0-b test-g0-b-runtime test-g0-b-facing test-g0-b-determinism test-g0-b-optimization-determinism test-g0-b-sanitize test-g0-b-posix-smoke demo-g0-b check-g0-b audit-g0-b
.PHONY: test-g0-c test-g0-c-editor test-g0-c-camera test-g0-c-runtime test-g0-c-determinism test-g0-c-sanitize test-g0-c-posix-smoke demo-g0-c check-g0-c audit-g0-c test-g0-d test-g0-d-world-save-load test-g0-d-log test-g0-d-reset test-g0-d-menu-settings test-g0-d-settings test-g0-d-h test-g0-d-i test-g0-d-settings-layout test-g0-d-ui-resize test-g0-d-new-world-cancel test-g0-d-header-layout test-g0-d-determinism test-g0-d-sanitize test-g0-d-posix-smoke demo-g0-d check-g0-d audit-g0-d

test-g0-a test-g0-a-determinism test-g0-a-optimization-determinism test-g0-a-sanitize test-g0-a-posix-smoke demo-g0-a check-g0-a audit-g0-a test-g0-b test-g0-b-runtime test-g0-b-facing test-g0-b-determinism test-g0-b-optimization-determinism test-g0-b-sanitize test-g0-b-posix-smoke demo-g0-b check-g0-b audit-g0-b:
	$(MAKE) -C $(WORLDS_G0_VISUALIZER_DIR) $@


test-g0-c test-g0-c-editor test-g0-c-camera test-g0-c-runtime test-g0-c-determinism test-g0-c-sanitize test-g0-c-posix-smoke demo-g0-c check-g0-c audit-g0-c test-g0-d test-g0-d-world-save-load test-g0-d-log test-g0-d-reset test-g0-d-menu-settings test-g0-d-settings test-g0-d-h test-g0-d-i test-g0-d-settings-layout test-g0-d-ui-resize test-g0-d-new-world-cancel test-g0-d-header-layout test-g0-d-determinism test-g0-d-sanitize test-g0-d-posix-smoke demo-g0-d check-g0-d audit-g0-d:
	$(MAKE) -C $(WORLDS_G0_VISUALIZER_DIR) $@

.PHONY: test-wd2-lifecycle test-wd2-starvation test-wd2-dead-actions test-wd2-persistence test-wd2-atomicity test-wd2-determinism test-wd2-optimization-determinism test-wd2-sanitize test-wd2-posix-smoke demo-wd2 check-wd2 audit-wd2

worlds-wf1:
	$(MAKE) -C $(WORLDS_WF1_TRAINABLE_FISH_DIR) all

test-wf1-a-episode test-wf1-a-terminal-feedback test-wf1-a-evaluation-freeze test-wf1-a-save-load test-wf1-a-no-scripted-policy test-wf1-a-determinism test-wf1-a-posix-smoke experiment-wf1-a demo-wf1-a check-wf1-a test-wf1-a1-metrics test-wf1-a1-no-scripted-policy test-wf1-a1-determinism experiment-wf1-a1 demo-wf1-a1 check-wf1-a1:
	$(MAKE) -C $(WORLDS_WF1_TRAINABLE_FISH_DIR) $@

audit-wf1-a: test-architecture
	$(MAKE) -C $(WORLDS_WF1_TRAINABLE_FISH_DIR) audit-wf1-a

audit-wf1-a1: test-architecture
	$(MAKE) -C $(WORLDS_WF1_TRAINABLE_FISH_DIR) audit-wf1-a1
.PHONY: worlds-wf1 test-wf1-a-episode test-wf1-a-terminal-feedback \
        test-wf1-a-evaluation-freeze test-wf1-a-save-load \
        test-wf1-a-no-scripted-policy test-wf1-a-determinism test-wf1-a-posix-smoke \
        experiment-wf1-a demo-wf1-a check-wf1-a audit-wf1-a \
        test-wf1-a1-metrics test-wf1-a1-no-scripted-policy test-wf1-a1-determinism \
        experiment-wf1-a1 demo-wf1-a1 check-wf1-a1 audit-wf1-a1
test-wf1-a2-determinism test-wf1-a2-no-scripted-policy experiment-wf1-a2 demo-wf1-a2 check-wf1-a2 probe-wf1-a3 check-wf1-a3 test-wf1-a3-determinism test-wf1-a3-no-scripted-policy probe-wf1-a4 check-wf1-a4 test-wf1-a4-determinism pilot-wf1-a4 check-wf1-a4-pilot probe-wf1-a5 check-wf1-a5 test-wf1-a5-determinism:
	$(MAKE) -C $(WORLDS_WF1_TRAINABLE_FISH_DIR) $@

audit-wf1-a2: test-architecture
	$(MAKE) -C $(WORLDS_WF1_TRAINABLE_FISH_DIR) audit-wf1-a2

.PHONY: test-wf1-a2-determinism test-wf1-a2-no-scripted-policy experiment-wf1-a2 demo-wf1-a2 check-wf1-a2 audit-wf1-a2
.PHONY: probe-wf1-a3 check-wf1-a3 test-wf1-a3-determinism test-wf1-a3-no-scripted-policy
.PHONY: probe-wf1-a4 check-wf1-a4 test-wf1-a4-determinism
.PHONY: pilot-wf1-a4 check-wf1-a4-pilot
.PHONY: probe-wf1-a5 check-wf1-a5 test-wf1-a5-determinism