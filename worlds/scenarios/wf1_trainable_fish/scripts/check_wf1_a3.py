from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path


def read_summary(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if "=" not in line:
            raise SystemExit(f"WF1-A.3 checker: malformed summary line: {line!r}")
        key, value = line.split("=", 1)
        values[key] = value
    return values


def finite(row: dict[str, str], name: str) -> float:
    value = float(row[name])
    if not math.isfinite(value):
        raise SystemExit(f"WF1-A.3 checker: nonfinite {name}")
    return value


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", required=True)
    directory = Path(parser.parse_args().directory)
    csv_path = directory / "reward_probe.csv"
    summary_path = directory / "summary.txt"
    if not csv_path.is_file() or not summary_path.is_file():
        raise SystemExit("WF1-A.3 checker: missing reward_probe.csv or summary.txt")
    summary = read_summary(summary_path)
    required = {
        "experiment": "WF1-A.3",
        "source_protocol": "WF1-A.2",
        "behavior_tuning": "NONE",
        "terminal_reward": "0",
        "terminal_boundary": "STARVATION_DEATH",
        "positive_reward": "EAT_APPLIED_ENERGY_GAIN",
        "brain_seed": "101",
    }
    for key, expected in required.items():
        if summary.get(key) != expected:
            raise SystemExit(f"WF1-A.3 checker: {key} is not {expected}")
    with csv_path.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    if not 2 <= len(rows) <= 6:
        raise SystemExit("WF1-A.3 checker: expected two to six positive rewards")
    changed = 0
    for row in rows:
        if row["brain_seed"] != "101" or row["action"] != "EAT" or float(row["reward"]) != 1.0:
            raise SystemExit("WF1-A.3 checker: row is not a seed-101 real +1 EAT reward")
        if int(row["world_seed"]) != 300000 + int(row["episode"]):
            raise SystemExit("WF1-A.3 checker: episode and world seed provenance mismatch")
        if finite(row, "energy_gain") <= 0.0:
            raise SystemExit("WF1-A.3 checker: EAT did not gain energy")
        if int(row["reward_event_count_delta"]) != 1:
            raise SystemExit("WF1-A.3 checker: reward stats are not per-delivery deltas")
        if int(row["active_eligibility_count"]) < 0 or int(row["modified_connection_count"]) < 0:
            raise SystemExit("WF1-A.3 checker: negative connection count")
        signed = finite(row, "weight_signed_change")
        absolute = finite(row, "weight_absolute_change")
        if absolute < abs(signed):
            raise SystemExit("WF1-A.3 checker: absolute change is incoherent")
        for name in (
            "max_absolute_weight_change_observed", "max_absolute_weight_change_delta",
            "eligibility_mean", "eligibility_mean_absolute", "eligibility_min",
            "eligibility_max", "eligibility_max_absolute",
        ):
            finite(row, name)
        if int(row["weight_clamp_min_events_delta"]) < 0 or int(row["weight_clamp_max_events_delta"]) < 0:
            raise SystemExit("WF1-A.3 checker: negative clamp delta")
        if row["weight_signature_before"] != row["weight_signature_after"]:
            changed += 1
    if int(summary["rewards_observed"]) != len(rows):
        raise SystemExit("WF1-A.3 checker: summary reward count mismatch")
    if int(summary["weight_changed_reward_count"]) != changed:
        raise SystemExit("WF1-A.3 checker: summary signature count mismatch")
    for name in (
        "mean_active_eligibility_count", "mean_modified_connection_count",
        "mean_weight_absolute_change", "max_absolute_weight_change_observed",
        "eligibility_mean_absolute", "eligibility_max_absolute_observed",
    ):
        if not math.isfinite(float(summary[name])):
            raise SystemExit(f"WF1-A.3 checker: nonfinite summary {name}")
    print("WF1-A.3 reward probe validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())