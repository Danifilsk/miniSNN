#!/usr/bin/env python3
"""Classify the WD0 sanitizer harness contract."""
from __future__ import annotations

import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--domain-root", required=True)
    args = parser.parse_args()
    source = (Path(args.domain_root) / "tests" / "test_wd0_sanitize.py").read_text(encoding="utf-8")
    for token in ("UNAVAILABLE", "PASS", "FAIL", "-fsanitize=address,undefined"):
        if token not in source:
            raise SystemExit(f"WD0 sanitizer classification FAILED: {token} missing")
    print("WD0 sanitizer classification validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())