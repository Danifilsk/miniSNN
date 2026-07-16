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
RUNNER = BUILD / "minisnn_runner.exe"
MODES = (
    ("homeostasis_off", False, False, False, False),
    ("threshold_only", True, True, False, False),
    ("threshold_scaling", True, True, True, False),
    ("threshold_scaling_gain", True, True, True, True),
    ("all_with_history", True, True, True, True),
)


def execute(command: list[str], timeout: float) -> tuple[float, str]:
    started = time.perf_counter()
    completed = subprocess.run(
        command,
        cwd=ROOT,
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


def write_config(
    path: Path,
    run_name: str,
    enabled: bool,
    intrinsic: bool,
    scaling: bool,
    gain: bool,
    history: bool,
) -> None:
    parser = ConfigParser()
    parser.read(ROOT / "configs" / "random.ini", encoding="utf-8")
    parser.set("run", "run_name", run_name)
    parser.set("network", "neurons", "100")
    parser.set("network", "connection_probability", "0.05")
    parser.set("network", "seed", "23")
    parser.set("input", "source_count", "20")
    parser.set("simulation", "steps", "2000")

    if not parser.has_section("output"):
        parser.add_section("output")
    parser.set("output", "auto_unique_run", "false")
    parser.set("output", "history_enabled", "false")

    if not parser.has_section("plasticity"):
        parser.add_section("plasticity")
    for key, value in {
        "enabled": "true",
        "rule": "stdp_pair_trace",
        "a_plus": "0.2",
        "a_minus": "0.21",
        "tau_plus": "20.0",
        "tau_minus": "20.0",
        "trace_increment": "1.0",
        "weight_min": "0.0",
        "weight_max": "1000.0",
        "record_weights": "true",
        "record_history": "false",
        "record_interval_steps": "20",
        "record_connection_limit": "256",
    }.items():
        parser.set("plasticity", key, value)

    parser.add_section("homeostasis")
    values = {
        "enabled": str(enabled).lower(),
        "intrinsic_enabled": str(intrinsic).lower(),
        "target_rate": "0.05",
        "rate_tau": "100.0",
        "update_interval_steps": "10",
        "threshold_eta": "0.05",
        "threshold_min": "-60.0",
        "threshold_max": "-40.0",
        "synaptic_scaling_enabled": str(scaling).lower(),
        "scaling_target_mode": "initial_incoming_sum",
        "scaling_eta": "0.10",
        "scaling_min_factor": "0.50",
        "scaling_max_factor": "2.00",
        "scaling_weight_min": "0.0",
        "scaling_weight_max": "1000.0",
        "inhibitory_gain_enabled": str(gain).lower(),
        "inhibitory_gain_initial": "1.0",
        "inhibitory_gain_eta": "0.05",
        "inhibitory_gain_min": "0.25",
        "inhibitory_gain_max": "4.0",
        "record_history": str(history).lower(),
        "record_interval_steps": "10",
        "record_neuron_limit": "64",
    }
    for key, value in values.items():
        parser.set("homeostasis", key, value)
    with path.open("w", encoding="utf-8") as file:
        parser.write(file)


def parse_summary(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
    return values


def directory_size(path: Path) -> int:
    return sum(item.stat().st_size for item in path.rglob("*") if item.is_file())


def compiler_version() -> str:
    completed = subprocess.run(
        ["gcc", "--version"],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    return completed.stdout.splitlines()[0] if completed.stdout else "unknown"


def git_commit() -> str:
    completed = subprocess.run(
        ["git", "rev-parse", "--short", "HEAD"],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return completed.stdout.strip() or "unknown"


def main() -> int:
    if not RUNNER.is_file():
        print("FAIL: build/minisnn_runner.exe is missing")
        return 1
    BUILD.mkdir(exist_ok=True)
    RESULTS.mkdir(parents=True, exist_ok=True)
    rows: list[dict[str, object]] = []
    temporary: list[Path] = []
    try:
        for mode, enabled, intrinsic, scaling, gain in MODES:
            history = mode == "all_with_history"
            run_name = f"benchmark_c15_{mode}"
            config_path = BUILD / f"{run_name}.ini"
            run_dir = ROOT / "results" / "scenarios" / run_name
            temporary.extend((config_path, run_dir))
            write_config(
                config_path, run_name, enabled, intrinsic, scaling, gain, history
            )
            walls: list[float] = []
            for _ in range(3):
                shutil.rmtree(run_dir, ignore_errors=True)
                wall, _ = execute([str(RUNNER), str(config_path)], 180.0)
                walls.append(wall)
            summary = parse_summary(run_dir / "summary.txt")
            plot_seconds: float | str = "NA"
            html_seconds: float | str = "NA"
            if history:
                plot_seconds, _ = execute(
                    [sys.executable, "scripts/plot_homeostasis.py", str(run_dir)],
                    180.0,
                )
            if enabled:
                html_seconds, _ = execute(
                    [
                        sys.executable,
                        "scripts/generate_run_reports.py",
                        str(run_dir),
                        "--all",
                    ],
                    180.0,
                )
            mean_wall = statistics.fmean(walls)
            rows.append(
                {
                    "mode": mode,
                    "repetitions": 3,
                    "neurons": 100,
                    "steps": 2000,
                    "connections": int(summary["connection_count"]),
                    "homeostasis_interval": 10,
                    "wall_mean_seconds": mean_wall,
                    "steps_per_second": 2000.0 / mean_wall,
                    "neuron_updates_per_second": 200000.0 / mean_wall,
                    "output_bytes": directory_size(run_dir),
                    "plot_seconds": plot_seconds,
                    "html_seconds": html_seconds,
                }
            )

        output = RESULTS / "homeostasis_c15.csv"
        with output.open("w", encoding="utf-8", newline="") as file:
            writer = csv.DictWriter(file, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)
        environment = (
            f"platform={platform.platform()}\n"
            f"processor={platform.processor() or os.environ.get('PROCESSOR_IDENTIFIER', 'unknown')}\n"
            f"python={platform.python_version()}\n"
            f"compiler={compiler_version()}\n"
            f"commit={git_commit()}\n"
            "scenario=100 neurons, 2000 steps, p=0.05, STDP enabled\n"
            "repetitions=3\n"
            "scope=local controlled C1.5 benchmark; no universal threshold\n"
        )
        (RESULTS / "environment_c15.txt").write_text(environment, encoding="utf-8")
    except (OSError, RuntimeError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: C1.5 benchmark aborted: {error}")
        return 1
    finally:
        for path in temporary:
            if path.is_dir():
                shutil.rmtree(path, ignore_errors=True)
            else:
                path.unlink(missing_ok=True)

    print("=== C1.5 local benchmark ===")
    print("mode | wall_s | steps/s | neuron_updates/s | output_bytes | plot_s | html_s")
    for row in rows:
        print(
            f"{row['mode']:24s} | {row['wall_mean_seconds']:.6f} | "
            f"{row['steps_per_second']:.0f} | "
            f"{row['neuron_updates_per_second']:.0f} | {row['output_bytes']} | "
            f"{row['plot_seconds']} | {row['html_seconds']}"
        )
    print(f"Benchmark results: {RESULTS}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
