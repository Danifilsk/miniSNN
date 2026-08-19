#!/usr/bin/env python3
"""Compare WD2 demo artifacts built with O0 and O2."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile


def build(compiler: str, domain: Path, output: Path, optimization: str) -> None:
    kernel = domain.parent / "kernel"
    command = [
        compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2",
        "-Wstrict-prototypes", optimization, f"-I{domain / 'include'}",
        f"-I{kernel / 'include'}", f"-I{domain / 'app'}",
        str(domain / "app" / "wd2_lifecycle_demo.c"),
        str(domain / "app" / "wd1_domain_snapshot_file.c"),
        str(domain / "src" / "minisnn_worlds_domain.c"),
        str(kernel / "src" / "minisnn_worlds_kernel.c"),
        str(kernel / "src" / "minisnn_worlds_kernel_snapshot.c"),
        str(kernel / "src" / "minisnn_worlds_kernel_restore.c"),
        str(kernel / "src" / "minisnn_worlds_kernel_command_log.c"),
        "-o", str(output),
    ]
    completed = subprocess.run(command, text=True, capture_output=True, check=False)
    if completed.returncode != 0:
        raise RuntimeError(completed.stdout + completed.stderr)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--domain-root", required=True)
    args = parser.parse_args()
    domain = Path(args.domain_root).resolve()
    with tempfile.TemporaryDirectory(prefix="wd2_o0_o2_") as temporary:
        root = Path(temporary)
        exe0 = root / "wd2_o0.exe"
        exe2 = root / "wd2_o2.exe"
        build(args.compiler, domain, exe0, "-O0")
        build(args.compiler, domain, exe2, "-O2")
        left, right = root / "left", root / "right"
        left.mkdir()
        right.mkdir()
        for exe, output in ((exe0, left), (exe2, right)):
            completed = subprocess.run([str(exe), str(output)], text=True, capture_output=True, check=False)
            if completed.returncode != 0:
                print("WD2 O0/O2 FAIL")
                return 1
        for name in ("summary.txt", "events.csv", "actions.csv", "dead_snapshot_v2.bin"):
            if (left / name).read_bytes() != (right / name).read_bytes():
                print(f"WD2 O0/O2 FAIL: {name}")
                return 1
    print("WD2 O0/O2 determinism PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())