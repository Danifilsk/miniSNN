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
    "src/reward.c",
    "src/reward.h",
    "include/minisnn.h",
    "include/minisnn_types.h",
    "scripts/plot_reward.py",
    "scripts/run_benchmarks_c2.py",
    "scripts/check_c2.py",
    "scripts/generate_history_report.py",
    "tests/test_reward.c",
    "tests/test_reward_long.c",
    "tests/test_plot_reward.py",
    "tests/test_history_report.py",
    "docs/GUIA_DE_RECOMPENSA.md",
    "docs/BENCHMARKS_C2_RECOMPENSA.md",
    "configs/reward_positive_demo.ini",
    "configs/punishment_negative_demo.ini",
    "configs/reward_delayed_demo.ini",
    "configs/reward_mixed_demo.ini",
)
TARGETS = (
    "test-reward",
    "test-reward-long",
    "test-plot-reward",
    "scenario-reward-positive",
    "scenario-punishment-negative",
    "scenario-reward-delayed",
    "scenario-reward-mixed",
    "plot-reward",
    "benchmark-c2",
    "check-c2",
    "test-history-report",
    "report-history",
)
API_SYMBOLS = (
    "minisnn_default_reward_config",
    "minisnn_set_reward_config",
    "minisnn_get_reward_config",
    "minisnn_queue_reward",
    "minisnn_get_pending_reward",
    "minisnn_clear_pending_reward",
    "minisnn_get_last_applied_reward",
    "minisnn_reset_reward_learning",
    "minisnn_get_reward_stats",
    "minisnn_get_connection_eligibility",
    "minisnn_get_reward_connection_stats",
    "minisnn_reward_eligible_connection_count",
)
OUTPUTS = (
    "reward_metrics.csv",
    "reward_events.csv",
    "reward_history.csv",
    "eligibility_history.csv",
    "reward_connections.csv",
    "reward_report.txt",
    "reward_report.html",
)


def run(command: list[str], timeout: float = 240.0) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout,
        check=False,
    )


def add_error(errors: list[str], label: str, completed: subprocess.CompletedProcess[str]) -> None:
    if completed.returncode != 0:
        errors.append(f"{label} failed:\n{completed.stdout}")


def make_temporary_run(source_name: str, suffix: str) -> tuple[Path, Path]:
    parser = ConfigParser()
    parser.read(ROOT / "configs" / source_name, encoding="utf-8")
    run_name = f"c2_check_{suffix}"
    parser.set("run", "run_name", run_name)
    if not parser.has_section("output"):
        parser.add_section("output")
    parser.set("output", "auto_unique_run", "false")
    parser.set("output", "history_enabled", "false")
    config_path = BUILD / f"{run_name}.ini"
    with config_path.open("w", encoding="utf-8") as file:
        parser.write(file)
    run_dir = RUN_ROOT / run_name
    shutil.rmtree(run_dir, ignore_errors=True)
    completed = run([str(RUNNER), str(config_path)])
    if completed.returncode != 0:
        raise RuntimeError(f"{source_name} failed:\n{completed.stdout}")
    return config_path, run_dir


def read_metrics(path: Path) -> dict[str, str]:
    with path.open(encoding="utf-8", newline="") as file:
        rows = list(csv.DictReader(file))
    if len(rows) != 1:
        raise ValueError(f"expected one metrics row in {path}")
    return rows[0]


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
    temporary_paths: list[Path] = []
    BUILD.mkdir(exist_ok=True)
    RUN_ROOT.mkdir(parents=True, exist_ok=True)

    for relative in ESSENTIAL:
        if not (ROOT / relative).is_file():
            errors.append(f"essential file missing: {relative}")

    makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
    make_targets = set(re.findall(r"^([A-Za-z0-9_.-]+):", makefile, re.MULTILINE))
    for target in TARGETS:
        if target not in make_targets:
            errors.append(f"Makefile target missing: {target}")

    api = (ROOT / "include" / "minisnn.h").read_text(encoding="utf-8")
    for symbol in API_SYMBOLS:
        if symbol not in api:
            errors.append(f"public API symbol missing: {symbol}")

    parser_source = (ROOT / "app" / "scenario_config.c").read_text(encoding="utf-8")
    for token in (
        "learning_mode",
        "reward_modulated_stdp",
        "learning_rate",
        "eligibility_tau",
        "reward_min",
        "clip_reward",
        "reward_events",
    ):
        if token not in parser_source:
            errors.append(f"scenario parser token missing: {token}")

    studio = (ROOT / "app" / "minisnn_studio.c").read_text(encoding="utf-8")
    for button in ("RECOMPENSA", "GRAFICO RECOMPENSA", "ABRIR RECOMPENSA"):
        if f'"{button}"' not in studio:
            errors.append(f"Studio button missing: {button}")

    roadmap = (ROOT / "docs" / "ROADMAP.md").read_text(encoding="utf-8")
    if "C2 — recompensa e punição (concluído)" not in roadmap:
        errors.append("roadmap does not mark C2 as concluded")
    if "C3 — neuroevolução (próximo; não implementado)" not in roadmap:
        errors.append("roadmap does not keep C3 as next")

    if not RUNNER.is_file():
        errors.append("build/minisnn_runner.exe is missing")
    else:
        add_error(errors, "reward mathematical tests", run(["mingw32-make", "test-reward"]))
        add_error(errors, "runner and distributed reward sample", run(["mingw32-make", "test-runner"]))
        add_error(errors, "history HTML report", run([sys.executable, "tests/test_history_report.py"]))
        add_error(errors, "v0.2/v0.3/v0.4 regression", run([sys.executable, "tests/test_regression_baseline.py"]))
        add_error(errors, "C1 regression", run([sys.executable, "scripts/check_c1.py"]))
        add_error(errors, "C1.5 regression", run([sys.executable, "scripts/check_c15.py"]))

        metrics_by_name: dict[str, dict[str, str]] = {}
        try:
            for source, suffix in (
                ("reward_positive_demo.ini", "positive"),
                ("punishment_negative_demo.ini", "negative"),
                ("reward_delayed_demo.ini", "delayed"),
            ):
                config_path, run_dir = make_temporary_run(source, suffix)
                temporary_paths.extend((config_path, run_dir))
                for filename in OUTPUTS[:-1]:
                    if not (run_dir / filename).is_file():
                        errors.append(f"missing output {run_dir.name}/{filename}")
                for filename in OUTPUTS[:5]:
                    if (run_dir / filename).is_file():
                        validate_csv(run_dir / filename, errors)
                report = run([
                    sys.executable,
                    "scripts/generate_run_reports.py",
                    str(run_dir),
                    "--reward",
                ])
                add_error(errors, f"HTML report for {suffix}", report)
                if not (run_dir / "reward_report.html").is_file():
                    errors.append(f"missing output {run_dir.name}/reward_report.html")
                metrics_by_name[suffix] = read_metrics(run_dir / "reward_metrics.csv")

            positive_dir = RUN_ROOT / "c2_check_positive"
            plot = run([sys.executable, "scripts/plot_reward.py", str(positive_dir)])
            add_error(errors, "reward plot", plot)
            if not (positive_dir / "reward_overview.png").is_file():
                errors.append("reward_overview.png was not generated")

            positive = float(metrics_by_name["positive"]["reward_weight_total_signed_change"])
            negative = float(metrics_by_name["negative"]["reward_weight_total_signed_change"])
            delayed = float(metrics_by_name["delayed"]["reward_weight_total_signed_change"])
            if positive <= 0.0:
                errors.append("positive demo did not increase eligible weight")
            if negative >= 0.0:
                errors.append("punishment demo did not decrease eligible weight")
            if not (0.0 < delayed < positive):
                errors.append("delayed reward did not preserve a smaller positive effect")
        except (OSError, RuntimeError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
            errors.append(f"reward demo validation failed: {error}")
        finally:
            for path in temporary_paths:
                if path.is_dir():
                    shutil.rmtree(path, ignore_errors=True)
                else:
                    path.unlink(missing_ok=True)

    generated = {".csv", ".png", ".html", ".exe", ".o", ".obj"}
    for item in ROOT.iterdir():
        if item.is_file() and item.suffix.lower() in generated:
            errors.append(f"generated artifact in repository root: {item.name}")

    print("Studio manual validation: PENDING (34 interaction checks are not automated)")
    if errors:
        print("C2 validation FAILED")
        for error in sorted(set(errors)):
            print(f"- {error}")
        return 1
    print("C2 automated validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
