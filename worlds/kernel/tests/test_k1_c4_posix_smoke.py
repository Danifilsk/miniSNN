#!/usr/bin/env python3
"""Build and run the K1-C4 demo and stress test with a POSIX C11 compiler when available."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(command: list[str]) -> bool:
    return subprocess.run(command, check=False).returncode == 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--kernel-root", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--golden", required=True)
    args = parser.parse_args()

    compiler = shutil.which("cc")
    if compiler is None or os.name == "nt":
        print("K1-C4 POSIX smoke UNAVAILABLE")
        return 0

    root = Path(args.kernel_root).resolve()
    with tempfile.TemporaryDirectory(prefix="k1_c4_posix_") as directory:
        temporary = Path(directory)
        demo = temporary / "k1_spatial_links_demo"
        stress = temporary / "test_k1_c4_stress"
        output = temporary / "output"
        output.mkdir()
        flags = [
            compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2",
            "-Wstrict-prototypes", "-DMINISNN_WORLDS_KERNEL_TESTING",
            "-DMINISNN_WORLDS_KERNEL_C3_INVARIANT_TESTING",
            f"-I{root / 'include'}",
        ]
        if not run([
            *flags, f"-I{root / 'app'}",
            str(root / "app" / "k1_spatial_links_demo.c"),
            str(root / "app" / "k1_c4_config.c"),
            str(root / "src" / "minisnn_worlds_kernel.c"),
            "-o", str(demo),
        ]):
            print("K1-C4 POSIX smoke FAIL")
            return 1
        if not run([
            *flags,
            str(root / "tests" / "test_k1_c4_stress.c"),
            str(root / "src" / "minisnn_worlds_kernel.c"),
            "-o", str(stress),
        ]):
            print("K1-C4 POSIX smoke FAIL")
            return 1
        if not run([str(demo), str(root / args.config), str(output)]):
            print("K1-C4 POSIX smoke FAIL")
            return 1
        if not run([
            os.environ.get("PYTHON", "python3"),
            str(root / "scripts" / "check_k1_c4.py"),
            "--directory", str(output),
            "--golden", str(root / args.golden),
        ]):
            print("K1-C4 POSIX smoke FAIL")
            return 1
        if not run([str(stress)]):
            print("K1-C4 POSIX smoke FAIL")
            return 1
    print("K1-C4 POSIX smoke PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())