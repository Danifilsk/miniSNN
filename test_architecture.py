from __future__ import annotations

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parent
CORE = ROOT / "core"
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
