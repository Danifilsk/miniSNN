#!/usr/bin/env python3
"""Regression contracts for K1-C2 sanitizer availability classification."""
from __future__ import annotations

from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from sanitizer_support import classify_unavailable, has_sanitizer_failure, project_has_failed


def completed(returncode: int, stderr: str) -> subprocess.CompletedProcess[str]:
    return subprocess.CompletedProcess(["cc"], returncode, "", stderr)


def main() -> int:
    unavailable_probe = completed(1, "ld: cannot find -lasan")
    invalid_project = completed(1, "fatal error: missing_project_source.c: No such file")
    runtime_report = completed(1, "ERROR: AddressSanitizer: heap-use-after-free")
    if not classify_unavailable(unavailable_probe):
        raise SystemExit("sanitizer availability regression: missing libasan not unavailable")
    if classify_unavailable(invalid_project) or not project_has_failed(invalid_project):
        raise SystemExit("sanitizer availability regression: invalid project misclassified")
    if not has_sanitizer_failure(runtime_report):
        raise SystemExit("sanitizer availability regression: runtime report not detected")
    print("K1-C2 sanitizer classification regression OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())