#!/usr/bin/env python3
"""POSIX smoke for the portable G0-D persistence and log modules."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from g0_build_common import portable_compile_command


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--visualizer-root", required=True)
    args = parser.parse_args()
    root = Path(args.visualizer_root).resolve()
    compiler = shutil.which("cc")
    if os.name == "nt" or compiler is None:
        print("G0-D POSIX smoke UNAVAILABLE")
        return 0
    with tempfile.TemporaryDirectory(prefix="g0_d_posix_") as temporary_text:
        executable = Path(temporary_text) / "test_g0_d"
        command = portable_compile_command(compiler, root, root / "tests" / "test_g0_d.c", executable)
        build = subprocess.run(command, capture_output=True, text=True)
        if build.returncode != 0:
            raise SystemExit("G0-D POSIX smoke FAIL\n" + build.stdout + build.stderr)
        run = subprocess.run([str(executable)], capture_output=True, text=True)
        if run.returncode != 0:
            raise SystemExit("G0-D POSIX smoke FAIL\n" + run.stdout + run.stderr)
    print("G0-D POSIX smoke PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
