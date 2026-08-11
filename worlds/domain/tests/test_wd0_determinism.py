#!/usr/bin/env python3
"""Run the WD0 demo twice and compare every canonical artifact byte for byte."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile

ARTIFACTS = (
    "summary.txt", "domain_entities.csv", "domain_actions.csv", "domain_events.csv",
    "perceptions.csv", "kernel_hashes.csv", "domain_hashes.csv",
)


def run(demo: Path, output: Path) -> None:
    output.mkdir()
    completed = subprocess.run([str(demo), str(output)], text=True, capture_output=True, check=False)
    if completed.returncode != 0:
        raise SystemExit("WD0 determinism FAILED:\n" + completed.stdout + completed.stderr)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--demo", required=True)
    args = parser.parse_args()
    demo = Path(args.demo).resolve()
    with tempfile.TemporaryDirectory(prefix="wd0_determinism_") as directory:
        root = Path(directory)
        first, second = root / "first output", root / "second output"
        run(demo, first)
        run(demo, second)
        for name in ARTIFACTS:
            if (first / name).read_bytes() != (second / name).read_bytes():
                raise SystemExit(f"WD0 determinism FAILED: {name} differs")
    print("WD0 deterministic artifacts validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())