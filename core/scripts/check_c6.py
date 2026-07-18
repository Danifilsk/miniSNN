from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parent.parent


def fail(message: str) -> None:
    print(f"C6.1 check failed: {message}", file=sys.stderr)
    raise SystemExit(1)


def require(path: Path) -> str:
    if not path.is_file():
        fail(f"missing required file: {path.relative_to(ROOT)}")
    return path.read_text(encoding="utf-8")


def main() -> None:
    config = require(ROOT / "app" / "scenario_config.c")
    protocol = require(ROOT / "app" / "working_memory.c")
    runner = require(ROOT / "app" / "scenario_runner.c")
    makefile = require(ROOT / "Makefile")
    require(ROOT / "configs" / "working_memory_demo.ini")
    require(ROOT / "tests" / "test_working_memory.c")

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

    print("C6.1 working memory validation OK")


if __name__ == "__main__":
    main()
