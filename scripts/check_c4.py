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
SCENARIO_ROOT = ROOT / "results" / "scenarios"

ESSENTIAL = (
    "src/structure.c", "src/structure.h",
    "src/structural_plasticity.c", "src/structural_plasticity.h",
    "scripts/plot_topology.py", "scripts/run_benchmarks_c4.py",
    "scripts/check_c4.py", "tests/test_structure.c",
    "tests/test_structural_plasticity.c", "tests/test_structure_resume.c",
    "tests/test_structure_long.c", "tests/test_plot_topology.py",
    "docs/GUIA_DE_TOPOLOGIA_ADAPTATIVA.md",
    "docs/BENCHMARKS_C4_TOPOLOGIA.md",
    "configs/evolution_structure_target_base.ini",
    "configs/evolution_structure_target_demo.ini",
    "configs/structural_pruning_demo.ini",
    "configs/structural_growth_demo.ini",
    "configs/evolution_structure_learning_base.ini",
    "configs/evolution_structure_learning_demo.ini",
)

TARGETS = (
    "test-structure", "test-structural-plasticity",
    "test-structure-resume", "test-structure-long",
    "test-plot-topology", "evolution-structure-demo",
    "structural-pruning-demo", "structural-growth-demo",
    "evolution-structure-learning-demo", "plot-topology",
    "benchmark-c4", "check-c4",
)

EVOLUTION_OUTPUTS = (
    "structures.csv", "structural_events.csv", "best_topology.csv",
    "best_topology_initial.csv", "best_topology_lifetime_final.csv",
    "checkpoint_structure.txt", "generations.csv", "individuals.csv",
    "lineage.csv", "evolution_report.txt", "evolution_report.html",
    "evolution_overview.png",
)

SCENARIO_OUTPUTS = (
    "topology_initial.csv", "topology_final.csv",
    "structural_plasticity_events.csv",
    "structural_plasticity_metrics.csv", "topology_history.csv",
    "topology_report.txt", "topology_report.html", "topology_overview.png",
)


def run(command: list[str], timeout: float = 600.0) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, timeout=timeout, check=False,
    )


def run_checked(errors: list[str], label: str, command: list[str]) -> None:
    try:
        completed = run(command)
    except subprocess.TimeoutExpired:
        errors.append(f"{label} timed out")
        return
    if completed.returncode != 0:
        errors.append(f"{label} failed:\n{completed.stdout}")


def csv_rows(path: Path, errors: list[str]) -> list[dict[str, str]]:
    try:
        with path.open(encoding="utf-8-sig", newline="") as file:
            reader = csv.DictReader(file)
            if not reader.fieldnames:
                errors.append(f"invalid CSV header: {path}")
                return []
            rows = list(reader)
    except (OSError, csv.Error) as error:
        errors.append(f"cannot read CSV {path}: {error}")
        return []
    if not rows:
        errors.append(f"empty CSV: {path}")
    for line, row in enumerate(rows, start=2):
        for key, value in row.items():
            if value is None:
                errors.append(f"incomplete CSV {path.name}:{line}:{key}")
                continue
            lowered = value.strip().lower()
            if lowered in {"nan", "+nan", "-nan", "inf", "+inf", "-inf", "infinity"}:
                errors.append(f"non-finite CSV {path.name}:{line}:{key}")
    return rows


def topology_signature(path: Path) -> tuple[tuple[str, ...], ...]:
    with path.open(encoding="utf-8-sig", newline="") as file:
        rows = csv.DictReader(file)
        return tuple(
            (row.get("connection_key", ""), row.get("source", ""),
             row.get("target", ""), row.get("delay", ""))
            for row in rows
        )


def check_events(
    path: Path,
    expected_operation: str,
    errors: list[str],
) -> None:
    rows = csv_rows(path, errors)
    successful = [
        row for row in rows
        if expected_operation in row.get(
            "operation", row.get("event", row.get("event_type", ""))).lower()
        and row.get("status", "applied").strip().lower() == "applied"
    ]
    if not successful:
        errors.append(f"no successful {expected_operation} event in {path}")


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

    structure = (ROOT / "src" / "structure.c").read_text(encoding="utf-8")
    structural = (ROOT / "src" / "structural_plasticity.c").read_text(encoding="utf-8")
    runner = (ROOT / "app" / "evolution_runner.c").read_text(encoding="utf-8")
    studio = (ROOT / "app" / "minisnn_studio.c").read_text(encoding="utf-8")
    api = (ROOT / "include" / "minisnn.h").read_text(encoding="utf-8")
    for token in (
        "structure_connection_key", "structure_is_legal_pair",
        "structure_apply_network_patch", "structure_genome_crossover",
        "structure_genome_mutate", "structure_genome_has_required_reachability",
        "structure_jaccard_distance", "structure_apply_complexity_penalty",
        "FNV", "plasticity_state_prepare_topology_rebuild",
    ):
        if token.lower() not in structure.lower():
            errors.append(f"structural core contract missing: {token}")
    for token in (
        "structural_plasticity_apply_step", "prune", "growth",
        "activity_score", "grace_period_steps", "growth_seed",
    ):
        if token.lower() not in structural.lower():
            errors.append(f"lifetime structural contract missing: {token}")
    for token in (
        "genome_mode", "checkpoint_structure.txt", "structures.csv",
        "structural_events.csv", "best_topology.csv",
        "best_topology_lifetime_final.csv", "complexity_penalty",
    ):
        if token not in runner:
            errors.append(f"evolution runner C4 contract missing: {token}")
    for token in (
        "minisnn_apply_topology_patch", "minisnn_validate_topology_patch",
        "minisnn_connection_key", "minisnn_get_topology_signature",
    ):
        if token not in api:
            errors.append(f"public C4 API missing: {token}")
    for button in (
        "TOPOLOGIA ADAPTATIVA", "ABRIR TOPOLOGIA",
        "GRAFICO TOPOLOGIA", "ABRIR EVENTOS ESTRUTURAIS",
        "ABRIR MELHOR TOPOLOGIA",
    ):
        if f'"{button}"' not in studio:
            errors.append(f"Studio C4 control missing: {button}")

    if not errors:
        commands = (
            ("structure", ["mingw32-make", "test-structure"]),
            ("structural plasticity", ["mingw32-make", "test-structural-plasticity"]),
            ("exact structural resume", ["mingw32-make", "test-structure-resume"]),
            ("long structural run", ["mingw32-make", "test-structure-long"]),
            ("topology plot", [sys.executable, "tests/test_plot_topology.py"]),
            ("documentation", [sys.executable, "tests/test_docs.py"]),
            ("regression", [sys.executable, "tests/test_regression_baseline.py"]),
            ("C1 regression", [sys.executable, "scripts/check_c1.py"]),
            ("C1.5 regression", [sys.executable, "scripts/check_c15.py"]),
            ("C2 regression", [sys.executable, "scripts/check_c2.py"]),
            ("C3 regression", [sys.executable, "scripts/check_c3.py"]),
            ("structure target demo", [str(BUILD / "evolution_runner.exe"),
                                       "configs/evolution_structure_target_demo.ini"]),
            ("pruning demo", [str(BUILD / "minisnn_runner.exe"),
                              "configs/structural_pruning_demo.ini"]),
            ("growth demo", [str(BUILD / "minisnn_runner.exe"),
                             "configs/structural_growth_demo.ini"]),
            ("structure learning demo", [str(BUILD / "evolution_runner.exe"),
                                         "configs/evolution_structure_learning_demo.ini"]),
        )
        for label, command in commands:
            run_checked(errors, label, command)

    evolution_demos = (
        "evolution_structure_target_demo",
        "evolution_structure_learning_demo",
    )
    for name in evolution_demos:
        run_dir = EVOLUTION_ROOT / name
        run_checked(errors, f"{name} PNG", [
            sys.executable, "scripts/plot_evolution.py", str(run_dir),
        ])
        run_checked(errors, f"{name} HTML", [
            sys.executable, "scripts/generate_evolution_report.py", str(run_dir),
        ])
        for relative in EVOLUTION_OUTPUTS:
            if not (run_dir / relative).is_file():
                errors.append(f"output missing: {name}/{relative}")
        for filename in ("structures.csv", "structural_events.csv", "generations.csv"):
            if (run_dir / filename).is_file():
                csv_rows(run_dir / filename, errors)

    scenario_demos = (
        ("structural_pruning_demo", "remove"),
        ("structural_growth_demo", "add"),
    )
    for name, operation in scenario_demos:
        run_dir = SCENARIO_ROOT / name
        run_checked(errors, f"{name} PNG", [
            sys.executable, "scripts/plot_topology.py", str(run_dir),
        ])
        for relative in SCENARIO_OUTPUTS:
            if not (run_dir / relative).is_file():
                errors.append(f"output missing: {name}/{relative}")
        events = run_dir / "structural_plasticity_events.csv"
        if events.is_file():
            check_events(events, operation, errors)

    learning = EVOLUTION_ROOT / "evolution_structure_learning_demo"
    try:
        inherited = topology_signature(learning / "best_topology_initial.csv")
        lifetime = topology_signature(learning / "best_topology_lifetime_final.csv")
        if inherited == lifetime:
            errors.append("lifetime learning demo did not change phenotype topology")
    except OSError as error:
        errors.append(f"cannot compare lifetime topology: {error}")

    target_generations = EVOLUTION_ROOT / "evolution_structure_target_demo" / "generations.csv"
    if target_generations.is_file():
        rows = csv_rows(target_generations, errors)
        if rows:
            initial = float(rows[0]["fitness_best"])
            final = float(rows[-1]["global_best_fitness"])
            if not math.isfinite(initial) or not math.isfinite(final) or final <= initial:
                errors.append("structure target demo did not improve over generation zero")

    documentation = "\n".join(
        path.read_text(encoding="utf-8")
        for path in (ROOT / "docs").glob("*.md")
    )
    for token in (
        "connection key", "genoma estrutural", "plasticidade estrutural",
        "distância de Jaccard", "herança darwiniana", "NEAT",
        "C4 — topologia adaptativa e evolução estrutural (concluído)",
    ):
        if token.lower() not in documentation.lower():
            errors.append(f"C4 documentation contract missing: {token}")

    generated = {".csv", ".png", ".html", ".exe", ".o", ".obj"}
    for item in ROOT.iterdir():
        if item.is_file() and item.suffix.lower() in generated:
            errors.append(f"generated artifact in repository root: {item.name}")

    print("Studio manual validation: PENDING (C4 interaction checklist not automated)")
    if errors:
        print("C4 validation FAILED")
        for error in sorted(set(errors)):
            print(f"- {error}")
        return 1
    print("C4 automated validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
