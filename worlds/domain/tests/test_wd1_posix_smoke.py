#!/usr/bin/env python3
"""Compile and run focused WD1 coverage on a clean POSIX C11 toolchain."""
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
        print("WD1 POSIX smoke UNAVAILABLE")
        return 0
    domain = Path(args.domain_root).resolve()
    kernel = domain.parent / "kernel"
    kernel_sources = [kernel / "src" / name for name in (
        "minisnn_worlds_kernel.c", "minisnn_worlds_kernel_snapshot.c",
        "minisnn_worlds_kernel_restore.c", "minisnn_worlds_kernel_command_log.c")]
    with tempfile.TemporaryDirectory(prefix="wd1_posix_") as temporary:
        root = Path(temporary)
        for name, extra in (("test_wd1_snapshot.c", []),
                            ("test_wd1_file_roundtrip.c", [str(domain / "app" / "wd1_domain_snapshot_file.c")])):
            executable = root / Path(name).stem
            command = [compiler, *FLAGS, "-DMINISNN_WORLDS_DOMAIN_TESTING", f"-I{domain / 'include'}",
                       f"-I{kernel / 'include'}", f"-I{domain / 'tests'}", f"-I{domain / 'app'}",
                       str(domain / "tests" / name), *extra, str(domain / "src" / "minisnn_worlds_domain.c"),
                       *(str(source) for source in kernel_sources), "-o", str(executable)]
            if subprocess.run(command, check=False).returncode != 0 or \
               subprocess.run([str(executable), str(root / (Path(name).stem + ".bin"))], check=False).returncode != 0:
                print("WD1 POSIX smoke FAIL")
                return 1
    print("WD1 POSIX smoke PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())