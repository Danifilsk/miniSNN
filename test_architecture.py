from __future__ import annotations

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parent
CORE = ROOT / "core"
WORLDS_KERNEL = ROOT / "worlds" / "kernel"
WORLDS_DOMAIN = ROOT / "worlds" / "domain"
EXPECTED_DIRECTORIES = (
    "include", "src", "app", "studio", "tests", "scripts", "configs", "docs",
    "examples", "experiments", "results",
)
GENERATED_SUFFIXES = {".csv", ".png", ".html", ".exe", ".o", ".obj"}
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
                   "test-wd0-domain", "test-wd0-actions", "test-wd0-perception",
                   "test-wd0-invariants", "test-wd0-stress", "test-wd0-determinism",
                   "test-wd0-optimization-determinism", "test-wd0-sanitize",
                   "test-wd0-posix-smoke", "demo-wd0", "check-wd0:", "audit-wd0:",
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
        for include in INCLUDE_PATTERN.findall(content):
            normalized = include.replace("\\", "/")
            if re.match(r"^[A-Za-z]:/", normalized) or normalized.startswith("/"):
                fail(f"include absoluto em {path.relative_to(ROOT)}")
            if "worlds/" in normalized.lower() or normalized.lower().startswith("worlds"):
                fail(f"dependencia de Worlds em {path.relative_to(ROOT)}")

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
    for path in WORLDS_DOMAIN.rglob("*"):
        if path.is_dir() or path.suffix.lower() not in {".c", ".h"}:
            continue
        content = path.read_text(encoding="utf-8", errors="replace")
        for include in INCLUDE_PATTERN.findall(content):
            normalized = include.replace("\\", "/").lower()
            if ("core/" in normalized or "minisnn.h" == normalized or
                    "windows.h" == normalized or "minisnn_worlds_kernel_internal" in normalized or
                    "/src/" in normalized or normalized.startswith("src/")):
                fail(f"Worlds Domain viola a fronteira publica: {path.relative_to(ROOT)}")
    for path in list((WORLDS_KERNEL / "include").rglob("*")) + list((WORLDS_KERNEL / "src").rglob("*")):
        if path.is_dir() or path.suffix.lower() not in {".c", ".h"}:
            continue
        if "minisnn_worlds_domain" in path.read_text(encoding="utf-8", errors="replace").lower():
            fail(f"Worlds Kernel depende do Domain: {path.relative_to(ROOT)}")

    if (CORE / "app" / "minisnn_studio.c").exists():
        fail("Studio ainda esta misturado a core/app")
    if not (CORE / "studio" / "minisnn_studio.c").is_file():
        fail("entry point do Studio ausente em core/studio")

    for source in (CORE / "src").glob("*.c"):
        if "windows.h" in source.read_text(encoding="utf-8", errors="replace"):
            fail(f"Core depende de Win32: {source.relative_to(ROOT)}")

    generated = [path.name for path in ROOT.iterdir()
                 if path.is_file() and path.suffix.lower() in GENERATED_SUFFIXES]
    if generated:
        fail("artefatos gerados na raiz: " + ", ".join(sorted(generated)))

    print("Monorepo architecture validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
