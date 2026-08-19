from __future__ import annotations

import argparse
import csv
from pathlib import Path


SUMMARY_KEYS = {
    "experiment", "predecessor_result", "calibration_config", "calibration_brain_seeds",
    "calibration_world_seed_base", "brain_seeds", "training_world_seed_base",
    "evaluation_world_seed_base", "training_episodes_per_seed",
    "evaluation_episodes_per_seed", "original_wait_rate", "original_move_rate",
    "original_eat_rate", "bootstrap_wait_rate", "bootstrap_move_rate",
    "bootstrap_eat_rate", "original_zero_score_decisions", "original_tied_winner_decisions",
    "bootstrap_zero_score_decisions", "bootstrap_tied_winner_decisions",
    "evaluation_plasticity", "evaluation_weight_freeze", "training_successful_eats",
    "positive_rewards", "untrained_mean_food", "trained_mean_food", "control_mean_food",
    "untrained_success_rate", "trained_success_rate", "control_success_rate",
    "improved_seed_count", "changed_weight_seed_count", "exploration_bootstrapped",
    "positive_reward_reached", "result",
}

METRIC_FIELDS = (
    "decision_count", "wait_selected", "move_pos_x_selected", "move_neg_x_selected",
    "move_pos_y_selected", "move_neg_y_selected", "eat_selected", "move_applied",
    "move_rejected", "eat_applied", "eat_rejected", "successful_eat_count",
    "positive_reward_count", "terminal_reward_count", "total_spikes",
    "active_decision_count", "zero_score_decisions", "tied_winner_decisions",
    "starvation_count",
)


def summary_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    return values


def validate_metrics(row: dict[str, str], label: str) -> None:
    values = {name: int(row[name]) for name in METRIC_FIELDS}
    selected = (values["wait_selected"] + values["move_pos_x_selected"] +
                values["move_neg_x_selected"] + values["move_pos_y_selected"] +
                values["move_neg_y_selected"] + values["eat_selected"])
    moves = (values["move_pos_x_selected"] + values["move_neg_x_selected"] +
             values["move_pos_y_selected"] + values["move_neg_y_selected"])
    if selected != values["decision_count"]:
        raise SystemExit(f"WF1-A.1 checker: action accounting mismatch in {label}")
    if values["move_applied"] + values["move_rejected"] > moves:
        raise SystemExit(f"WF1-A.1 checker: move accounting mismatch in {label}")
    if values["eat_applied"] + values["eat_rejected"] > values["eat_selected"]:
        raise SystemExit(f"WF1-A.1 checker: eat accounting mismatch in {label}")
    if values["successful_eat_count"] > values["eat_applied"]:
        raise SystemExit(f"WF1-A.1 checker: successful EAT was not applied in {label}")
    if values["positive_reward_count"] != values["successful_eat_count"]:
        raise SystemExit(f"WF1-A.1 checker: positive reward contract changed in {label}")
    if values["active_decision_count"] > values["decision_count"]:
        raise SystemExit(f"WF1-A.1 checker: activity accounting mismatch in {label}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", required=True)
    directory = Path(parser.parse_args().directory)
    required = ("summary.txt", "training.csv", "evaluation.csv", "seeds.csv")
    for name in required:
        path = directory / name
        if not path.is_file() or path.stat().st_size == 0:
            raise SystemExit(f"WF1-A.1 checker: missing artifact {path}")
    summary = summary_values(directory / "summary.txt")
    missing = SUMMARY_KEYS - summary.keys()
    if missing:
        raise SystemExit(f"WF1-A.1 checker: missing summary keys: {sorted(missing)}")
    if summary["experiment"] != "WF1-A.1 exploration bootstrap":
        raise SystemExit("WF1-A.1 checker: experiment identity changed")
    if summary["predecessor_result"] != "WF1-A:NO_IMPROVEMENT":
        raise SystemExit("WF1-A.1 checker: predecessor record changed")
    if summary["evaluation_plasticity"] != "OFF" or summary["evaluation_weight_freeze"] != "YES":
        raise SystemExit("WF1-A.1 checker: evaluation freeze contract changed")
    if summary["exploration_bootstrapped"] not in {"YES", "NO"}:
        raise SystemExit("WF1-A.1 checker: invalid exploration status")
    if summary["positive_reward_reached"] not in {"YES", "NO"}:
        raise SystemExit("WF1-A.1 checker: invalid positive reward status")
    if summary["result"] not in {"LEARNING_DETECTED", "NO_IMPROVEMENT", "INCONCLUSIVE"}:
        raise SystemExit("WF1-A.1 checker: invalid scientific result")
    if float(summary["control_mean_food"]) != float(summary["untrained_mean_food"]):
        raise SystemExit("WF1-A.1 checker: frozen control diverged from untrained baseline")

    with (directory / "training.csv").open(newline="", encoding="utf-8") as handle:
        training = list(csv.DictReader(handle))
    with (directory / "evaluation.csv").open(newline="", encoding="utf-8") as handle:
        evaluation = list(csv.DictReader(handle))
    if not training or not evaluation:
        raise SystemExit("WF1-A.1 checker: empty episode CSV")
    if any(field not in training[0] for field in METRIC_FIELDS):
        raise SystemExit("WF1-A.1 checker: training metrics are incomplete")
    if any(field not in evaluation[0] for field in METRIC_FIELDS):
        raise SystemExit("WF1-A.1 checker: evaluation metrics are incomplete")
    for index, row in enumerate(training):
        validate_metrics(row, f"training row {index}")
    for index, row in enumerate(evaluation):
        validate_metrics(row, f"evaluation row {index}")
        if row["weight_signature_before"] != row["weight_signature_after"]:
            raise SystemExit("WF1-A.1 checker: evaluation changed weights")
    with (directory / "seeds.csv").open(newline="", encoding="utf-8") as handle:
        seeds = list(csv.DictReader(handle))
    if len(seeds) != 4:
        raise SystemExit("WF1-A.1 checker: expected one result for each brain seed")
    for row in seeds:
        if row["evaluation_weight_before"] != row["evaluation_weight_after"]:
            raise SystemExit("WF1-A.1 checker: seed evaluation was not frozen")
        if row["trained_weight_signature"] != row["loaded_weight_signature"]:
            raise SystemExit("WF1-A.1 checker: save/load changed learned weights")
    print("WF1-A.1 exploration bootstrap validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())