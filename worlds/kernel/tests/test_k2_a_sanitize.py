#!/usr/bin/env python3
"""Run K2-A functional and long-run snapshot coverage after an ASan/UBSan probe."""
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


def compile_program(compiler: str, arguments: list[str]):
    return run([compiler, *BASE_FLAGS, *SANITIZER_FLAGS, *arguments])


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--kernel-root", required=True)
    args = parser.parse_args()
    root = Path(args.kernel_root).resolve()
    with tempfile.TemporaryDirectory(prefix="k2_a_sanitize_") as directory:
        temporary = Path(directory)
        probe = probe_toolchain(args.compiler, temporary, "K2-A")
        if probe is None:
            return 0
        if not probe:
            return 1
        for stage, test in (("sanitized functional", "test_k2_a_snapshot.c"),
                            ("sanitized long run", "test_k2_a_long_run.c")):
            executable = temporary / f"k2_a_{stage.replace(' ', '_')}.exe"
            build = compile_program(args.compiler, [
                f"-I{root / 'include'}", "-DMINISNN_WORLDS_KERNEL_TESTING",
                str(root / "tests" / test),
                str(root / "src" / "minisnn_worlds_kernel.c"),
                str(root / "src" / "minisnn_worlds_kernel_snapshot.c"),
                "-o", str(executable),
            ])
            if project_has_failed(build):
                return project_failed("K2-A", f"{stage} compilation", build)
            execution = run([str(executable)], sanitizer_environment())
            if project_has_failed(execution):
                return project_failed("K2-A", f"{stage} execution", execution)
    print("K2-A sanitizer: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())