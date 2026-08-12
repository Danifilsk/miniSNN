#!/usr/bin/env python3
"""Run K1-C2 coverage only after an independent ASan/UBSan probe succeeds."""
from __future__ import annotations

import argparse
from pathlib import Path
import tempfile
import sys

sys.dont_write_bytecode = True
from sanitizer_support import (
    BASE_FLAGS, SANITIZER_FLAGS, probe_toolchain, project_failed,
    project_has_failed, run, sanitizer_environment,
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--include", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--test", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()
    include = Path(args.include).resolve()
    source = Path(args.source).resolve()
    test = Path(args.test).resolve()
    output = Path(args.output_dir).resolve()
    output.mkdir(parents=True, exist_ok=True)
    probe = probe_toolchain(args.compiler, output, "K1-C2")
    if probe is None:
        return 0
    if not probe:
        return 1
    with tempfile.TemporaryDirectory(prefix="k1_c2_sanitize_", dir=output) as directory:
        executable = Path(directory) / "k1_c2_sanitize.exe"
        build = run([
            args.compiler, *BASE_FLAGS, *SANITIZER_FLAGS,
            "-DMINISNN_WORLDS_KERNEL_TESTING", f"-I{include}",
            str(test), str(source), "-o", str(executable),
        ])
        if project_has_failed(build):
            return project_failed("K1-C2", "subtree test compilation after valid ASan/UBSan probe", build)
        execution = run([str(executable)], sanitizer_environment())
        if project_has_failed(execution):
            return project_failed("K1-C2", "subtree test execution", execution)
        if "K1-C2 subtree translation, events, atomicity and V5 validation OK" not in execution.stdout:
            return project_failed("K1-C2", "subtree test did not confirm execution", execution)
    print("K1-C2 sanitizer PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())