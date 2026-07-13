from __future__ import annotations

from pathlib import Path
import argparse
import csv
import os
import platform
import shutil
import statistics
import subprocess
import sys
import time


PROJECT_ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = PROJECT_ROOT / "build"
RESULT_DIR = PROJECT_ROOT / "results" / "benchmarks"
RUNNER = BUILD_DIR / "minisnn_runner.exe"
MODES = (
    ("stdp_off", False, False),
    ("stdp_on_no_history", True, False),
    ("stdp_on_with_history", True, True),
)


def execute(command: list[str], timeout: float) -> tuple[float, str]:
    started = time.perf_counter()
    completed = subprocess.run(
        command,
        cwd=PROJECT_ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout,
        check=False,
    )
    elapsed = time.perf_counter() - started
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}\n"
            f"{completed.stdout}"
        )
    return elapsed, completed.stdout


def config_text(run_name: str, enabled: bool, history: bool) -> str:
    return f"""[run]
run_name = {run_name}

[network]
topology = random_balanced
neurons = 100
inhibitory_fraction = 0.20
connection_probability = 0.05
seed = 23
delay = 1
max_synaptic_delay = 8

[weights]
excitatory_weight = 100.0
inhibitory_weight = -150.0

[input]
source_count = 20
input_current = 20.0

[simulation]
steps = 2000
dt = 0.1
tau = 20.0
v_rest = -65.0
v_reset = -65.0
v_threshold = -50.0
resistance = 1.0
synaptic_decay = 0.95

[recording]
record_neuron = 0

[output]
auto_unique_run = false
history_enabled = false

[diagnostics]
level = basic

[plasticity]
enabled = {'true' if enabled else 'false'}
rule = stdp_pair_trace
a_plus = 0.2
a_minus = 0.21
tau_plus = 20.0
tau_minus = 20.0
trace_increment = 1.0
weight_min = 0.0
weight_max = 200.0
record_weights = {'true' if enabled else 'false'}
record_history = {'true' if history else 'false'}
record_interval_steps = 10
record_connection_limit = 256
"""


def read_single_row(path: Path) -> dict[str, str]:
    with path.open(encoding="utf-8", newline="") as file:
        rows = list(csv.DictReader(file))
    if len(rows) != 1:
        raise RuntimeError(f"expected one data row in {path}")
    return rows[0]


def directory_size(path: Path) -> int:
    return sum(item.stat().st_size for item in path.rglob("*") if item.is_file())


def benchmark_mode(
    mode: str,
    enabled: bool,
    history: bool,
    repeats: int,
    timeout: float,
) -> dict[str, object]:
    run_name = f"benchmark_c1_{mode}"
    config_path = BUILD_DIR / f"{run_name}.ini"
    run_dir = PROJECT_ROOT / "results" / "scenarios" / run_name
    config_path.write_text(config_text(run_name, enabled, history), encoding="utf-8")

    walls: list[float] = []
    simulation_times: list[float] = []
    steps_rates: list[float] = []
    update_rates: list[float] = []
    connections = 0
    eligible = 0
    output_bytes = 0

    for _ in range(repeats):
        shutil.rmtree(run_dir, ignore_errors=True)
        wall, _ = execute([str(RUNNER), str(config_path)], timeout)
        metrics = read_single_row(run_dir / "metrics.csv")
        walls.append(wall)
        simulation_times.append(float(metrics["performance_simulation_time_seconds"]))
        steps_rates.append(float(metrics["performance_steps_per_second"]))
        update_rates.append(float(metrics["performance_neuron_updates_per_second"]))
        connections = int(metrics["network_total_connections"])
        if enabled:
            plasticity = read_single_row(run_dir / "plasticity_metrics.csv")
            eligible = int(plasticity["plasticity_eligible_connection_count"])
        output_bytes = directory_size(run_dir)

    return {
        "mode": mode,
        "repetitions": repeats,
        "neurons": 100,
        "steps": 2000,
        "connections": connections,
        "eligible_connections": eligible,
        "simulation_mean_seconds": statistics.fmean(simulation_times),
        "wall_mean_seconds": statistics.fmean(walls),
        "steps_per_second": statistics.fmean(steps_rates),
        "neuron_updates_per_second": statistics.fmean(update_rates),
        "output_bytes": output_bytes,
        "plot_wall_seconds": "NA",
    }


def compiler_version() -> str:
    completed = subprocess.run(
        ["gcc", "--version"],
        cwd=PROJECT_ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    return completed.stdout.splitlines()[0] if completed.stdout else "unknown"


def main() -> int:
    parser = argparse.ArgumentParser(description="Benchmark local do STDP da miniSNN.")
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--timeout", type=float, default=180.0)
    args = parser.parse_args()
    if args.repeats < 1 or args.timeout <= 0.0:
        print("FAIL: repeats and timeout must be positive")
        return 1
    if not RUNNER.exists():
        print("FAIL: minisnn_runner.exe is missing")
        return 1

    BUILD_DIR.mkdir(exist_ok=True)
    RESULT_DIR.mkdir(parents=True, exist_ok=True)
    rows: list[dict[str, object]] = []
    run_dirs = [
        PROJECT_ROOT / "results" / "scenarios" / f"benchmark_c1_{mode}"
        for mode, _, _ in MODES
    ]
    config_paths = [
        BUILD_DIR / f"benchmark_c1_{mode}.ini"
        for mode, _, _ in MODES
    ]
    try:
        for mode, enabled, history in MODES:
            rows.append(
                benchmark_mode(mode, enabled, history, args.repeats, args.timeout)
            )

        history_dir = run_dirs[-1]
        plot_seconds, _ = execute(
            [sys.executable, "scripts/plot_plasticity.py", str(history_dir)],
            args.timeout,
        )
        rows[-1]["plot_wall_seconds"] = plot_seconds

        output_path = RESULT_DIR / "stdp_c1.csv"
        with output_path.open("w", encoding="utf-8", newline="") as file:
            writer = csv.DictWriter(file, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)

        environment = (
            f"platform={platform.platform()}\n"
            f"processor={platform.processor() or os.environ.get('PROCESSOR_IDENTIFIER', 'unknown')}\n"
            f"python={platform.python_version()}\n"
            f"compiler={compiler_version()}\n"
            f"repetitions={args.repeats}\n"
            "scope=local controlled C1 benchmark; not universal\n"
        )
        (RESULT_DIR / "environment_c1.txt").write_text(environment, encoding="utf-8")
    except (OSError, RuntimeError, subprocess.TimeoutExpired, ValueError) as error:
        print(f"FAIL: C1 benchmark aborted: {error}")
        return 1
    finally:
        for path in run_dirs:
            shutil.rmtree(path, ignore_errors=True)
        for path in config_paths:
            path.unlink(missing_ok=True)

    print("=== C1 STDP local benchmark ===")
    print("mode | simulation_s | steps/s | neuron_updates/s | eligible | output_bytes")
    for row in rows:
        print(
            f"{row['mode']:20s} | {row['simulation_mean_seconds']:.6f} | "
            f"{row['steps_per_second']:.0f} | {row['neuron_updates_per_second']:.0f} | "
            f"{row['eligible_connections']} | {row['output_bytes']}"
        )
    print(f"plot_seconds={rows[-1]['plot_wall_seconds']:.6f}")
    print(f"Benchmark results: {RESULT_DIR}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
