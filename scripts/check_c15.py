from __future__ import annotations

from configparser import ConfigParser
from pathlib import Path
import csv
import math
import re
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
RUNNER = BUILD / "minisnn_runner.exe"
RUN_ROOT = ROOT / "results" / "scenarios"
ESSENTIAL = (
    "src/homeostasis.c",
    "src/homeostasis.h",
    "include/minisnn.h",
    "include/minisnn_types.h",
    "scripts/plot_homeostasis.py",
    "scripts/run_benchmarks_c15.py",
    "scripts/check_c15.py",
    "tests/test_homeostasis.c",
    "tests/test_homeostasis_runner.c",
    "tests/test_homeostasis_long.c",
    "tests/test_plot_homeostasis.py",
    "docs/GUIA_DE_HOMEOSTASE.md",
    "docs/BENCHMARKS_C15_HOMEOSTASE.md",
    "configs/homeostasis_silence_recovery_demo.ini",
    "configs/homeostasis_explosion_control_demo.ini",
    "configs/homeostasis_stdp_scaling_demo.ini",
)
TARGETS = (
    "test-homeostasis",
    "test-plot-homeostasis",
    "test-homeostasis-long",
    "scenario-homeostasis-silence",
    "scenario-homeostasis-explosion",
    "scenario-homeostasis-stdp",
    "plot-homeostasis",
    "benchmark-c15",
    "check-c15",
)
KEYS = (
    "enabled",
    "intrinsic_enabled",
    "target_rate",
    "rate_tau",
    "update_interval_steps",
    "threshold_eta",
    "threshold_min",
    "threshold_max",
    "synaptic_scaling_enabled",
    "scaling_target_mode",
    "scaling_eta",
    "scaling_min_factor",
    "scaling_max_factor",
    "scaling_weight_min",
    "scaling_weight_max",
    "inhibitory_gain_enabled",
    "inhibitory_gain_initial",
    "inhibitory_gain_eta",
    "inhibitory_gain_min",
    "inhibitory_gain_max",
    "record_history",
    "record_interval_steps",
    "record_neuron_limit",
)
OUTPUTS = (
    "homeostasis_metrics.csv",
    "homeostasis_history.csv",
    "threshold_history.csv",
    "homeostasis_neurons.csv",
    "homeostasis_report.txt",
    "homeostasis_report.html",
)


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def temporary_demo(source: Path, suffix: str) -> Path:
    parser = ConfigParser()
    parser.read(source, encoding="utf-8")
    run_name = f"c15_check_{suffix}"
    parser.set("run", "run_name", run_name)
    if not parser.has_section("output"):
        parser.add_section("output")
    parser.set("output", "auto_unique_run", "false")
    config = BUILD / f"{run_name}.ini"
    with config.open("w", encoding="utf-8") as file:
        parser.write(file)
    run_dir = RUN_ROOT / run_name
    shutil.rmtree(run_dir, ignore_errors=True)
    completed = run([str(RUNNER), str(config)])
    config.unlink(missing_ok=True)
    if completed.returncode != 0:
        raise RuntimeError(f"{source.name} failed:\n{completed.stdout}")
    return run_dir


def validate_csv(path: Path, errors: list[str]) -> None:
    with path.open(encoding="utf-8", newline="") as file:
        rows = list(csv.DictReader(file))
    if not rows:
        errors.append(f"empty CSV: {path.name}")
        return
    for row in rows:
        for key, value in row.items():
            if value is None or value == "":
                errors.append(f"empty value {key} in {path.name}")
                continue
            try:
                number = float(value)
            except ValueError:
                continue
            if not math.isfinite(number):
                errors.append(f"non-finite value {key} in {path.name}")


def main() -> int:
    errors: list[str] = []
    temporary_runs: list[Path] = []
    BUILD.mkdir(exist_ok=True)
    RUN_ROOT.mkdir(parents=True, exist_ok=True)

    for relative in ESSENTIAL:
        if not (ROOT / relative).is_file():
            errors.append(f"essential file missing: {relative}")

    makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
    targets = set(re.findall(r"^([A-Za-z0-9_.-]+):", makefile, re.MULTILINE))
    for target in TARGETS:
        if target not in targets:
            errors.append(f"Makefile target missing: {target}")

    parser_source = (ROOT / "app" / "scenario_config.c").read_text(encoding="utf-8")
    for key in KEYS:
        if (f'{{"{key}",' not in parser_source and
                f'strcmp(key, "{key}")' not in parser_source):
            errors.append(f"homeostasis parser key missing: {key}")

    api = (ROOT / "include" / "minisnn.h").read_text(encoding="utf-8")
    for symbol in (
        "minisnn_default_homeostasis_config",
        "minisnn_set_homeostasis_config",
        "minisnn_get_homeostasis_config",
        "minisnn_reset_homeostasis",
        "minisnn_get_homeostasis_stats",
        "minisnn_get_neuron_rate_trace",
        "minisnn_get_neuron_effective_threshold",
        "minisnn_get_inhibitory_gain",
    ):
        if symbol not in api:
            errors.append(f"public API symbol missing: {symbol}")

    roadmap = (ROOT / "docs" / "ROADMAP.md").read_text(encoding="utf-8")
    if "C1.5" not in roadmap or "conclu" not in roadmap.lower():
        errors.append("roadmap does not mark C1.5 as concluded")
    if ("C2" not in roadmap or
            ("proxim" not in roadmap.lower() and "próxim" not in roadmap.lower())):
        errors.append("roadmap does not identify C2 as next")

    if not RUNNER.is_file():
        errors.append("build/minisnn_runner.exe is missing")
    else:
        baseline = run([sys.executable, "tests/test_regression_baseline.py"])
        if baseline.returncode != 0:
            errors.append(f"v0.2/v0.3 regression failed:\n{baseline.stdout}")
        try:
            for config_name in (
                "homeostasis_silence_recovery_demo.ini",
                "homeostasis_explosion_control_demo.ini",
                "homeostasis_stdp_scaling_demo.ini",
            ):
                run_dir = temporary_demo(ROOT / "configs" / config_name, config_name[:-4])
                temporary_runs.append(run_dir)
                for filename in OUTPUTS:
                    if not (run_dir / filename).is_file():
                        errors.append(f"missing output {run_dir.name}/{filename}")
                for filename in (
                    "homeostasis_metrics.csv",
                    "homeostasis_history.csv",
                    "threshold_history.csv",
                    "homeostasis_neurons.csv",
                ):
                    if (run_dir / filename).is_file():
                        validate_csv(run_dir / filename, errors)
                html = run_dir / "homeostasis_report.html"
                if html.is_file() and re.search(r"https?://", html.read_text(encoding="utf-8")):
                    errors.append(f"external resource in {run_dir.name}/homeostasis_report.html")
        except (OSError, RuntimeError, ValueError, KeyError) as error:
            errors.append(f"demo validation failed: {error}")
        finally:
            for run_dir in temporary_runs:
                shutil.rmtree(run_dir, ignore_errors=True)

    generated = {".csv", ".png", ".html", ".exe", ".o", ".obj"}
    for item in ROOT.iterdir():
        if item.is_file() and item.suffix.lower() in generated:
            errors.append(f"generated artifact in repository root: {item.name}")

    print("Studio manual validation: PENDING (not automated by check-c15)")
    if errors:
        print("C1.5 validation FAILED")
        for error in sorted(set(errors)):
            print(f"- {error}")
        return 1
    print("C1.5 automated validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
