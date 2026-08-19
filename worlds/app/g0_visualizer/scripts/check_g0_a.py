#!/usr/bin/env python3
import argparse
from pathlib import Path
import sys


REQUIRED = {
    "format": "G0-A Visualizer V1",
    "tile_pixels": "64",
    "entity_pixels": "32",
    "water_overlay_alpha": "0.50",
    "draw_order": "base,rock,food_fish,water,ui",
    "ticks": "3",
    "moves": "1",
    "eats": "1",
    "final_x": "1000",
    "final_y": "0",
    "remaining_food": "0",
}


def fail(message):
    print(f"G0-A checker FAIL: {message}", file=sys.stderr)
    return 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", required=True)
    args = parser.parse_args()
    directory = Path(args.directory)
    trace = directory / "g0_trace.csv"
    summary = directory / "g0_summary.txt"
    if not trace.is_file() or not summary.is_file():
        return fail("missing g0_trace.csv or g0_summary.txt")
    values = {}
    for line in summary.read_text(encoding="utf-8").splitlines():
        if "=" not in line:
            return fail("invalid summary line")
        key, value = line.split("=", 1)
        values[key] = value
    for key, value in REQUIRED.items():
        if values.get(key) != value:
            return fail(f"{key} mismatch")
    for key in ("terrain_hash", "final_kernel_hash", "final_domain_hash", "trace_digest"):
        if key not in values or not values[key].isdigit() or int(values[key]) == 0:
            return fail(f"{key} is absent or invalid")
    rows = trace.read_text(encoding="utf-8").splitlines()
    if len(rows) != 4 or not rows[0].startswith("tick,x,y,energy,action,"):
        return fail("trace shape mismatch")
    if ",MOVE," not in rows[1] or ",EAT," not in rows[2]:
        return fail("trace does not contain real MOVE then EAT actions")
    print("G0-A visualizer validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
