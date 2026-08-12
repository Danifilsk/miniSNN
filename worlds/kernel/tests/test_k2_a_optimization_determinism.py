#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile


def run(command: list[str]) -> str:
    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.returncode:
        raise SystemExit("K2-A O0/O2 FAILED:\n" + completed.stdout + completed.stderr)
    return completed.stdout


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--kernel-root", required=True)
    args = parser.parse_args()
    root = Path(args.kernel_root).resolve()
    with tempfile.TemporaryDirectory(prefix="k2_a_o") as temporary:
        outputs: list[str] = []
        for optimization in ("-O0", "-O2"):
            executable = Path(temporary) / f"snapshot_{optimization[2:]}.exe"
            run([
                args.compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
                "-Wformat=2", "-Wstrict-prototypes", "-DMINISNN_WORLDS_KERNEL_TESTING",
                optimization, "-I", str(root / "include"),
                str(root / "tests" / "test_k2_a_snapshot.c"),
                str(root / "src" / "minisnn_worlds_kernel.c"),
                str(root / "src" / "minisnn_worlds_kernel_snapshot.c"),
                "-o", str(executable),
            ])
            outputs.append(run([str(executable)]))
        if outputs[0] != outputs[1]:
            raise SystemExit("K2-A O0/O2 FAILED: canonical snapshot output differs")
    print("K2-A O0/O2 deterministic validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())