from __future__ import annotations

import csv
import os
import platform
import subprocess
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RUNNER = ROOT / "build" / "minisnn_runner.exe"
CASES = (
    ("lif", 10, 2000, 0.1, 20.0),
    ("lif", 100, 1000, 0.1, 20.0),
    ("lif", 500, 300, 0.1, 20.0),
    ("adex", 10, 2000, 0.1, 500.0),
    ("adex", 100, 1000, 0.1, 500.0),
    ("adex", 500, 300, 0.1, 500.0),
    ("hodgkin_huxley", 10, 2000, 0.01, 10.0),
    ("hodgkin_huxley", 100, 500, 0.01, 10.0),
)


def config_text(model: str, neurons: int, steps: int, dt: float, current: float) -> str:
    return f"""[run]
run_name = benchmark_c5_{model}_{neurons}
[network]
topology = chain
neurons = {neurons}
inhibitory_fraction = 0
[neuron]
model = {model}
[input]
source_count = 1
input_current = {current}
[simulation]
steps = {steps}
dt = {dt}
[recording]
record_neuron = 0
"""


def main() -> int:
    if not RUNNER.exists():
        print("Compile build/minisnn_runner.exe antes do benchmark.")
        return 1
    build = ROOT / "build"
    output = ROOT / "results" / "benchmarks"
    output.mkdir(parents=True, exist_ok=True)
    rows = []
    for model, neurons, steps, dt, current in CASES:
        path = build / f"benchmark_c5_{model}_{neurons}.ini"
        path.write_text(config_text(model, neurons, steps, dt, current), encoding="utf-8")
        start = time.perf_counter()
        completed = subprocess.run([str(RUNNER), str(path)], cwd=ROOT,
                                   stdout=subprocess.DEVNULL, check=False)
        elapsed = time.perf_counter() - start
        path.unlink(missing_ok=True)
        if completed.returncode != 0:
            return 1
        run_dir = ROOT / "results" / "scenarios" / f"benchmark_c5_{model}_{neurons}"
        csv_bytes = sum(p.stat().st_size for p in run_dir.glob("*.csv"))
        rows.append({
            "model": model, "neurons": neurons, "connections": neurons - 1,
            "steps": steps, "dt": dt, "elapsed_seconds": elapsed,
            "steps_per_second": steps / elapsed,
            "time_per_step_seconds": elapsed / steps,
            "estimated_state_bytes_per_neuron": {"lif": 40, "adex": 48, "hodgkin_huxley": 72}[model],
            "csv_bytes": csv_bytes, "platform": platform.platform(),
            "compiler": "gcc", "commit": subprocess.run(
                ["git", "rev-parse", "--short", "HEAD"], cwd=ROOT,
                capture_output=True, text=True, check=False).stdout.strip() or "NA",
        })
    destination = output / "c5_models.csv"
    with destination.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader(); writer.writerows(rows)
    print(f"Benchmark C5 gerado: {destination.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
