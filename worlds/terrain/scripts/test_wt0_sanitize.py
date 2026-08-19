#!/usr/bin/env python3
import argparse
from pathlib import Path
import subprocess
import tempfile

UNAVAILABLE_MARKERS = (
    "unrecognized command-line option",
    "unsupported option",
    "cannot find -lasan",
    "cannot find -lubsan",
    "asan is not supported",
)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--terrain-root", required=True)
    args = parser.parse_args()
    root = Path(args.terrain_root)
    with tempfile.TemporaryDirectory(prefix="wt0_sanitize_") as temporary:
        temporary = Path(temporary)
        executable = temporary / "test_wt0_sanitize.exe"
        command = [
            args.compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            "-Iinclude", "-I../kernel/include", "tests/test_wt0_terrain.c",
            "src/minisnn_worlds_terrain.c",
            "../kernel/src/minisnn_worlds_kernel.c", "../kernel/src/minisnn_worlds_kernel_snapshot.c", "../kernel/src/minisnn_worlds_kernel_restore.c", "../kernel/src/minisnn_worlds_kernel_command_log.c", "-o", str(executable)
        ]
        built = subprocess.run(command, cwd=root, capture_output=True, text=True)
        if built.returncode != 0:
            output = (built.stdout + built.stderr).lower()
            if any(marker in output for marker in UNAVAILABLE_MARKERS):
                print("WT0 sanitizer UNAVAILABLE")
                return
            raise SystemExit("WT0 sanitizer FAIL\n" + built.stdout + built.stderr)
        run = subprocess.run([str(executable)], capture_output=True, text=True)
        if run.returncode != 0:
            raise SystemExit("WT0 sanitizer FAIL\n" + run.stdout + run.stderr)
    print("WT0 sanitizer PASS")

if __name__ == "__main__":
    main()
