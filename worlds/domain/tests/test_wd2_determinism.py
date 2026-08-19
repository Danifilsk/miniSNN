#!/usr/bin/env python3
"""Run the WD2 demo twice and compare its scientific artifacts byte-for-byte."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile


def run_demo(demo: str, directory: Path) -> None:
    completed = subprocess.run([demo, str(directory)], text=True, capture_output=True, check=False)
    if completed.returncode != 0:
        raise RuntimeError(completed.stdout + completed.stderr)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--demo", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="wd2_determinism_") as temporary:
        root = Path(temporary)
        left, right = root / "left", root / "right"
        left.mkdir()
        right.mkdir()
        run_demo(args.demo, left)
        run_demo(args.demo, right)
        for name in ("summary.txt", "events.csv", "actions.csv", "dead_snapshot_v2.bin"):
            if (left / name).read_bytes() != (right / name).read_bytes():
                print(f"WD2 determinism FAIL: {name}")
                return 1
    print("WD2 determinism PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())