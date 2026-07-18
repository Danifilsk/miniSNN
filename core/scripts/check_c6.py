from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parent.parent


def fail(message: str) -> None:
    print(f"C6 check failed: {message}", file=sys.stderr)
    raise SystemExit(1)


def require(path: Path) -> str:
    if not path.is_file():
        fail(f"missing required file: {path.relative_to(ROOT)}")
    return path.read_text(encoding="utf-8")


def main() -> None:
    config = require(ROOT / "app" / "scenario_config.c")
    protocol = require(ROOT / "app" / "working_memory.c")
    associative_protocol = require(ROOT / "app" / "associative_memory.c")
    sequence_protocol = require(ROOT / "app" / "sequence_prediction.c")
    runner = require(ROOT / "app" / "scenario_runner.c")
    makefile = require(ROOT / "Makefile")
    require(ROOT / "configs" / "working_memory_demo.ini")
    require(ROOT / "tests" / "test_working_memory.c")
    require(ROOT / "configs" / "associative_memory_demo.ini")
    require(ROOT / "tests" / "test_associative_memory.c")
    require(ROOT / "configs" / "sequence_prediction_demo.ini")
    require(ROOT / "configs" / "sequence_prediction_context_demo.ini")
    require(ROOT / "tests" / "test_sequence_prediction.c")

    for token in (
        "working_memory_enabled",
        "working_memory_trials",
        "working_memory_cue_steps",
        "working_memory_delay_steps",
        "working_memory_probe_steps",
        "working_memory_cue_group_size",
        "working_memory_readout_group_size",
    ):
        if token not in config:
            fail(f"working-memory configuration is missing {token}")

    if "scenario_runtime_step_with_inputs" not in protocol:
        fail("protocol must use explicit temporal inputs")
    if "O cue nao e copiado para a resposta" not in protocol:
        fail("report must state the no-copy limitation")
    if "working_memory_write_outputs" not in runner:
        fail("scenario runner does not write working-memory outputs")
    if "build_working_memory" not in runner:
        fail("scenario runner does not build the recurrent working-memory motif")
    for token in ("chance_accuracy", "control_accuracy", "retention_margin"):
        if token not in protocol:
            fail(f"working-memory control metric is missing {token}")
    if "test-working-memory" not in makefile or "scenario-working-memory" not in makefile:
        fail("Makefile targets for C6.1 are missing")

    forbidden = (
        r"recalled_pattern\s*=\s*(?:trial->)?cue_pattern",
        r"recalled_pattern\s*=\s*(?:trial->)?expected_pattern",
        r"recall_score\s*=\s*(?:trial->)?cue_pattern",
        r"recalled_pattern\s*=\s*working_memory_control_expected_pattern",
    )
    for expression in forbidden:
        if re.search(expression, protocol):
            fail("protocol copies the cue directly into the recall result")

    for token in (
        "associative_memory_enabled",
        "associative_memory_pair_count",
        "associative_memory_training_epochs",
        "associative_memory_cue_corruption",
        "associative_memory_freeze_plasticity_during_recall",
    ):
        if token not in config:
            fail(f"associative-memory configuration is missing {token}")
    for token in (
        "train_pairs",
        "run_recall_trials",
        "scenario_runtime_step_with_inputs",
        "associative_memory_write_outputs",
        "association_margin",
        "untrained_accuracy",
        "shuffled_target_accuracy",
        "frozen_training_accuracy",
    ):
        if token not in associative_protocol:
            fail(f"associative-memory protocol is missing {token}")
    if "build_associative_memory" not in runner:
        fail("scenario runner does not build the associative-memory motif")
    if ("test-associative-memory" not in makefile or
            "scenario-associative-memory" not in makefile):
        fail("Makefile targets for C6.2 are missing")

    forbidden_associative = (
        r"trial\.recalled_pattern\s*=\s*(?:trial\.)?expected_pattern",
        r"trial\.recalled_pattern\s*=\s*(?:trial\.)?pair_id",
        r"inputs\s*\[[^]]+\]\s*=\s*.*expected_pattern",
    )
    for expression in forbidden_associative:
        if re.search(expression, associative_protocol):
            fail("associative protocol bypasses neural recall")

    for token in (
        "sequence_prediction_enabled",
        "sequence_prediction_sequence_count",
        "sequence_prediction_sequence_length",
        "sequence_prediction_prefix_length",
        "sequence_prediction_prediction_delay_steps",
        "sequence_prediction_prediction_probe_steps",
        "sequence_prediction_freeze_plasticity_during_evaluation",
    ):
        if token not in config:
            fail(f"sequence-prediction configuration is missing {token}")
    for token in (
        "train_sequences",
        "run_prediction_trials",
        "decode_prediction",
        "scenario_runtime_step_with_inputs",
        "sequence_prediction_write_outputs",
        "prediction_margin",
        "untrained_control_accuracy",
        "shuffled_order_control_accuracy",
        "frozen_training_control_accuracy",
        "permuted_labels_control_accuracy",
        "last_symbol_only_control_accuracy",
        "context_margin",
        "teacher pulse supervisionado",
    ):
        if token not in sequence_protocol:
            fail(f"sequence-prediction protocol is missing {token}")
    if "build_sequence_prediction" not in runner:
        fail("scenario runner does not build the sequence-prediction motif")
    if ("test-sequence-prediction" not in makefile or
            "scenario-sequence-prediction" not in makefile or
            "scenario-sequence-prediction-context" not in makefile):
        fail("Makefile targets for C6.3 are missing")
    if "prediction_frame_recorded" in sequence_protocol:
        fail("sequence prediction must decode the complete probe")

    decode_index = sequence_protocol.find("if (!decode_prediction(")
    expected_index = sequence_protocol.rfind("trial.expected_next_pattern =")
    if decode_index < 0 or expected_index < 0 or decode_index > expected_index:
        fail("expected next pattern must be scored after neural decoding")
    forbidden_sequence = (
        r"trial\.predicted_pattern\s*=\s*(?:trial\.)?expected_next_pattern",
        r"trial\.predicted_pattern\s*=\s*(?:trial\.)?sequence_id",
        r"predicted_pattern\s*=\s*sequence_prediction_pattern_id",
        r"transition_table",
        r"inputs\s*\[[^]]+\]\s*=\s*.*expected_next_pattern",
    )
    for expression in forbidden_sequence:
        if re.search(expression, sequence_protocol):
            fail("sequence-prediction protocol bypasses neural dynamics")

    print("C6.3 sequence prediction validation OK")


if __name__ == "__main__":
    main()
