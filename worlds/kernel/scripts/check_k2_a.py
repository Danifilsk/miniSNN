#!/usr/bin/env python3
"""Static and golden checks for the K2-A canonical snapshot contract."""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import subprocess


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--snapshot-test", required=True)
    parser.add_argument("--golden", required=True)
    args = parser.parse_args()

    root = Path(args.root).resolve()
    public_header = root / "include" / "minisnn_worlds_kernel_snapshot.h"
    source = root / "src" / "minisnn_worlds_kernel_snapshot.c"
    contract = root / "docs" / "K2_A_CANONICAL_SNAPSHOT_CONTRACT.md"
    audit = root / "docs" / "K2_A_CANONICAL_SNAPSHOT_AUDIT.md"
    for required in (public_header, source, contract, audit, Path(args.golden)):
        if not required.is_file():
            raise SystemExit(f"K2-A checker missing required file: {required}")

    source_text = source.read_text(encoding="utf-8")
    forbidden = ("fopen(", "fwrite(", "fread(", "CreateFile", "memcpy(buffer, &")
    if any(token in source_text for token in forbidden):
        raise SystemExit("K2-A checker: snapshot implementation must remain memory-only and explicit")
    required_tokens = (
        "writer_u8", "writer_u16", "writer_u32", "writer_u64", "writer_i64",
        "writer_bool", "MINISNN_WORLDS_KERNEL_SNAPSHOT_FORMAT_VERSION_V1",
        "minisnn_worlds_kernel_snapshot_capture",
    )
    if any(token not in source_text for token in required_tokens):
        raise SystemExit("K2-A checker: canonical encoder contract missing")

    golden = {}
    for line in Path(args.golden).read_text(encoding="utf-8").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            golden[key] = value
    expected = {
        "snapshot_format_version", "snapshot_size", "snapshot_digest", "state_hash"
    }
    if set(golden) != expected:
        raise SystemExit("K2-A checker: malformed golden")

    completed = subprocess.run([args.snapshot_test], text=True, capture_output=True)
    if completed.returncode:
        raise SystemExit("K2-A checker: snapshot test failed:\n" + completed.stdout + completed.stderr)
    match = re.search(
        r"format=(\d+) size=(\d+) digest=(0x[0-9A-F]+) state_hash=(0x[0-9A-F]+)",
        completed.stdout,
    )
    if match is None:
        raise SystemExit("K2-A checker: snapshot test output malformed")
    actual = {
        "snapshot_format_version": match.group(1),
        "snapshot_size": match.group(2),
        "snapshot_digest": match.group(3),
        "state_hash": match.group(4),
    }
    if actual != golden:
        raise SystemExit(f"K2-A checker: golden mismatch: expected {golden}, got {actual}")
    print("K2-A canonical snapshot validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())