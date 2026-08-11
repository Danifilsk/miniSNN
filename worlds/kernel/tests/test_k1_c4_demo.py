#!/usr/bin/env python3
"""Runs the K1-C4 demo twice and verifies byte-identical canonical artifacts."""

from __future__ import annotations

import argparse
import filecmp
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ARTIFACTS = (
    "config_used.ini", "commands.csv", "events.csv", "entities.csv",
    "spatial_links.csv", "diagnostics.csv", "state_hash.txt", "summary.txt",
)


def run(command: list[str]) -> None:
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode:
        raise SystemExit(
            "K1-C4 demo determinism FAILED:\n" + result.stdout + result.stderr
        )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--demo", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--checker", required=True)
    parser.add_argument("--golden", required=True)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="k1 c4 ") as temporary:
        root = Path(temporary)
        left = root / "run one"
        right = root / "run two"
        left.mkdir()
        right.mkdir()
        for output in (left, right):
            run([args.demo, args.config, str(output)])
            run([sys.executable, args.checker, "--directory", str(output), "--golden", args.golden])
        for name in ARTIFACTS:
            if not filecmp.cmp(left / name, right / name, shallow=False):
                raise SystemExit(f"K1-C4 demo determinism FAILED: {name} differs")
        invalid = root / "invalid.ini"
        invalid.write_text("[scenario]\nscenario_version=1\n", encoding="ascii")
        rejected = subprocess.run([args.demo, str(invalid), str(root / "bad")], capture_output=True)
        if rejected.returncode == 0:
            raise SystemExit("K1-C4 demo determinism FAILED: malformed config accepted")
    print("K1-C4 demo parser, artifacts and determinism OK")


if __name__ == "__main__":
    main()