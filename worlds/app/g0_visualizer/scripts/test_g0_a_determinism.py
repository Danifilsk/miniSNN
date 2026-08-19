#!/usr/bin/env python3
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--demo", required=True)
    parser.add_argument("--repository-root", required=True)
    args = parser.parse_args()
    demo = Path(args.demo).resolve()
    repository_root = Path(args.repository_root).resolve()
    with tempfile.TemporaryDirectory(prefix="g0_a_determinism_") as temporary:
        root = Path(temporary)
        first = root / "first"
        second = root / "second"
        first.mkdir()
        second.mkdir()
        subprocess.run([str(demo), str(first), str(repository_root)], check=True)
        subprocess.run([str(demo), str(second), str(repository_root)], check=True)
        for name in ("g0_trace.csv", "g0_summary.txt"):
            if (first / name).read_bytes() != (second / name).read_bytes():
                raise SystemExit(f"G0-A deterministic FAIL: {name}")
    print("G0-A deterministic trace and summary OK")


if __name__ == "__main__":
    main()
