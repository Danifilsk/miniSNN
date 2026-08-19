#!/usr/bin/env python3
import argparse
from pathlib import Path
import subprocess
import tempfile

FILES = ("wt0_map.txt", "wt0_summary.txt")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--demo", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="wt0_determinism_") as temporary:
        root = Path(temporary)
        first = root / "first"
        second = root / "second"
        first.mkdir()
        second.mkdir()
        for directory in (first, second):
            subprocess.run([args.demo, str(directory)], check=True)
        for name in FILES:
            if (first / name).read_bytes() != (second / name).read_bytes():
                raise SystemExit(f"WT0 determinism FAIL: {name}")
    print("WT0 determinism OK")

if __name__ == "__main__":
    main()
