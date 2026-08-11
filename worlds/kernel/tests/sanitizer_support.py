"""Shared ASan/UBSan probe helpers for Worlds Kernel test gates."""
from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys


BASE_FLAGS = (
    "-std=c11",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Wformat=2",
    "-Wstrict-prototypes",
)
SANITIZER_FLAGS = ("-fsanitize=address,undefined", "-fno-omit-frame-pointer")
UNAVAILABLE_MARKERS = (
    "cannot find -lasan",
    "cannot find -lubsan",
    "unrecognized command-line option",
    "unrecognized command line option",
    "unsupported option",
    "libasan",
    "libubsan",
    "addresssanitizer runtime is not available",
    "undefinedbehaviorsanitizer runtime is not available",
    "not recognized as an internal or external command",
)
SANITIZER_FAILURE_MARKERS = (
    "error: addresssanitizer",
    "addresssanitizer:",
    "leaksanitizer",
    "runtime error:",
    "undefinedbehaviorsanitizer",
)


def run(command: list[str], environment: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, text=True, capture_output=True, check=False, env=environment)


def completed_output(completed: subprocess.CompletedProcess[str]) -> str:
    return f"{completed.stdout}\n{completed.stderr}"


def classify_unavailable(completed: subprocess.CompletedProcess[str]) -> bool:
    output = completed_output(completed).lower()
    return any(marker in output for marker in UNAVAILABLE_MARKERS)


def has_sanitizer_failure(completed: subprocess.CompletedProcess[str]) -> bool:
    output = completed_output(completed).lower()
    return any(marker in output for marker in SANITIZER_FAILURE_MARKERS)


def project_has_failed(completed: subprocess.CompletedProcess[str]) -> bool:
    """Project commands are failures after the sanitizer probe is valid."""
    return completed.returncode != 0 or has_sanitizer_failure(completed)

def sanitizer_environment() -> dict[str, str]:
    environment = os.environ.copy()
    current = environment.get("ASAN_OPTIONS", "")
    environment["ASAN_OPTIONS"] = (
        f"{current}:detect_leaks=1" if current else "detect_leaks=1"
    )
    return environment


def print_completed(completed: subprocess.CompletedProcess[str]) -> None:
    if completed.stdout:
        sys.stderr.write(completed.stdout)
    if completed.stderr:
        sys.stderr.write(completed.stderr)


def probe_toolchain(
    compiler: str,
    output_dir: Path,
    label: str,
) -> bool | None:
    """Return True when usable, None when ASan/UBSan is unavailable, else False."""
    probe_source = output_dir / f"{label.lower().replace('-', '_')}_probe.c"
    probe_executable = output_dir / f"{label.lower().replace('-', '_')}_probe.exe"
    probe_source.write_text("int main(void) { return 0; }\n", encoding="ascii")
    try:
        build = run([
            compiler,
            *BASE_FLAGS,
            *SANITIZER_FLAGS,
            str(probe_source),
            "-o",
            str(probe_executable),
        ])
    except OSError as error:
        print(f"{label} sanitizer UNAVAILABLE: compiler unavailable: {error}")
        return None
    if build.returncode != 0:
        if classify_unavailable(build):
            print(f"{label} sanitizer UNAVAILABLE: ASan/UBSan probe compilation unavailable")
            return None
        print(f"{label} sanitizer FAIL: ASan/UBSan probe compilation")
        print_completed(build)
        return False

    probe_run = run([str(probe_executable)], sanitizer_environment())
    if probe_run.returncode != 0:
        if classify_unavailable(probe_run):
            print(f"{label} sanitizer UNAVAILABLE: ASan/UBSan runtime unavailable")
            return None
        print(f"{label} sanitizer FAIL: ASan/UBSan probe execution")
        print_completed(probe_run)
        return False
    if has_sanitizer_failure(probe_run):
        print(f"{label} sanitizer FAIL: ASan/UBSan probe reported a runtime error")
        print_completed(probe_run)
        return False
    return True


def project_failed(
    label: str,
    description: str,
    completed: subprocess.CompletedProcess[str],
) -> int:
    """A project failure is always FAIL after a successful probe."""
    print(f"{label} sanitizer FAIL: {description}")
    print_completed(completed)
    return 1