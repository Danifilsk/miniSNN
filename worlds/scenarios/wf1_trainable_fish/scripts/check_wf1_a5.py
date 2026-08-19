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
            raise SystemExit("WF1-A.5 checker: malformed summary")
        values[key] = value
    return values


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", required=True)
    directory = Path(parser.parse_args().directory)
    csv_path = directory / "tau_probe.csv"
    summary_path = directory / "summary.txt"
    if not csv_path.is_file() or not summary_path.is_file():
        raise SystemExit("WF1-A.5 checker: missing output")

    summary = summary_values(summary_path)
    source = (Path(__file__).resolve().parents[1] / "src" / "wf1_a5_tau_probe.c").read_text(encoding="utf-8")
    required_source = (
        "wf1_a2_terminal_ablation_config(&config);",
        "WF1_A5_LEARNING_RATE 0.01",
        "minisnn_worlds_trainable_brain_set_reward_learning_rate",
        "minisnn_worlds_trainable_brain_set_reward_eligibility_tau",
    )
    if any(token not in source for token in required_source):
        raise SystemExit("WF1-A.5 checker: fixed A.2 protocol or tau setter missing")
    for forbidden in (
        "connection_probability =", "excitatory_weight =", "inhibitory_weight =",
        "food_count =", "reward_profile.", "learning_rate =",
    ):
        if forbidden in source:
            raise SystemExit(f"WF1-A.5 checker: unexpected tuning token {forbidden}")
    for key, expected in {
        "experiment": "WF1-A.5", "source_protocol": "WF1-A.4",
        "behavior_tuning": "NONE", "neural_variable_changed": "eligibility_tau_only",
        "reward_learning_rate": "0.01", "terminal_reward": "0",
        "pre_reward_equivalence": "PASS",
    }.items():
        if summary.get(key) != expected:
            raise SystemExit(f"WF1-A.5 checker: {key} mismatch")

    with csv_path.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    if len(rows) != 3:
        raise SystemExit("WF1-A.5 checker: expected tau 100, 50, and 20")

    expected_taus = [100.0, 50.0, 20.0]
    first = rows[0]
    for row, tau in zip(rows, expected_taus):
        if float(row["eligibility_tau"]) != tau or row["brain_seed"] != "101" or \
           row["world_seed"] != "300001" or row["episode"] != "1" or \
           row["episode_tick"] != "3" or row["action"] != "EAT" or \
           float(row["reward"]) != 1.0 or float(row["energy_gain"]) != 20.0:
            raise SystemExit("WF1-A.5 checker: not the fixed first positive reward")
        for key in (
            "eligibility_mean", "eligibility_mean_absolute", "eligibility_min",
            "eligibility_max", "eligibility_max_absolute", "weight_absolute_change",
            "weight_signed_change",
        ):
            if not math.isfinite(float(row[key])):
                raise SystemExit(f"WF1-A.5 checker: nonfinite {key}")
        if int(row["active_eligibility_count"]) < 0 or \
           int(row["modified_connection_count"]) < 0 or \
           int(row["clamp_min_delta"]) < 0 or int(row["clamp_max_delta"]) < 0:
            raise SystemExit("WF1-A.5 checker: incoherent nonnegative metric")
        if row is not first:
            for key in ("weight_signature_before", "core_tick", "episode_tick", "action", "reward", "energy_gain"):
                if row[key] != first[key]:
                    raise SystemExit(f"WF1-A.5 checker: pre-reward trajectory diverged at {key}")
    print("WF1-A.5 eligibility tau probe validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())