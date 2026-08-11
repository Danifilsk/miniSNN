from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


PUBLIC_HEADERS = (
    "minisnn_worlds_kernel.h",
    "minisnn_worlds_kernel_types.h",
    "minisnn_worlds_kernel_config.h",
    "minisnn_worlds_kernel_diagnostics.h",
)
REQUIRED_DOCUMENTS = (
    "README.md",
    "docs/ARCHITECTURE.md",
    "docs/PUBLIC_API.md",
    "docs/TIME_AND_TICK_CONTRACT.md",
    "docs/ERROR_MODEL.md",
    "docs/K0_A_FOUNDATION_AUDIT.md",
)
GENERATED_SUFFIXES = {".a", ".dll", ".exe", ".lib", ".o", ".obj", ".pyc"}
# Later modules are verified by their own phase gates. Historical gates keep
# auditing their original surface without treating a future public extension as
# a forbidden K0 concept.
LATER_MODULE_PATHS = {
    "include/minisnn_worlds_kernel.h",
    "include/minisnn_worlds_kernel_snapshot.h",
    "include/minisnn_worlds_kernel_command_log.h",
    "src/minisnn_worlds_kernel_internal.h",
    "src/minisnn_worlds_kernel_snapshot.c",
    "src/minisnn_worlds_kernel_restore.c",
    "src/minisnn_worlds_kernel_command_log.c",
}
INCLUDE_PATTERN = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)


def fail(message: str) -> None:
    print(f"K0-A Worlds Kernel foundation validation FAILED: {message}")
    raise SystemExit(1)


def run(command: list[str], description: str, cwd: Path) -> str:
    completed = subprocess.run(command, cwd=cwd, text=True, capture_output=True, check=False)
    if completed.returncode != 0:
        fail(f"{description} falhou\n{completed.stdout}{completed.stderr}")
    return completed.stdout


def validate_source_boundary(kernel_root: Path, repository_root: Path) -> None:
    forbidden_includes = ("minisnn.h", "core/", "windows.h", "pthread")
    forbidden_terms = (
        "neuron", "spike", "sensor", "stdp", "creature", "body", "hunger",
        "food", "combat", "government", "callback", "plugin",
        "grid", "snapshot", "replay",
        "action", "map",
    )
    forbidden_calls = (
        r"\btime\s*\(", r"\bclock\s*\(", r"\bgettickcount\b",
        r"\bqueryperformancecounter\b", r"\bsleep\s*\(",
        r"\bnanosleep\s*\(", r"\brand\s*\(", r"\bsrand\s*\(",
        r"\bcreatethread\b",
    )

    for directory in (kernel_root / "include", kernel_root / "src"):
        for path in directory.rglob("*"):
            if not path.is_file():
                continue
            if path.suffix.lower() in GENERATED_SUFFIXES:
                fail(f"artefato junto ao fonte: {path.relative_to(repository_root)}")
            if path.relative_to(kernel_root).as_posix() in LATER_MODULE_PATHS:
                continue
            if path.suffix.lower() not in {".c", ".h"}:
                continue
            content = path.read_text(encoding="utf-8", errors="replace")
            lowered = content.lower()
            for include in INCLUDE_PATTERN.findall(content):
                normalized = include.replace("\\", "/").lower()
                if any(item in normalized for item in forbidden_includes):
                    fail(f"include proibido em {path.relative_to(repository_root)}: {include}")
            for term in forbidden_terms:
                if re.search(rf"\b{re.escape(term)}\b", lowered):
                    fail(f"conceito fora de escopo em {path.relative_to(repository_root)}: {term}")
            for pattern in forbidden_calls:
                if re.search(pattern, lowered):
                    fail(f"tempo real, aleatoriedade ou thread em {path.relative_to(repository_root)}")

    for directory_name in ("include", "src", "app", "tests"):
        directory = kernel_root / directory_name
        for path in directory.rglob("*"):
            if path.is_file() and path.suffix.lower() in GENERATED_SUFFIXES:
                fail(f"artefato junto ao codigo do Kernel: {path.relative_to(repository_root)}")

    for path in (repository_root / "core").rglob("*"):
        if not path.is_file() or path.suffix.lower() not in {".c", ".h"}:
            continue
        content = path.read_text(encoding="utf-8", errors="replace")
        for include in INCLUDE_PATTERN.findall(content):
            normalized = include.replace("\\", "/").lower()
            if "worlds/" in normalized or normalized.startswith("worlds"):
                fail(f"Core depende do Kernel: {path.relative_to(repository_root)}")


def validate_symbols(library: Path, repository_root: Path) -> None:
    nm = shutil.which("nm")
    if nm is None:
        fail("nm ausente para auditoria de simbolos")
    output = run([nm, "-g", "--defined-only", str(library)], "auditoria de simbolos", repository_root)
    names = [line.split()[-1] for line in output.splitlines() if len(line.split()) >= 3]
    expected = {
        "minisnn_worlds_kernel_config_default",
        "minisnn_worlds_kernel_create",
        "minisnn_worlds_kernel_destroy",
        "minisnn_worlds_kernel_tick",
        "minisnn_worlds_kernel_state",
        "minisnn_worlds_kernel_last_error",
        "minisnn_worlds_kernel_step",
        "minisnn_worlds_kernel_get_diagnostics",
    }
    if not expected.issubset(set(names)):
        fail("biblioteca sem a superficie publica K0-A esperada")
    for name in names:
        if name.startswith("minisnn_") and not name.startswith("minisnn_worlds_kernel_"):
            fail(f"simbolo fora do namespace Worlds Kernel: {name}")
        if "testing_" in name or name == "WinMain":
            fail(f"simbolo proibido no build normal: {name}")
    prohibited_symbols = {"minisnn_create", "rand", "srand", "time", "clock"}
    if any(name.lower() in prohibited_symbols for name in names):
        fail("biblioteca exporta simbolo proibido")


def validate_portability(kernel_root: Path) -> None:
    compiler = os.environ.get("CC") or shutil.which("clang") or shutil.which("gcc")
    if compiler is None:
        print("K0-A portable C11 smoke UNAVAILABLE")
        return
    output = kernel_root.parents[1] / "build" / "worlds" / "kernel" / "tests" / "k0_portable_smoke.exe"
    output.parent.mkdir(parents=True, exist_ok=True)
    command = [
        compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2",
        "-Wstrict-prototypes", "-DMINISNN_WORLDS_KERNEL_TESTING", "-Iinclude",
        "tests/test_worlds_kernel.c", "src/minisnn_worlds_kernel.c", "-o", str(output),
    ]
    run(command, "smoke C11 portavel", kernel_root)
    run([str(output)], "execucao do smoke C11 portavel", kernel_root)
    print("K0-A portable C11 smoke PASS")


def validate_configuration_prefix(kernel_root: Path, test_output: str) -> None:
    source = (kernel_root / "src" / "minisnn_worlds_kernel.c").read_text(
        encoding="utf-8")
    test_source = (kernel_root / "tests" / "test_worlds_kernel.c").read_text(
        encoding="utf-8")

    if re.search(r"\b\w+\s*=\s*\*config\b", source):
        fail("create copia a configuracao inteira antes de validar o prefixo")
    if "memcpy(&struct_size" not in source or "memcpy(&format_version" not in source:
        fail("leitura de prefixo da configuracao nao esta explicita")
    if "malloc(sizeof(uint32_t))" not in test_source:
        fail("teste de configuracao fisicamente truncada ausente")
    if "larger_config_size" not in test_source:
        fail("teste de cauda desconhecida ausente")
    if "Worlds Kernel configuration prefix and tail validation OK" not in test_output:
        fail("teste funcional do prefixo e cauda nao confirmou execucao")


def validate_sanitizer(kernel_root: Path) -> None:
    compiler = os.environ.get("CC") or shutil.which("clang") or shutil.which("gcc")
    script = kernel_root / "tests" / "test_k0_a_sanitize.py"
    output_dir = kernel_root.parents[1] / "build" / "worlds" / "kernel" / "tests"

    if compiler is None:
        print("K0-A sanitizer validation UNAVAILABLE: compilador C ausente")
        return
    if not script.is_file():
        fail("script de regressao ASan/UBSan ausente")

    output = run([
        sys.executable,
        str(script),
        "--compiler",
        compiler,
        "--include",
        str(kernel_root / "include"),
        "--source",
        str(kernel_root / "src" / "minisnn_worlds_kernel.c"),
        "--test",
        str(kernel_root / "tests" / "test_worlds_kernel.c"),
        "--output-dir",
        str(output_dir),
    ], "regressao ASan/UBSan", kernel_root)
    if "K0-A sanitizer validation PASS" in output:
        return
    if "K0-A sanitizer validation UNAVAILABLE:" in output:
        print(output, end="")
        return
    fail("regressao ASan/UBSan sem classificacao valida")


def main() -> int:
    parser = argparse.ArgumentParser(description="Valida a fundacao K0-A do Worlds Kernel.")
    parser.add_argument("--root", required=True)
    parser.add_argument("--library", required=True)
    parser.add_argument("--demo", required=True)
    parser.add_argument("--test", required=True)
    args = parser.parse_args()

    kernel_root = Path(args.root).resolve()
    repository_root = kernel_root.parents[1]
    library = (kernel_root / args.library).resolve()
    demo = (kernel_root / args.demo).resolve()
    test_executable = (kernel_root / args.test).resolve()

    for directory in ("include", "src", "app", "tests", "scripts", "configs", "docs", "results"):
        if not (kernel_root / directory).is_dir():
            fail(f"diretorio ausente: worlds/kernel/{directory}")
    for header in PUBLIC_HEADERS:
        if not (kernel_root / "include" / header).is_file():
            fail(f"header publico ausente: {header}")
    for document in REQUIRED_DOCUMENTS:
        if not (kernel_root / document).is_file():
            fail(f"documentacao ausente: worlds/kernel/{document}")
    # O Domain pode existir como consumidor posterior da API publica do Kernel.
    # Bridge e App continuam fora do escopo da fundacao K0-A.
    if (repository_root / "worlds" / "bridge").exists() or \
       (repository_root / "worlds" / "app").exists():
        fail("produto futuro criado antes do sub-bloco correspondente")
    kernel_makefile = (kernel_root / "Makefile").read_text(encoding="utf-8")
    if "libminisnn_core" in kernel_makefile or "core/" in kernel_makefile:
        fail("Makefile do Kernel depende do Core")
    if not library.is_file() or not demo.is_file() or not test_executable.is_file():
        fail("produto de build K0-A ausente")

    validate_source_boundary(kernel_root, repository_root)
    validate_symbols(library, repository_root)
    output = run([str(test_executable)], "teste do Kernel", kernel_root)
    if "Worlds Kernel lifecycle and logical tick validation OK" not in output:
        fail("teste do Kernel nao confirmou lifecycle e tick")
    validate_configuration_prefix(kernel_root, output)
    demo_output = run([str(demo)], "demo K0-A", kernel_root)
    expected_demo = (
        "miniSNN Worlds Kernel K0-A tick demo\n"
        "initial_tick=0\n"
        "steps_requested=10\n"
        "completed_ticks=10\n"
        "state=READY\n"
        "status=OK\n"
    )
    if demo_output.replace("\r\n", "\n") != expected_demo:
        fail("saida do demo nao corresponde ao contrato estavel")
    validate_portability(kernel_root)
    validate_sanitizer(kernel_root)
    print("K0-A Worlds Kernel foundation validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
