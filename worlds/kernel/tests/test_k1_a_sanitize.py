#!/usr/bin/env python3
"""Probe a real sanitizer toolchain for the K1-A public test."""
from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--include", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--test", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()
    output = pathlib.Path(args.output_dir)
    output.mkdir(parents=True, exist_ok=True)
    executable = output / "k1_a_sanitize.exe"
    command = [args.compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
               "-DMINISNN_WORLDS_KERNEL_TESTING", "-fsanitize=address,undefined",
               f"-I{args.include}", args.test, args.source, "-o", str(executable)]
    try:
        build = subprocess.run(command, text=True, capture_output=True)
    except OSError as error:
        print(f"K1-A sanitizer UNAVAILABLE: {error}")
        return 0
    if build.returncode != 0:
        print("K1-A sanitizer UNAVAILABLE: compiler has no usable ASan/UBSan")
        return 0
    result = subprocess.run([str(executable)], text=True, capture_output=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout + result.stderr)
        print("K1-A sanitizer FAIL", file=sys.stderr)
        return 1
    print("K1-A sanitizer PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
