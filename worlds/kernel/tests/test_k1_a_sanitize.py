#!/usr/bin/env python3
"""Run the K1-A public test only after a separate ASan/UBSan probe succeeds."""
from __future__ import annotations

import argparse
from pathlib import Path
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
    test_source = Path(args.test).resolve()
    output = Path(args.output_dir).resolve()
    output.mkdir(parents=True, exist_ok=True)
    executable = output / "k1_a_sanitize.exe"

    probe = probe_toolchain(args.compiler, output, "K1-A")
    if probe is None:
        return 0
    if not probe:
        return 1

    build = run([
        args.compiler,
        *BASE_FLAGS,
        *SANITIZER_FLAGS,
        "-DMINISNN_WORLDS_KERNEL_TESTING",
        f"-I{include}",
        str(test_source),
        str(source),
        "-o",
        str(executable),
    ])
    if project_has_failed(build):
        return project_failed(
            "K1-A", "project compilation after a valid ASan/UBSan probe", build
        )

    result = run([str(executable)], sanitizer_environment())
    if project_has_failed(result):
        return project_failed("K1-A", "project test execution", result)
    if "K1-A scalar, space, transform and hash validation OK" not in result.stdout:
        return project_failed("K1-A", "project test did not confirm execution", result)

    print("K1-A sanitizer PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())