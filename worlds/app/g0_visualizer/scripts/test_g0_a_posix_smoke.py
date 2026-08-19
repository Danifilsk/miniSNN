#!/usr/bin/env python3
import argparse
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

from g0_build_common import portable_compile_command, repository_root


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--visualizer-root", required=True)
    args = parser.parse_args()
    root = Path(args.visualizer_root).resolve()
    compiler = shutil.which("cc")
    if os.name == "nt" or compiler is None:
        print("G0-A POSIX smoke UNAVAILABLE")
        return 0

    with tempfile.TemporaryDirectory(prefix="g0_a_posix_") as temporary:
        executable = Path(temporary) / "test_g0_visualizer"
        command = portable_compile_command(
            compiler, root, root / "tests" / "test_g0_visualizer.c", executable
        )
        build = subprocess.run(command, capture_output=True, text=True)
        if build.returncode != 0:
            raise SystemExit("G0-A POSIX smoke FAIL\n" + build.stdout + build.stderr)
        run = subprocess.run(
            [str(executable), str(repository_root(root))], capture_output=True, text=True
        )
        if run.returncode != 0:
            raise SystemExit("G0-A POSIX smoke FAIL\n" + run.stdout + run.stderr)
    print("G0-A POSIX smoke PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())