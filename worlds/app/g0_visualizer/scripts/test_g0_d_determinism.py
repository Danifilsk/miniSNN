#!/usr/bin/env python3
"""Run the G0-D blueprint-only product demo twice and compare its artifacts."""

import argparse
from pathlib import Path
import subprocess
import tempfile


def run_demo(executable: Path, destination: Path) -> tuple[bytes, bytes]:
    result = subprocess.run([str(executable), str(destination)], capture_output=True, text=True)
    if result.returncode != 0:
        raise SystemExit("G0-D determinism FAIL\n" + result.stdout + result.stderr)
    event_log = destination.with_suffix(destination.suffix + ".events.csv")
    return destination.read_bytes(), event_log.read_bytes()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--demo", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="g0_d_determinism_") as directory:
        root = Path(directory)
        first = run_demo(Path(args.demo), root / "first.txt")
        second = run_demo(Path(args.demo), root / "second.txt")
        if first != second:
            raise SystemExit("G0-D determinism FAIL: generated artifacts differ")
    print("G0-D persistence determinism PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
