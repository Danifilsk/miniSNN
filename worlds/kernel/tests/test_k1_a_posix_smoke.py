#!/usr/bin/env python3
"""K1-A headless C11 smoke for an actual POSIX compiler when available."""
from __future__ import annotations

import argparse
import os
import pathlib
import shutil
import subprocess


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--include", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--test", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()
    compiler = shutil.which("cc")
    if compiler is None or os.name == "nt":
        print("K1-A POSIX smoke UNAVAILABLE")
        return 0
    output = pathlib.Path(args.output_dir)
    output.mkdir(parents=True, exist_ok=True)
    executable = output / "k1_a_posix"
    command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
               "-DMINISNN_WORLDS_KERNEL_TESTING", f"-I{args.include}",
               args.test, args.source, "-o", str(executable)]
    if subprocess.run(command).returncode != 0:
        print("K1-A POSIX smoke FAIL")
        return 1
    if subprocess.run([str(executable)]).returncode != 0:
        print("K1-A POSIX smoke FAIL")
        return 1
    print("K1-A POSIX smoke PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
