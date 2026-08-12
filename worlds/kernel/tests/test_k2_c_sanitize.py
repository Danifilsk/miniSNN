#!/usr/bin/env python3
"""Run complete K2-C command replay coverage after an ASan/UBSan probe."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from sanitizer_support import (BASE_FLAGS, SANITIZER_FLAGS, probe_toolchain,
                               project_failed, project_has_failed, run,
                               sanitizer_environment)


def compile_program(compiler: str, arguments: list[str]):
    return run([compiler, *BASE_FLAGS, *SANITIZER_FLAGS, *arguments])


def run_checked(command: list[str], *, cwd: Path, environment: dict[str, str]):
    return subprocess.run(command, text=True, capture_output=True, check=False,
                          cwd=cwd, env=environment)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--kernel-root", required=True)
    args = parser.parse_args()
    root = Path(args.kernel_root).resolve()
    sources = [
        str(root / "src" / "minisnn_worlds_kernel.c"),
        str(root / "src" / "minisnn_worlds_kernel_snapshot.c"),
        str(root / "src" / "minisnn_worlds_kernel_restore.c"),
        str(root / "src" / "minisnn_worlds_kernel_command_log.c"),
    ]
    with tempfile.TemporaryDirectory(prefix="k2_c_sanitize_") as directory:
        temporary = Path(directory)
        probe = probe_toolchain(args.compiler, temporary, "K2-C")
        if probe is None:
            return 0
        if not probe:
            return 1
        stages = (
            ("sanitized command log", "test_k2_c_command_log.c"),
            ("sanitized long replay", "test_k2_c_long_run.c"),
        )
        for stage, test in stages:
            executable = temporary / f"{test[:-2]}.exe"
            build = compile_program(args.compiler, [
                "-I" + str(root / "include"), "-I" + str(root / "app"),
                "-DMINISNN_WORLDS_KERNEL_TESTING", str(root / "tests" / test),
                str(root / "app" / "k2_command_log_file.c"), *sources,
                "-o", str(executable),
            ])
            if project_has_failed(build):
                return project_failed("K2-C", f"{stage} compilation", build)
            execution = run_checked([str(executable)], cwd=temporary,
                                    environment=sanitizer_environment())
            if project_has_failed(execution):
                return project_failed("K2-C", f"{stage} execution", execution)

        demo = temporary / "k2_command_replay_demo.exe"
        build = compile_program(args.compiler, [
            "-I" + str(root / "include"), "-I" + str(root / "app"),
            str(root / "app" / "k2_command_replay_demo.c"),
            str(root / "app" / "k2_snapshot_file.c"),
            str(root / "app" / "k2_command_log_file.c"), *sources,
            "-o", str(demo),
        ])
        if project_has_failed(build):
            return project_failed("K2-C", "sanitized demo compilation", build)
        output = temporary / "demo"
        output.mkdir()
        execution = run_checked([str(demo), str(output)], cwd=temporary,
                                environment=sanitizer_environment())
        if project_has_failed(execution):
            return project_failed("K2-C", "sanitized demo execution", execution)
        checker = run_checked([
            sys.executable, str(root / "scripts" / "check_k2_c.py"),
            "--directory", str(output), "--golden",
            str(root / "tests" / "golden" / "k2_c_command_replay_v1.txt"),
        ], cwd=temporary, environment=sanitizer_environment())
        if project_has_failed(checker):
            return project_failed("K2-C", "sanitized demo checker", checker)
    print("K2-C sanitizer: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())