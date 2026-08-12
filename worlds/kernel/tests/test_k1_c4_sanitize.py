#!/usr/bin/env python3
"""Run the K1-C4 demo and stress coverage after an ASan/UBSan toolchain probe."""
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
    parser.add_argument("--config", required=True)
    parser.add_argument("--golden", required=True)
    args = parser.parse_args()

    root = Path(args.kernel_root).resolve()
    with tempfile.TemporaryDirectory(prefix="k1_c4_sanitize_") as directory:
        temporary = Path(directory)
        probe = probe_toolchain(args.compiler, temporary, "K1-C4")
        if probe is None:
            return 0
        if not probe:
            return 1

        demo = temporary / "k1_c4_demo_sanitize.exe"
        stress = temporary / "k1_c4_stress_sanitize.exe"
        output = temporary / "output"
        output.mkdir()
        common = [
            "-DMINISNN_WORLDS_KERNEL_TESTING",
            "-DMINISNN_WORLDS_KERNEL_C3_INVARIANT_TESTING",
            f"-I{root / 'include'}",
        ]
        demo_build = compile_program(args.compiler, [
            *common, f"-I{root / 'app'}",
            str(root / "app" / "k1_spatial_links_demo.c"),
            str(root / "app" / "k1_c4_config.c"),
            str(root / "src" / "minisnn_worlds_kernel.c"),
            "-o", str(demo),
        ])
        if project_has_failed(demo_build):
            return project_failed("K1-C4", "sanitized demo compilation", demo_build)
        # The sanitizer exercises the same structural paths using the smaller
        # profile selected only by MINISNN_K1_C4_SANITIZER_STRESS.
        stress_build = compile_program(args.compiler, [
            *common, "-DMINISNN_K1_C4_SANITIZER_STRESS",
            str(root / "tests" / "test_k1_c4_stress.c"),
            str(root / "src" / "minisnn_worlds_kernel.c"),
            "-o", str(stress),
        ])
        if project_has_failed(stress_build):
            return project_failed("K1-C4", "sanitized stress compilation", stress_build)
        demo_run = run([str(demo), str(root / args.config), str(output)], sanitizer_environment())
        if project_has_failed(demo_run):
            return project_failed("K1-C4", "sanitized demo execution", demo_run)
        checker = run([
            sys.executable, str(root / "scripts" / "check_k1_c4.py"),
            "--directory", str(output), "--golden", str(root / args.golden),
        ])
        if project_has_failed(checker):
            return project_failed("K1-C4", "sanitized artifact validation", checker)
        stress_run = run([str(stress)], sanitizer_environment())
        if project_has_failed(stress_run):
            return project_failed("K1-C4", "sanitized stress execution", stress_run)
    print("K1-C4 sanitizer: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())