from __future__ import annotations

from pathlib import Path
import csv
import math
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
EVOLUTION_ROOT = ROOT / "results" / "evolution"

ESSENTIAL = (
    "src/evolution.c", "src/evolution.h",
    "app/evolution_config.c", "app/evolution_config.h",
    "app/evolution_runner.c", "app/evolution_runner.h",
    "app/scenario_runtime.c", "app/scenario_runtime.h",
    "scripts/plot_evolution.py", "scripts/generate_evolution_report.py",
    "scripts/run_benchmarks_c3.py", "scripts/check_c3.py",
    "tests/test_evolution.c", "tests/test_evolution_runner.c",
    "tests/test_evolution_long.c", "tests/test_plot_evolution.py",
    "tests/test_evolution_report.py", "docs/GUIA_DE_NEUROEVOLUCAO.md",
    "docs/BENCHMARKS_C3_NEUROEVOLUCAO.md",
    "configs/evolution_weight_target_base.ini",
    "configs/evolution_weight_target_demo.ini",
    "configs/evolution_homeostasis_base.ini",
    "configs/evolution_homeostasis_demo.ini",
    "configs/evolution_plasticity_base.ini",
    "configs/evolution_plasticity_demo.ini",
    "results/evolution/.gitkeep",
)
TARGETS = (
    "evolution-build", "test-evolution", "test-evolution-runner",
    "test-evolution-resume", "test-evolution-long", "test-plot-evolution",
    "test-evolution-report", "evolution-weight-demo",
    "evolution-homeostasis-demo", "evolution-plasticity-demo",
    "plot-evolution", "report-evolution", "report-evolution-history",
    "benchmark-c3", "check-c3",
)
RUN_OUTPUTS = (
    "evolution_config_used.ini", "base_scenario_used.ini",
    "evolution_manifest.txt", "generations.csv", "individuals.csv",
    "replicates.csv", "fitness_terms.csv", "genomes.csv", "lineage.csv",
    "best_genome.csv", "best_network_initial.csv", "checkpoint.txt",
    "evolution_report.txt", "evolution_report.html", "evolution_overview.png",
    "best_run/summary.txt", "best_run/population.csv", "best_run/raster.csv",
    "best_run/metrics.csv", "best_run/metrics_report.html",
    "best_run/weights_initial.csv", "best_run/weights_final.csv",
)


def run(command: list[str], timeout: float = 300.0) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, timeout=timeout, check=False,
    )


def add_failure(
    errors: list[str],
    label: str,
    completed: subprocess.CompletedProcess[str],
) -> None:
    if completed.returncode != 0:
        errors.append(f"{label} failed:\n{completed.stdout}")


def validate_csv(path: Path, errors: list[str]) -> None:
    try:
        with path.open(encoding="utf-8-sig", newline="") as file:
            reader = csv.DictReader(file)
            if not reader.fieldnames:
                errors.append(f"invalid CSV header: {path}")
                return
            rows = list(reader)
    except (OSError, csv.Error) as error:
        errors.append(f"cannot read CSV {path}: {error}")
        return
    if not rows:
        errors.append(f"empty CSV: {path}")
        return
    for line, row in enumerate(rows, start=2):
        for key, value in row.items():
            if value is None:
                errors.append(f"incomplete CSV {path.name}:{line}:{key}")
                continue
            text = value.strip().lower()
            if text in {"nan", "+nan", "-nan", "inf", "+inf", "-inf", "infinity"}:
                errors.append(f"non-finite CSV {path.name}:{line}:{key}")


def read_first_last_fitness(path: Path) -> tuple[float, float]:
    with path.open(encoding="utf-8-sig", newline="") as file:
        rows = list(csv.DictReader(file))
    return float(rows[0]["fitness_best"]), float(rows[-1]["global_best_fitness"])


def weights_differ(initial_path: Path, final_path: Path) -> bool:
    with initial_path.open(encoding="utf-8-sig", newline="") as file:
        initial = {row["connection_id"]: float(row["weight"])
                   for row in csv.DictReader(file)}
    with final_path.open(encoding="utf-8-sig", newline="") as file:
        final = {row["connection_id"]: float(row["weight"])
                 for row in csv.DictReader(file)}
    return initial.keys() == final.keys() and any(
        not math.isclose(initial[key], final[key], rel_tol=0.0, abs_tol=1e-12)
        for key in initial
    )


def main() -> int:
    errors: list[str] = []
    for relative in ESSENTIAL:
        if not (ROOT / relative).is_file():
            errors.append(f"essential file missing: {relative}")

    makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
    targets = set(re.findall(r"^([A-Za-z0-9_.-]+):", makefile, re.MULTILINE))
    for target in TARGETS:
        if target not in targets:
            errors.append(f"Makefile target missing: {target}")

    runner = (ROOT / "app" / "evolution_runner.c").read_text(encoding="utf-8")
    engine = (ROOT / "src" / "evolution.c").read_text(encoding="utf-8")
    studio = (ROOT / "app" / "minisnn_studio.c").read_text(encoding="utf-8")
    for token in (
        "scenario_runner_capture_blueprint", "scenario_runtime_step",
        "evaluation_seed_base", "best_network_initial.csv", "checkpoint.txt",
        "last_experiment.txt", "inheritance=darwinian",
        "lamarckian_inheritance=disabled", "parallel_evaluation=disabled",
    ):
        if token not in runner:
            errors.append(f"evolution runner contract missing: {token}")
    for token in (
        "evolution_prng_seed", "evolution_prng_unit",
        "evolution_engine_tournament_select", "evolution_engine_breed_next_generation",
    ):
        if token.lower() not in engine.lower():
            errors.append(f"evolution engine contract missing: {token}")
    for button in (
        "NEUROEVOLUCAO", "RODAR EVOLUCAO", "RETOMAR EVOLUCAO",
        "ABRIR EVOLUCAO", "ABRIR RELATORIO", "ABRIR GRAFICO",
        "ABRIR MELHOR", "HISTORICO EVOLUTIVO",
    ):
        if f'"{button}"' not in studio:
            errors.append(f"Studio button missing: {button}")
    if "CreateProcessA" not in studio or "STUDIO_EVOLUTION_TIMER_ID" not in studio:
        errors.append("Studio asynchronous evolution process missing")

    if not errors:
        commands = (
            ("evolution numerical", ["mingw32-make", "test-evolution"]),
            ("evolution runner/resume", ["mingw32-make", "test-evolution-runner"]),
            ("evolution long", ["mingw32-make", "test-evolution-long"]),
            ("evolution plot", [sys.executable, "tests/test_plot_evolution.py"]),
            ("evolution HTML", [sys.executable, "tests/test_evolution_report.py"]),
            ("history HTML", [sys.executable, "tests/test_history_report.py"]),
            ("regression", [sys.executable, "tests/test_regression_baseline.py"]),
            ("C1 regression", [sys.executable, "scripts/check_c1.py"]),
            ("C1.5 regression", [sys.executable, "scripts/check_c15.py"]),
            ("C2 regression", [sys.executable, "scripts/check_c2.py"]),
            ("weight demo", [str(BUILD / "evolution_runner.exe"),
                             "configs/evolution_weight_target_demo.ini"]),
            ("homeostasis demo", [str(BUILD / "evolution_runner.exe"),
                                  "configs/evolution_homeostasis_demo.ini"]),
            ("plasticity demo", [str(BUILD / "evolution_runner.exe"),
                                 "configs/evolution_plasticity_demo.ini"]),
        )
        for label, command in commands:
            try:
                add_failure(errors, label, run(command))
            except subprocess.TimeoutExpired:
                errors.append(f"{label} timed out")

    demos = (
        "evolution_weight_target_demo",
        "evolution_homeostasis_demo",
        "evolution_plasticity_demo",
    )
    for demo in demos:
        run_dir = EVOLUTION_ROOT / demo
        if not run_dir.is_dir():
            errors.append(f"demo directory missing: {demo}")
            continue
        for script, arguments in (
            ("scripts/plot_evolution.py", [str(run_dir)]),
            ("scripts/generate_evolution_report.py", [str(run_dir)]),
        ):
            try:
                add_failure(errors, f"{demo} {script}",
                            run([sys.executable, script, *arguments]))
            except subprocess.TimeoutExpired:
                errors.append(f"{demo} {script} timed out")
        for relative in RUN_OUTPUTS:
            if not (run_dir / relative).is_file():
                errors.append(f"output missing: {demo}/{relative}")
        for filename in (
            "generations.csv", "individuals.csv", "replicates.csv",
            "fitness_terms.csv", "genomes.csv", "lineage.csv", "best_genome.csv",
        ):
            if (run_dir / filename).is_file():
                validate_csv(run_dir / filename, errors)

    weight_generations = EVOLUTION_ROOT / demos[0] / "generations.csv"
    if weight_generations.is_file():
        initial, final = read_first_last_fitness(weight_generations)
        if not final > initial:
            errors.append("weight demo did not improve over generation zero")
    plasticity_best = EVOLUTION_ROOT / demos[2] / "best_run"
    if (plasticity_best.is_dir() and not weights_differ(
            plasticity_best / "weights_initial.csv",
            plasticity_best / "weights_final.csv")):
        errors.append("plasticity best run did not distinguish initial/final weights")

    try:
        add_failure(errors, "evolution history", run([
            sys.executable, "scripts/generate_evolution_report.py",
            "results/evolution", "--history",
        ]))
    except subprocess.TimeoutExpired:
        errors.append("evolution history timed out")
    if not (EVOLUTION_ROOT / "history.html").is_file():
        errors.append("evolution history.html missing")

    roadmap = (ROOT / "docs" / "ROADMAP.md").read_text(encoding="utf-8")
    if (
        "C3 — neuroevolução (concluído)" not in roadmap or
        "C4 — topologia adaptativa e evolução estrutural (concluído)" not in roadmap or
        "C5 — próximo: modelos neurais avançados" not in roadmap
    ):
        errors.append("roadmap C3/C4/C5 status is incorrect")

    generated = {".csv", ".png", ".html", ".exe", ".o", ".obj"}
    for item in ROOT.iterdir():
        if item.is_file() and item.suffix.lower() in generated:
            errors.append(f"generated artifact in repository root: {item.name}")

    print("Studio manual validation: PENDING (C3 interaction checklist not automated)")
    if errors:
        print("C3 validation FAILED")
        for error in sorted(set(errors)):
            print(f"- {error}")
        return 1
    print("C3 automated validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
