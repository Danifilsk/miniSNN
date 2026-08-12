#!/usr/bin/env python3
"""Validate the canonical WD1 Domain Snapshot V1 demo artifact."""
from __future__ import annotations

import argparse
from pathlib import Path

MAGIC = b"MSWDOMS1"
DIGEST_OFFSET = 44


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit("WD1 checker: " + message)


def normalize_text_line_endings(data: bytes) -> bytes:
    return data.replace(b"\r\n", b"\n")


def fnv1a_snapshot(data: bytes) -> int:
    value = 14695981039346656037
    for index, byte in enumerate(data):
        value ^= 0 if DIGEST_OFFSET <= index < DIGEST_OFFSET + 8 else byte
        value = (value * 1099511628211) & ((1 << 64) - 1)
    return value


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--domain-root", required=True)
    parser.add_argument("--directory", required=True)
    args = parser.parse_args()
    domain = Path(args.domain_root).resolve()
    output = Path(args.directory)
    summary_path = output / "wd1_summary.txt"
    snapshot_path = output / "domain_snapshot_v1.bin"
    golden_path = domain / "tests" / "golden" / "wd1_demo_summary.txt"
    require((domain / "include" / "minisnn_worlds_domain.h").is_file(), "public header missing")
    require((domain / "src" / "minisnn_worlds_domain.c").is_file(), "Domain source missing")
    require(summary_path.is_file(), "missing wd1_summary.txt")
    require(snapshot_path.is_file(), "missing domain_snapshot_v1.bin")
    require(not (output / "domain_snapshot_v1.bin.tmp").exists(), "temporary snapshot was left behind")
    summary_bytes = summary_path.read_bytes()
    require(b"\r" not in summary_bytes, "wd1_summary.txt must use canonical LF line endings")
    values = dict(line.split("=", 1) for line in summary_bytes.decode("utf-8").splitlines() if "=" in line)
    require(values.get("format") == "Domain Snapshot V1", "wrong snapshot format")
    require(values.get("round_trip") == "PASS", "continuous and restored runs differ")
    data = snapshot_path.read_bytes()
    require(data.startswith(MAGIC), "wrong snapshot magic")
    require(len(data) == int(values.get("domain_snapshot_size", "0")), "snapshot size mismatch")
    stored_digest = int.from_bytes(data[DIGEST_OFFSET:DIGEST_OFFSET + 8], "little")
    require(stored_digest == fnv1a_snapshot(data), "snapshot digest mismatch")
    require(values.get("domain_snapshot_digest") == f"0x{stored_digest:016X}", "summary digest mismatch")
    require(golden_path.is_file(), "missing WD1 golden")
    require(summary_bytes == normalize_text_line_endings(golden_path.read_bytes()),
            "canonical WD1 golden changed")
    print("WD1 Domain Snapshot V1 validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())