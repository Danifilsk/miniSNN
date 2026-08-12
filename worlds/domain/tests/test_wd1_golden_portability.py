#!/usr/bin/env python3
"""Verify WD1 textual golden portability without relaxing semantic checks."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile


def run(command: list[str], expected: int) -> None:
    completed = subprocess.run(command, text=True, capture_output=True, check=False)
    if completed.returncode != expected:
        raise SystemExit(
            "WD1 golden text portability FAILED:\n" + completed.stdout + completed.stderr
        )


def alter_field(summary: bytes, field: bytes) -> bytes:
    prefix = field + b"="
    start = summary.find(prefix)
    if start < 0:
        raise SystemExit("WD1 golden text portability FAILED: missing " + field.decode("ascii"))
    value_start = start + len(prefix)
    value_end = summary.find(b"\n", value_start)
    if value_end < 0:
        value_end = len(summary)
    value = summary[value_start:value_end]
    if not value:
        raise SystemExit("WD1 golden text portability FAILED: empty " + field.decode("ascii"))
    replacement = (b"FAIL" if field == b"round_trip" else value[:-1] +
                   (b"0" if value[-1:] != b"0" else b"1"))
    return summary[:value_start] + replacement + summary[value_end:]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--python", required=True)
    parser.add_argument("--checker", required=True)
    parser.add_argument("--demo", required=True)
    parser.add_argument("--domain-root", required=True)
    args = parser.parse_args()
    demo = Path(args.demo).resolve()
    checker = Path(args.checker).resolve()
    domain = Path(args.domain_root).resolve()

    with tempfile.TemporaryDirectory(prefix="wd1_golden_text_") as temporary:
        output = Path(temporary) / "output"
        output.mkdir()
        run([str(demo), str(output)], 0)
        summary_path = output / "wd1_summary.txt"
        summary = summary_path.read_bytes()
        if b"\r" in summary:
            raise SystemExit("WD1 golden text portability FAILED: summary is not canonical LF")
        run([args.python, str(checker), "--domain-root", str(domain), "--directory", str(output)], 0)
        for field in (b"checkpoint_tick", b"domain_snapshot_size", b"domain_snapshot_digest",
                      b"checkpoint_kernel_hash", b"checkpoint_domain_hash", b"final_kernel_hash",
                      b"final_domain_hash", b"round_trip"):
            summary_path.write_bytes(alter_field(summary, field))
            run([args.python, str(checker), "--domain-root", str(domain), "--directory", str(output)], 1)
            summary_path.write_bytes(summary)
    print("WD1 golden text portability validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())