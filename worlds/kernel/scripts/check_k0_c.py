from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


REQUIRED_HEADERS = (
    "minisnn_worlds_kernel_random.h",
    "minisnn_worlds_kernel_hash.h",
    "minisnn_worlds_kernel_diagnostics.h",
    "minisnn_worlds_kernel_config.h",
    "minisnn_worlds_kernel.h",
)
REQUIRED_DOCUMENTS = (
    "README.md",
    "docs/RANDOMNESS_CONTRACT.md",
    "docs/STATE_HASH_CONTRACT.md",
    "docs/OBSERVABILITY_CONTRACT.md",
    "docs/K0_C_RANDOM_HASH_OBSERVABILITY_AUDIT.md",
)
REQUIRED_TEST_SOURCES = (
    "tests/test_k0_c_random.c",
    "tests/test_k0_c_hash.c",
    "tests/test_k0_c_observability.c",
    "tests/test_k0_c_determinism.c",
    "tests/k0_c_optimization_runner.c",
    "tests/test_k0_c_optimization_determinism.py",
    "tests/test_k0_c_sanitize.py",
)
INCLUDE_PATTERN = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)
GENERATED_SUFFIXES = {".a", ".dll", ".exe", ".lib", ".o", ".obj", ".pyc"}


def fail(message: str) -> None:
    print(f"K0-C Worlds Kernel validation FAILED: {message}")
    raise SystemExit(1)


def run(command: list[str], description: str, cwd: Path) -> str:
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        fail(f"{description} falhou\n{result.stdout}{result.stderr}")
    return result.stdout


def validate_boundary(kernel_root: Path, repository_root: Path) -> None:
    forbidden_includes = ("minisnn.h", "core/", "windows.h", "pthread")
    forbidden_terms = (
        "neuron", "spike", "sensor", "stdp", "creature", "body", "hunger",
        "food", "combat", "government", "profession", "brain bridge", "domain",
        "worlds app", "component", "position", "grid", "map", "collision",
        "barrier", "link", "callback", "plugin", "snapshot", "replay", "sleep",
        "thread",
    )
    forbidden_calls = (
        r"\btime\s*\(", r"\bclock\s*\(", r"\bgettickcount\b",
        r"\bqueryperformancecounter\b", r"\bsleep\s*\(",
        r"\bnanosleep\s*\(", r"\brand\s*\(", r"\bsrand\s*\(",
        r"\bcreatethread\b",
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
                    fail(f"conceito futuro no Kernel: {term}")
            for pattern in forbidden_calls:
                if re.search(pattern, lowered):
                    fail(f"tempo real, libc rand ou thread em {path.relative_to(repository_root)}")


def validate_source_contracts(kernel_root: Path) -> None:
    source = (kernel_root / "src" / "minisnn_worlds_kernel.c").read_text(encoding="utf-8")
    config = (kernel_root / "include" / "minisnn_worlds_kernel_config.h").read_text(
        encoding="utf-8")
    random_header = (kernel_root / "include" / "minisnn_worlds_kernel_random.h").read_text(
        encoding="utf-8")
    hash_header = (kernel_root / "include" / "minisnn_worlds_kernel_hash.h").read_text(
        encoding="utf-8")
    aggregate = (kernel_root / "include" / "minisnn_worlds_kernel.h").read_text(
        encoding="utf-8")

    for required in (
        "MINISNN_WORLDS_KERNEL_DEFAULT_MASTER_SEED",
        "uint64_t master_seed",
        "MINISNN_WORLDS_KERNEL_PRNG_VERSION",
        "MiniSNNWorldsKernelRandomStreamKey",
        "random_bounded_u32",
        "random_stream_at",
    ):
        if required not in config and required not in random_header:
            fail(f"API de seed ou PRNG ausente: {required}")
    for required in (
        "MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION",
        "minisnn_worlds_kernel_state_hash",
    ):
        if required not in hash_header:
            fail(f"API de hash ausente: {required}")
    if "minisnn_worlds_kernel_random.h" not in aggregate or \
            "minisnn_worlds_kernel_hash.h" not in aggregate:
        fail("header agregado nao expoe as APIs K0-C")
    for required in (
        "pcg32_next", "splitmix64_permute", "random_prepare_candidate",
        "random_commit_candidate", "threshold =", "compute_state_hash",
        "fnv1a_append_u64", "canonical_random_stream_at",
        "total_random_u32_generated", "last_error",
    ):
        if required not in source:
            fail(f"contrato interno K0-C ausente: {required}")
    if re.search(r"(?:hash|fnv)[^(]*\(\s*&[^,]+,\s*sizeof", source, re.IGNORECASE):
        fail("hash bruto de struct detectado")
    hash_start = source.find("static uint64_t compute_state_hash")
    hash_end = source.find("MiniSNNWorldsKernelError minisnn_worlds_kernel_state_hash", hash_start)
    hash_body = source[hash_start:hash_end]
    if hash_start < 0 or hash_end < 0:
        fail("implementacao do hash de estado ausente")
    for excluded in ("last_error", "_capacity", "malloc(", "kernel_allocate("):
        if excluded in hash_body:
            fail(f"campo ou alocacao excluida presente no hash: {excluded}")
    if "random_stream_set" in aggregate or "set_master_seed" in aggregate:
        fail("setter publico de seed ou stream detectado")
    if "value % exclusive_upper_bound" not in source or "while (value < threshold)" not in source:
        fail("bounded draw sem rejection sampling explicito")


def validate_tests(kernel_root: Path) -> None:
    for relative_path in REQUIRED_TEST_SOURCES:
        if not (kernel_root / relative_path).is_file():
            fail(f"fonte de teste ausente: {relative_path}")
    random_test = (kernel_root / "tests" / "test_k0_c_random.c").read_text(encoding="utf-8")
    hash_test = (kernel_root / "tests" / "test_k0_c_hash.c").read_text(encoding="utf-8")
    observability_test = (kernel_root / "tests" / "test_k0_c_observability.c").read_text(
        encoding="utf-8")
    determinism_test = (kernel_root / "tests" / "test_k0_c_determinism.c").read_text(
        encoding="utf-8")
    for required in ("UINT64_MAX", "fail_next_allocation", "INVALID_BOUND",
                     "IDENTIFIER_OVERFLOW", "random_stream_at"):
        if required not in random_test:
            fail(f"cobertura random ausente: {required}")
    for required in ("last_error", "queue_create_entity", "UINT64_C(6726247500959070114)"):
        if required not in hash_test:
            fail(f"cobertura hash ausente: {required}")
    for required in ("capture_trace_point", "current_state_hash", "random_streams"):
        if required not in observability_test:
            fail(f"cobertura observabilidade ausente: {required}")
    for required in ("run_trajectory", "hashes", "UINT64_C(12346)"):
        if required not in determinism_test:
            fail(f"cobertura determinismo ausente: {required}")


def validate_documents(kernel_root: Path) -> None:
    audit = (kernel_root / "docs" / "K0_C_RANDOM_HASH_OBSERVABILITY_AUDIT.md").read_text(
        encoding="utf-8")
    for required in ("PCG32", "FNV-1a", "K0-D"):
        if required not in audit:
            fail(f"auditoria K0-C incompleta: {required}")
    if "K0-D" not in audit:
        fail("auditoria K0-C nao referencia a continuidade para K0-D")
    for relative_path in REQUIRED_DOCUMENTS:
        if not (kernel_root / relative_path).is_file():
            fail(f"documentacao ausente: {relative_path}")


def validate_symbols(library: Path, repository_root: Path) -> None:
    nm = shutil.which("nm")
    if nm is None:
        fail("nm ausente para auditoria de simbolos")
    output = run([nm, "-g", "--defined-only", str(library)], "auditoria de simbolos", repository_root)
    names = {line.split()[-1] for line in output.splitlines() if len(line.split()) >= 3}
    expected = {
        "minisnn_worlds_kernel_master_seed",
        "minisnn_worlds_kernel_random_u32",
        "minisnn_worlds_kernel_random_u64",
        "minisnn_worlds_kernel_random_bounded_u32",
        "minisnn_worlds_kernel_random_stream_count",
        "minisnn_worlds_kernel_random_stream_at",
        "minisnn_worlds_kernel_state_hash",
        "minisnn_worlds_kernel_capture_trace_point",
    }
    if not expected.issubset(names):
        fail("superficie publica K0-C incompleta")
    for name in names:
        if name.startswith("minisnn_") and not name.startswith("minisnn_worlds_kernel_"):
            fail(f"simbolo fora do namespace Worlds Kernel: {name}")
        if "testing_" in name or name == "WinMain":
            fail(f"simbolo de teste ou Win32 na biblioteca: {name}")
    if {"rand", "srand", "time", "clock"}.intersection(name.lower() for name in names):
        fail("simbolo proibido de libc exportado")


def validate_portability(kernel_root: Path) -> None:
    compiler = os.environ.get("CC") or shutil.which("clang") or shutil.which("gcc")
    if compiler is None:
        print("K0-C portable C11 smoke UNAVAILABLE: compilador C ausente")
        return
    output = kernel_root.parents[1] / "build" / "worlds" / "kernel" / "tests" / \
        "k0_c_portable_smoke.exe"
    output.parent.mkdir(parents=True, exist_ok=True)
    run([
        compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2",
        "-Wstrict-prototypes", "-DMINISNN_WORLDS_KERNEL_TESTING", "-Iinclude",
        "tests/test_k0_c_determinism.c", "src/minisnn_worlds_kernel.c", "-o", str(output),
    ], "smoke C11 portavel K0-C", kernel_root)
    run([str(output)], "execucao do smoke C11 portavel K0-C", kernel_root)
    print("K0-C portable C11 smoke PASS")


def main() -> int:
    parser = argparse.ArgumentParser(description="Valida PRNG, hash e observabilidade K0-C.")
    parser.add_argument("--root", required=True)
    parser.add_argument("--library", required=True)
    parser.add_argument("--demo", required=True)
    parser.add_argument("--random-test", required=True)
    parser.add_argument("--hash-test", required=True)
    parser.add_argument("--observability-test", required=True)
    parser.add_argument("--determinism-test", required=True)
    args = parser.parse_args()

    kernel_root = Path(args.root).resolve()
    repository_root = kernel_root.parents[1]
    library = (kernel_root / args.library).resolve()
    demo = (kernel_root / args.demo).resolve()
    tests = (
        ((kernel_root / args.random_test).resolve(), "K0-C random validation OK"),
        ((kernel_root / args.hash_test).resolve(), "K0-C state hash validation OK"),
        ((kernel_root / args.observability_test).resolve(), "K0-C observability validation OK"),
        ((kernel_root / args.determinism_test).resolve(), "K0-C deterministic random/hash validation OK"),
    )

    for header in REQUIRED_HEADERS:
        if not (kernel_root / "include" / header).is_file():
            fail(f"header publico K0-C ausente: {header}")
    if not library.is_file() or not demo.is_file():
        fail("biblioteca ou demo K0-C ausente")
    validate_boundary(kernel_root, repository_root)
    validate_source_contracts(kernel_root)
    validate_tests(kernel_root)
    validate_documents(kernel_root)
    validate_symbols(library, repository_root)
    for executable, marker in tests:
        if not executable.is_file() or marker not in run([str(executable)], "teste funcional K0-C", kernel_root):
            fail(f"teste K0-C nao confirmou contrato: {marker}")
    expected_demo = (
        "miniSNN Worlds Kernel K0-C random/hash demo\n"
        "seed=12345\n"
        "initial_hash=14445501914872736870\n"
        "stream_1_1_value_1=1577453522\n"
        "stream_1_2_value_1=1681730853\n"
        "tick=1 hash=14080488493108930979\n"
        "tick=2 hash=17115150544015036402\n"
        "repeat_match=yes\n"
        "different_seed_diverged=yes\n"
        "random_streams=2\n"
        "status=OK\n"
    )
    if run([str(demo)], "demo K0-C", kernel_root).replace("\r\n", "\n") != expected_demo:
        fail("saida do demo K0-C nao corresponde ao contrato estavel")
    validate_portability(kernel_root)
    print("K0-C Worlds Kernel random, hash and observability validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
