from __future__ import annotations

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parent
CORE = ROOT / "core"
WORLDS_KERNEL = ROOT / "worlds" / "kernel"
WORLDS_DOMAIN = ROOT / "worlds" / "domain"
WORLDS_BRAIN_BRIDGE = ROOT / "worlds" / "brain_bridge"
WORLDS_TERRAIN = ROOT / "worlds" / "terrain"
WORLDS_G0_VISUALIZER = ROOT / "worlds" / "app" / "g0_visualizer"
EXPECTED_DIRECTORIES = (
    "include", "src", "app", "studio", "tests", "scripts", "configs", "docs",
    "examples", "experiments", "results",
)
GENERATED_SUFFIXES = {".csv", ".png", ".html", ".exe", ".o", ".obj"}
PUBLISHED_ROOT_PRODUCTS = {"minisnn_studio.exe", "minisnn_worlds.exe"}
INCLUDE_PATTERN = re.compile(r'^\s*#\s*include\s*[<\"]([^>\"]+)[>\"]', re.MULTILINE)


def fail(message: str) -> None:
    print(f"Architecture validation failed: {message}")
    raise SystemExit(1)


def main() -> int:
    if not CORE.is_dir():
        fail("core/ ausente")
    for directory in EXPECTED_DIRECTORIES:
        if not (CORE / directory).is_dir():
            fail(f"core/{directory}/ ausente")
    for filename in ("Makefile", "README.md", "API_REFERENCE.md"):
        if not (CORE / filename).is_file():
            fail(f"core/{filename} ausente")

    root_makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
    for target in ("core:", "core-lib:", "core-headless:", "core-test:",
                   "core-tests:", "core-studio:", "core-evolution:",
                   "worlds-kernel-lib:", "worlds-kernel:", "worlds-kernel-test:",
                   "worlds-domain-lib:", "worlds-domain:", "worlds-domain-test:",
                   "worlds-brain-bridge-lib:", "worlds-brain-bridge:", "worlds-brain-bridge-test:",
                   "test-wb0-encoder", "test-wb0-decoder", "test-wb0-binding",
                   "test-wb0-core-integration", "test-wb0-cache", "test-wb0-failure-retry",
                   "test-wb0-multi-organism", "test-wb0-invariants", "test-wb0-determinism",
                   "test-wb0-optimization-determinism", "test-wb0-sanitize",
                   "test-wb0-posix-smoke", "demo-wb0", "check-wb0:", "audit-wb0:",
                   "test-wd0-domain", "test-wd0-actions", "test-wd0-perception",
                   "test-wd0-invariants", "test-wd0-stress", "test-wd0-determinism",
                   "test-wd0-optimization-determinism", "test-wd0-sanitize",
                   "test-wd0-posix-smoke", "demo-wd0", "check-wd0:", "audit-wd0:",
                   "test-wd1-snapshot", "test-wd1-restore", "test-wd1-corruption",
                   "test-wd1-file-roundtrip", "test-wd1-determinism",
                   "test-wd1-optimization-determinism", "test-wd1-sanitize",
                   "test-wd1-posix-smoke", "demo-wd1", "check-wd1:", "audit-wd1:",
                   "test-wt0", "test-wt0-materialization-failure", "test-wt0-category-mask",
                   "test-wt0-determinism", "test-wt0-optimization-determinism", "test-wt0-sanitize", "test-wt0-posix-smoke", "demo-wt0", "check-wt0:", "audit-wt0:",
                   "worlds:", "worlds-app:", "publish-worlds:", "test-worlds-published-product:", "test-studio-worlds-launcher:", "test-g0-a", "test-g0-a-determinism", "test-g0-a-optimization-determinism", "test-g0-a-sanitize", "test-g0-a-posix-smoke", "demo-g0-a", "check-g0-a", "audit-g0-a", "test-g0-b", "test-g0-b-runtime", "test-g0-b-facing", "test-g0-b-determinism", "test-g0-b-optimization-determinism", "test-g0-b-sanitize", "test-g0-b-posix-smoke", "demo-g0-b", "check-g0-b", "audit-g0-b", "test-g0-c", "test-g0-c-editor", "test-g0-c-camera", "test-g0-c-runtime", "test-g0-c-determinism", "test-g0-c-sanitize", "test-g0-c-posix-smoke", "demo-g0-c", "check-g0-c", "audit-g0-c", "test-g0-d", "test-g0-d-world-save-load", "test-g0-d-log", "test-g0-d-reset", "test-g0-d-menu-settings", "test-g0-d-settings", "test-g0-d-h", "test-g0-d-settings-layout", "test-g0-d-i", "test-g0-d-ui-resize", "test-g0-d-new-world-cancel", "test-g0-d-header-layout", "test-g0-d-determinism", "test-g0-d-sanitize", "test-g0-d-posix-smoke", "demo-g0-d", "check-g0-d", "audit-g0-d",
                   "test-k0-a-sanitize:", "audit-k0-a:",
                   "test-k0-b-entities", "test-k0-b-commands", "test-k0-b-events",
                   "test-k0-b-determinism", "test-k0-b-sanitize", "demo-k0-b",
                   "audit-k0-b", "test-k0-c-random", "test-k0-c-hash",
                   "test-k0-c-observability", "test-k0-c-determinism",
                   "test-k0-c-optimization-determinism", "test-k0-c-sanitize",
                   "demo-k0-c", "audit-k0-c", "demo-k0-d", "test-k0-d-config",
                   "test-k0-d-artifacts", "test-k0-d-determinism",
                   "test-k0-d-optimization-determinism", "test-k0-d-corruption",
                   "test-k0-d-stress", "test-k0-d-long-run", "test-k0-d-sanitize",
                   "test-k0-external-consumer", "audit-k0-d", "audit-k0:",
                   "clean-worlds-kernel:",
                   "audit-d1-build-products:", "audit-d1-api:", "test:",
                   "test-architecture:"):
        if target not in root_makefile:
            fail(f"target raiz ausente: {target}")
    if "$(MAKE) -C $(CORE_DIR) $@" not in root_makefile:
        fail("encaminhamento de targets legados ausente")

    for path in CORE.rglob("*"):
        if path.is_dir() or path.suffix.lower() not in {".c", ".h"}:
            continue
        content = path.read_text(encoding="utf-8", errors="replace")
        if "brain_bridge" in content.lower():
            fail(f"Core depende do Brain Bridge: {path.relative_to(ROOT)}")
        for include in INCLUDE_PATTERN.findall(content):
            normalized = include.replace("\\", "/")
            if re.match(r"^[A-Za-z]:/", normalized) or normalized.startswith("/"):
                fail(f"include absoluto em {path.relative_to(ROOT)}")
            if "worlds/" in normalized.lower() or normalized.lower().startswith("worlds"):
                fail(f"dependencia de Worlds em {path.relative_to(ROOT)}")

    for directory in ("include", "src", "app", "tests", "docs", "scripts"):
        if not (WORLDS_BRAIN_BRIDGE / directory).is_dir():
            fail(f"worlds/brain_bridge/{directory}/ ausente")
    for filename in ("Makefile", "README.md"):
        if not (WORLDS_BRAIN_BRIDGE / filename).is_file():
            fail(f"worlds/brain_bridge/{filename} ausente")
    bridge_makefile = (WORLDS_BRAIN_BRIDGE / "Makefile").read_text(encoding="utf-8")
    if "CORE_ROOT" not in bridge_makefile or "DOMAIN_ROOT" not in bridge_makefile:
        fail("Makefile do Brain Bridge nao declara dependencias Core e Domain")
    for path in list((WORLDS_BRAIN_BRIDGE / "include").rglob("*")) + list((WORLDS_BRAIN_BRIDGE / "src").rglob("*")):
        if path.is_dir() or path.suffix.lower() not in {".c", ".h"}:
            continue
        content = path.read_text(encoding="utf-8", errors="replace")
        for include in INCLUDE_PATTERN.findall(content):
            normalized = include.replace("\\", "/").lower()
            if ("minisnn_worlds_kernel_internal" in normalized or "/kernel/src/" in normalized or
                    normalized.startswith("kernel/src/") or normalized == "minisnn_worlds_kernel.h"):
                fail(f"Brain Bridge inclui Kernel diretamente: {path.relative_to(ROOT)}")
        if path.parent == WORLDS_BRAIN_BRIDGE / "include":
            allowed = {"stddef.h", "stdint.h", "minisnn.h", "minisnn_worlds_domain.h"}
            for include in INCLUDE_PATTERN.findall(content):
                if include.replace("\\", "/") not in allowed:
                    fail(f"header publico do Brain Bridge inclui dependencia inesperada: {path.relative_to(ROOT)}")
    core_makefile = (CORE / "Makefile").read_text(encoding="utf-8")
    for target in ("core:", "core-lib:", "headless:", "core-test:",
                   "audit-d1-build-products:", "audit-d1-api:"):
        if target not in core_makefile:
            fail(f"target interno ausente: {target}")
    for obsolete_path in ("../include", "../src", "../app", "../configs", "../scripts", "../results"):
        if obsolete_path in core_makefile.replace("\\", "/"):
            fail(f"caminho anterior da raiz no Makefile do Core: {obsolete_path}")
    if "worlds" in core_makefile.lower():
        fail("o Makefile do Core exige uma dependencia de Worlds")

    for directory in ("include", "src", "app", "tests", "scripts", "configs", "docs", "results"):
        if not (WORLDS_KERNEL / directory).is_dir():
            fail(f"worlds/kernel/{directory}/ ausente")
    worlds_makefile = (WORLDS_KERNEL / "Makefile").read_text(encoding="utf-8")
    if "libminisnn_core" in worlds_makefile or "core/" in worlds_makefile:
        fail("Makefile do Worlds Kernel depende do Core")
    for path in list((WORLDS_KERNEL / "include").rglob("*")) + list((WORLDS_KERNEL / "src").rglob("*")):
        if path.is_dir() or path.suffix.lower() not in {".c", ".h"}:
            continue
        content = path.read_text(encoding="utf-8", errors="replace")
        for include in INCLUDE_PATTERN.findall(content):
            normalized = include.replace("\\", "/").lower()
            if ("minisnn.h" in normalized or "core/" in normalized or
                    "windows.h" in normalized or "pthread" in normalized):
                fail(f"Worlds Kernel depende de produto externo: {path.relative_to(ROOT)}")

    for path in (WORLDS_KERNEL / "app").glob("k0_scenario_*.c"):
        content = path.read_text(encoding="utf-8", errors="replace")
        if "minisnn_worlds_kernel.h" not in content and path.name != "k0_scenario_artifacts.c":
            fail(f"ferramenta K0-D nao usa a API publica: {path.relative_to(ROOT)}")
        if "src/minisnn_worlds_kernel" in content:
            fail(f"ferramenta K0-D inclui implementacao privada: {path.relative_to(ROOT)}")

    for directory in ("include", "src", "app", "tests", "docs", "scripts"):
        if not (WORLDS_DOMAIN / directory).is_dir():
            fail(f"worlds/domain/{directory}/ ausente")
    for filename in ("Makefile", "README.md"):
        if not (WORLDS_DOMAIN / filename).is_file():
            fail(f"worlds/domain/{filename} ausente")
    domain_makefile = (WORLDS_DOMAIN / "Makefile").read_text(encoding="utf-8")
    if "core/" in domain_makefile.replace("\\", "/"):
        fail("Makefile do Worlds Domain depende do Core")
    for path in list((WORLDS_DOMAIN / "include").rglob("*")) + list((WORLDS_DOMAIN / "src").rglob("*")):
        if path.is_dir() or path.suffix.lower() not in {".c", ".h"}:
            continue
        content = path.read_text(encoding="utf-8", errors="replace")
        for include in INCLUDE_PATTERN.findall(content):
            normalized = include.replace("\\", "/").lower()
            if ("core/" in normalized or "minisnn.h" == normalized or
                    "windows.h" == normalized or "minisnn_worlds_kernel_internal" in normalized or
                    "/src/" in normalized or normalized.startswith("src/")):
                fail(f"Worlds Domain library violates the public boundary: {path.relative_to(ROOT)}")
    wd1_file_adapter = WORLDS_DOMAIN / "app" / "wd1_domain_snapshot_file.c"
    if wd1_file_adapter.is_file() and "windows.h" not in wd1_file_adapter.read_text(encoding="utf-8", errors="replace"):
        fail("WD1 file adapter must keep its Win32 replacement call in app-layer")
    for path in list((WORLDS_KERNEL / "include").rglob("*")) + list((WORLDS_KERNEL / "src").rglob("*")):
        if path.is_dir() or path.suffix.lower() not in {".c", ".h"}:
            continue
        if "minisnn_worlds_domain" in path.read_text(encoding="utf-8", errors="replace").lower():
            fail(f"Worlds Kernel depende do Domain: {path.relative_to(ROOT)}")

    for path in WORLDS_DOMAIN.rglob("*"):
        if path.is_dir() or path.suffix.lower() not in {".c", ".h"}:
            continue
        if "brain_bridge" in path.read_text(encoding="utf-8", errors="replace").lower():
            fail(f"Worlds Domain depende do Brain Bridge: {path.relative_to(ROOT)}")
    for path in list((WORLDS_KERNEL / "include").rglob("*")) + list((WORLDS_KERNEL / "src").rglob("*")):
        if path.is_dir() or path.suffix.lower() not in {".c", ".h"}:
            continue
        if "brain_bridge" in path.read_text(encoding="utf-8", errors="replace").lower():
            fail(f"Worlds Kernel depende do Brain Bridge: {path.relative_to(ROOT)}")
    if (CORE / "app" / "minisnn_studio.c").exists():
        fail("Studio ainda esta misturado a core/app")
    if not (CORE / "studio" / "minisnn_studio.c").is_file():
        fail("entry point do Studio ausente em core/studio")

    for source in (CORE / "src").glob("*.c"):
        if "windows.h" in source.read_text(encoding="utf-8", errors="replace"):
            fail(f"Core depende de Win32: {source.relative_to(ROOT)}")

    generated = [path.name for path in ROOT.iterdir()
                 if path.is_file() and path.suffix.lower() in GENERATED_SUFFIXES
                 and path.name not in PUBLISHED_ROOT_PRODUCTS]
    if generated:
        fail("artefatos gerados na raiz: " + ", ".join(sorted(generated)))


    wf0 = ROOT / "worlds" / "scenarios" / "wf0_fish"
    for directory in ("include", "src", "app", "tests", "docs", "scripts"):
        if not (wf0 / directory).is_dir():
            fail(f"worlds/scenarios/wf0_fish/{directory}/ ausente")
    for filename in ("Makefile", "README.md"):
        if not (wf0 / filename).is_file():
            fail(f"worlds/scenarios/wf0_fish/{filename} ausente")
    wf0_makefile = (wf0 / "Makefile").read_text(encoding="utf-8")
    for target in (
        "test-wf0:", "test-wf0-neural-causality:", "test-wf0-neural-continuity:", "test-wf0-determinism:",
        "test-wf0-optimization-determinism:", "test-wf0-sanitize:",
        "test-wf0-posix-smoke:", "demo-wf0:", "check-wf0:", "audit-wf0:",
    ):
        if target not in wf0_makefile:
            fail(f"target WF0 ausente: {target}")
    for target in (
        "worlds-wf0:", "test-wf0", "test-wf0-neural-causality",
        "test-wf0-determinism", "test-wf0-optimization-determinism",
        "test-wf0-sanitize", "test-wf0-posix-smoke", "demo-wf0",
        "check-wf0:", "audit-wf0:",
    ):
        if target not in root_makefile:
            fail(f"target raiz WF0 ausente: {target}")

    for layer in (CORE, WORLDS_KERNEL, WORLDS_DOMAIN, WORLDS_BRAIN_BRIDGE):
        for path in list((layer / "include").rglob("*")) + list((layer / "src").rglob("*")):
            if path.is_dir() or path.suffix.lower() not in {".c", ".h"}:
                continue
            if "wf0_fish" in path.read_text(encoding="utf-8", errors="replace").lower():
                fail(f"camada generica conhece WF0: {path.relative_to(ROOT)}")

    for path in list((wf0 / "include").rglob("*")) + list((wf0 / "src").rglob("*")):
        if path.is_dir() or path.suffix.lower() not in {".c", ".h"}:
            continue
        content = path.read_text(encoding="utf-8", errors="replace")
        for include in INCLUDE_PATTERN.findall(content):
            normalized = include.replace("\\", "/").lower()
            if (normalized.startswith("/") or re.match(r"^[a-z]:/", normalized) or
                    "/src/" in normalized or normalized.startswith("src/")):
                fail(f"WF0 inclui implementacao privada: {path.relative_to(ROOT)}")

    wf0_action = (wf0 / "src" / "wf0_fish.c").read_text(encoding="utf-8", errors="replace")
    wf0_observation = (wf0 / "src" / "wf0_fish_observation.c").read_text(
        encoding="utf-8", errors="replace")
    if "minisnn_worlds_domain_perceive" in wf0_action:
        fail("acao WF0 le percepcao diretamente")
    if "minisnn_worlds_domain_perceive" not in wf0_observation:
        fail("observabilidade WF0 nao contem leitura de percepcao")
    if "minisnn_worlds_trainable_brain" in wf0_action:
        fail("WF0 historico nao pode depender do TrainableBrain")
    if not re.search(
        r"minisnn_worlds_brain_bridge_decide\([\s\S]*?&record\.decision\.action[\s\S]*?"
        r"minisnn_worlds_domain_step\([\s\S]*?&record\.decision\.action",
        wf0_action,
    ):
        fail("acao WF0 nao segue o BrainBridge fixo para Domain.step")


    for directory in ("include", "src", "app", "tests", "docs", "scripts"):
        if not (WORLDS_TERRAIN / directory).is_dir():
            fail(f"worlds/terrain/{directory}/ ausente")
    for filename in ("Makefile", "README.md"):
        if not (WORLDS_TERRAIN / filename).is_file():
            fail(f"worlds/terrain/{filename} ausente")
    terrain_makefile = (WORLDS_TERRAIN / "Makefile").read_text(encoding="utf-8")
    for target in (
            "test-wt0:", "test-wt0-materialization-failure:",
            "test-wt0-category-mask:", "test-wt0-determinism:",
            "test-wt0-optimization-determinism:", "test-wt0-sanitize:",
            "test-wt0-posix-smoke:", "demo-wt0:", "check-wt0:",
            "audit-wt0:"):
        if target not in terrain_makefile:
            fail(f"target Terrain ausente: {target}")
    for path in list((WORLDS_TERRAIN / "include").rglob("*")) + list((WORLDS_TERRAIN / "src").rglob("*")):
        if path.is_dir() or path.suffix.lower() not in {".c", ".h"}:
            continue
        content = path.read_text(encoding="utf-8", errors="replace")
        for include in INCLUDE_PATTERN.findall(content):
            normalized = include.replace("\\", "/").lower()
            if ("core/" in normalized or "minisnn.h" == normalized or
                    "minisnn_worlds_domain" in normalized or
                    "minisnn_worlds_brain_bridge" in normalized or
                    "/src/" in normalized or normalized.startswith("src/")):
                fail(f"Terrain viola fronteira publica do Kernel: {path.relative_to(ROOT)}")
    for layer in (CORE, WORLDS_KERNEL, WORLDS_DOMAIN, WORLDS_BRAIN_BRIDGE):
        for path in list((layer / "include").rglob("*")) + list((layer / "src").rglob("*")):
            if path.is_dir() or path.suffix.lower() not in {".c", ".h"}:
                continue
            if "minisnn_worlds_terrain" in path.read_text(encoding="utf-8", errors="replace").lower():
                fail(f"camada generica conhece Terrain: {path.relative_to(ROOT)}")

    g0 = WORLDS_G0_VISUALIZER
    for directory in ("include", "src", "app", "tests", "docs", "scripts"):
        if not (g0 / directory).is_dir():
            fail(f"worlds/app/g0_visualizer/{directory}/ ausente")
    for filename in ("Makefile", "README.md", "tests/test_g0_visualizer_source_contracts.py", "tests/test_g0_b_source_contracts.py", "tests/test_g0_c_source_contracts.py", "tests/test_g0_d_h_source_contracts.py", "tests/test_g0_d_i_source_contracts.py", "scripts/test_worlds_published_product.py", "include/g0_world_config.h", "include/g0_visualizer_ui_layout.h", "docs/G0_B_VISUAL_RUNTIME.md", "docs/G0_C_INTERACTIVE_SANDBOX.md", "docs/G0_D_PRODUCT_SHELL.md"):
        if not (g0 / filename).is_file():
            fail(f"worlds/app/g0_visualizer/{filename} ausente")
    g0_makefile = (g0 / "Makefile").read_text(encoding="utf-8")
    for target in (
            "worlds-app:", "test-g0-a:", "test-g0-a-determinism:",
            "test-g0-a-optimization-determinism:", "test-g0-a-sanitize:",
            "test-g0-a-posix-smoke:", "demo-g0-a:", "check-g0-a:",
            "audit-g0-a:", "test-g0-b:", "test-g0-b-runtime:", "test-g0-b-facing:", "test-g0-b-determinism:", "test-g0-b-optimization-determinism:", "test-g0-b-sanitize:", "test-g0-b-posix-smoke:", "demo-g0-b:", "check-g0-b:", "audit-g0-b:", "test-g0-c:", "test-g0-c-editor:", "test-g0-c-camera:", "test-g0-c-runtime:", "test-g0-c-determinism:", "test-g0-c-sanitize:", "test-g0-c-posix-smoke:", "demo-g0-c:", "check-g0-c:", "audit-g0-c:", "test-g0-d:", "test-g0-d-settings:", "test-g0-d-h:", "test-g0-d-settings-layout:", "test-g0-d-i:", "test-g0-d-ui-resize:", "test-g0-d-new-world-cancel:", "test-g0-d-header-layout:", "test-g0-d-determinism:", "test-g0-d-sanitize:", "test-g0-d-posix-smoke:", "demo-g0-d:", "check-g0-d:", "audit-g0-d:"):
        if target not in g0_makefile:
            fail(f"target G0-A ausente: {target}")
    for layer in (CORE, WORLDS_KERNEL, WORLDS_DOMAIN, WORLDS_BRAIN_BRIDGE):
        for path in list((layer / "include").rglob("*")) + list((layer / "src").rglob("*")):
            if path.is_dir() or path.suffix.lower() not in {".c", ".h"}:
                continue
            if "g0_visualizer" in path.read_text(encoding="utf-8", errors="replace").lower():
                fail(f"camada generica conhece o visualizador G0: {path.relative_to(ROOT)}")
    for path in list((g0 / "include").rglob("*")) + list((g0 / "src").rglob("*")):
        if path.is_dir() or path.suffix.lower() not in {".c", ".h"}:
            continue
        content = path.read_text(encoding="utf-8", errors="replace")
        for include in INCLUDE_PATTERN.findall(content):
            normalized = include.replace("\\", "/").lower()
            if (normalized.startswith("/") or re.match(r"^[a-z]:/", normalized) or
                    "/src/" in normalized or normalized.startswith("src/")):
                fail(f"G0-A inclui implementacao privada: {path.relative_to(ROOT)}")
    print("Monorepo architecture validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
