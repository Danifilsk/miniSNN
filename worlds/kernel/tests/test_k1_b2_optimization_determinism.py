#!/usr/bin/env python3
"""Compare K1-B2 movement under -O0 and -O2 with strict warnings."""
from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys


def run(command: list[str]) -> str:
    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.returncode != 0:
        sys.stderr.write(completed.stdout + completed.stderr)
        raise RuntimeError("command failed")
    return completed.stdout


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
    results: list[str] = []
    for level in ("-O0", "-O2"):
        executable = output / f"k1_b2_{level[1:]}.exe"
        results.append(run([
            args.compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
            "-Wformat=2", "-Wstrict-prototypes",
            "-DMINISNN_WORLDS_KERNEL_TESTING", level, f"-I{args.include}",
            args.test, args.source, "-o", str(executable)
        ]) + run([str(executable)]))
    if results[0] != results[1]:
        raise RuntimeError("K1-B2 O0/O2 outputs differ")
    print("K1-B2 O0/O2 deterministic validation OK")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"K1-B2 O0/O2 deterministic validation FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
