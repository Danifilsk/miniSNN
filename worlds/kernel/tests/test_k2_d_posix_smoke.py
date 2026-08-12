#!/usr/bin/env python3
"""Compile K2-D state binding and persistence on a clean POSIX C11 toolchain."""
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
        print("K2-D POSIX smoke UNAVAILABLE")
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
    with tempfile.TemporaryDirectory(prefix="k2_d_posix_") as directory:
        temporary = Path(directory)
        for name, test, needs_snapshot_file in (
            ("state_binding", "test_k2_d_state_binding.c", False),
            ("negative", "test_k2_d_negative.c", False),
            ("persistence", "test_k2_d_persistence.c", True),
        ):
            executable = temporary / name
            extras = [str(root / "app" / "k2_command_log_file.c")]
            if needs_snapshot_file:
                extras.insert(0, str(root / "app" / "k2_snapshot_file.c"))
            if not run([*flags, str(root / "tests" / test), *extras, *sources,
                        "-o", str(executable)]) or not run([str(executable)], cwd=temporary):
                print("K2-D POSIX smoke FAIL")
                return 1
    print("K2-D POSIX smoke PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())