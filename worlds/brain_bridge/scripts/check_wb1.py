"""Artifact and source contracts for the WB1 trainable-brain demonstration."""

from __future__ import annotations

import argparse
from pathlib import Path


def require(path: Path) -> None:
    if not path.is_file() or path.stat().st_size == 0:
        raise SystemExit(f"WB1 artifact missing or empty: {path}")


def read_key_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="ascii").splitlines():
        key, separator, value = line.partition("=")
        if not separator or not key or key in values:
            raise SystemExit(f"WB1 summary malformed: {path}")
        values[key] = value
    return values


def require_source_contracts(repository: Path) -> None:
    source = (repository / "worlds" / "brain_bridge" / "src" /
              "minisnn_worlds_trainable_brain.c").read_text(encoding="utf-8")
    if "minisnn_topology_factory_build" not in source:
        raise SystemExit("WB1 source contract failed: public topology factory is not used")
    if "MiniSNNTopologyFactoryRequiredConnection" in source or \
       "required_connection_count" in source:
        raise SystemExit("WB1 source contract failed: initial topology has task-specific wiring")
    if "minisnn_agent_cycle_drain_due_feedback" not in source:
        raise SystemExit("WB1 source contract failed: Training-to-Evaluation boundary is unsafe")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", required=True)
    args = parser.parse_args()
    root = Path(args.directory)
    summary_path = root / "summary.txt"
    manifest_path = root / "brain.wb1"
    require(summary_path)
    require(manifest_path)

    summary = read_key_values(summary_path)
    required = (
        "brain", "neuron_model", "topology", "neurons", "topology_signature",
        "config_signature", "initial_weight_signature", "learned_weight_signature",
        "post_episode_reset_weight_signature", "evaluation_weight_signature",
        "loaded_weight_signature", "core_tick", "learning_resumed",
    )
    if any(key not in summary for key in required):
        raise SystemExit("WB1 summary missing the learning proof fields")
    if summary["neuron_model"] != "lif" or summary["learning_resumed"] != "true":
        raise SystemExit("WB1 summary has an invalid model or resume result")
    if summary["initial_weight_signature"] == summary["learned_weight_signature"]:
        raise SystemExit("WB1 learning proof failed: reward did not change weights")
    if not (summary["learned_weight_signature"] ==
            summary["post_episode_reset_weight_signature"] ==
            summary["evaluation_weight_signature"]):
        raise SystemExit("WB1 learning proof failed: reset or evaluation changed frozen weights")
    if summary["loaded_weight_signature"] == summary["evaluation_weight_signature"]:
        raise SystemExit("WB1 learning proof failed: resumed training did not change weights")

    manifest = manifest_path.read_text(encoding="ascii")
    for key in (
        "generation=", "sensor_schema_signature=", "action_schema_signature=",
        "weight_signature=", "model=lif",
    ):
        if key not in manifest:
            raise SystemExit(f"WB1 manifest missing {key}")
    if f"weight_signature={summary['loaded_weight_signature']}" not in manifest:
        raise SystemExit("WB1 manifest does not preserve the loaded learned weights")

    require_source_contracts(Path(__file__).resolve().parents[3])
    print("WB1 trainable brain validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())