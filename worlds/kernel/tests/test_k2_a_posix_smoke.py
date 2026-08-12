#!/usr/bin/env python3
"""Compile and execute the K2-A snapshot tests on a POSIX C11 toolchain when available."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(command: list[str]) -> bool:
    return subprocess.run(command, check=False).returncode == 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--kernel-root", required=True)
    args = parser.parse_args()
    compiler = shutil.which("cc")
    if compiler is None or os.name == "nt":
        print("K2-A POSIX smoke UNAVAILABLE")
        return 0
    root = Path(args.kernel_root).resolve()
    with tempfile.TemporaryDirectory(prefix="k2_a_posix_") as directory:
        temporary = Path(directory)
        flags = [compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2",
                 "-Wstrict-prototypes", "-DMINISNN_WORLDS_KERNEL_TESTING", f"-I{root / 'include'}"]
        for name, test in (("snapshot", "test_k2_a_snapshot.c"), ("long", "test_k2_a_long_run.c")):
            executable = temporary / name
            if not run([*flags, str(root / "tests" / test),
                        str(root / "src" / "minisnn_worlds_kernel.c"),
                        str(root / "src" / "minisnn_worlds_kernel_snapshot.c"),
                        "-o", str(executable)]) or not run([str(executable)]):
                print("K2-A POSIX smoke FAIL")
                return 1
    print("K2-A POSIX smoke PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())