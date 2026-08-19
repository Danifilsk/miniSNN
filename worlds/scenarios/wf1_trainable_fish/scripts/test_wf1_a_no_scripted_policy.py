from __future__ import annotations

import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scenario-root", required=True)
    args = parser.parse_args()
    root = Path(args.scenario_root)
    source = (root / "src" / "wf1_trainable_fish.c").read_text(encoding="utf-8")
    required = (
        "minisnn_worlds_trainable_brain_decide",
        "minisnn_worlds_trainable_brain_apply_action_result",
        "minisnn_worlds_trainable_brain_apply_terminal_feedback",
        "minisnn_worlds_trainable_brain_reset_episode",
        "MINISNN_WORLDS_BRAIN_TOPOLOGY_RANDOM",
    )
    forbidden = (
        "minisnn_connect_delayed",
        "minisnn_worlds_brain_bridge_decode_scores_v1",
        "nearest_food_delta_x",
        "nearest_food_delta_y",
        "food_dx >",
        "food_dx <",
        "food_dy >",
        "food_dy <",
        "action.type = MINISNN_WORLDS_DOMAIN_ACTION_",
    )
    for token in required:
        if token not in source:
            raise SystemExit(f"WF1-A policy checker: missing causal contract {token}")
    for token in forbidden:
        if token in source:
            raise SystemExit(f"WF1-A policy checker: forbidden scripted policy token {token}")
    if "result.rejected_actions++" not in source:
        raise SystemExit("WF1-A policy checker: rejected actions must be measured, not rewarded")
    print("WF1-A no scripted policy contract OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())