from __future__ import annotations

from configparser import ConfigParser
from pathlib import Path
import csv
import math
import re
import shutil
import subprocess
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = PROJECT_ROOT / "build"
RUNNER = BUILD_DIR / "minisnn_runner.exe"
RESULT_ROOT = PROJECT_ROOT / "results" / "scenarios"

ESSENTIAL_FILES = (
    "src/plasticity.c",
    "src/plasticity.h",
    "include/minisnn.h",
    "include/minisnn_types.h",
    "configs/stdp_ltp_demo.ini",
    "configs/stdp_ltd_demo.ini",
    "configs/stdp_mixed_demo.ini",
    "tests/test_plasticity.c",
    "tests/test_plasticity_long.c",
    "tests/test_plot_plasticity.py",
    "scripts/plot_plasticity.py",
    "docs/GUIA_DE_PLASTICIDADE.md",
    "docs/BENCHMARKS_C1_STDP.md",
)
REQUIRED_TARGETS = (
    "test-plasticity",
    "test-plot-plasticity",
    "test-plasticity-long",
    "scenario-stdp-ltp",
    "scenario-stdp-ltd",
    "scenario-stdp-mixed",
    "plot-stdp-ltp",
    "check-c1",
)
PLASTICITY_KEYS = (
    "enabled",
    "rule",
    "a_plus",
    "a_minus",
    "tau_plus",
    "tau_minus",
    "trace_increment",
    "weight_min",
    "weight_max",
    "record_weights",
    "record_history",
    "record_interval_steps",
    "record_connection_limit",
)
OUTPUT_FILES = (
    "weights_initial.csv",
    "weights_final.csv",
    "weight_history.csv",
    "plasticity_metrics.csv",
    "stdp_report.txt",
)


def fail(errors: list[str], message: str) -> None:
    errors.append(message)


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=PROJECT_ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def one_row(path: Path) -> dict[str, str]:
    with path.open(encoding="utf-8", newline="") as file:
        rows = list(csv.DictReader(file))
    if len(rows) != 1:
        raise ValueError(f"expected one row in {path}")
    return rows[0]


def run_demo(source: Path, suffix: str) -> Path:
    parser = ConfigParser()
    parser.read(source, encoding="utf-8")
    run_name = f"c1_check_{suffix}"
    parser.set("run", "run_name", run_name)
    if not parser.has_section("output"):
        parser.add_section("output")
    parser.set("output", "auto_unique_run", "false")
    config_path = BUILD_DIR / f"{run_name}.ini"
    with config_path.open("w", encoding="utf-8") as file:
        parser.write(file)
    run_dir = RESULT_ROOT / run_name
    shutil.rmtree(run_dir, ignore_errors=True)
    completed = run([str(RUNNER), str(config_path)])
    config_path.unlink(missing_ok=True)
    if completed.returncode != 0:
        raise RuntimeError(f"{source.name} failed:\n{completed.stdout}")
    return run_dir


def validate_demo(run_dir: Path, expected_direction: str, errors: list[str]) -> None:
    for filename in OUTPUT_FILES:
        if not (run_dir / filename).is_file():
            fail(errors, f"missing demo output: {run_dir.name}/{filename}")
    metrics_path = run_dir / "plasticity_metrics.csv"
    if not metrics_path.is_file():
        return
    metrics = one_row(metrics_path)
    numeric_values = []
    for key, value in metrics.items():
        if key in {"plasticity_enabled", "plasticity_rule"}:
            continue
        number = float(value)
        numeric_values.append(number)
        if not math.isfinite(number):
            fail(errors, f"non-finite metric {key} in {run_dir.name}")
    signed_change = float(metrics["plasticity_total_signed_change"])
    if expected_direction == "positive" and signed_change <= 0.0:
        fail(errors, f"LTP demo did not have positive net change: {signed_change}")
    if expected_direction == "negative" and signed_change >= 0.0:
        fail(errors, f"LTD demo did not have negative net change: {signed_change}")
    report = (run_dir / "stdp_report.txt").read_text(encoding="utf-8").lower()
    if re.search(r"(^|[^a-z])(nan|[+-]?inf)([^a-z]|$)", report):
        fail(errors, f"non-finite token in {run_dir.name}/stdp_report.txt")


def validate_mixed_inhibitory_weights(run_dir: Path, errors: list[str]) -> None:
    initial_path = run_dir / "weights_initial.csv"
    final_path = run_dir / "weights_final.csv"
    if not initial_path.is_file() or not final_path.is_file():
        return
    with initial_path.open(encoding="utf-8", newline="") as file:
        initial = {row["connection_id"]: row for row in csv.DictReader(file)}
    with final_path.open(encoding="utf-8", newline="") as file:
        final = {row["connection_id"]: row for row in csv.DictReader(file)}
    inhibitory = 0
    for connection_id, row in initial.items():
        if row["source_type"] != "INH":
            continue
        inhibitory += 1
        if row["eligible"] != "0":
            fail(errors, f"inhibitory connection {connection_id} marked eligible")
        if float(row["weight"]) != float(final[connection_id]["final_weight"]):
            fail(errors, f"inhibitory connection {connection_id} changed")
    if inhibitory == 0:
        fail(errors, "mixed demo has no inhibitory-origin connection")


def main() -> int:
    errors: list[str] = []
    temporary_runs: list[Path] = []
    BUILD_DIR.mkdir(exist_ok=True)
    RESULT_ROOT.mkdir(parents=True, exist_ok=True)

    for relative in ESSENTIAL_FILES:
        if not (PROJECT_ROOT / relative).is_file():
            fail(errors, f"essential file missing: {relative}")

    makefile = (PROJECT_ROOT / "Makefile").read_text(encoding="utf-8")
    targets = set(re.findall(r"^([A-Za-z0-9_.-]+):", makefile, re.MULTILINE))
    for target in REQUIRED_TARGETS:
        if target not in targets:
            fail(errors, f"Makefile target missing: {target}")

    parser_source = (PROJECT_ROOT / "app" / "scenario_config.c").read_text(
        encoding="utf-8"
    )
    for key in PLASTICITY_KEYS:
        if f'{{"{key}",' not in parser_source:
            fail(errors, f"plasticity parser key missing: {key}")

    roadmap = (PROJECT_ROOT / "docs" / "ROADMAP.md")
    if roadmap.is_file():
        roadmap_text = roadmap.read_text(encoding="utf-8")
        if "C1" not in roadmap_text or "implementado" not in roadmap_text.lower():
            fail(errors, "roadmap does not mark C1 as implemented")
        if "C1.5" not in roadmap_text or "homeostase" not in roadmap_text.lower():
            fail(errors, "roadmap does not keep C1.5 homeostasis as future work")

    if not RUNNER.is_file():
        fail(errors, "build/minisnn_runner.exe is missing")
    else:
        baseline = run([sys.executable, "tests/test_regression_baseline.py"])
        if baseline.returncode != 0:
            fail(errors, f"v0.2 regression failed:\n{baseline.stdout}")
        try:
            for name, direction in (
                ("stdp_ltp_demo.ini", "positive"),
                ("stdp_ltd_demo.ini", "negative"),
                ("stdp_mixed_demo.ini", "mixed"),
            ):
                run_dir = run_demo(PROJECT_ROOT / "configs" / name, name[:-4])
                temporary_runs.append(run_dir)
                validate_demo(run_dir, direction, errors)
                if direction == "mixed":
                    validate_mixed_inhibitory_weights(run_dir, errors)
        except (OSError, RuntimeError, ValueError, KeyError) as error:
            fail(errors, f"demo validation failed: {error}")
        finally:
            for run_dir in temporary_runs:
                shutil.rmtree(run_dir, ignore_errors=True)

    generated_suffixes = {".csv", ".png", ".exe", ".o", ".obj"}
    for item in PROJECT_ROOT.iterdir():
        if item.is_file() and item.suffix.lower() in generated_suffixes:
            fail(errors, f"generated artifact in repository root: {item.name}")

    print("Studio manual validation: PENDING (not automated by check-c1)")
    if errors:
        print("C1 validation FAILED")
        for error in errors:
            print(f"- {error}")
        return 1
    print("C1 automated validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
