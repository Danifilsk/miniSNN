from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys


SANITIZER_FLAGS = ("-fsanitize=address,undefined", "-fno-omit-frame-pointer")
UNAVAILABLE_MARKERS = (
    "unrecognized command-line option",
    "unsupported option",
    "cannot find -lasan",
    "cannot find -lubsan",
    "libasan",
    "libubsan",
    "not found",
    "not recognized as an internal or external command",
)


def emit_failure(description: str, completed: subprocess.CompletedProcess[str]) -> int:
    print(f"K0-A sanitizer validation FAIL: {description}")
    if completed.stdout:
        print(completed.stdout, end="")
    if completed.stderr:
        print(completed.stderr, end="", file=sys.stderr)
    return 1


def emit_unavailable(description: str, completed: subprocess.CompletedProcess[str]) -> int:
    detail = f"{completed.stdout}\n{completed.stderr}".strip().replace("\n", " ")
    if detail:
        print(f"K0-A sanitizer validation UNAVAILABLE: {description}: {detail[:400]}")
    else:
        print(f"K0-A sanitizer validation UNAVAILABLE: {description}")
    return 0


def sanitizer_unavailable(completed: subprocess.CompletedProcess[str]) -> bool:
    output = f"{completed.stdout}\n{completed.stderr}".lower()
    return any(marker in output for marker in UNAVAILABLE_MARKERS)


def run(command: list[str], environment: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, text=True, capture_output=True, check=False, env=environment)


def main() -> int:
    parser = argparse.ArgumentParser(description="Executa a regressao ASan/UBSan do K0-A.")
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--include", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--test", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    include = Path(args.include).resolve()
    source = Path(args.source).resolve()
    test_source = Path(args.test).resolve()
    output_dir = Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    probe_source = output_dir / "k0_a_sanitizer_probe.c"
    probe_executable = output_dir / "k0_a_sanitizer_probe.exe"
    test_executable = output_dir / "test_worlds_kernel_sanitize.exe"
    environment = os.environ.copy()

    probe_source.write_text("int main(void) { return 0; }\n", encoding="ascii")
    probe = run([
        args.compiler,
        "-std=c11",
        *SANITIZER_FLAGS,
        str(probe_source),
        "-o",
        str(probe_executable),
    ])
    if probe.returncode != 0:
        if sanitizer_unavailable(probe):
            return emit_unavailable("ASan/UBSan probe compilation unavailable", probe)
        return emit_failure("ASan/UBSan probe compilation", probe)

    environment.setdefault("ASAN_OPTIONS", "detect_leaks=0")
    probe_run = run([str(probe_executable)], environment)
    if probe_run.returncode != 0:
        if sanitizer_unavailable(probe_run):
            return emit_unavailable("ASan/UBSan runtime unavailable", probe_run)
        return emit_failure("ASan/UBSan probe execution", probe_run)

    kernel_test = run([
        args.compiler,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Wformat=2",
        "-Wstrict-prototypes",
        *SANITIZER_FLAGS,
        "-DMINISNN_WORLDS_KERNEL_TESTING",
        f"-I{include}",
        str(test_source),
        str(source),
        "-o",
        str(test_executable),
    ])
    if kernel_test.returncode != 0:
        return emit_failure("compilacao do teste do Kernel apos probe valido", kernel_test)

    test_run = run([str(test_executable)], environment)
    if test_run.returncode != 0:
        return emit_failure("execucao do teste do Kernel", test_run)
    if "Worlds Kernel configuration prefix and tail validation OK" not in test_run.stdout:
        print("K0-A sanitizer validation FAIL: teste do prefixo e cauda nao confirmou execucao")
        return 1
    if "Worlds Kernel lifecycle and logical tick validation OK" not in test_run.stdout:
        print("K0-A sanitizer validation FAIL: teste de lifecycle nao confirmou execucao")
        return 1

    print("K0-A sanitizer validation PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
