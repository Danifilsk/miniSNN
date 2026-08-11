#!/usr/bin/env python3
"""Compile and execute K2-C replay coverage on a clean POSIX C11 toolchain."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(command: list[str], *, cwd: Path | None = None) -> bool:
    return subprocess.run(command, check=False, cwd=cwd).returncode == 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--kernel-root", required=True)
    args = parser.parse_args()
    compiler = shutil.which("cc")
    if compiler is None or os.name == "nt":
        print("K2-C POSIX smoke UNAVAILABLE")
        return 0
    root = Path(args.kernel_root).resolve()
    sources = [
        str(root / "src" / "minisnn_worlds_kernel.c"),
        str(root / "src" / "minisnn_worlds_kernel_snapshot.c"),
        str(root / "src" / "minisnn_worlds_kernel_restore.c"),
        str(root / "src" / "minisnn_worlds_kernel_command_log.c"),
    ]
    flags = [
        compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2",
        "-Wstrict-prototypes", "-DMINISNN_WORLDS_KERNEL_TESTING",
        f"-I{root / 'include'}", f"-I{root / 'app'}",
    ]
    with tempfile.TemporaryDirectory(prefix="k2_c_posix_") as directory:
        temporary = Path(directory)
        tests = (
            ("command_log", "test_k2_c_command_log.c"),
            ("long_run", "test_k2_c_long_run.c"),
        )
        for name, test in tests:
            executable = temporary / name
            if (not run([*flags, str(root / "tests" / test),
                        str(root / "app" / "k2_command_log_file.c"), *sources,
                        "-o", str(executable)]) or
                    not run([str(executable)], cwd=temporary)):
                print("K2-C POSIX smoke FAIL")
                return 1
        demo = temporary / "demo"
        demo.mkdir()
        executable = temporary / "k2_command_replay_demo"
        if (not run([*flags, str(root / "app" / "k2_command_replay_demo.c"),
                    str(root / "app" / "k2_snapshot_file.c"),
                    str(root / "app" / "k2_command_log_file.c"), *sources,
                    "-o", str(executable)]) or
                not run([str(executable), str(demo)], cwd=temporary)):
            print("K2-C POSIX smoke FAIL")
            return 1
    print("K2-C POSIX smoke PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())