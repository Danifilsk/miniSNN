#!/usr/bin/env python3
"""Run complete K1-C3 coverage after an independent ASan/UBSan probe."""
from __future__ import annotations

import argparse
from pathlib import Path
import sys
import tempfile

sys.dont_write_bytecode = True

from sanitizer_support import (
    BASE_FLAGS,
    SANITIZER_FLAGS,
    probe_toolchain,
    project_failed,
    project_has_failed,
    run,
    sanitizer_environment,
)


TEST_CASES = (
    (
        "ordering",
        "ordering_test",
        "k1_c3_ordering_sanitize.exe",
        "K1-C3 ordering, lifecycle, occupancy, links and canonical query validation OK",
    ),
    (
        "invariants",
        "invariants_test",
        "k1_c3_invariants_sanitize.exe",
        "K1-C3 planned and official invariant validation OK",
    ),
    (
        "limits",
        "limits_test",
        "k1_c3_limits_sanitize.exe",
        "K1-C3 limits, identifiers, counters and allocation atomicity OK",
    ),
    (
        "long_run",
        "long_run_test",
        "k1_c3_long_run_sanitize.exe",
        "K1-C3 long run deterministic OK",
    ),
)


def existing_file(parser: argparse.ArgumentParser, value: str, label: str) -> Path:
    path = Path(value).resolve()
    if not path.is_file():
        parser.error(f"{label} does not exist or is not a file: {path}")
    return path


def compile_project(
    compiler: str,
    include: Path,
    source: Path,
    test: Path,
    executable: Path,
):
    return run([
        compiler,
        *BASE_FLAGS,
        *SANITIZER_FLAGS,
        "-DMINISNN_WORLDS_KERNEL_TESTING",
        "-DMINISNN_WORLDS_KERNEL_C3_INVARIANT_TESTING",
        f"-I{include}",
        str(test),
        str(source),
        "-o",
        str(executable),
    ])


def fail_case(label: str, description: str, completed) -> int:
    print(f"K1-C3 sanitizer {label}: FAIL")
    return project_failed("K1-C3", description, completed)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--include", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--ordering-test", required=True)
    parser.add_argument("--invariants-test", required=True)
    parser.add_argument("--limits-test", required=True)
    parser.add_argument("--long-run-test", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    include = Path(args.include).resolve()
    if not include.is_dir():
        parser.error(f"include directory does not exist: {include}")
    source = existing_file(parser, args.source, "kernel source")
    tests = {
        "ordering_test": existing_file(parser, args.ordering_test, "ordering test"),
        "invariants_test": existing_file(parser, args.invariants_test, "invariants test"),
        "limits_test": existing_file(parser, args.limits_test, "limits test"),
        "long_run_test": existing_file(parser, args.long_run_test, "long-run test"),
    }
    output = Path(args.output_dir).resolve()
    try:
        output.mkdir(parents=True, exist_ok=True)
    except OSError as error:
        print(f"K1-C3 sanitizer FAIL: output directory setup: {error}")
        return 1

    try:
        with tempfile.TemporaryDirectory(prefix="k1_c3_sanitize_", dir=output) as directory:
            temporary = Path(directory)
            probe = probe_toolchain(args.compiler, temporary, "K1-C3")
            if probe is None:
                return 0
            if not probe:
                return 1

            for label, test_key, executable_name, expected in TEST_CASES:
                executable = temporary / executable_name
                build = compile_project(
                    args.compiler, include, source, tests[test_key], executable
                )
                if project_has_failed(build):
                    return fail_case(
                        label,
                        f"{label} compilation after valid ASan/UBSan probe",
                        build,
                    )
                execution = run([str(executable)], sanitizer_environment())
                if project_has_failed(execution):
                    return fail_case(label, f"{label} execution", execution)
                if expected not in execution.stdout:
                    return fail_case(
                        label,
                        f"{label} test did not confirm execution",
                        execution,
                    )
                print(f"K1-C3 sanitizer {label}: PASS")
    except OSError as error:
        print(f"K1-C3 sanitizer FAIL: temporary artifact setup: {error}")
        return 1

    print("K1-C3 sanitizer: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())