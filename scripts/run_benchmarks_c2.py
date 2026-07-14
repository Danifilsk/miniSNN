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
STEPS = 200
NEURONS = 100
MODES = (
    ("plasticity_off", "off", "none", False, False),
    ("direct_stdp", "direct", "none", False, False),
    ("rstdp_no_reward", "rstdp", "none", False, False),
    ("rstdp_sparse_reward", "rstdp", "sparse", False, False),
    ("rstdp_reward_every_step", "rstdp", "every", False, False),
    ("rstdp_homeostasis", "rstdp", "sparse", True, False),
    ("rstdp_history", "rstdp", "sparse", False, True),
)


def execute(command: list[str], timeout: float = 180.0) -> tuple[float, str]:
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


def write_config(
    path: Path,
    run_name: str,
    learning: str,
    events: str,
    homeostasis: bool,
    history: bool,
) -> None:
    parser = ConfigParser()
    parser.read(ROOT / "configs" / "random.ini", encoding="utf-8")
    set_values(parser, "run", {"run_name": run_name})
    set_values(parser, "network", {
        "neurons": NEURONS,
        "connection_probability": 0.05,
        "seed": 31,
    })
    set_values(parser, "input", {"source_count": 20})
    set_values(parser, "simulation", {"steps": STEPS})
    set_values(parser, "output", {
        "auto_unique_run": False,
        "history_enabled": False,
    })
    plasticity_enabled = learning != "off"
    set_values(parser, "plasticity", {
        "enabled": plasticity_enabled,
        "rule": "stdp_pair_trace",
        "learning_mode": (
            "reward_modulated_stdp" if learning == "rstdp" else "direct_stdp"
        ),
        "a_plus": 0.2,
        "a_minus": 0.21,
        "tau_plus": 20.0,
        "tau_minus": 20.0,
        "trace_increment": 1.0,
        "weight_min": 0.0,
        "weight_max": 1000.0,
        "record_weights": plasticity_enabled,
        "record_history": False,
        "record_interval_steps": 20,
        "record_connection_limit": 256,
    })
    reward_enabled = learning == "rstdp"
    set_values(parser, "reward", {
        "enabled": reward_enabled,
        "mode": "rstdp",
        "learning_rate": 1.0,
        "eligibility_tau": 100.0,
        "eligibility_min": -200.0,
        "eligibility_max": 200.0,
        "reward_min": -1.0,
        "reward_max": 1.0,
        "clip_reward": True,
        "record_history": history,
        "record_interval_steps": 10,
        "record_connection_limit": 256,
    })
    set_values(parser, "homeostasis", {
        "enabled": homeostasis,
        "intrinsic_enabled": homeostasis,
        "target_rate": 0.05,
        "rate_tau": 100.0,
        "update_interval_steps": 10,
        "threshold_eta": 0.05,
        "threshold_min": -60.0,
        "threshold_max": -40.0,
        "synaptic_scaling_enabled": homeostasis,
        "scaling_target_mode": "initial_incoming_sum",
        "scaling_eta": 0.10,
        "scaling_min_factor": 0.50,
        "scaling_max_factor": 2.00,
        "scaling_weight_min": 0.0,
        "scaling_weight_max": 1000.0,
        "inhibitory_gain_enabled": homeostasis,
        "inhibitory_gain_initial": 1.0,
        "inhibitory_gain_eta": 0.05,
        "inhibitory_gain_min": 0.25,
        "inhibitory_gain_max": 4.0,
        "record_history": False,
        "record_interval_steps": 10,
        "record_neuron_limit": 64,
    })
    if parser.has_section("reward_events"):
        parser.remove_section("reward_events")
    if events != "none":
        parser.add_section("reward_events")
        steps = range(STEPS) if events == "every" else (50, 100, 150)
        for index, step in enumerate(steps):
            value = 0.5 if index % 2 == 0 else -0.25
            parser.set("reward_events", f"event_{index}", f"{step},{value}")
    with path.open("w", encoding="utf-8") as file:
        parser.write(file)


def read_single(path: Path) -> dict[str, str]:
    if not path.is_file():
        return {}
    with path.open(encoding="utf-8", newline="") as file:
        return next(csv.DictReader(file), {})


def directory_size(path: Path) -> int:
    return sum(item.stat().st_size for item in path.rglob("*") if item.is_file())


def command_line(command: list[str]) -> str:
    completed = subprocess.run(
        command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, check=False,
    )
    return completed.stdout.splitlines()[0] if completed.stdout else "unknown"


def main() -> int:
    if not RUNNER.is_file():
        print("FAIL: build/minisnn_runner.exe is missing")
        return 1
    BUILD.mkdir(exist_ok=True)
    RESULTS.mkdir(parents=True, exist_ok=True)
    rows: list[dict[str, object]] = []
    temporary: list[Path] = []
    try:
        for mode, learning, events, homeostasis, history in MODES:
            run_name = f"benchmark_c2_{mode}"
            config_path = BUILD / f"{run_name}.ini"
            run_dir = ROOT / "results" / "scenarios" / run_name
            temporary.extend((config_path, run_dir))
            write_config(config_path, run_name, learning, events, homeostasis, history)
            walls: list[float] = []
            for _ in range(3):
                shutil.rmtree(run_dir, ignore_errors=True)
                wall, _ = execute([str(RUNNER), str(config_path)])
                walls.append(wall)
            summary = read_single(run_dir / "metrics.csv")
            reward = read_single(run_dir / "reward_metrics.csv")
            plot_seconds: float | str = "NA"
            html_seconds: float | str = "NA"
            if history:
                plot_seconds, _ = execute(
                    [sys.executable, "scripts/plot_reward.py", str(run_dir)]
                )
                html_seconds, _ = execute([
                    sys.executable, "scripts/generate_run_reports.py",
                    str(run_dir), "--reward",
                ])
            mean_wall = statistics.fmean(walls)
            rows.append({
                "mode": mode,
                "repetitions": 3,
                "neurons": NEURONS,
                "steps": STEPS,
                "connections": int(float(summary.get("network_total_connections", 0))),
                "eligible_connections": int(float(reward.get("reward_eligible_connection_count", 0))),
                "reward_events": int(float(reward.get("reward_event_count", 0))),
                "wall_mean_seconds": mean_wall,
                "steps_per_second": STEPS / mean_wall,
                "neuron_updates_per_second": STEPS * NEURONS / mean_wall,
                "eligibility_cost_seconds_approx": "NA",
                "reward_application_cost_seconds_approx": "NA",
                "output_bytes": directory_size(run_dir),
                "plot_seconds": plot_seconds,
                "html_seconds": html_seconds,
            })
        off_wall = float(rows[0]["wall_mean_seconds"])
        direct_wall = float(rows[1]["wall_mean_seconds"])
        for row in rows:
            wall = float(row["wall_mean_seconds"])
            if str(row["mode"]).startswith("rstdp"):
                row["eligibility_cost_seconds_approx"] = max(0.0, wall - direct_wall)
            if int(row["reward_events"]) > 0:
                row["reward_application_cost_seconds_approx"] = max(0.0, wall - off_wall)

        output = RESULTS / "reward_c2.csv"
        with output.open("w", encoding="utf-8", newline="") as file:
            writer = csv.DictWriter(file, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)
        environment = (
            f"platform={platform.platform()}\n"
            f"processor={platform.processor() or os.environ.get('PROCESSOR_IDENTIFIER', 'unknown')}\n"
            f"python={platform.python_version()}\n"
            f"compiler={command_line(['gcc', '--version'])}\n"
            f"commit={command_line(['git', 'rev-parse', '--short', 'HEAD'])}\n"
            f"scenario={NEURONS} neurons, {STEPS} steps, p=0.05, seed=31\n"
            "repetitions=3\n"
            "cost_fields=rough wall-time differences; not isolated profilers\n"
            "scope=local controlled C2 benchmark; no universal threshold\n"
        )
        (RESULTS / "environment_c2.txt").write_text(environment, encoding="utf-8")
    except (OSError, RuntimeError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: C2 benchmark aborted: {error}")
        return 1
    finally:
        for path in temporary:
            if path.is_dir():
                shutil.rmtree(path, ignore_errors=True)
            else:
                path.unlink(missing_ok=True)

    print("=== C2 local reward benchmark ===")
    print("mode | wall_s | steps/s | eligible | events | output_bytes | plot_s | html_s")
    for row in rows:
        print(
            f"{row['mode']:25s} | {row['wall_mean_seconds']:.6f} | "
            f"{row['steps_per_second']:.0f} | {row['eligible_connections']} | "
            f"{row['reward_events']} | {row['output_bytes']} | "
            f"{row['plot_seconds']} | {row['html_seconds']}"
        )
    print(f"Benchmark results: {RESULTS}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
