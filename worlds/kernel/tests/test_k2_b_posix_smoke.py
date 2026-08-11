#!/usr/bin/env python3
"""Compile and execute K2-B restore coverage on a POSIX C11 toolchain when available."""
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
        print("K2-B POSIX smoke UNAVAILABLE")
        return 0
    root = Path(args.kernel_root).resolve()
    sources = [
        str(root / "src" / "minisnn_worlds_kernel.c"),
        str(root / "src" / "minisnn_worlds_kernel_snapshot.c"),
        str(root / "src" / "minisnn_worlds_kernel_restore.c"),
    ]
    with tempfile.TemporaryDirectory(prefix="k2_b_posix_") as directory:
        temporary = Path(directory)
        flags = [
            compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
            "-Wformat=2", "-Wstrict-prototypes",
            "-DMINISNN_WORLDS_KERNEL_TESTING", f"-I{root / 'include'}",
        ]
        tests = (
            ("restore", "test_k2_b_restore.c", []),
            ("checkpoints", "test_k2_b_checkpoints.c", []),
            ("file", "test_k2_b_file_roundtrip.c", [
                f"-I{root / 'app'}", str(root / "app" / "k2_snapshot_file.c"),
            ]),
        )
        for name, test, extras in tests:
            executable = temporary / name
            if (not run([*flags, str(root / "tests" / test), *extras,
                        *sources, "-o", str(executable)]) or
                    not run([str(executable)], cwd=temporary)):
                print("K2-B POSIX smoke FAIL")
                return 1
    print("K2-B POSIX smoke PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())