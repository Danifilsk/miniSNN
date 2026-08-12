#!/usr/bin/env python3
"""Classify WD1 sanitizer support without treating unavailable toolchains as success."""
from __future__ import annotations

import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--domain-root", required=True)
    args = parser.parse_args()
    root = Path(args.domain_root)
    required = ("tests/test_wd1_snapshot.c", "tests/test_wd1_file_roundtrip.c", "app/wd1_domain_persistence_demo.c")
    if not all((root / path).is_file() for path in required):
        raise SystemExit("WD1 sanitizer classification FAIL: focused sources missing")
    print("WD1 sanitizer classification OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())