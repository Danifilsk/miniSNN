#!/usr/bin/env python3
"""Run focused WD2 lifecycle tests under ASan/UBSan when supported by the compiler."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile

FLAGS = ["-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2", "-Wstrict-prototypes",
         "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]


def run(command: list[str], cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, text=True, capture_output=True, check=False, cwd=cwd)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--domain-root", required=True)
    args = parser.parse_args()
    domain = Path(args.domain_root).resolve()
    kernel = domain.parent / "kernel"
    kernel_sources = [kernel / "src" / name for name in (
        "minisnn_worlds_kernel.c", "minisnn_worlds_kernel_snapshot.c",
        "minisnn_worlds_kernel_restore.c", "minisnn_worlds_kernel_command_log.c")]
    with tempfile.TemporaryDirectory(prefix="wd2_sanitize_") as temporary:
        root = Path(temporary)
        probe = root / "probe.c"
        probe.write_text("int main(void){return 0;}\n", encoding="ascii")
        probe_exe = root / "probe.exe"
        if run([args.compiler, *FLAGS, str(probe), "-o", str(probe_exe)]).returncode != 0 or \
           run([str(probe_exe)], root).returncode != 0:
            print("WD2 sanitizer: UNAVAILABLE")
            return 0
        for name, extra in (
            ("test_wd2_lifecycle.c", []),
            ("test_wd2_persistence.c", []),
            ("test_wd2_atomicity.c", ["-DMINISNN_WORLDS_KERNEL_TESTING"]),
        ):
            executable = root / (Path(name).stem + ".exe")
            command = [args.compiler, *FLAGS, "-DMINISNN_WORLDS_DOMAIN_TESTING", *extra,
                       f"-I{domain / 'include'}", f"-I{kernel / 'include'}",
                       f"-I{domain / 'tests'}", str(domain / "tests" / name),
                       str(domain / "src" / "minisnn_worlds_domain.c"),
                       *(str(source) for source in kernel_sources), "-o", str(executable)]
            completed = run(command)
            if completed.returncode != 0:
                print("WD2 sanitizer: FAIL\n" + completed.stdout + completed.stderr)
                return 1
            completed = run([str(executable)], root)
            if completed.returncode != 0:
                print("WD2 sanitizer: FAIL\n" + completed.stdout + completed.stderr)
                return 1
    print("WD2 sanitizer: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())