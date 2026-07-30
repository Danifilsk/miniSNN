from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


REQUIRED_HEADERS = (
    "minisnn_worlds_kernel_entity.h",
    "minisnn_worlds_kernel_command.h",
    "minisnn_worlds_kernel_event.h",
)
REQUIRED_DOCUMENTS = (
    "docs/ENTITY_ID_CONTRACT.md",
    "docs/COMMAND_AND_EVENT_CONTRACT.md",
    "docs/K0_B_ENTITY_COMMAND_EVENT_AUDIT.md",
)
REQUIRED_TEST_SOURCES = (
    "tests/test_k0_b_entities.c",
    "tests/test_k0_b_commands.c",
    "tests/test_k0_b_events.c",
    "tests/test_k0_b_determinism.c",
)
INCLUDE_PATTERN = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)
GENERATED_SUFFIXES = {".a", ".dll", ".exe", ".lib", ".o", ".obj", ".pyc"}


def fail(message: str) -> None:
    print(f"K0-B Worlds Kernel validation FAILED: {message}")
    raise SystemExit(1)


def run(command: list[str], description: str, cwd: Path) -> str:
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        fail(f"{description} falhou\n{result.stdout}{result.stderr}")
    return result.stdout


def validate_boundary(kernel_root: Path, repository_root: Path) -> None:
    forbidden_includes = ("minisnn.h", "core/", "windows.h", "pthread")
    forbidden_terms = (
        "neuron", "spike", "sensor", "stdp", "creature", "food", "hunger",
        "body", "combat", "government", "profession", "brain bridge", "domain",
        "worlds app", "component", "position", "grid", "map", "collision",
        "barrier", "link", "callback", "plugin", "snapshot", "replay", "seed",
        "hash", "rand", "srand", "time", "clock", "sleep", "thread",
    )

    for directory_name in ("include", "src"):
        for path in (kernel_root / directory_name).rglob("*"):
            if not path.is_file():
                continue
            if path.suffix.lower() in GENERATED_SUFFIXES:
                fail(f"artefato junto ao codigo: {path.relative_to(repository_root)}")
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


def validate_source_contracts(kernel_root: Path) -> None:
    types = (kernel_root / "include" / "minisnn_worlds_kernel_types.h").read_text(
        encoding="utf-8")
    command = (kernel_root / "include" / "minisnn_worlds_kernel_command.h").read_text(
        encoding="utf-8")
    event = (kernel_root / "include" / "minisnn_worlds_kernel_event.h").read_text(
        encoding="utf-8")
    source = (kernel_root / "src" / "minisnn_worlds_kernel.c").read_text(
        encoding="utf-8")

    for name in ("MiniSNNWorldsKernelEntityId", "MiniSNNWorldsKernelCommandId",
                 "MiniSNNWorldsKernelEventId"):
        if f"}} {name};" not in types or "uint64_t value;" not in types:
            fail(f"tipo forte ausente: {name}")
    for required in ("queue_create_entity", "queue_destroy_entity",
                     "pending_command_at", "last_tick_event_at"):
        if required not in command and required not in event:
            fail(f"API K0-B ausente: {required}")
    if "qsort(" not in source or "command_qsort_compare" not in source:
        fail("ordenacao canonica nao esta implementada")
    if re.search(r"return\s*\(\s*int\s*\)\s*\([^)]*-[^)]*\)", source):
        fail("comparador pode usar subtracao sujeita a overflow")
    if "prepare_step_plan" not in source or "step_plan_destroy" not in source:
        fail("preflight atomico ausente")
    for required in ("planned_entities", "next_entity_id", "next_event_id",
                     "MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_ISSUER_NOT_ALIVE"):
        if required not in source:
            fail(f"contrato de resolucao K0-B ausente: {required}")
    if "MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED" not in source:
        fail("rejeicao semantica nao emite evento")
    if "minisnn_worlds_kernel_create_entity" in source or \
       "minisnn_worlds_kernel_destroy_entity" in source:
        fail("mutacao direta de entidade encontrada")


def validate_test_sources(kernel_root: Path) -> None:
    for relative_path in REQUIRED_TEST_SOURCES:
        path = kernel_root / relative_path
        if not path.is_file():
            fail(f"fonte de teste ausente: {relative_path}")
    entities = (kernel_root / "tests" / "test_k0_b_entities.c").read_text(
        encoding="utf-8")
    commands = (kernel_root / "tests" / "test_k0_b_commands.c").read_text(
        encoding="utf-8")
    events = (kernel_root / "tests" / "test_k0_b_events.c").read_text(
        encoding="utf-8")
    determinism = (kernel_root / "tests" / "test_k0_b_determinism.c").read_text(
        encoding="utf-8")
    for required in ("UINT64_MAX", "entity_at", "entity_exists"):
        if required not in entities and required not in determinism:
            fail(f"cobertura de Entity ID ausente: {required}")
    for required in ("INVALID_TICK", "IDENTIFIER_OVERFLOW", "fail_next_allocation"):
        if required not in commands:
            fail(f"cobertura de submissao ausente: {required}")
    for required in ("ISSUER_NOT_ALIVE", "TARGET_NOT_ALIVE",
                     "fail_allocation_after", "set_next_event_id"):
        if required not in events:
            fail(f"cobertura de evento ou atomicidade ausente: {required}")
    if "snapshot_matches" not in determinism or "swapped_priorities" not in determinism:
        fail("cobertura de determinismo ausente")


def validate_symbols(library: Path, repository_root: Path) -> None:
    nm = shutil.which("nm")
    if nm is None:
        fail("nm ausente para auditoria de simbolos")
    output = run([nm, "-g", "--defined-only", str(library)], "auditoria de simbolos", repository_root)
    names = {line.split()[-1] for line in output.splitlines() if len(line.split()) >= 3}
    expected = {
        "minisnn_worlds_kernel_entity_exists",
        "minisnn_worlds_kernel_entity_count",
        "minisnn_worlds_kernel_entity_at",
        "minisnn_worlds_kernel_queue_create_entity",
        "minisnn_worlds_kernel_queue_destroy_entity",
        "minisnn_worlds_kernel_pending_command_count",
        "minisnn_worlds_kernel_pending_command_at",
        "minisnn_worlds_kernel_last_tick_event_count",
        "minisnn_worlds_kernel_last_tick_event_at",
    }
    if not expected.issubset(names):
        fail("superficie publica K0-B incompleta")
    for name in names:
        if name.startswith("minisnn_") and not name.startswith("minisnn_worlds_kernel_"):
            fail(f"simbolo fora do namespace Worlds Kernel: {name}")
        if "testing_" in name or name == "WinMain":
            fail(f"simbolo proibido na biblioteca normal: {name}")
    if any(token in output.lower() for token in ("rand", "srand", "time", "clock")):
        fail("biblioteca exporta simbolo de aleatoriedade ou tempo real")


def validate_portability(kernel_root: Path) -> None:
    compiler = os.environ.get("CC") or shutil.which("clang") or shutil.which("gcc")
    if compiler is None:
        print("K0-B portable C11 smoke UNAVAILABLE: compilador C ausente")
        return
    output = kernel_root.parents[1] / "build" / "worlds" / "kernel" / "tests" / \
        "k0_b_portable_smoke.exe"
    output.parent.mkdir(parents=True, exist_ok=True)
    run([
        compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2",
        "-Wstrict-prototypes", "-DMINISNN_WORLDS_KERNEL_TESTING", "-Iinclude",
        "tests/test_k0_b_determinism.c", "src/minisnn_worlds_kernel.c",
        "-o", str(output),
    ], "smoke C11 portavel K0-B", kernel_root)
    run([str(output)], "execucao do smoke C11 portavel K0-B", kernel_root)
    print("K0-B portable C11 smoke PASS")


def validate_sanitizer(kernel_root: Path) -> None:
    compiler = os.environ.get("CC") or shutil.which("clang") or shutil.which("gcc")
    if compiler is None:
        print("K0-B sanitizer validation UNAVAILABLE: compilador C ausente")
        return
    output_dir = kernel_root.parents[1] / "build" / "worlds" / "kernel" / "tests"
    output = run([
        sys.executable,
        str(kernel_root / "tests" / "test_k0_b_sanitize.py"),
        "--compiler", compiler,
        "--include", str(kernel_root / "include"),
        "--source", str(kernel_root / "src" / "minisnn_worlds_kernel.c"),
        "--output-dir", str(output_dir),
        "--demo", str(kernel_root / "app" / "k0_entity_command_demo.c"),
        "--tests",
        str(kernel_root / "tests" / "test_k0_b_entities.c"),
        str(kernel_root / "tests" / "test_k0_b_commands.c"),
        str(kernel_root / "tests" / "test_k0_b_events.c"),
        str(kernel_root / "tests" / "test_k0_b_determinism.c"),
    ], "regressao ASan/UBSan K0-B", kernel_root)
    if "K0-B sanitizer validation PASS" in output:
        return
    if "K0-B sanitizer validation UNAVAILABLE:" in output:
        print(output, end="")
        return
    fail("regressao ASan/UBSan K0-B sem classificacao valida")


def main() -> int:
    parser = argparse.ArgumentParser(description="Valida entidades, comandos e eventos K0-B.")
    parser.add_argument("--root", required=True)
    parser.add_argument("--library", required=True)
    parser.add_argument("--demo", required=True)
    parser.add_argument("--entity-test", required=True)
    parser.add_argument("--command-test", required=True)
    parser.add_argument("--event-test", required=True)
    parser.add_argument("--determinism-test", required=True)
    args = parser.parse_args()

    kernel_root = Path(args.root).resolve()
    repository_root = kernel_root.parents[1]
    library = (kernel_root / args.library).resolve()
    demo = (kernel_root / args.demo).resolve()
    tests = (
        ((kernel_root / args.entity_test).resolve(), "K0-B entity validation OK"),
        ((kernel_root / args.command_test).resolve(), "K0-B command validation OK"),
        ((kernel_root / args.event_test).resolve(), "K0-B event and atomicity validation OK"),
        ((kernel_root / args.determinism_test).resolve(), "K0-B deterministic resolution validation OK"),
    )

    for header in REQUIRED_HEADERS:
        if not (kernel_root / "include" / header).is_file():
            fail(f"header K0-B ausente: {header}")
    for document in REQUIRED_DOCUMENTS:
        if not (kernel_root / document).is_file():
            fail(f"documentacao K0-B ausente: {document}")
    audit_document = (kernel_root / "docs" / "K0_B_ENTITY_COMMAND_EVENT_AUDIT.md").read_text(
        encoding="utf-8")
    for required in ("Severidade", "Armazenamento, ordem e erros", "K0-C"):
        if required not in audit_document:
            fail(f"auditoria K0-B incompleta: {required}")
    if not library.is_file() or not demo.is_file():
        fail("biblioteca ou demo K0-B ausente")
    validate_boundary(kernel_root, repository_root)
    validate_source_contracts(kernel_root)
    validate_test_sources(kernel_root)
    validate_symbols(library, repository_root)
    for test, marker in tests:
        if not test.is_file():
            fail(f"teste K0-B ausente: {test}")
        if marker not in run([str(test)], "teste funcional K0-B", kernel_root):
            fail(f"teste K0-B nao confirmou contrato: {marker}")
    expected_demo = (
        "miniSNN Worlds Kernel K0-B entity/command demo\n"
        "tick=1 event=1 type=ENTITY_CREATED subject=1\n"
        "tick=1 event=2 type=ENTITY_CREATED subject=2\n"
        "tick=1 event=3 type=ENTITY_CREATED subject=3\n"
        "tick=2 event=4 type=ENTITY_DESTROYED subject=2\n"
        "tick=2 event=5 type=COMMAND_REJECTED subject=2 reason=TARGET_NOT_ALIVE\n"
        "tick=3 events=0\n"
        "tick=4 event=6 type=ENTITY_CREATED subject=4\n"
        "alive_entities=3\n"
        "pending_commands=0\n"
        "status=OK\n"
    )
    demo_output = run([str(demo)], "demo K0-B", kernel_root).replace("\r\n", "\n")
    if demo_output != expected_demo:
        fail("saida do demo K0-B nao corresponde ao contrato estavel")
    validate_portability(kernel_root)
    validate_sanitizer(kernel_root)
    print("K0-B Worlds Kernel entity, command and event validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
