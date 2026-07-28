from __future__ import annotations

from pathlib import Path
import os
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = ROOT.parent
INCLUDE = ROOT / "include"
BUILD = REPOSITORY / "build" / "tests" / "headers"


def fail(message: str) -> None:
    print(f"D1-A public API validation failed: {message}")
    raise SystemExit(1)


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def compile_headers(headers: list[Path]) -> None:
    compiler = os.environ.get("CC", "gcc")
    BUILD.mkdir(parents=True, exist_ok=True)
    for header in headers:
        source = BUILD / f"{header.stem}_standalone.c"
        executable = BUILD / f"{header.stem}_standalone.exe"
        source.write_text(
            f'#include "{header.name}"\nint main(void) {{ return 0; }}\n',
            encoding="ascii",
        )
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-pedantic",
                   "-I", str(INCLUDE), str(source), "-o", str(executable)]
        result = subprocess.run(command, text=True, capture_output=True)
        if result.returncode != 0:
            fail(f"header nao e autonomo: {header.name}\n{result.stderr}")


def main() -> int:
    required_docs = (
        "PUBLIC_API_MANIFEST.md", "OWNERSHIP_AND_LIFETIME.md", "ERROR_MODEL.md",
        "LIMITS.md", "PERSISTENCE_COMPATIBILITY.md", "D1_A_API_ARCHITECTURE_AUDIT.md",
        "BUILD_PRODUCTS.md", "CORE_BRIDGE_API_CANDIDATE.md",
    )
    for name in required_docs:
        if not (ROOT / "docs" / name).is_file():
            fail(f"documento ausente: {name}")

    manifest = read(ROOT / "docs" / "PUBLIC_API_MANIFEST.md")
    headers = sorted(INCLUDE.glob("*.h"))
    if not headers:
        fail("nenhum header publico encontrado")
    for header in headers:
        content = read(header)
        if "windows.h" in content or "../src" in content or "studio/" in content:
            fail(f"header publico depende de implementacao ou Studio: {header.name}")
        if header.name not in manifest:
            fail(f"header publico ausente do manifesto: {header.name}")
        guard = "MINISNN_" if header.name != "minisnn.h" else "MINISNN_H"
        if guard not in content:
            fail(f"include guard nao namespaced: {header.name}")

    compile_headers(headers)

    legacy = INCLUDE / "minisnn_evolution_legacy.h"
    if not legacy.is_file() or "LEGACY_SUPPORTED" not in manifest:
        fail("superficie historica de evolucao nao esta documentada")

    version = INCLUDE / "minisnn_version.h"
    if not version.is_file() or "MINISNN_VERSION_STRING" not in read(version):
        fail("identidade de versao publica ausente")

    declaration = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(")
    for header in headers:
        if header == legacy:
            continue
        declarations = re.sub(r"/\*.*?\*/|//[^\n]*", "", read(header),
                              flags=re.DOTALL)
        for name in declaration.findall(declarations):
            if name in {"if", "while", "switch", "sizeof"} or name.isupper():
                continue
            if not name.startswith("minisnn_"):
                fail(f"funcao publica sem namespace em {header.name}: {name}")

    core_makefile = read(ROOT / "Makefile")
    if "audit-d1-build-products:" not in core_makefile or "audit-d1-api:" not in core_makefile:
        fail("targets de auditoria D1-A ausentes")
    if "check_public_api.py" not in core_makefile or "check_build_products.py" not in core_makefile:
        fail("Makefile nao executa checkers D1-A")

    print("D1-A public API and architecture validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
