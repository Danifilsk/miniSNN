from __future__ import annotations

import argparse
from pathlib import Path


FORBIDDEN = (
    "nearest_food_delta_x", "nearest_food_delta_y", "food_dx >", "food_dx <",
    "food_dy >", "food_dy <", "distance ->", "food_present ->",
    "minisnn_connect_delayed", "action.type = MINISNN_WORLDS_DOMAIN_ACTION_",
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scenario-root", required=True)
    root = Path(parser.parse_args().scenario_root)
    base = (root / "src" / "wf1_trainable_fish.c").read_text(encoding="utf-8")
    bootstrap = (root / "src" / "wf1_a1_exploration.c").read_text(encoding="utf-8")
    a2_path = root / "src" / "wf1_a2_terminal_ablation.c"
    ablation = a2_path.read_text(encoding="utf-8") if a2_path.is_file() else ""
    for token in FORBIDDEN:
        if token in base or token in bootstrap or token in ablation:
            raise SystemExit(f"WF1-A.1 policy checker: forbidden food policy token {token}")
    required = (
        "action_population_size", "connection_probability", "decision_steps_per_tick",
        "food_count", "food_grid_radius", "metrics_is_better",
    )
    for token in required:
        if token not in bootstrap:
            raise SystemExit(f"WF1-A.1 policy checker: missing generic bootstrap contract {token}")
    if "energy_after > action_result->energy_before" not in base:
        raise SystemExit("WF1-A.1 policy checker: positive reward must require real EAT gain")
    print("WF1-A.1 no scripted policy contract OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())