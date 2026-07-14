from __future__ import annotations

from pathlib import Path
import csv
import math
import shutil
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SCRIPTS_DIR = PROJECT_ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS_DIR))

from analyze_run import analyze


TEMP_ROOT = PROJECT_ROOT / "build" / "test_analyze_run"


def write_run(name: str, per_step: list[list[tuple[int, str]]], complete: bool = True) -> Path:
    run = TEMP_ROOT / name
    run.mkdir(parents=True, exist_ok=True)
    steps = len(per_step)
    config = (
        "[run]\nrun_name = " + name + "\n"
        "[network]\ntopology = chain\nneurons = 4\ninhibitory_fraction = 0.25\nseed = 7\n"
        "[simulation]\nsteps = " + str(steps) + "\ndt = 0.1\nv_threshold = -50\n"
        "[recording]\nrecord_neuron = 0\n"
        "[diagnostics]\nlevel = basic\ntime_bin_steps = 2\nburst_z_threshold = 2.0\n"
        "min_burst_steps = 1\nisi_min_spikes = 4\ncorrelation_sample_size = 4\n"
        "neuron_sample_limit = 4\nsample_stride = 1\n"
    )
    (run / "config_used.ini").write_text(config, encoding="utf-8")
    (run / "summary.txt").write_text(
        f"run_name={name}\nactual_run_name={name}\ntopology=chain\nneurons=4\n"
        f"inhibitory_count=1\nconnection_count=3\nsteps={steps}\n",
        encoding="utf-8",
    )
    (run / "run_manifest.txt").write_text("miniSNN_version=test\ntimestamp=20260101_000000\n", encoding="utf-8")
    with (run / "population.csv").open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        writer.writerow(["tempo", "spikes_total", "spikes_exc", "spikes_inh", "mean_potential", "mean_syn_current"])
        for step, events in enumerate(per_step):
            exc = sum(kind == "EXC" for _, kind in events)
            inh = sum(kind == "INH" for _, kind in events)
            writer.writerow([step, len(events), exc, inh, -65 + len(events), 0])
    if complete:
        with (run / "raster.csv").open("w", newline="", encoding="utf-8") as file:
            writer = csv.writer(file)
            writer.writerow(["tempo", "neuronio", "tipo"])
            for step, events in enumerate(per_step):
                for neuron_id, kind in events:
                    writer.writerow([step, neuron_id, kind])
    with (run / "neuron_0.csv").open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        writer.writerow(["tempo", "V", "spike", "corrente_externa", "corrente_sinaptica"])
        for step, events in enumerate(per_step):
            writer.writerow([step, -64.0, int(any(item[0] == 0 for item in events)), 20.0, 0.0])
    return run


def add_plasticity_outputs(run: Path) -> None:
    with (run / "config_used.ini").open("a", encoding="utf-8") as file:
        file.write(
            "\n[plasticity]\n"
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
    with (run / "plasticity_metrics.csv").open(
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
                "plasticity_final_weight_mean": 102.0,
                "plasticity_mean_absolute_change": 2.0,
                "plasticity_total_signed_change": 4.0,
                "plasticity_potentiation_events": 8,
                "plasticity_depression_events": 3,
            }
        )
    (run / "weights_initial.csv").write_text("connection_id\n", encoding="utf-8")
    (run / "weights_final.csv").write_text("connection_id\n", encoding="utf-8")


def add_homeostasis_outputs(run: Path) -> None:
    fields = [
        "homeostasis_enabled",
        "homeostasis_intrinsic_enabled",
        "homeostasis_synaptic_scaling_enabled",
        "homeostasis_inhibitory_gain_enabled",
        "homeostasis_target_rate",
        "homeostasis_population_rate_final",
        "homeostasis_rate_error_final",
        "homeostasis_threshold_final_mean",
        "homeostasis_scaling_events",
        "homeostasis_inhibitory_gain_final",
    ]
    with (run / "homeostasis_metrics.csv").open(
        "w", encoding="utf-8", newline=""
    ) as file:
        writer = csv.DictWriter(file, fieldnames=fields)
        writer.writeheader()
        writer.writerow(
            {
                "homeostasis_enabled": "true",
                "homeostasis_intrinsic_enabled": "true",
                "homeostasis_synaptic_scaling_enabled": "true",
                "homeostasis_inhibitory_gain_enabled": "false",
                "homeostasis_target_rate": 0.05,
                "homeostasis_population_rate_final": 0.04,
                "homeostasis_rate_error_final": -0.01,
                "homeostasis_threshold_final_mean": -51.0,
                "homeostasis_scaling_events": 4,
                "homeostasis_inhibitory_gain_final": 1.0,
            }
        )


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def close(actual: object, expected: float, message: str) -> None:
    if actual is None or not math.isclose(
        float(actual), expected, rel_tol=1.0e-12, abs_tol=1.0e-12
    ):
        raise AssertionError(f"{message}: expected {expected}, got {actual}")


def main() -> int:
    shutil.rmtree(TEMP_ROOT, ignore_errors=True)
    TEMP_ROOT.mkdir(parents=True)
    try:
        silent = write_run("silent", [[] for _ in range(20)])
        metrics = analyze(silent, "basic")
        check(metrics["activity_total_spikes"] == 0, "silent total")
        check(metrics["activity_active_timesteps"] == 0, "silent active timesteps")
        check(metrics["activity_silent_timesteps"] == 20, "silent timesteps")
        check(metrics["activity_fraction"] == 0, "silent activity fraction")
        check(metrics["silence_fraction"] == 1, "silent silence fraction")
        check(metrics["diagnostic_regime"] == "silent", "silent regime")
        check(metrics["diagnostic_stability_score"] == 0, "silent stability")

        sustained_events = []
        for step in range(20):
            events = [(step % 3, "EXC")]
            if step % 2 == 0:
                events.append((3, "INH"))
            sustained_events.append(events)
        sustained = write_run("sustained", sustained_events)
        add_plasticity_outputs(sustained)
        add_homeostasis_outputs(sustained)
        sustained_metrics = analyze(sustained, "full")
        check(sustained_metrics["activity_total_spikes"] == 30, "sustained total")
        close(sustained_metrics["activity_mean_spikes_per_step"], 1.5, "sustained mean")
        close(sustained_metrics["activity_median_spikes_per_step"], 1.5, "sustained median")
        close(sustained_metrics["activity_min_spikes_per_step"], 1.0, "sustained minimum")
        close(sustained_metrics["activity_max_spikes_per_step"], 2.0, "sustained maximum")
        close(sustained_metrics["activity_variance_spikes_per_step"], 0.25, "sustained variance")
        close(sustained_metrics["activity_std_spikes_per_step"], 0.5, "sustained std")
        check(sustained_metrics["activity_first_active_step"] == 0, "first activity")
        check(sustained_metrics["activity_last_active_step"] == 19, "last activity")
        check(sustained_metrics["activity_early_total_spikes"] == 9, "early total")
        check(sustained_metrics["activity_middle_total_spikes"] == 11, "middle total")
        check(sustained_metrics["activity_late_total_spikes"] == 10, "late total")
        check(sustained_metrics["activity_last_quarter_total_spikes"] == 7, "quarter total")
        close(sustained_metrics["activity_population_cv"], 1.0 / 3.0, "population CV")
        close(sustained_metrics["activity_population_fano_factor"], 1.0 / 6.0, "Fano")
        check(sustained_metrics["activity_has_late_activity"], "sustained late activity")
        check(sustained_metrics["diagnostic_regime"] == "sustained", "sustained regime")
        check(sustained_metrics["plasticity_enabled"] is True, "plasticity enabled")
        check(
            sustained_metrics["plasticity_metrics_source"] == "plasticity_metrics.csv",
            "plasticity authoritative source",
        )
        close(
            sustained_metrics["plasticity_final_weight_mean"],
            102.0,
            "plasticity final mean",
        )
        check(sustained_metrics["homeostasis_enabled"] is True,
              "homeostasis enabled")
        close(sustained_metrics["homeostasis_population_rate_final"], 0.04,
              "homeostasis final rate")
        report_text = (sustained / "metrics_report.txt").read_text(encoding="utf-8")
        check("Plasticidade" in report_text, "plasticity report section")
        check("HOMEOSTASE E ESTABILIDADE" in report_text,
              "homeostasis text report section")
        html_report = (sustained / "metrics_report.html").read_text(encoding="utf-8")
        check("RELATORIO DE METRICAS" in html_report, "HTML metrics report")
        check("Plasticidade" in html_report, "HTML plasticity section")
        check("13. Homeostase" in html_report, "HTML homeostasis section")

        expected_counts = [7, 7, 6, 10]
        close(sustained_metrics["neuron_mean_spikes"], 7.5, "neuron mean")
        close(sustained_metrics["neuron_median_spikes"], 7.0, "neuron median")
        close(sustained_metrics["neuron_std_spikes"], 1.5, "neuron std")
        check(sustained_metrics["neuron_min_spikes"] == 6, "neuron minimum")
        check(sustained_metrics["neuron_max_spikes"] == 10, "neuron maximum")
        check(sustained_metrics["neuron_most_active"] == 3, "most active neuron")
        close(sustained_metrics["neuron_spike_gini"], 0.1, "neuron Gini")
        close(sustained_metrics["neuron_top_10_percent_spike_share"], 1.0 / 3.0, "top 10 share")
        probabilities = [value / 30.0 for value in expected_counts]
        expected_entropy = -sum(value * math.log2(value) for value in probabilities)
        close(sustained_metrics["neuron_spike_entropy"], expected_entropy, "spike entropy")
        close(
            sustained_metrics["neuron_normalized_spike_entropy"],
            expected_entropy / 2.0,
            "normalized spike entropy",
        )

        check(sustained_metrics["exc_total_spikes"] == 20, "EXC total")
        check(sustained_metrics["inh_total_spikes"] == 10, "INH total")
        close(sustained_metrics["exc_mean_spikes_per_neuron"], 20.0 / 3.0, "EXC mean")
        close(sustained_metrics["inh_mean_spikes_per_neuron"], 10.0, "INH mean")
        close(sustained_metrics["exc_inh_total_spike_ratio"], 2.0, "EXC/INH total ratio")
        close(sustained_metrics["exc_inh_rate_ratio"], 2.0 / 3.0, "EXC/INH rate ratio")

        check(sustained_metrics["neuron_detailed_spike_count"] == 7, "detailed count")
        check(sustained_metrics["neuron_detailed_first_spike"] == 0, "detailed first")
        check(sustained_metrics["neuron_detailed_last_spike"] == 18, "detailed last")
        close(sustained_metrics["neuron_detailed_mean_isi"], 3.0, "detailed ISI")
        for key, expected in (
            ("voltage_mean", -64.0),
            ("voltage_median", -64.0),
            ("voltage_min", -64.0),
            ("voltage_max", -64.0),
            ("voltage_std", 0.0),
            ("current_external_mean", 20.0),
            ("current_external_std", 0.0),
            ("current_synaptic_mean", 0.0),
            ("current_synaptic_std", 0.0),
            ("current_positive_synaptic_fraction", 0.0),
            ("current_negative_synaptic_fraction", 0.0),
        ):
            close(sustained_metrics[key], expected, key)

        isi_values = [3.0] * 17 + [2.0] * 9
        isi_mean = sum(isi_values) / len(isi_values)
        isi_variance = sum((value - isi_mean) ** 2 for value in isi_values) / len(isi_values)
        check(sustained_metrics["isi_neurons_with_valid_isi"] == 4, "valid ISI neurons")
        close(sustained_metrics["isi_mean"], isi_mean, "ISI mean")
        close(sustained_metrics["isi_median"], 3.0, "ISI median")
        close(sustained_metrics["isi_std"], math.sqrt(isi_variance), "ISI std")
        close(sustained_metrics["isi_mean_cv"], 0.0, "ISI mean CV")
        check((sustained / "metrics_neurons.csv").exists(), "full neuron metrics")
        check((sustained / "metrics_windows.csv").exists(), "full window metrics")
        check((sustained / "diagnostics_isi.png").exists(), "full ISI plot")

        burst_events = [[] for _ in range(20)]
        burst_events[8] = [(0, "EXC"), (1, "EXC"), (2, "EXC"), (3, "INH")]
        burst_events[9] = list(burst_events[8])
        burst = write_run("burst", burst_events)
        burst_metrics = analyze(burst, "basic")
        check(burst_metrics["burst_count"] == 1, "burst count")
        check(burst_metrics["burst_timesteps"] == 2, "burst duration")
        check(burst_metrics["burst_total_spikes"] == 8, "burst size")
        close(burst_metrics["burst_fraction"], 0.1, "burst fraction")
        close(burst_metrics["burst_spike_share"], 1.0, "burst share")

        dominant_events = [[(0, "EXC")] for _ in range(20)]
        dominant_events[0].extend([(1, "EXC"), (2, "EXC"), (3, "INH")])
        dominant = write_run("dominant", dominant_events)
        dominant_metrics = analyze(dominant, "basic")
        check(dominant_metrics["neuron_most_active"] == 0, "dominant neuron")
        close(dominant_metrics["neuron_top_10_percent_spike_share"], 20.0 / 23.0, "dominant share")
        close(dominant_metrics["neuron_spike_gini"], 57.0 / 92.0, "dominant Gini")
        close(dominant_metrics["neuron_dead_fraction"], 0.0, "dominant dead fraction")

        ei = write_run("ei", [[(0, "EXC"), (3, "INH")] for _ in range(10)])
        ei_metrics = analyze(ei, "basic")
        check(ei_metrics["exc_total_spikes"] == 10, "EXC count")
        check(ei_metrics["inh_total_spikes"] == 10, "INH count")
        close(ei_metrics["exc_mean_spikes_per_neuron"], 10.0 / 3.0, "EXC group mean")
        close(ei_metrics["inh_mean_spikes_per_neuron"], 10.0, "INH group mean")
        close(ei_metrics["exc_active_fraction"], 1.0 / 3.0, "EXC active fraction")
        close(ei_metrics["inh_active_fraction"], 1.0, "INH active fraction")
        close(ei_metrics["exc_inh_total_spike_ratio"], 1.0, "EXC/INH ratio")

        incomplete = write_run("incomplete", [[(0, "EXC")]], complete=False)
        analyze(incomplete, "basic")
        for filename in (
            "metrics.csv",
            "metrics_report.txt",
            "metrics_report.html",
            "diagnostics_overview.png",
        ):
            check((incomplete / filename).exists(), f"incomplete missing {filename}")
    finally:
        shutil.rmtree(TEMP_ROOT, ignore_errors=True)

    print("Run diagnostics validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
