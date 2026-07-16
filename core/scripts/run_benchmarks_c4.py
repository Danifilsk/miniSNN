from __future__ import annotations

from configparser import ConfigParser
from pathlib import Path
import csv
import os
import platform
import shutil
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
RESULTS = ROOT / "results" / "benchmarks"
SCENARIO_ROOT = ROOT / "results" / "scenarios"
EVOLUTION_ROOT = ROOT / "results" / "evolution"
SCENARIO_RUNNER = BUILD / "minisnn_runner.exe"
EVOLUTION_RUNNER = BUILD / "evolution_runner.exe"


def execute(command: list[str], timeout: float = 300.0) -> float:
    started = time.perf_counter()
    completed = subprocess.run(
        command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, check=False, timeout=timeout,
    )
    elapsed = time.perf_counter() - started
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}\n"
            f"{completed.stdout}"
        )
    return elapsed


def set_values(parser: ConfigParser, section: str, values: dict[str, object]) -> None:
    if not parser.has_section(section):
        parser.add_section(section)
    for key, value in values.items():
        parser.set(
            section, key,
            str(value).lower() if isinstance(value, bool) else str(value),
        )


def write_parser(parser: ConfigParser, path: Path) -> None:
    with path.open("w", encoding="utf-8") as file:
        parser.write(file)


def file_bytes(path: Path, pattern: str = "*") -> int:
    return sum(item.stat().st_size for item in path.glob(pattern) if item.is_file())


def first_line(command: list[str]) -> str:
    completed = subprocess.run(
        command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, check=False,
    )
    return completed.stdout.splitlines()[0] if completed.stdout else "unknown"


def prepare_evolution(
    mode: str,
    temporary: list[Path],
    structure_values: dict[str, object],
    base_source: str = "evolution_structure_target_base.ini",
    scenario_values: dict[str, dict[str, object]] | None = None,
) -> tuple[Path, Path, int, int, int]:
    base_name = f"benchmark_c4_{mode}_base"
    experiment_name = f"benchmark_c4_{mode}"
    base_path = BUILD / f"{base_name}.ini"
    config_path = BUILD / f"{experiment_name}.ini"
    run_dir = EVOLUTION_ROOT / experiment_name
    base = ConfigParser(interpolation=None)
    base.read(ROOT / "configs" / base_source, encoding="utf-8")
    set_values(base, "run", {"run_name": base_name})
    set_values(base, "simulation", {"steps": 120})
    set_values(base, "output", {"auto_unique_run": False, "history_enabled": False})
    if scenario_values:
        for section, values in scenario_values.items():
            set_values(base, section, values)
    write_parser(base, base_path)

    config = ConfigParser(interpolation=None)
    config_source = (
        "evolution_weight_target_demo.ini"
        if mode == "c3_fixed_numeric" else
        "evolution_structure_target_demo.ini"
    )
    config.read(
        ROOT / "configs" / config_source,
        encoding="utf-8",
    )
    genome_mode = structure_values.get("genome_mode", "structural_connections")
    set_values(config, "evolution", {
        "experiment_name": experiment_name,
        "base_scenario": base_path.relative_to(ROOT).as_posix(),
        "population_size": 5,
        "generations": 3,
        "elite_count": 1,
        "evaluation_replicates": 1,
        "auto_unique_run": False,
        "history_enabled": False,
        "save_best_run": False,
        "genome_mode": genome_mode,
    })
    set_values(config, "structure", {
        key: value for key, value in structure_values.items()
        if key != "genome_mode"
    })
    write_parser(config, config_path)
    temporary.extend((base_path, config_path, run_dir))
    return config_path, run_dir, 5, 3, 120


def evolution_row(
    mode: str,
    temporary: list[Path],
    structure_values: dict[str, object],
    base_source: str = "evolution_structure_target_base.ini",
    scenario_values: dict[str, dict[str, object]] | None = None,
    generate_artifacts: bool = False,
) -> dict[str, object]:
    config, run_dir, population, generations, steps = prepare_evolution(
        mode, temporary, structure_values, base_source, scenario_values,
    )
    shutil.rmtree(run_dir, ignore_errors=True)
    wall = execute([str(EVOLUTION_RUNNER), str(config)])
    png_seconds: float | str = "NA"
    html_seconds: float | str = "NA"
    if generate_artifacts:
        png_seconds = execute([
            sys.executable, "scripts/plot_evolution.py", str(run_dir),
        ])
        html_seconds = execute([
            sys.executable, "scripts/generate_evolution_report.py", str(run_dir),
        ])
    evaluations = population * generations
    checkpoint = run_dir / "checkpoint_structure.txt"
    return {
        "mode": mode,
        "kind": "evolution",
        "neuron_count": 5,
        "connection_count_initial": 4,
        "population": population,
        "generations": generations,
        "replicates": 1,
        "evaluations": evaluations,
        "neural_steps": evaluations * steps,
        "wall_seconds": wall,
        "evaluations_per_second": evaluations / wall,
        "steps_per_second": evaluations * steps / wall,
        "seconds_per_generation": wall / generations,
        "crossover_seconds": "NA",
        "mutation_seconds": "NA",
        "validation_seconds": "NA",
        "rebuild_seconds": "NA",
        "structural_maintenance_seconds": "NA",
        "checkpoint_bytes": checkpoint.stat().st_size if checkpoint.is_file() else 0,
        "csv_bytes": file_bytes(run_dir, "*.csv"),
        "png_seconds": png_seconds,
        "html_seconds": html_seconds,
    }


def scenario_row(
    mode: str,
    source: str,
    temporary: list[Path],
    overrides: dict[str, dict[str, object]],
    plot: bool = False,
    steps: int = 240,
) -> dict[str, object]:
    run_name = f"benchmark_c4_{mode}"
    config_path = BUILD / f"{run_name}.ini"
    run_dir = SCENARIO_ROOT / run_name
    parser = ConfigParser(interpolation=None)
    parser.read(ROOT / "configs" / source, encoding="utf-8")
    set_values(parser, "run", {"run_name": run_name})
    set_values(parser, "simulation", {"steps": steps})
    set_values(parser, "output", {"auto_unique_run": False, "history_enabled": False})
    for section, values in overrides.items():
        if section == "reward_events":
            parser.remove_section(section)
        set_values(parser, section, values)
    write_parser(parser, config_path)
    temporary.extend((config_path, run_dir))
    shutil.rmtree(run_dir, ignore_errors=True)
    wall = execute([str(SCENARIO_RUNNER), str(config_path)])
    png_seconds: float | str = "NA"
    if plot:
        png_seconds = execute([
            sys.executable, "scripts/plot_topology.py", str(run_dir),
        ])
    return {
        "mode": mode,
        "kind": "scenario",
        "neuron_count": int(parser.get("network", "neurons")),
        "connection_count_initial": "NA",
        "population": 1,
        "generations": 1,
        "replicates": 1,
        "evaluations": 1,
        "neural_steps": steps,
        "wall_seconds": wall,
        "evaluations_per_second": 1.0 / wall,
        "steps_per_second": float(steps) / wall,
        "seconds_per_generation": wall,
        "crossover_seconds": "NA",
        "mutation_seconds": "NA",
        "validation_seconds": "NA",
        "rebuild_seconds": "NA",
        "structural_maintenance_seconds": "NA",
        "checkpoint_bytes": 0,
        "csv_bytes": file_bytes(run_dir, "*.csv"),
        "png_seconds": png_seconds,
        "html_seconds": "NA",
    }


def main() -> int:
    if not SCENARIO_RUNNER.is_file() or not EVOLUTION_RUNNER.is_file():
        print("FAIL: compile minisnn_runner.exe and evolution_runner.exe first")
        return 1
    RESULTS.mkdir(parents=True, exist_ok=True)
    temporary: list[Path] = []
    last_evolution = EVOLUTION_ROOT / "last_experiment.txt"
    last_scenario = SCENARIO_ROOT / "last_run.txt"
    previous_evolution = last_evolution.read_bytes() if last_evolution.is_file() else None
    previous_scenario = last_scenario.read_bytes() if last_scenario.is_file() else None
    rows: list[dict[str, object]] = []
    off = {
        "enabled": False, "genome_mode": "fixed_numeric",
    }
    quiet = {
        "enabled": True, "allow_add": True, "allow_remove": True,
        "allow_rewire": True, "evolve_delays": True,
        "add_rate": 0.0, "remove_rate": 0.0, "rewire_rate": 0.0,
        "delay_mutation_rate": 0.0,
    }
    try:
        rows.append(evolution_row(
            "c3_fixed_numeric", temporary, off,
            "evolution_weight_target_base.ini"))
        rows.append(evolution_row("c4_structural_no_mutations", temporary, quiet))
        rows.append(evolution_row("add_remove", temporary, {
            **quiet, "add_rate": 0.5, "remove_rate": 0.5,
        }))
        rows.append(evolution_row("structural_crossover", temporary, {
            **quiet, "add_rate": 0.2, "remove_rate": 0.2,
        }))
        rows.append(evolution_row("reachability_on", temporary, {
            **quiet, "preserve_required_reachability": True,
            "required_input_neurons": "0", "required_output_neurons": "4",
        }))
        rows.append(scenario_row("structural_plasticity_off", "structural_growth_demo.ini",
            temporary, {"structural_plasticity": {"enabled": False}}))
        rows.append(scenario_row("pruning_on", "structural_pruning_demo.ini",
            temporary, {}, True))
        rows.append(scenario_row("growth_on", "structural_growth_demo.ini",
            temporary, {}, True, 340))
        rows.append(evolution_row("stdp_plus_structure", temporary, {
            **quiet, "add_rate": 0.25,
        }, "evolution_structure_learning_base.ini", generate_artifacts=True))
        rows.append(scenario_row("rstdp_plus_structure", "reward_positive_demo.ini",
            temporary, {"structural_plasticity": {
                "enabled": True, "maintenance_interval_steps": 20,
                "grace_period_steps": 20, "pruning_enabled": False,
                "growth_enabled": True, "growth_candidate_count": 8,
                "growth_score_threshold": 0, "max_growth_per_interval": 1,
                "growth_seed": 7001, "new_exc_weight": 5,
                "new_inh_magnitude": 5, "new_delay": 1,
                "min_connections": 1, "max_connections": 2,
                "record_history": False, "record_interval_steps": 20,
            }, "reward_events": {
                "event_0": "100,1.0", "event_1": "180,1.0",
            }}))
        rows.append(scenario_row("homeostasis_plus_structure",
            "homeostasis_stdp_scaling_demo.ini", temporary,
            {"structural_plasticity": {
                "enabled": True, "maintenance_interval_steps": 20,
                "grace_period_steps": 20, "pruning_enabled": True,
                "prune_weight_threshold": 0.1, "prune_activity_threshold": 0,
                "max_prunes_per_interval": 1, "growth_enabled": False,
                "growth_candidate_count": 8, "growth_score_threshold": 0,
                "max_growth_per_interval": 1, "growth_seed": 7002,
                "new_exc_weight": 5, "new_inh_magnitude": 5,
                "new_delay": 1, "min_connections": 1,
                "max_connections": 2, "record_history": False,
                "record_interval_steps": 20,
            }}))
        rows.append(scenario_row("structural_history_on", "structural_growth_demo.ini",
            temporary, {"structural_plasticity": {
                "record_history": True, "record_interval_steps": 5,
            }}, True, 340))

        output = RESULTS / "topology_c4.csv"
        with output.open("w", encoding="utf-8", newline="") as file:
            writer = csv.DictWriter(file, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)
        environment = (
            f"platform={platform.platform()}\n"
            f"processor={platform.processor() or os.environ.get('PROCESSOR_IDENTIFIER', 'unknown')}\n"
            f"python={platform.python_version()}\n"
            f"compiler={first_line(['gcc', '--version'])}\n"
            f"commit={first_line(['git', 'rev-parse', '--short', 'HEAD'])}\n"
            "configuration=small deterministic local C4 benchmark\n"
            "threshold=none; values are local observations\n"
            "internal_phase_timings=NA because crossover, mutation, validation, rebuild, and maintenance are not separately instrumented\n"
            "complexity_note=structural maintenance runs only at configured intervals, never on every timestep\n"
        )
        (RESULTS / "environment_c4.txt").write_text(environment, encoding="utf-8")
    except (OSError, RuntimeError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: C4 benchmark aborted: {error}")
        return 1
    finally:
        for path in temporary:
            if path.is_dir():
                shutil.rmtree(path, ignore_errors=True)
            else:
                path.unlink(missing_ok=True)
        if previous_evolution is None:
            last_evolution.unlink(missing_ok=True)
        else:
            last_evolution.write_bytes(previous_evolution)
        if previous_scenario is None:
            last_scenario.unlink(missing_ok=True)
        else:
            last_scenario.write_bytes(previous_scenario)

    print("=== C4 local adaptive-topology benchmark ===")
    print("mode | wall_s | eval/s | steps/s | csv_bytes | png_s | html_s")
    for row in rows:
        print(
            f"{str(row['mode']):30s} | {float(row['wall_seconds']):.6f} | "
            f"{float(row['evaluations_per_second']):.2f} | "
            f"{float(row['steps_per_second']):.0f} | {row['csv_bytes']} | "
            f"{row['png_seconds']} | {row['html_seconds']}"
        )
    print(f"Benchmark results: {RESULTS}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
