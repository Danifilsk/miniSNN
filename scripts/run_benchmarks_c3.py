from __future__ import annotations

from configparser import ConfigParser
from pathlib import Path
import csv
import os
import platform
import shutil
import statistics
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
RESULTS = ROOT / "results" / "benchmarks"
EVOLUTION_ROOT = ROOT / "results" / "evolution"
SCENARIO_ROOT = ROOT / "results" / "scenarios"
EVOLUTION_RUNNER = BUILD / "evolution_runner.exe"
SCENARIO_RUNNER = BUILD / "minisnn_runner.exe"

MODES = (
    ("population_no_plasticity", "weight", 1, True),
    ("population_stdp", "plasticity", 1, True),
    ("population_rstdp", "reward", 1, True),
    ("population_homeostasis", "homeostasis", 1, True),
    ("multiple_replicates", "weight", 3, True),
    ("save_all_genomes_on", "weight", 1, True),
    ("save_all_genomes_off", "weight", 1, False),
)


def execute(command: list[str], timeout: float = 300.0) -> tuple[float, str]:
    started = time.perf_counter()
    completed = subprocess.run(
        command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, timeout=timeout, check=False,
    )
    elapsed = time.perf_counter() - started
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}\n"
            f"{completed.stdout}"
        )
    return elapsed, completed.stdout


def set_values(parser: ConfigParser, section: str, values: dict[str, object]) -> None:
    if not parser.has_section(section):
        parser.add_section(section)
    for key, value in values.items():
        parser.set(section, key, str(value).lower() if isinstance(value, bool) else str(value))


def write_base(kind: str, path: Path, run_name: str) -> None:
    sources = {
        "weight": "evolution_weight_target_base.ini",
        "plasticity": "evolution_plasticity_base.ini",
        "reward": "reward_positive_demo.ini",
        "homeostasis": "evolution_homeostasis_base.ini",
    }
    parser = ConfigParser(interpolation=None)
    parser.read(ROOT / "configs" / sources[kind], encoding="utf-8")
    set_values(parser, "run", {"run_name": run_name})
    set_values(parser, "simulation", {"steps": 180})
    set_values(parser, "output", {"auto_unique_run": False, "history_enabled": False})
    set_values(parser, "diagnostics", {"level": "off"})
    if parser.has_section("reward_events"):
        parser.remove_section("reward_events")
        parser.add_section("reward_events")
        parser.set("reward_events", "event_0", "90,1.0")
    with path.open("w", encoding="utf-8") as file:
        parser.write(file)


def write_evolution(
    path: Path,
    experiment_name: str,
    base_path: Path,
    kind: str,
    replicates: int,
    save_all: bool,
) -> None:
    parser = ConfigParser(interpolation=None)
    parser.add_section("evolution")
    set_values(parser, "evolution", {
        "enabled": True, "experiment_name": experiment_name,
        "base_scenario": base_path.relative_to(ROOT).as_posix(),
        "population_size": 6, "generations": 3, "elite_count": 1,
        "selection": "tournament", "tournament_size": 2,
        "crossover": "uniform", "crossover_rate": 0.8,
        "mutation": "uniform_delta", "mutation_rate": 0.35,
        "mutation_scale": 0.15, "initialization": "baseline_plus_mutation",
        "initialization_scale": 0.25, "evolution_seed": 73001,
        "evaluation_replicates": replicates, "evaluation_seed_base": 74000,
        "replicate_std_penalty": 0.1, "checkpoint_interval_generations": 1,
        "save_all_genomes": save_all, "save_best_run": True,
        "auto_unique_run": False, "history_enabled": False,
    })
    parser.add_section("genome")
    set_values(parser, "genome", {
        "evolve_exc_weights": kind != "homeostasis",
        "exc_weight_min": 0.0, "exc_weight_max": 500.0,
        "evolve_inh_magnitudes": False,
        "inh_magnitude_min": 0.0, "inh_magnitude_max": 500.0,
    })
    if kind == "plasticity":
        parser.set("genome", "scalar_gene_0", "plasticity.a_plus,0.1,3.0,0.1")
    elif kind == "reward":
        parser.set("genome", "scalar_gene_0", "reward.learning_rate,0.1,2.0,0.1")
    elif kind == "homeostasis":
        parser.set("genome", "scalar_gene_0", "homeostasis.target_rate,0.01,0.2,0.05")
    parser.add_section("fitness")
    parser.set("fitness", "term_0", "activity_total_spikes,target,10.0,10.0,1.0")
    with path.open("w", encoding="utf-8") as file:
        parser.write(file)


def directory_size(path: Path) -> int:
    return sum(item.stat().st_size for item in path.rglob("*") if item.is_file())


def csv_size(path: Path) -> int:
    return sum(item.stat().st_size for item in path.glob("*.csv") if item.is_file())


def read_manifest(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    return values


def first_line(command: list[str]) -> str:
    completed = subprocess.run(
        command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, check=False,
    )
    return completed.stdout.splitlines()[0] if completed.stdout else "unknown"


def scenario_only(temporary: list[Path]) -> dict[str, object]:
    config = BUILD / "benchmark_c3_scenario_only.ini"
    run_name = "benchmark_c3_scenario_only"
    run_dir = SCENARIO_ROOT / run_name
    temporary.extend((config, run_dir))
    write_base("weight", config, run_name)
    shutil.rmtree(run_dir, ignore_errors=True)
    wall, _ = execute([str(SCENARIO_RUNNER), str(config)])
    return {
        "mode": "isolated_scenario", "population_size": 1, "generations": 1,
        "replicates": 1, "gene_count": 0, "evaluations": 1,
        "neural_steps": 180, "wall_seconds": wall,
        "individuals_per_second": 1.0 / wall,
        "evaluations_per_second": 1.0 / wall,
        "neural_steps_per_second": 180.0 / wall,
        "seconds_per_generation": wall, "serialization_bytes": csv_size(run_dir),
        "total_output_bytes": directory_size(run_dir), "checkpoint_seconds": "NA",
        "resume_seconds": "NA", "png_seconds": "NA", "html_seconds": "NA",
    }


def main() -> int:
    if not EVOLUTION_RUNNER.is_file() or not SCENARIO_RUNNER.is_file():
        print("FAIL: compile evolution_runner.exe and minisnn_runner.exe first")
        return 1
    BUILD.mkdir(exist_ok=True)
    RESULTS.mkdir(parents=True, exist_ok=True)
    EVOLUTION_ROOT.mkdir(parents=True, exist_ok=True)
    rows: list[dict[str, object]] = []
    temporary: list[Path] = []
    last_path = EVOLUTION_ROOT / "last_experiment.txt"
    previous_last = last_path.read_bytes() if last_path.is_file() else None
    try:
        rows.append(scenario_only(temporary))
        for mode, kind, replicates, save_all in MODES:
            experiment_name = f"benchmark_c3_{mode}"
            base_path = BUILD / f"{experiment_name}_base.ini"
            config_path = BUILD / f"{experiment_name}.ini"
            run_dir = EVOLUTION_ROOT / experiment_name
            temporary.extend((base_path, config_path, run_dir))
            write_base(kind, base_path, experiment_name + "_base")
            write_evolution(config_path, experiment_name, base_path, kind,
                            replicates, save_all)
            shutil.rmtree(run_dir, ignore_errors=True)
            checkpoint_seconds: float | str = "NA"
            resume_seconds: float | str = "NA"
            if mode == "multiple_replicates":
                checkpoint_seconds, _ = execute([
                    str(EVOLUTION_RUNNER), str(config_path), "--stop-after", "1",
                ])
                resume_seconds, _ = execute([
                    str(EVOLUTION_RUNNER), "--resume", str(run_dir),
                ])
                wall = checkpoint_seconds + resume_seconds
            else:
                wall, _ = execute([str(EVOLUTION_RUNNER), str(config_path)])

            manifest = read_manifest(run_dir / "evolution_manifest.txt")
            evaluations = 6 * 3 * replicates
            neural_steps = evaluations * 180
            png_seconds: float | str = "NA"
            html_seconds: float | str = "NA"
            if mode == "save_all_genomes_on":
                png_seconds, _ = execute([
                    sys.executable, "scripts/plot_evolution.py", str(run_dir),
                ])
                html_seconds, _ = execute([
                    sys.executable, "scripts/generate_evolution_report.py", str(run_dir),
                ])
            rows.append({
                "mode": mode, "population_size": 6, "generations": 3,
                "replicates": replicates,
                "gene_count": int(manifest.get("gene_count", "0")),
                "evaluations": evaluations, "neural_steps": neural_steps,
                "wall_seconds": wall, "individuals_per_second": 18.0 / wall,
                "evaluations_per_second": evaluations / wall,
                "neural_steps_per_second": neural_steps / wall,
                "seconds_per_generation": wall / 3.0,
                "serialization_bytes": csv_size(run_dir),
                "total_output_bytes": directory_size(run_dir),
                "checkpoint_seconds": checkpoint_seconds,
                "resume_seconds": resume_seconds, "png_seconds": png_seconds,
                "html_seconds": html_seconds,
            })

        output = RESULTS / "evolution_c3.csv"
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
            "configuration=population 6, generations 3, steps 180\n"
            "execution=serial deterministic C3 benchmark\n"
            "threshold=none; results are local observations\n"
            "checkpoint_seconds=wall time through first generation, not isolated I/O\n"
        )
        (RESULTS / "environment_c3.txt").write_text(environment, encoding="utf-8")
    except (OSError, RuntimeError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: C3 benchmark aborted: {error}")
        return 1
    finally:
        for path in temporary:
            if path.is_dir():
                shutil.rmtree(path, ignore_errors=True)
            else:
                path.unlink(missing_ok=True)
        if previous_last is None:
            last_path.unlink(missing_ok=True)
        else:
            last_path.write_bytes(previous_last)

    print("=== C3 local neuroevolution benchmark ===")
    print("mode | wall_s | eval/s | neural_steps/s | csv_bytes | png_s | html_s")
    for row in rows:
        print(
            f"{row['mode']:26s} | {float(row['wall_seconds']):.6f} | "
            f"{float(row['evaluations_per_second']):.2f} | "
            f"{float(row['neural_steps_per_second']):.0f} | "
            f"{row['serialization_bytes']} | {row['png_seconds']} | {row['html_seconds']}"
        )
    print(f"Benchmark results: {RESULTS}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
