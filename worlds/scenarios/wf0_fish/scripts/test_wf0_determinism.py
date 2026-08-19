#!/usr/bin/env python3
"""Compare two deterministic WF0 Fish executions byte for byte."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile

ARTIFACTS = ("wf0_summary.txt", "wf0_fish_trace.csv")


def run(demo: Path, output: Path) -> None:
    output.mkdir()
    result = subprocess.run([str(demo), str(output)], capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise SystemExit("WF0 determinism FAILED:\n" + result.stdout + result.stderr)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--demo", required=True)
    args = parser.parse_args()
    demo = Path(args.demo).resolve()
    with tempfile.TemporaryDirectory(prefix="wf0_determinism_") as temporary:
        root = Path(temporary)
        first, second = root / "first output", root / "second output"
        run(demo, first)
        run(demo, second)
        for artifact in ARTIFACTS:
            if (first / artifact).read_bytes() != (second / artifact).read_bytes():
                raise SystemExit(f"WF0 determinism FAILED: {artifact} differs")
    print("WF0 Fish deterministic validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
