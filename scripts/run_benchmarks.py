from __future__ import annotations

from pathlib import Path
import argparse
import csv
import os
import platform
import re
import shutil
import statistics
import subprocess
import sys
import time


PROJECT_ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = PROJECT_ROOT / "build"
RESULT_DIR = PROJECT_ROOT / "results" / "benchmarks"
CORE_EXE = BUILD_DIR / "benchmark_core.exe"
RUNNER_EXE = BUILD_DIR / "minisnn_runner.exe"
BASE_CONFIG = PROJECT_ROOT / "tests" / "benchmark_configs" / "diagnostics.ini"
CORE_CASES = ((100, 2000), (1000, 500), (10000, 100))


def python_with_plot_dependencies() -> str | None:
    candidates: list[str] = []
    configured = os.environ.get("MINISNN_PYTHON")
    if configured:
        candidates.append(configured)
    candidates.append(sys.executable)

    local_app_data = os.environ.get("LOCALAPPDATA")
    if local_app_data:
        candidates.extend(
            str(path)
            for path in sorted(
                (Path(local_app_data) / "Python").glob("*/python.exe"),
                reverse=True,
            )
        )

    path_python = shutil.which("python")
    if path_python:
        candidates.append(path_python)

    seen: set[str] = set()
    for candidate in candidates:
        normalized = str(Path(candidate))
        if normalized.lower() in seen or not Path(normalized).exists():
            continue
        seen.add(normalized.lower())
        completed = subprocess.run(
            [normalized, "-c", "import pandas, matplotlib"],
            cwd=PROJECT_ROOT,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
        if completed.returncode == 0:
            return normalized
    return None


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
        raise RuntimeError(f"command failed ({completed.returncode}): {' '.join(command)}\n{completed.stdout}")
    return elapsed, completed.stdout


def core_benchmarks(repeats: int, timeout: float) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    pattern = re.compile(r"connections=(\d+) spikes=(\d+)")
    for neurons, steps in CORE_CASES:
        samples: list[float] = []
        connections = spikes = 0
        for _ in range(repeats):
            elapsed, output = execute([str(CORE_EXE), str(neurons), str(steps)], timeout)
            match = pattern.search(output)
            if match is None:
                raise RuntimeError(f"invalid benchmark output: {output}")
            connections, spikes = map(int, match.groups())
            samples.append(elapsed)
        mean = statistics.fmean(samples)
        rows.append({
            "neurons": neurons,
            "steps": steps,
            "repetitions": repeats,
            "connections": connections,
            "spikes": spikes,
            "wall_mean_seconds": mean,
            "wall_min_seconds": min(samples),
            "wall_max_seconds": max(samples),
            "wall_std_seconds": statistics.pstdev(samples),
            "steps_per_second": steps / mean,
            "neuron_updates_per_second": neurons * steps / mean,
        })
    return rows


def directory_size(path: Path) -> int:
    return sum(item.stat().st_size for item in path.rglob("*") if item.is_file())


def diagnostic_benchmarks(
    repeats: int,
    timeout: float,
    analysis_python: str,
) -> list[dict[str, object]]:
    base = BASE_CONFIG.read_text(encoding="utf-8")
    rows: list[dict[str, object]] = []
    for level in ("off", "basic", "full"):
        run_name = f"benchmark_diagnostics_{level}"
        config_text = base.replace(
            "run_name = benchmark_diagnostics", f"run_name = {run_name}"
        ).replace("level = off", f"level = {level}")
        config_path = BUILD_DIR / f"{run_name}.ini"
        config_path.write_text(config_text, encoding="utf-8")
        run_dir = PROJECT_ROOT / "results" / "scenarios" / run_name
        samples: list[float] = []
        size = 0
        for _ in range(repeats):
            shutil.rmtree(run_dir, ignore_errors=True)
            elapsed, _ = execute([str(RUNNER_EXE), str(config_path)], timeout)
            if level != "off":
                analysis_time, _ = execute(
                    [analysis_python, "scripts/analyze_run.py", str(run_dir), "--level", level],
                    timeout,
                )
                elapsed += analysis_time
            samples.append(elapsed)
            size = directory_size(run_dir)
        rows.append({
            "diagnostics_level": level,
            "neurons": 100,
            "steps": 500,
            "repetitions": repeats,
            "wall_mean_seconds": statistics.fmean(samples),
            "wall_min_seconds": min(samples),
            "wall_max_seconds": max(samples),
            "wall_std_seconds": statistics.pstdev(samples),
            "output_bytes": size,
        })
        shutil.rmtree(run_dir, ignore_errors=True)
        config_path.unlink(missing_ok=True)
    return rows


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def command_output(command: list[str]) -> str:
    completed = subprocess.run(
        command,
        cwd=PROJECT_ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    return completed.stdout.strip()


def main() -> int:
    parser = argparse.ArgumentParser(description="Benchmark controlado da miniSNN Core v0.2.")
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--timeout", type=float, default=120.0)
    args = parser.parse_args()
    if args.repeats < 1 or args.timeout <= 0:
        print("FAIL: repeats and timeout must be positive")
        return 1
    if not CORE_EXE.exists() or not RUNNER_EXE.exists():
        print("FAIL: benchmark executables are missing; use make benchmark-v02")
        return 1

    analysis_python = python_with_plot_dependencies()
    if analysis_python is None:
        print(
            "FAIL: no Python with pandas and matplotlib was found for diagnostics benchmarks. "
            "Set MINISNN_PYTHON."
        )
        return 1

    RESULT_DIR.mkdir(parents=True, exist_ok=True)
    try:
        core_rows = core_benchmarks(args.repeats, args.timeout)
        diagnostic_rows = diagnostic_benchmarks(
            args.repeats,
            args.timeout,
            analysis_python,
        )
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: benchmark aborted: {error}")
        return 1

    write_csv(RESULT_DIR / "core_v02.csv", core_rows)
    write_csv(RESULT_DIR / "diagnostics_v02.csv", diagnostic_rows)
    environment = [
        f"platform={platform.platform()}",
        f"processor={platform.processor() or os.environ.get('PROCESSOR_IDENTIFIER', 'unknown')}",
        f"python={platform.python_version()}",
        f"analysis_python={analysis_python}",
        f"compiler={command_output(['gcc', '--version']).splitlines()[0]}",
        f"commit={command_output(['git', 'rev-parse', 'HEAD'])}",
        "flags=-std=c11 -Wall -Wextra -pedantic -fanalyzer",
        f"repetitions={args.repeats}",
        f"timeout_seconds={args.timeout}",
        "scope=local controlled benchmark; not universal",
    ]
    (RESULT_DIR / "environment_v02.txt").write_text("\n".join(environment) + "\n", encoding="utf-8")

    print("=== Core scale benchmark ===")
    print("neurons | steps | mean_s | updates_per_s")
    for row in core_rows:
        print(
            f"{row['neurons']:7d} | {row['steps']:5d} | "
            f"{row['wall_mean_seconds']:.6f} | {row['neuron_updates_per_second']:.0f}"
        )
    print("=== Diagnostics benchmark ===")
    print("level | mean_s | output_bytes")
    for row in diagnostic_rows:
        print(f"{row['diagnostics_level']:5s} | {row['wall_mean_seconds']:.6f} | {row['output_bytes']}")
    print(f"Benchmark results: {RESULT_DIR}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
