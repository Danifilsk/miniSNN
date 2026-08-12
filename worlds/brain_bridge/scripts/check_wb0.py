#!/usr/bin/env python3
"""Validate the semantic contents of deterministic WB0 Brain Bridge artifacts."""
from __future__ import annotations

import argparse
import csv
from pathlib import Path

ACTIONS = {"WAIT", "MOVE", "EAT"}
CHANNELS = {"WAIT", "MOVE_POS_X", "MOVE_NEG_X", "MOVE_POS_Y", "MOVE_NEG_Y", "EAT"}
FALLBACKS = {"NONE", "NO_OUTPUT", "EAT_WITHOUT_FOOD"}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"WB0 checker: {message}")


def rows(path: Path) -> list[dict[str, str]]:
    require(path.is_file(), f"missing {path.name}")
    with path.open("r", encoding="utf-8", newline="") as file:
        return list(csv.DictReader(file))


def summary(path: Path) -> dict[str, str]:
    require(path.is_file(), "missing summary.txt")
    return dict(
        line.split("=", 1)
        for line in path.read_text(encoding="utf-8").splitlines()
        if "=" in line
    )


def finite_frame(row: dict[str, str]) -> bool:
    fields = (
        "self_energy", "self_hunger", "nearest_food_present",
        "nearest_food_delta_x", "nearest_food_delta_y", "nearest_food_distance",
    )
    try:
        values = [float(row[field]) for field in fields]
    except (KeyError, ValueError):
        return False
    return all(-1.0 <= value <= 1.0 for value in values)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", required=True)
    args = parser.parse_args()
    output = Path(args.directory)

    sensor = rows(output / "sensor_frames.csv")
    outputs = rows(output / "neural_outputs.csv")
    decisions = rows(output / "decisions.csv")
    hashes = rows(output / "world_hashes.csv")
    values = summary(output / "summary.txt")

    require(len(sensor) == 3, "expected three primary decision frames")
    require(len(outputs) == len(sensor) == len(decisions), "decision artifact row mismatch")
    require(len(hashes) == 4, "expected initial plus three post-step hashes")
    require(values.get("decisions_computed") == "3", "decision count mismatch")
    require(values.get("cache_hits") == "1", "same-tick cache proof missing")
    require(values.get("core_steps_per_decision") == "8", "unexpected decision window")
    require(values.get("final_kernel_hash", "").startswith("0x"), "kernel hash missing")
    require(values.get("final_domain_hash", "").startswith("0x"), "domain hash missing")

    prior_tick = -1
    for sensor_row, output_row, decision in zip(sensor, outputs, decisions):
        tick = int(decision["tick"])
        require(tick > prior_tick, "domain ticks do not progress")
        prior_tick = tick
        require(sensor_row["tick"] == decision["tick"] == output_row["tick"], "tick mismatch")
        require(sensor_row["actor"] == decision["actor"] == output_row["actor"], "actor mismatch")
        require(finite_frame(sensor_row), "sensor frame is outside the documented bounded range")
        require(decision["channel"] in CHANNELS, "invalid semantic output channel")
        require(decision["action"] in ACTIONS, "invalid Domain action")
        require(decision["fallback"] in FALLBACKS, "invalid fallback")
        require(decision["cache_hit"] == "0", "primary decision unexpectedly marked cached")
        require(int(decision["core_steps"]) == 8, "decision did not execute exactly eight Core steps")
        for field in ("wait", "move_pos_x", "move_neg_x", "move_pos_y", "move_neg_y", "eat"):
            require(int(output_row[field]) >= 0, f"negative score in {field}")
    require(any(row["action"] == "MOVE" for row in decisions), "real Core did not produce a move")
    require(all(row["kernel_hash"].startswith("0x") and row["domain_hash"].startswith("0x")
                for row in hashes), "hash history malformed")

    print("WB0 Brain Bridge artifact validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())