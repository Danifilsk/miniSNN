#!/usr/bin/env python3
"""Compare WD1 canonical artifacts compiled at -O0 and -O2."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile

ARTIFACTS = ("wd1_summary.txt", "domain_snapshot_v1.bin")
FLAGS = ["-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2", "-Wstrict-prototypes"]


def call(command: list[str]) -> None:
    completed = subprocess.run(command, text=True, capture_output=True, check=False)
    if completed.returncode != 0:
        raise SystemExit("WD1 O0/O2 FAILED:\n" + completed.stdout + completed.stderr)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--domain-root", required=True)
    args = parser.parse_args()
    domain = Path(args.domain_root).resolve()
    kernel = domain.parent / "kernel"
    sources = [
        domain / "app" / "wd1_domain_persistence_demo.c",
        domain / "app" / "wd1_domain_snapshot_file.c",
        domain / "src" / "minisnn_worlds_domain.c",
        kernel / "src" / "minisnn_worlds_kernel.c",
        kernel / "src" / "minisnn_worlds_kernel_snapshot.c",
        kernel / "src" / "minisnn_worlds_kernel_restore.c",
        kernel / "src" / "minisnn_worlds_kernel_command_log.c",
    ]
    with tempfile.TemporaryDirectory(prefix="wd1_o") as temporary:
        root = Path(temporary)
        outputs: list[tuple[bytes, ...]] = []
        for optimization in ("-O0", "-O2"):
            executable = root / f"wd1_{optimization[2:]}.exe"
            output = root / optimization[1:]
            output.mkdir()
            call([args.compiler, *FLAGS, optimization, f"-I{domain / 'include'}", f"-I{kernel / 'include'}",
                  f"-I{domain / 'app'}", *(str(source) for source in sources), "-o", str(executable)])
            call([str(executable), str(output)])
            outputs.append(tuple((output / name).read_bytes() for name in ARTIFACTS))
        if outputs[0] != outputs[1]:
            raise SystemExit("WD1 O0/O2 FAILED: canonical artifacts differ")
    print("WD1 O0/O2 deterministic validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())