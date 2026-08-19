#!/usr/bin/env python3
import argparse
from pathlib import Path
import subprocess
import tempfile

from g0_build_common import portable_compile_command, repository_root


UNAVAILABLE_MARKERS = (
    "unrecognized command-line option",
    "unsupported option",
    "cannot find -lasan",
    "cannot find -lubsan",
    "asan is not supported",
    "addresssanitizer is not supported",
)


def sanitizer_probe(compiler: str, temporary: Path) -> bool:
    probe_source = temporary / "sanitizer_probe.c"
    probe_executable = temporary / "sanitizer_probe.exe"
    probe_source.write_text("int main(void) { return 0; }\n", encoding="ascii")
    command = [
        compiler,
        "-std=c11",
        "-fsanitize=address,undefined",
        "-fno-omit-frame-pointer",
        str(probe_source),
        "-o",
        str(probe_executable),
    ]
    build = subprocess.run(command, capture_output=True, text=True)
    if build.returncode != 0:
        diagnostic = (build.stdout + build.stderr).lower()
        if any(marker in diagnostic for marker in UNAVAILABLE_MARKERS):
            return False
        raise SystemExit("G0-A sanitizer FAIL\n" + build.stdout + build.stderr)
    run = subprocess.run([str(probe_executable)], capture_output=True, text=True)
    if run.returncode != 0:
        diagnostic = (run.stdout + run.stderr).lower()
        if any(marker in diagnostic for marker in UNAVAILABLE_MARKERS):
            return False
        raise SystemExit("G0-A sanitizer FAIL\n" + run.stdout + run.stderr)
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--visualizer-root", required=True)
    args = parser.parse_args()
    root = Path(args.visualizer_root).resolve()

    with tempfile.TemporaryDirectory(prefix="g0_a_sanitize_") as temporary_text:
        temporary = Path(temporary_text)
        if not sanitizer_probe(args.compiler, temporary):
            print("G0-A sanitizer UNAVAILABLE")
            return 0
        executable = temporary / "test_g0_visualizer_sanitize.exe"
        command = portable_compile_command(
            args.compiler,
            root,
            root / "tests" / "test_g0_visualizer.c",
            executable,
            ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"],
        )
        build = subprocess.run(command, capture_output=True, text=True)
        if build.returncode != 0:
            raise SystemExit("G0-A sanitizer FAIL\n" + build.stdout + build.stderr)
        run = subprocess.run(
            [str(executable), str(repository_root(root))], capture_output=True, text=True
        )
        if run.returncode != 0:
            raise SystemExit("G0-A sanitizer FAIL\n" + run.stdout + run.stderr)
    print("G0-A sanitizer PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())