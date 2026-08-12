#!/usr/bin/env python3
"""Run the WD1 demo twice and compare canonical snapshot artifacts byte for byte."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile

ARTIFACTS = ("wd1_summary.txt", "domain_snapshot_v1.bin")


def run(demo: Path, output: Path) -> None:
    output.mkdir()
    completed = subprocess.run([str(demo), str(output)], text=True, capture_output=True, check=False)
    if completed.returncode != 0:
        raise SystemExit("WD1 determinism FAILED:\n" + completed.stdout + completed.stderr)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--demo", required=True)
    args = parser.parse_args()
    demo = Path(args.demo).resolve()
    with tempfile.TemporaryDirectory(prefix="wd1_determinism_") as temporary:
        root = Path(temporary)
        first, second = root / "first output", root / "second output"
        run(demo, first)
        run(demo, second)
        for name in ARTIFACTS:
            if (first / name).read_bytes() != (second / name).read_bytes():
                raise SystemExit(f"WD1 determinism FAILED: {name} differs")
    print("WD1 deterministic snapshot validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())