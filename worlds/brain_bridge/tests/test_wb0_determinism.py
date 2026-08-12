#!/usr/bin/env python3
"""Run the WB0 demo twice and compare every canonical artifact byte-for-byte."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile

ARTIFACTS = (
    "summary.txt",
    "sensor_frames.csv",
    "neural_outputs.csv",
    "decisions.csv",
    "world_hashes.csv",
)


def run(demo: Path, output: Path) -> None:
    output.mkdir()
    completed = subprocess.run([str(demo), str(output)], text=True, capture_output=True, check=False)
    if completed.returncode != 0:
        raise SystemExit("WB0 determinism FAILED:\n" + completed.stdout + completed.stderr)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--demo", required=True)
    args = parser.parse_args()
    demo = Path(args.demo).resolve()
    with tempfile.TemporaryDirectory(prefix="wb0_determinism_") as directory:
        root = Path(directory)
        first = root / "first output"
        second = root / "second output"
        run(demo, first)
        run(demo, second)
        for artifact in ARTIFACTS:
            if (first / artifact).read_bytes() != (second / artifact).read_bytes():
                raise SystemExit(f"WB0 determinism FAILED: {artifact} differs")
    print("WB0 deterministic artifacts validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())