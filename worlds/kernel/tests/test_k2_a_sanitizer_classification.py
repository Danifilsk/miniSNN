#!/usr/bin/env python3
"""Regression contracts for K2-A sanitizer availability classification."""
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
    if not classify_unavailable(completed(1, "ld: cannot find -lasan")):
        raise SystemExit("K2-A sanitizer classification: unavailable probe was not classified")
    invalid = completed(1, "fatal error: missing_project_source.c: No such file")
    if classify_unavailable(invalid) or not project_has_failed(invalid):
        raise SystemExit("K2-A sanitizer classification: invalid project was misclassified")
    if not has_sanitizer_failure(completed(1, "ERROR: AddressSanitizer: heap-use-after-free")):
        raise SystemExit("K2-A sanitizer classification: runtime sanitizer error was missed")
    harness = (Path(__file__).resolve().parent / "test_k2_a_sanitize.py").read_text(encoding="utf-8")
    for stage in ("sanitized functional", "sanitized long run"):
        if stage not in harness:
            raise SystemExit("K2-A sanitizer classification: required stage missing")
    print("K2-A sanitizer classification regression OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())