#!/usr/bin/env python3
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--terrain-root", required=True)
    args = parser.parse_args()
    if os.name == "nt":
        print("WT0 POSIX smoke UNAVAILABLE")
        return
    compiler = shutil.which("cc")
    if compiler is None:
        print("WT0 POSIX smoke UNAVAILABLE")
        return
    root = Path(args.terrain_root)
    with tempfile.TemporaryDirectory(prefix="wt0_posix_") as temporary:
        executable = Path(temporary) / "wt0_posix"
        subprocess.run([
            compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
            "-Wformat=2", "-Wstrict-prototypes", "-Iinclude",
            "-I../kernel/include", "tests/test_wt0_terrain.c",
            "src/minisnn_worlds_terrain.c",
            "../kernel/src/minisnn_worlds_kernel.c", "../kernel/src/minisnn_worlds_kernel_snapshot.c", "../kernel/src/minisnn_worlds_kernel_restore.c", "../kernel/src/minisnn_worlds_kernel_command_log.c", "-o", str(executable)
        ], cwd=root, check=True)
        subprocess.run([str(executable)], check=True)
    print("WT0 POSIX smoke PASS")

if __name__ == "__main__":
    main()
