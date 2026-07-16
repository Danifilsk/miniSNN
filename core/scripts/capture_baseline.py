from __future__ import annotations

from configparser import ConfigParser
from pathlib import Path
import argparse
import csv
import json
import shutil
import subprocess
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
FIXTURE_DIR = PROJECT_ROOT / "tests" / "fixtures" / "regression"
GOLDEN_PATH = PROJECT_ROOT / "tests" / "golden" / "core_v02_baseline.json"
RUNNER = PROJECT_ROOT / "build" / "minisnn_runner.exe"


def key_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
    return values


def run_name(config_path: Path) -> str:
    parser = ConfigParser()
    parser.read(config_path, encoding="utf-8")
    return parser.get("run", "run_name")


def collect_run(run_dir: Path) -> dict[str, object]:
    summary = key_values(run_dir / "summary.txt")
    with (run_dir / "population.csv").open(encoding="utf-8", newline="") as file:
        population = list(csv.DictReader(file))
    with (run_dir / "raster.csv").open(encoding="utf-8", newline="") as file:
        raster = list(csv.DictReader(file))

    spike_sequence = [int(row["spikes_total"]) for row in population]
    return {
        "topology": summary["topology"],
        "topology_signature": summary["topology_signature"],
        "connection_count": int(summary["connection_count"]),
        "spikes_total": sum(spike_sequence),
        "active_timesteps": sum(value > 0 for value in spike_sequence),
        "peak_spikes": max(spike_sequence, default=0),
        "spikes_per_timestep": spike_sequence,
        "raster": [
            [int(row["tempo"]), int(row["neuronio"]), row["tipo"]]
            for row in raster
        ],
        "headers": {
            "population": list(population[0].keys()) if population else [
                "tempo", "spikes_total", "spikes_exc", "spikes_inh",
                "mean_potential", "mean_syn_current",
            ],
            "raster": list(raster[0].keys()) if raster else ["tempo", "neuronio", "tipo"],
        },
    }


def capture(runner: Path = RUNNER) -> dict[str, object]:
    if not runner.exists():
        raise RuntimeError(f"runner ausente: {runner}")

    baseline: dict[str, object] = {}
    for config_path in sorted(FIXTURE_DIR.glob("*.ini")):
        name = run_name(config_path)
        run_dir = PROJECT_ROOT / "results" / "scenarios" / name
        shutil.rmtree(run_dir, ignore_errors=True)
        completed = subprocess.run(
            [str(runner), str(config_path)],
            cwd=PROJECT_ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        if completed.returncode != 0:
            raise RuntimeError(f"cenario {config_path.name} falhou:\n{completed.stdout}")
        baseline[config_path.stem] = collect_run(run_dir)
        shutil.rmtree(run_dir, ignore_errors=True)
    return baseline


def main() -> int:
    parser = argparse.ArgumentParser(description="Captura ou confere o baseline pequeno da miniSNN.")
    parser.add_argument("--write", action="store_true", help="Atualiza explicitamente o golden.")
    args = parser.parse_args()
    try:
        current = capture()
    except RuntimeError as error:
        print(f"FAIL: {error}")
        return 1

    if args.write:
        GOLDEN_PATH.parent.mkdir(parents=True, exist_ok=True)
        GOLDEN_PATH.write_text(
            json.dumps(current, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        print(f"Baseline written: {GOLDEN_PATH}")
        return 0

    if not GOLDEN_PATH.exists():
        print(f"FAIL: golden ausente: {GOLDEN_PATH}")
        return 1
    expected = json.loads(GOLDEN_PATH.read_text(encoding="utf-8"))
    if current != expected:
        print("FAIL: regression baseline changed. Use --write only after scientific review.")
        return 1
    print("Core v0.2 regression baseline OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
