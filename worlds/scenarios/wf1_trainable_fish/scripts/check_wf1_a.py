from __future__ import annotations

import argparse
from pathlib import Path


REQUIRED_SUMMARY = {
    "brain_topology",
    "topology_signature",
    "brain_config_signature",
    "brain_seed_count",
    "training_episode_count",
    "evaluation_episode_count",
    "initial_weight_signature",
    "trained_weight_signature",
    "loaded_weight_signature",
    "evaluation_weight_before",
    "evaluation_weight_after",
    "untrained_mean_food",
    "trained_mean_food",
    "control_mean_food",
    "positive_reward",
    "starvation_terminal_reward",
    "evaluation_plasticity",
    "result",
}


def parse_summary(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    return values


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", required=True)
    args = parser.parse_args()
    directory = Path(args.directory)
    expected = ("summary.txt", "training.csv", "evaluation.csv", "seeds.csv")
    for name in expected:
        path = directory / name
        if not path.is_file() or path.stat().st_size == 0:
            raise SystemExit(f"WF1-A checker: missing artifact {path}")
    summary = parse_summary(directory / "summary.txt")
    missing = REQUIRED_SUMMARY - summary.keys()
    if missing:
        raise SystemExit(f"WF1-A checker: missing summary keys: {sorted(missing)}")
    if summary["positive_reward"] != "EAT_APPLIED_ENERGY_GAIN":
        raise SystemExit("WF1-A checker: positive reward contract changed")
    if summary["evaluation_plasticity"] != "OFF":
        raise SystemExit("WF1-A checker: evaluation must be frozen")
    if summary["evaluation_weight_before"] != summary["evaluation_weight_after"]:
        raise SystemExit("WF1-A checker: evaluation changed weights")
    if summary["trained_weight_signature"] != summary["loaded_weight_signature"]:
        raise SystemExit("WF1-A checker: learned brain save/load mismatch")
    if summary["result"] not in {"LEARNING_DETECTED", "NO_IMPROVEMENT", "INCONCLUSIVE"}:
        raise SystemExit("WF1-A checker: invalid scientific result")
    for name, header in {
        "training.csv": "brain_seed,episode,world_seed,ticks,foods_eaten,cumulative_reward,death_cause,initial_or_start_weight_signature,end_weight_signature",
        "evaluation.csv": "phase,brain_seed,world_seed,ticks,foods_eaten,success,cumulative_reward,weight_signature_before,weight_signature_after",
    }.items():
        first = (directory / name).read_text(encoding="utf-8").splitlines()[0]
        if first != header:
            raise SystemExit(f"WF1-A checker: {name} schema changed")
    print("WF1-A trainable fish validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())