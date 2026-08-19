#!/usr/bin/env python3
"""ASan/UBSan gate for the portable G0-C runtime when supported."""

import argparse
from pathlib import Path
import subprocess
import tempfile

from g0_build_common import portable_compile_command


UNAVAILABLE = (
    "unrecognized command-line option",
    "unsupported option",
    "asan is not supported",
    "cannot find -lasan",
    "cannot find -lubsan",
)


def sanitizer_available(compiler: str, directory: Path) -> bool:
    source = directory / "sanitizer_probe.c"
    executable = directory / "sanitizer_probe.exe"
    source.write_text("int main(void) { return 0; }" + chr(10), encoding="ascii")
    probe = subprocess.run(
        [compiler, str(source), "-fsanitize=address,undefined",
         "-fno-omit-frame-pointer", "-o", str(executable)],
        capture_output=True,
        text=True,
    )
    if probe.returncode == 0:
        return True
    diagnostic = (probe.stdout + probe.stderr).lower()
    if any(marker in diagnostic for marker in UNAVAILABLE):
        return False
    raise SystemExit("G0-C sanitizer probe FAIL" + chr(10) + probe.stdout + probe.stderr)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--visualizer-root", required=True)
    args = parser.parse_args()
    root = Path(args.visualizer_root).resolve()
    with tempfile.TemporaryDirectory(prefix="g0_c_sanitize_") as temporary_text:
        temporary = Path(temporary_text)
        if not sanitizer_available(args.compiler, temporary):
            print("G0-C sanitizer UNAVAILABLE")
            return 0
        executable = temporary / "test_g0_sandbox_sanitize.exe"
        command = portable_compile_command(
            args.compiler, root, root / "tests" / "test_g0_sandbox.c", executable,
            ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"],
        )
        build = subprocess.run(command, capture_output=True, text=True)
        if build.returncode != 0:
            raise SystemExit("G0-C sanitizer FAIL" + chr(10) + build.stdout + build.stderr)
        run = subprocess.run([str(executable)], capture_output=True, text=True)
        if run.returncode != 0:
            raise SystemExit("G0-C sanitizer FAIL" + chr(10) + run.stdout + run.stderr)
    print("G0-C sanitizer PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())