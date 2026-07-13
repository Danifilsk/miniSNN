from __future__ import annotations

from configparser import ConfigParser
import math
from pathlib import Path
import csv
import shutil
import sys

import numpy as np
import pandas as pd


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from metrics_common import (  # noqa: E402
    burst_metrics,
    classify_regime,
    diagnostics_parameters,
    entropy_metrics,
    gini,
    longest_streak,
    read_plasticity_metrics,
    sampled_correlation_metrics,
    segment_metrics,
    stability,
    top_share,
)


REL_TOL = 1.0e-12
ABS_TOL = 1.0e-12


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def close(actual: object, expected: float, message: str) -> None:
    if actual is None or not math.isclose(
        float(actual), expected, rel_tol=REL_TOL, abs_tol=ABS_TOL
    ):
        raise AssertionError(f"{message}: expected {expected}, got {actual}")


def regime_case(**overrides: object) -> dict[str, object]:
    metrics: dict[str, object] = {
        "activity_total_spikes": 10,
        "run_steps": 100,
        "network_num_neurons": 10,
        "activity_fraction": 0.15,
        "activity_mean_spikes_per_step": 0.1,
        "activity_max_spikes_per_step": 2.0,
        "burst_spike_share": 0.0,
        "burst_count": 0,
        "activity_longest_silence_streak": 0,
        "activity_has_late_activity": False,
    }
    metrics.update(overrides)
    return metrics


def test_parameters_and_scalar_metrics() -> None:
    parser = ConfigParser()
    parser.read_string("[diagnostics]\nburst_z_threshold = 0\n")
    close(
        diagnostics_parameters(parser)["burst_z_threshold"],
        0.0,
        "zero burst threshold must be preserved",
    )

    check(longest_streak(np.array([False, True, True, False, True])) == 2, "streak")
    close(gini(np.array([0, 0, 0, 4])), 0.75, "Gini")
    close(top_share(np.array([7, 1, 1, 1]), 0.10), 0.70, "top share")
    entropy, normalized = entropy_metrics(np.array([1, 1, 1, 1]))
    close(entropy, 2.0, "entropy")
    close(normalized, 1.0, "normalized entropy")

    total, mean, active_fraction = segment_metrics(np.array([0, 2, 0, 4]), 1, 4)
    check(total == 6, "segment total")
    close(mean, 2.0, "segment mean")
    close(active_fraction, 2.0 / 3.0, "segment active fraction")


def test_bursts() -> None:
    metrics = burst_metrics(
        np.array([0, 0, 4, 4, 0, 0, 5, 5, 0, 0], dtype=float),
        z_threshold=0.0,
        minimum_steps=1,
    )
    close(metrics["burst_threshold_used"], 1.8, "burst threshold")
    check(metrics["burst_count"] == 2, "burst count")
    check(metrics["burst_timesteps"] == 4, "burst timesteps")
    close(metrics["burst_fraction"], 0.4, "burst fraction")
    check(metrics["burst_total_spikes"] == 18, "burst total")
    close(metrics["burst_spike_share"], 1.0, "burst share")
    close(metrics["burst_mean_duration_steps"], 2.0, "burst mean duration")
    check(metrics["burst_max_duration_steps"] == 2, "burst max duration")
    close(metrics["burst_mean_size"], 9.0, "burst mean size")
    check(metrics["burst_max_size"] == 10, "burst max size")
    close(metrics["burst_mean_interburst_interval"], 2.0, "interburst interval")


def test_correlations() -> None:
    rows: list[dict[str, int]] = []
    for step in (0, 2):
        rows.extend(({"tempo": step, "neuronio": 0}, {"tempo": step, "neuronio": 1}))
    for step in (1, 3):
        rows.append({"tempo": step, "neuronio": 2})
    for step in range(4):
        rows.append({"tempo": step, "neuronio": 3})

    metrics, values, matrix, sampled_ids = sampled_correlation_metrics(
        pd.DataFrame(rows),
        num_neurons=4,
        steps=4,
        bin_size=1,
        sample_size=4,
        neuron_limit=4,
        sample_stride=1,
        seed=11,
    )
    check(sampled_ids == [0, 1, 2, 3], "correlation sample IDs")
    check(matrix is not None and matrix.shape == (3, 3), "constant row exclusion")
    check(values.size == 3, "pairwise correlation count")
    close(metrics["correlation_mean_pairwise"], -1.0 / 3.0, "correlation mean")
    close(metrics["correlation_median_pairwise"], -1.0, "correlation median")
    close(metrics["correlation_std_pairwise"], math.sqrt(8.0 / 9.0), "correlation std")
    close(metrics["correlation_min_pairwise"], -1.0, "correlation minimum")
    close(metrics["correlation_max_pairwise"], 1.0, "correlation maximum")
    close(metrics["correlation_positive_fraction"], 1.0 / 3.0, "positive fraction")
    close(metrics["correlation_negative_fraction"], 2.0 / 3.0, "negative fraction")


def test_stability() -> None:
    result = stability(
        {
            "activity_total_spikes": 10,
            "silence_fraction": 0.25,
            "network_num_neurons": 10,
            "activity_max_spikes_per_step": 2,
            "activity_has_late_activity": True,
            "neuron_normalized_spike_entropy": 0.8,
        }
    )
    close(result["stability_silence_component"], 0.75, "silence component")
    close(result["stability_explosion_component"], 0.6, "explosion component")
    close(result["stability_persistence_component"], 1.0, "persistence component")
    close(result["stability_distribution_component"], 0.8, "distribution component")
    close(result["diagnostic_stability_score"], 0.7875, "stability score")

    silent = stability({"activity_total_spikes": 0})
    close(silent["diagnostic_stability_score"], 0.0, "silent score")
    close(silent["stability_explosion_component"], 1.0, "silent explosion component")
    for key, value in result.items():
        check(0.0 <= value <= 1.0, f"bounded stability value {key}")


def test_regimes() -> None:
    cases = {
        "silent": regime_case(activity_total_spikes=0),
        "sparse": regime_case(
            activity_total_spikes=1,
            activity_fraction=0.01,
            activity_mean_spikes_per_step=0.01,
            activity_max_spikes_per_step=1,
        ),
        "sustained": regime_case(
            activity_total_spikes=30,
            activity_fraction=0.30,
            activity_mean_spikes_per_step=0.30,
            activity_has_late_activity=True,
        ),
        "intermittent": regime_case(
            activity_fraction=0.10,
            activity_has_late_activity=True,
            activity_longest_silence_streak=10,
        ),
        "bursting": regime_case(
            activity_fraction=0.20,
            activity_mean_spikes_per_step=0.20,
            activity_max_spikes_per_step=5,
            burst_spike_share=0.60,
            burst_count=2,
        ),
        "hyperactive": regime_case(
            activity_total_spikes=500,
            activity_fraction=0.20,
            activity_mean_spikes_per_step=5.0,
            activity_max_spikes_per_step=5,
        ),
        "mixed": regime_case(
            activity_total_spikes=500,
            activity_fraction=0.30,
            activity_mean_spikes_per_step=5.0,
            activity_max_spikes_per_step=8,
            activity_has_late_activity=True,
        ),
        "undetermined": regime_case(),
    }
    for expected, metrics in cases.items():
        actual, confidence, _ = classify_regime(metrics)
        check(actual == expected, f"regime {expected}: got {actual}")
        check(0.0 <= confidence <= 1.0, f"regime confidence {expected}")


def test_plasticity_metrics_loading() -> None:
    run_path = PROJECT_ROOT / "build" / "test_metrics_plasticity"
    shutil.rmtree(run_path, ignore_errors=True)
    run_path.mkdir(parents=True)
    parser = ConfigParser()
    parser.read_string(
        "[plasticity]\n"
        "enabled = true\n"
        "rule = stdp_pair_trace\n"
    )
    fields = [
        "plasticity_enabled",
        "plasticity_rule",
        "plasticity_modified_connection_fraction",
        "plasticity_initial_weight_mean",
        "plasticity_final_weight_mean",
        "plasticity_mean_absolute_change",
        "plasticity_total_signed_change",
        "plasticity_potentiation_events",
        "plasticity_depression_events",
    ]
    with (run_path / "plasticity_metrics.csv").open(
        "w", encoding="utf-8", newline=""
    ) as file:
        writer = csv.DictWriter(file, fieldnames=fields)
        writer.writeheader()
        writer.writerow(
            {
                "plasticity_enabled": "true",
                "plasticity_rule": "stdp_pair_trace",
                "plasticity_modified_connection_fraction": 0.5,
                "plasticity_initial_weight_mean": 100.0,
                "plasticity_final_weight_mean": 101.5,
                "plasticity_mean_absolute_change": 2.0,
                "plasticity_total_signed_change": 3.0,
                "plasticity_potentiation_events": 7,
                "plasticity_depression_events": 4,
            }
        )
    (run_path / "weights_initial.csv").write_text("connection_id\n", encoding="utf-8")
    (run_path / "weights_final.csv").write_text("connection_id\n", encoding="utf-8")

    warnings: list[str] = []
    metrics = read_plasticity_metrics(run_path, parser, warnings)
    check(not warnings, f"unexpected plasticity warnings: {warnings}")
    check(metrics["plasticity_metrics_source"] == "plasticity_metrics.csv", "source")
    close(metrics["plasticity_final_weight_mean"], 101.5, "final weight mean")
    close(metrics["plasticity_total_signed_change"], 3.0, "signed change")

    legacy_warnings: list[str] = []
    legacy = read_plasticity_metrics(run_path / "legacy", ConfigParser(), legacy_warnings)
    check(not legacy_warnings, "legacy run should not warn about missing STDP outputs")
    check(legacy["plasticity_enabled"] is False, "legacy plasticity default")
    check(legacy["plasticity_metrics_source"] == "config (STDP off)", "legacy source")
    shutil.rmtree(run_path, ignore_errors=True)


def main() -> int:
    test_parameters_and_scalar_metrics()
    test_bursts()
    test_correlations()
    test_stability()
    test_regimes()
    test_plasticity_metrics_loading()
    print("Shared diagnostics metrics validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
