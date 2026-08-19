#!/usr/bin/env python3
import argparse
from pathlib import Path
import sys

EXPECTED_MAP = "WWWWW\nWWRWW\nWWWWW\nWLLLW\nWLLLW\n"
EXPECTED = {
    "width": "5",
    "height": "5",
    "tile_size": "1000",
    "water_count": "19",
    "land_count": "6",
    "rock_count": "1",
    "successful_moves": "2",
    "blocked_by_land": "1",
    "blocked_by_rock": "1",
    "terrain_hash": "3326401265351576889",
    "final_kernel_hash": "6001033941956064286",
}

def fail(message):
    print(f"WT0 checker FAIL: {message}", file=sys.stderr)
    return 1

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", required=True)
    args = parser.parse_args()
    directory = Path(args.directory)
    map_path = directory / "wt0_map.txt"
    summary_path = directory / "wt0_summary.txt"
    if not map_path.is_file() or not summary_path.is_file():
        return fail("missing wt0_map.txt or wt0_summary.txt")
    if map_path.read_text(encoding="utf-8") != EXPECTED_MAP:
        return fail("canonical map mismatch")
    values = {}
    for line in summary_path.read_text(encoding="utf-8").splitlines():
        if "=" not in line:
            return fail("invalid summary line")
        key, value = line.split("=", 1)
        values[key] = value
    if set(values) != set(EXPECTED):
        return fail("summary key set mismatch")
    for key, expected in EXPECTED.items():
        if values.get(key) != expected:
            return fail(f"{key} mismatch")
    print("WT0 terrain validation OK")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
