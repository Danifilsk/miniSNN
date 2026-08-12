#!/usr/bin/env python3
"""Classify the WB0 sanitizer harness contract."""
from __future__ import annotations

import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bridge-root", required=True)
    args = parser.parse_args()
    source = (Path(args.bridge_root) / "tests" / "test_wb0_sanitize.py").read_text(encoding="utf-8")
    for token in ("UNAVAILABLE", "PASS", "FAIL", "-fsanitize=address,undefined"):
        if token not in source:
            raise SystemExit(f"WB0 sanitizer classification FAILED: {token} missing")
    print("WB0 sanitizer classification validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())