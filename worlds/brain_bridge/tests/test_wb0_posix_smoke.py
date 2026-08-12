#!/usr/bin/env python3
"""Compile and run a reduced WB0 integration on a clean POSIX C11 toolchain."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

FLAGS = ("-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2", "-Wstrict-prototypes")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bridge-root", required=True)
    args = parser.parse_args()
    compiler = shutil.which("cc")
    if os.name == "nt" or compiler is None:
        print("WB0 POSIX smoke UNAVAILABLE")
        return 0

    bridge = Path(args.bridge_root).resolve()
    repository = bridge.parent.parent
    core = repository / "core"
    domain = repository / "worlds" / "domain"
    kernel = repository / "worlds" / "kernel"
    core_library = repository / "build" / "core" / "lib" / "libminisnn_core.a"
    if not core_library.is_file():
        print("WB0 POSIX smoke UNAVAILABLE")
        return 0
    sources = (
        kernel / "src" / "minisnn_worlds_kernel.c",
        kernel / "src" / "minisnn_worlds_kernel_snapshot.c",
        kernel / "src" / "minisnn_worlds_kernel_command_log.c",
    )
    with tempfile.TemporaryDirectory(prefix="wb0_posix_") as directory:
        root = Path(directory)
        executable = root / "wb0"
        command = [
            compiler, *FLAGS, f"-I{bridge / 'include'}", f"-I{core / 'include'}",
            f"-I{domain / 'include'}", f"-I{kernel / 'include'}",
            str(bridge / "tests" / "test_wb0_brain_bridge.c"),
            str(bridge / "src" / "minisnn_worlds_brain_bridge.c"),
            str(domain / "src" / "minisnn_worlds_domain.c"),
            *(str(source) for source in sources),
            str(core_library), "-o", str(executable),
        ]
        if (subprocess.run(command, check=False).returncode != 0 or
                subprocess.run([str(executable)], check=False, cwd=root).returncode != 0):
            print("WB0 POSIX smoke FAIL")
            return 1
    print("WB0 POSIX smoke PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())