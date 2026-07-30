#!/usr/bin/env python3
"""Compare the public K1-A test output under -O0 and -O2."""
from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys


def run(command: list[str], cwd: pathlib.Path) -> str:
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout + result.stderr)
        raise RuntimeError("command failed")
    return result.stdout


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--include", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--test", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()
    root = pathlib.Path(".")
    output = pathlib.Path(args.output_dir)
    output.mkdir(parents=True, exist_ok=True)
    outputs = []
    for level in ("-O0", "-O2"):
        executable = output / f"k1_a_{level[1:]}.exe"
        run([args.compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
             "-DMINISNN_WORLDS_KERNEL_TESTING", level, f"-I{args.include}",
             args.test, args.source, "-o", str(executable)], root)
        outputs.append(run([str(executable)], root))
    if outputs[0] != outputs[1]:
        raise RuntimeError("K1-A O0/O2 outputs differ")
    print("K1-A O0/O2 deterministic validation OK")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"K1-A O0/O2 deterministic validation FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
