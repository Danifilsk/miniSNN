from pathlib import Path
import csv
import os
import shutil
import sys

os.environ.setdefault("MPLBACKEND", "Agg")

PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from compare_runs import compare_runs  # noqa: E402


def write_population(path: Path, values: list[int]) -> None:
    with (path / "population.csv").open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(
            [
                "tempo",
                "spikes_total",
                "spikes_exc",
                "spikes_inh",
                "mean_potential",
                "mean_syn_current",
            ]
        )

        for step, spikes in enumerate(values):
            writer.writerow([step, spikes, max(0, spikes - 1), min(1, spikes), -65.0, 0.0])


def write_raster(path: Path) -> None:
    with (path / "raster.csv").open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(["tempo", "neuronio", "tipo"])
        writer.writerow([1, 0, "EXC"])
        writer.writerow([2, 1, "EXC"])
        writer.writerow([2, 2, "INH"])
        writer.writerow([4, 0, "EXC"])


def write_config(path: Path, name: str, topology: str) -> None:
    (path / "config_used.ini").write_text(
        "[run]\n"
        f"run_name = {name}\n\n"
        "[network]\n"
        f"topology = {topology}\n"
        "neurons = 4\n"
        "inhibitory_fraction = 0.25\n"
        "seed = 1\n\n"
        "[simulation]\n"
        "steps = 5\n"
        "dt = 0.1\n\n"
        "[recording]\n"
        "record_neuron = 0\n",
        encoding="utf-8",
    )


def write_summary(path: Path, name: str, topology: str, connections: int) -> None:
    (path / "summary.txt").write_text(
        f"run_name={name}\n"
        f"topology={topology}\n"
        "neurons=4\n"
        "inhibitory_count=1\n"
        f"connection_count={connections}\n"
        "steps=5\n"
        "spikes_total=10\n",
        encoding="utf-8",
    )


def main() -> int:
    temp_root = PROJECT_ROOT / "build" / "test_compare_runs_temp"
    comparisons_root = temp_root / "comparisons"

    if temp_root.exists():
        shutil.rmtree(temp_root)

    try:
        run_a = temp_root / "run_a"
        run_b = temp_root / "run_b"
        run_a.mkdir(parents=True)
        run_b.mkdir(parents=True)

        write_population(run_a, [0, 2, 4, 0, 1])
        write_population(run_b, [1, 1, 1, 1, 1])
        write_raster(run_a)
        write_config(run_a, "run_a", "random")
        write_config(run_b, "run_b", "small_world")
        write_summary(run_a, "run_a", "random", 7)
        write_summary(run_b, "run_b", "small_world", 8)

        output_dir = compare_runs(
            [run_a, run_b],
            out_name="test_compare_runs",
            output_root=comparisons_root,
        )

        if output_dir is None:
            print("FAIL: compare_runs returned None")
            return 1

        required_files = [
            "comparison_summary.csv",
            "comparison_report.txt",
            "comparison_metrics.png",
            "comparison_activity_overlay.png",
        ]

        for filename in required_files:
            if not (output_dir / filename).exists():
                print(f"FAIL: missing output file {filename}")
                return 1

        with (output_dir / "comparison_summary.csv").open(
            "r",
            encoding="utf-8",
            newline="",
        ) as file:
            reader = csv.DictReader(file)
            rows = list(reader)

        if len(rows) != 2:
            print("FAIL: summary should contain two runs")
            return 1

        for column in (
            "total_spikes",
            "activity_fraction",
            "silent_timesteps",
            "burst_count",
            "stability_score",
        ):
            if column not in rows[0]:
                print(f"FAIL: summary missing metric {column}")
                return 1

        print("Run comparison validation OK")
        return 0
    finally:
        if temp_root.exists():
            shutil.rmtree(temp_root)


if __name__ == "__main__":
    raise SystemExit(main())
