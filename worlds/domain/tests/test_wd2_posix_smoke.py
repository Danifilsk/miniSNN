#!/usr/bin/env python3
"""Compile focused WD2 lifecycle tests on a clean POSIX C11 toolchain."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

FLAGS = ["-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2", "-Wstrict-prototypes"]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--domain-root", required=True)
    args = parser.parse_args()
    compiler = shutil.which("cc")
    if os.name == "nt" or compiler is None:
        print("WD2 POSIX smoke UNAVAILABLE")
        return 0
    domain = Path(args.domain_root).resolve()
    kernel = domain.parent / "kernel"
    kernel_sources = [kernel / "src" / name for name in (
        "minisnn_worlds_kernel.c", "minisnn_worlds_kernel_snapshot.c",
        "minisnn_worlds_kernel_restore.c", "minisnn_worlds_kernel_command_log.c")]
    tests = [
        ("test_wd2_lifecycle.c", []),
        ("test_wd2_persistence.c", []),
        ("test_wd2_atomicity.c", ["-DMINISNN_WORLDS_KERNEL_TESTING"]),
    ]
    with tempfile.TemporaryDirectory(prefix="wd2_posix_") as temporary:
        root = Path(temporary)
        for name, extra in tests:
            executable = root / Path(name).stem
            command = [compiler, *FLAGS, "-DMINISNN_WORLDS_DOMAIN_TESTING", *extra,
                       f"-I{domain / 'include'}", f"-I{kernel / 'include'}",
                       f"-I{domain / 'tests'}", str(domain / "tests" / name),
                       str(domain / "src" / "minisnn_worlds_domain.c"),
                       *(str(source) for source in kernel_sources), "-o", str(executable)]
            if subprocess.run(command, check=False).returncode != 0 or \
               subprocess.run([str(executable)], check=False).returncode != 0:
                print("WD2 POSIX smoke FAIL")
                return 1
    print("WD2 POSIX smoke PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())