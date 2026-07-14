from pathlib import Path
import csv
import math
import os
import shutil
import sys

os.environ.setdefault("MPLBACKEND", "Agg")

PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from compare_runs import compare_runs  # noqa: E402


def close(actual: str, expected: float, message: str) -> None:
    if not math.isclose(float(actual), expected, rel_tol=1.0e-12, abs_tol=1.0e-12):
        raise AssertionError(f"{message}: expected {expected}, got {actual}")


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
        "record_neuron = 0\n\n"
        "[plasticity]\n"
        f"enabled = {'true' if name == 'run_a' else 'false'}\n"
        "rule = stdp_pair_trace\n",
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


def write_stored_metrics(path: Path, name: str) -> None:
    with (path / "metrics.csv").open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(
            file,
            fieldnames=[
                "run_name",
                "activity_total_spikes",
                "activity_fraction",
                "silence_fraction",
                "activity_last_active_step",
                "activity_synchrony_proxy",
                "neuron_spike_gini",
                "diagnostic_stability_score",
                "diagnostic_regime",
            ],
        )
        writer.writeheader()
        writer.writerow(
            {
                "run_name": name,
                "activity_total_spikes": 7,
                "activity_fraction": 0.6,
                "silence_fraction": 0.4,
                "activity_last_active_step": 4,
                "activity_synchrony_proxy": 0.5,
                "neuron_spike_gini": 0.2,
                "diagnostic_stability_score": 0.7,
                "diagnostic_regime": "sustained",
            }
        )


def write_plasticity_metrics(path: Path) -> None:
    with (path / "plasticity_metrics.csv").open(
        "w", encoding="utf-8", newline=""
    ) as file:
        writer = csv.DictWriter(
            file,
            fieldnames=[
                "plasticity_enabled",
                "plasticity_rule",
                "plasticity_modified_connection_fraction",
                "plasticity_initial_weight_mean",
                "plasticity_final_weight_mean",
                "plasticity_mean_absolute_change",
                "plasticity_total_signed_change",
                "plasticity_potentiation_events",
                "plasticity_depression_events",
            ],
        )
        writer.writeheader()
        writer.writerow(
            {
                "plasticity_enabled": "true",
                "plasticity_rule": "stdp_pair_trace",
                "plasticity_modified_connection_fraction": 0.5,
                "plasticity_initial_weight_mean": 100,
                "plasticity_final_weight_mean": 101,
                "plasticity_mean_absolute_change": 1,
                "plasticity_total_signed_change": 2,
                "plasticity_potentiation_events": 5,
                "plasticity_depression_events": 3,
            }
        )
    (path / "weights_initial.csv").write_text("connection_id\n", encoding="utf-8")
    (path / "weights_final.csv").write_text("connection_id\n", encoding="utf-8")


def write_homeostasis_metrics(path: Path) -> None:
    with (path / "homeostasis_metrics.csv").open(
        "w", encoding="utf-8", newline=""
    ) as file:
        writer = csv.DictWriter(
            file,
            fieldnames=[
                "homeostasis_enabled",
                "homeostasis_population_rate_final",
                "homeostasis_rate_error_mean_absolute",
                "homeostasis_threshold_final_mean",
                "homeostasis_scaling_events",
                "homeostasis_inhibitory_gain_final",
            ],
        )
        writer.writeheader()
        writer.writerow(
            {
                "homeostasis_enabled": "true",
                "homeostasis_population_rate_final": 0.04,
                "homeostasis_rate_error_mean_absolute": 0.01,
                "homeostasis_threshold_final_mean": -51.0,
                "homeostasis_scaling_events": 3,
                "homeostasis_inhibitory_gain_final": 1.2,
            }
        )


def write_reward_metrics(path: Path) -> None:
    values = {
        "reward_enabled": "true",
        "reward_event_count": 2,
        "reward_positive_event_count": 1,
        "reward_negative_event_count": 1,
        "reward_cumulative_applied": 0.5,
        "reward_cumulative_absolute": 1.5,
        "reward_modified_connection_fraction": 0.5,
        "reward_eligibility_final_mean_absolute": 0.25,
        "reward_eligibility_max_absolute_observed": 1.0,
        "reward_weight_total_signed_change": 0.3,
        "reward_weight_total_absolute_change": 0.7,
        "reward_weight_mean_absolute_change": 0.35,
    }
    with (path / "reward_metrics.csv").open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=list(values))
        writer.writeheader()
        writer.writerow(values)


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
        write_stored_metrics(run_a, "run_a")
        write_plasticity_metrics(run_a)
        write_homeostasis_metrics(run_a)
        write_reward_metrics(run_a)

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

        sources = {row["run_name"]: row["metrics_source"] for row in rows}
        if sources.get("run_a") != "metrics.csv":
            print("FAIL: stored metrics.csv was not reused")
            return 1
        if "derivadas" not in sources.get("run_b", ""):
            print("FAIL: legacy run did not use derived fallback")
            return 1

        by_name = {row["run_name"]: row for row in rows}
        stored = by_name["run_a"]
        derived = by_name["run_b"]
        close(stored["total_spikes"], 7.0, "stored total")
        close(stored["activity_fraction"], 0.6, "stored activity fraction")
        close(stored["silence_fraction"], 0.4, "stored silence fraction")
        close(stored["last_active_step"], 4.0, "stored last active step")
        close(stored["synchrony_proxy"], 0.5, "stored synchrony")
        close(stored["spike_gini_approx"], 0.2, "stored Gini")
        close(stored["stability_score"], 0.7, "stored stability")
        if stored["diagnostic_regime"] != "sustained":
            print("FAIL: stored diagnostic regime changed")
            return 1
        close(
            stored["plasticity_final_weight_mean"],
            101.0,
            "stored plasticity final weight",
        )
        if stored["plasticity_metrics_source"] != "plasticity_metrics.csv":
            print("FAIL: plasticity_metrics.csv was not reused")
            return 1
        close(stored["homeostasis_population_rate_final"], 0.04,
              "stored homeostasis final rate")
        if stored["homeostasis_metrics_source"] != "homeostasis_metrics.csv":
            print("FAIL: homeostasis_metrics.csv was not reused")
            return 1
        close(stored["reward_weight_total_signed_change"], 0.3,
              "reward signed change")
        if stored["reward_metrics_source"] != "reward_metrics.csv":
            print("FAIL: reward_metrics.csv was not reused")
            return 1
        report = (output_dir / "comparison_report.txt").read_text(encoding="utf-8")
        if "Plasticidade:" not in report or "plasticity_final_weight_mean" not in report:
            print("FAIL: comparison report lacks plasticity evidence")
            return 1
        if "Homeostase:" not in report or "homeostasis_population_rate_final" not in report:
            print("FAIL: comparison report lacks homeostasis evidence")
            return 1
        if ("Recompensa, punicao e elegibilidade:" not in report or
                "reward_weight_total_signed_change" not in report):
            print("FAIL: reward comparison section missing")
            return 1

        close(derived["total_spikes"], 5.0, "derived total")
        close(derived["mean_spikes_per_step"], 1.0, "derived mean")
        close(derived["max_spikes_per_step"], 1.0, "derived maximum")
        close(derived["std_spikes_per_step"], 0.0, "derived standard deviation")
        close(derived["active_timesteps"], 5.0, "derived active timesteps")
        close(derived["silent_timesteps"], 0.0, "derived silent timesteps")
        close(derived["activity_fraction"], 1.0, "derived activity fraction")
        close(derived["silence_fraction"], 0.0, "derived silence fraction")
        close(derived["first_active_step"], 0.0, "derived first active step")
        close(derived["last_active_step"], 4.0, "derived last active step")
        close(derived["synchrony_proxy"], 0.2, "derived synchrony")

        second_output_dir = compare_runs(
            [run_a, run_b],
            out_name="test_compare_runs",
            output_root=comparisons_root,
        )

        if second_output_dir is None:
            print("FAIL: second compare_runs returned None")
            return 1

        if second_output_dir == output_dir:
            print("FAIL: repeated comparison overwrote the first output")
            return 1

        if not (output_dir / "comparison_summary.csv").exists():
            print("FAIL: first comparison was removed")
            return 1

        index_path = comparisons_root / "index.csv"
        if not index_path.exists():
            print("FAIL: comparison index was not created")
            return 1

        with index_path.open("r", encoding="utf-8", newline="") as file:
            index_rows = list(csv.DictReader(file))

        if len(index_rows) < 2:
            print("FAIL: comparison index should contain repeated comparisons")
            return 1

        print("Run comparison validation OK")
        return 0
    finally:
        if temp_root.exists():
            shutil.rmtree(temp_root)


if __name__ == "__main__":
    raise SystemExit(main())
