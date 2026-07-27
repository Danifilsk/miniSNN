from __future__ import annotations

import csv
from pathlib import Path
import statistics
import sys


ROOT = Path(__file__).resolve().parents[1]
CSV_PATH = ROOT / "results" / "d1_b_robustness" / "d1_b_performance.csv"
SUMMARY = ROOT / "results" / "d1_b_robustness" / "d1_b_performance_summary.txt"
EXPECTED = ("neural_steps", "structural_steps", "c7_ticks", "checkpoint_save", "checkpoint_load")


def main() -> int:
    if not CSV_PATH.is_file():
        print("D1-B benchmark FAILED: CSV absent")
        return 1
    grouped: dict[str, list[float]] = {metric: [] for metric in EXPECTED}
    with CSV_PATH.open(newline="", encoding="ascii") as file:
        for row in csv.DictReader(file):
            if row.get("metric") not in grouped:
                continue
            throughput = float(row["throughput"])
            seconds = float(row["seconds"])
            if throughput <= 0.0 or seconds <= 0.0:
                print("D1-B benchmark FAILED: non-positive throughput or elapsed time")
                return 1
            grouped[row["metric"]].append(seconds)
    if any(len(values) != 3 for values in grouped.values()):
        print("D1-B benchmark FAILED: every metric must have exactly three repetitions")
        return 1
    lines = ["D1-B local performance summary", "machine_dependent=true"]
    for metric in EXPECTED:
        values = sorted(grouped[metric])
        lines.append(f"{metric}: min={values[0]:.9f} median={statistics.median(values):.9f} max={values[-1]:.9f}")
    SUMMARY.write_text("\n".join(lines) + "\n", encoding="ascii")
    print("D1-B benchmark validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
