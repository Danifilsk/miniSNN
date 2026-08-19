from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path


def summary_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        key, separator, value = line.partition("=")
        if not separator:
            raise SystemExit("WF1-A.4 checker: malformed summary")
        values[key] = value
    return values


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", required=True)
    directory = Path(parser.parse_args().directory)
    csv_path = directory / "saturation_probe.csv"
    summary_path = directory / "summary.txt"
    if not csv_path.is_file() or not summary_path.is_file():
        raise SystemExit("WF1-A.4 checker: missing output")
    summary = summary_values(summary_path)
    source = (Path(__file__).resolve().parents[1] / "src" / "wf1_a4_saturation_probe.c").read_text(encoding="utf-8")
    if "wf1_a2_terminal_ablation_config(&config);" not in source or \
       "minisnn_worlds_trainable_brain_set_reward_learning_rate" not in source:
        raise SystemExit("WF1-A.4 checker: A.2 configuration or learning-rate setter missing")
    for forbidden in ("eligibility_tau =", "connection_probability =", "excitatory_weight =",
                      "inhibitory_weight =", "food_count =", "reward_profile."):
        if forbidden in source:
            raise SystemExit(f"WF1-A.4 checker: unexpected tuning token {forbidden}")
    for key, expected in {
        "experiment": "WF1-A.4", "source_protocol": "WF1-A.2",
        "behavior_tuning": "NONE", "neural_variable_changed": "learning_rate_only",
        "terminal_reward": "0", "pre_reward_equivalence": "PASS",
    }.items():
        if summary.get(key) != expected:
            raise SystemExit(f"WF1-A.4 checker: {key} mismatch")
    with csv_path.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    if len(rows) != 3:
        raise SystemExit("WF1-A.4 checker: expected three learning rates")
    expected_rates = [1.0, 0.1, 0.01]
    first = rows[0]
    for row, rate in zip(rows, expected_rates):
        if float(row["learning_rate"]) != rate or row["brain_seed"] != "101" or \
           row["world_seed"] != "300001" or row["episode"] != "1" or \
           row["episode_tick"] != "3" or row["action"] != "EAT" or \
           float(row["reward"]) != 1.0 or float(row["energy_gain"]) <= 0.0:
            raise SystemExit("WF1-A.4 checker: not the same real first reward")
        for key in ("weight_absolute_change", "weight_signed_change",
                    "eligibility_mean_absolute", "eligibility_min", "eligibility_max",
                    "eligibility_max_absolute"):
            if not math.isfinite(float(row[key])):
                raise SystemExit(f"WF1-A.4 checker: nonfinite {key}")
        if int(row["active_eligibility_count"]) < 0 or int(row["modified_connection_count"]) < 0 or \
           int(row["clamp_min_delta"]) < 0 or int(row["clamp_max_delta"]) < 0:
            raise SystemExit("WF1-A.4 checker: incoherent nonnegative metric")
        if row is not first and row["weight_signature_before"] != first["weight_signature_before"]:
            raise SystemExit("WF1-A.4 checker: pre-reward weights diverged")
    if float(rows[2]["weight_absolute_change"]) >= float(rows[0]["weight_absolute_change"]):
        raise SystemExit("WF1-A.4 checker: low LR did not reduce update magnitude")
    print("WF1-A.4 saturation probe validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())