#!/usr/bin/env python3
"""Validate WF0 Fish V1 artifacts and its no-hidden-controller source contract."""
from __future__ import annotations

import argparse
import csv
from pathlib import Path
import re

EXPECTED_SUMMARY = {
    "format": "WF0 Fish V1",
    "brain": "Fixed deterministic LIF network, no plasticity or learning",
    "ticks": "3",
    "decisions": "3",
    "moves": "1",
    "eats": "1",
    "food_consumed": "1",
    "initial_energy": "50",
    "final_energy": "60",
    "metabolism_per_tick": "1",
    "move_energy_cost": "2",
    "food_nutrition": "15",
    "core_steps_per_decision": "8",
    "final_core_step": "24",
}


def fail(message: str) -> None:
    raise SystemExit(f"WF0 check FAILED: {message}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", required=True)
    args = parser.parse_args()
    directory = Path(args.directory)
    summary_path = directory / "wf0_summary.txt"
    trace_path = directory / "wf0_fish_trace.csv"
    if not summary_path.is_file() or not trace_path.is_file():
        fail("summary or trace missing")

    summary: dict[str, str] = {}
    for line in summary_path.read_text(encoding="ascii").splitlines():
        if "=" not in line:
            fail("malformed summary")
        key, value = line.split("=", 1)
        summary[key] = value
    for key, expected in EXPECTED_SUMMARY.items():
        if summary.get(key) != expected:
            fail(f"summary {key} expected {expected!r}, got {summary.get(key)!r}")
    for key in ("final_kernel_hash", "final_domain_hash", "trace_digest"):
        if not summary.get(key, "").isdigit() or int(summary[key]) == 0:
            fail(f"summary {key} missing or invalid")

    with trace_path.open("r", newline="", encoding="ascii") as stream:
        rows = list(csv.DictReader(stream))
    if len(rows) != 3:
        fail("trace must contain exactly three decisions")
    for column in (
        "sensor_energy", "sensor_hunger", "sensor_food_present",
        "sensor_food_dx", "sensor_food_dy", "sensor_food_distance",
    ):
        if column not in rows[0]:
            fail(f"trace sensor frame column missing: {column}")
    expected_actions = ("MOVE", "EAT", "WAIT")
    expected_channels = ("1", "5", "0")
    expected_energy = ("47", "61", "60")
    for index, row in enumerate(rows):
        if row["domain_action"] != expected_actions[index]:
            fail(f"unexpected action at row {index}")
        if row["selected_channel"] != expected_channels[index]:
            fail(f"unexpected action channel at row {index}")
        if row["energy_after_tick"] != expected_energy[index]:
            fail(f"unexpected energy at row {index}")
        if row["cache_hit"] != "0":
            fail("fresh tick cannot be a bridge cache hit")
        if int(row["core_step_end"]) - int(row["core_step_start"]) != 8:
            fail("decision did not execute exactly eight core steps")
    if rows[0]["food_dx"] != "1000" or int(rows[0]["move_pos_x_score"]) <= int(rows[0]["move_neg_x_score"]):
        fail("positive X move is not supported by neural output scores")
    if rows[1]["food_distance"] != "0" or rows[1]["food_nutrition"] != "15":
        fail("EAT row does not consume the colocated food")
    if rows[2]["food_present"] != "0" or rows[2]["remaining_food"] != "0":
        fail("food persists after consumption")

    scenario_root = Path(__file__).resolve().parent.parent
    golden_path = scenario_root / "tests" / "golden" / "wf0_summary.txt"
    if not golden_path.is_file() or summary_path.read_bytes() != golden_path.read_bytes():
        fail("summary differs from deterministic golden")
    action_source = (scenario_root / "src" / "wf0_fish.c").read_text(encoding="ascii")
    observation_source = (scenario_root / "src" / "wf0_fish_observation.c").read_text(encoding="ascii")
    if "minisnn_worlds_domain_perceive" in action_source:
        fail("action loop reads Domain perception directly")
    if "minisnn_worlds_domain_perceive" not in observation_source:
        fail("observability module must own direct perception")
    if action_source.count("wf0_fish_world_reset_brain(") != 1:
        fail("normal action loop invokes the explicit brain reset")
    if "test_brain_continuity_and_manual_reset" not in (
        scenario_root / "tests" / "test_wf0_fish.c"
    ).read_text(encoding="ascii"):
        fail("neural continuity regression missing")
    if not re.search(
        r"minisnn_worlds_brain_bridge_decide\([\s\S]*?&record\.decision\.action[\s\S]*?"
        r"minisnn_worlds_domain_step\([\s\S]*?&record\.decision\.action",
        action_source,
    ):
        fail("action loop is not Bridge.decide followed by Domain.step")
    print("WF0 Fish artifact and causal boundary validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
