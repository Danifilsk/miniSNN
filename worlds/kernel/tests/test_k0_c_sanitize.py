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
)


def run(command: list[str], environment: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, text=True, capture_output=True, check=False, env=environment)


def unavailable(result: subprocess.CompletedProcess[str]) -> bool:
    content = f"{result.stdout}\n{result.stderr}".lower()
    return any(marker in content for marker in UNAVAILABLE_MARKERS)


def print_result(prefix: str, result: subprocess.CompletedProcess[str]) -> None:
    print(prefix)
    if result.stdout:
        print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)


def main() -> int:
    parser = argparse.ArgumentParser(description="Executa regressao ASan/UBSan K0-C.")
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--include", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--demo", required=True)
    parser.add_argument("--tests", nargs="+", required=True)
    args = parser.parse_args()

    include = Path(args.include).resolve()
    source = Path(args.source).resolve()
    output_dir = Path(args.output_dir).resolve()
    demo = Path(args.demo).resolve()
    test_sources = [Path(item).resolve() for item in args.tests]
    probe_source = output_dir / "k0_c_sanitizer_probe.c"
    probe_executable = output_dir / "k0_c_sanitizer_probe.exe"
    environment = os.environ.copy()

    output_dir.mkdir(parents=True, exist_ok=True)
    probe_source.write_text("int main(void) { return 0; }\n", encoding="ascii")
    result = run([args.compiler, "-std=c11", *SANITIZER_FLAGS, str(probe_source), "-o",
                  str(probe_executable)])
    if result.returncode != 0:
        if unavailable(result):
            detail = f"{result.stdout}\n{result.stderr}".strip().replace("\n", " ")
            print(f"K0-C sanitizer validation UNAVAILABLE: {detail[:400]}")
            return 0
        print_result("K0-C sanitizer validation FAIL: probe compilation", result)
        return 1

    environment.setdefault("ASAN_OPTIONS", "detect_leaks=0")
    result = run([str(probe_executable)], environment)
    if result.returncode != 0:
        if unavailable(result):
            detail = f"{result.stdout}\n{result.stderr}".strip().replace("\n", " ")
            print(f"K0-C sanitizer validation UNAVAILABLE: {detail[:400]}")
            return 0
        print_result("K0-C sanitizer validation FAIL: probe execution", result)
        return 1

    for test_source in test_sources:
        executable = output_dir / f"{test_source.stem}_sanitize.exe"
        result = run([
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
            str(executable),
        ])
        if result.returncode != 0:
            print_result("K0-C sanitizer validation FAIL: test compilation", result)
            return 1
        result = run([str(executable)], environment)
        if result.returncode != 0:
            print_result("K0-C sanitizer validation FAIL: test execution", result)
            return 1

    executable = output_dir / "k0_random_hash_demo_sanitize.exe"
    result = run([
        args.compiler,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Wformat=2",
        "-Wstrict-prototypes",
        *SANITIZER_FLAGS,
        f"-I{include}",
        str(demo),
        str(source),
        "-o",
        str(executable),
    ])
    if result.returncode != 0:
        print_result("K0-C sanitizer validation FAIL: demo compilation", result)
        return 1
    result = run([str(executable)], environment)
    if result.returncode != 0 or "status=OK" not in result.stdout:
        print_result("K0-C sanitizer validation FAIL: demo execution", result)
        return 1

    print("K0-C sanitizer validation PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
