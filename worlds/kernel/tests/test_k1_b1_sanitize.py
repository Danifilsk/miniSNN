#!/usr/bin/env python3
"""Run K1-B1 project coverage only after a separate ASan/UBSan probe succeeds."""
from __future__ import annotations

import argparse
from pathlib import Path
import tempfile
import sys

sys.dont_write_bytecode = True

from sanitizer_support import (
    BASE_FLAGS,
    SANITIZER_FLAGS,
    has_sanitizer_failure,
    probe_toolchain,
    project_failed,
    project_has_failed,
    run,
    sanitizer_environment,
)


def compile_project(
    compiler: str,
    include: Path,
    source: Path,
    input_source: Path,
    executable: Path,
) -> object:
    return run([
        compiler,
        *BASE_FLAGS,
        *SANITIZER_FLAGS,
        "-DMINISNN_WORLDS_KERNEL_TESTING",
        f"-I{include}",
        str(input_source),
        str(source),
        "-o",
        str(executable),
    ])


def run_checked(label: str, command: list[str], expected: str) -> int:
    result = run(command, sanitizer_environment())
    if project_has_failed(result):
        return project_failed("K1-B1", f"{label} execution", result)
    if expected not in result.stdout:
        return project_failed("K1-B1", f"{label} did not confirm execution", result)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--include", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--occupancy-test", required=True)
    parser.add_argument("--stress-test", required=True)
    parser.add_argument("--demo", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    include = Path(args.include).resolve()
    source = Path(args.source).resolve()
    occupancy_test = Path(args.occupancy_test).resolve()
    stress_test = Path(args.stress_test).resolve()
    demo = Path(args.demo).resolve()
    config = Path(args.config).resolve()
    output = Path(args.output_dir).resolve()
    output.mkdir(parents=True, exist_ok=True)

    probe = probe_toolchain(args.compiler, output, "K1-B1")
    if probe is None:
        return 0
    if not probe:
        return 1

    with tempfile.TemporaryDirectory(prefix="k1_b1_sanitize_", dir=output) as directory:
        temporary = Path(directory)
        jobs = (
            ("occupancy test", occupancy_test, temporary / "k1_b1_occupancy_sanitize.exe",
             "K1-B1 occupancy, AABB, masks, lifecycle and hash validation OK"),
            ("stress test", stress_test, temporary / "k1_b1_stress_sanitize.exe",
             "K1-B1 occupancy deterministic stress validation OK"),
            ("demo", demo, temporary / "k1_b1_demo_sanitize.exe", "status=OK"),
        )
        for label, input_source, executable, expected in jobs:
            build = compile_project(
                args.compiler, include, source, input_source, executable
            )
            if project_has_failed(build):
                return project_failed("K1-B1", f"{label} compilation after a valid ASan/UBSan probe", build)
            command = [str(executable)]
            if label == "demo":
                artifacts = temporary / "demo_artifacts"
                artifacts.mkdir(parents=True, exist_ok=True)
                command.extend([str(config), str(artifacts)])
            failure = run_checked(label, command, expected)
            if failure:
                return failure

    print("K1-B1 sanitizer PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())