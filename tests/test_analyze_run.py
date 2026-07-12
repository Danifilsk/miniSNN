from __future__ import annotations

from pathlib import Path
import csv
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


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    shutil.rmtree(TEMP_ROOT, ignore_errors=True)
    TEMP_ROOT.mkdir(parents=True)
    try:
        silent = write_run("silent", [[] for _ in range(20)])
        metrics = analyze(silent, "basic")
        check(metrics["activity_total_spikes"] == 0, "silent total")
        check(metrics["activity_fraction"] == 0, "silent activity fraction")
        check(metrics["silence_fraction"] == 1, "silent silence fraction")
        check(metrics["diagnostic_regime"] == "silent", "silent regime")

        sustained_events = []
        for step in range(20):
            events = [(step % 3, "EXC")]
            if step % 2 == 0:
                events.append((3, "INH"))
            sustained_events.append(events)
        sustained = write_run("sustained", sustained_events)
        sustained_metrics = analyze(sustained, "full")
        check(sustained_metrics["activity_has_late_activity"], "sustained late activity")
        check((sustained / "metrics_neurons.csv").exists(), "full neuron metrics")
        check((sustained / "metrics_windows.csv").exists(), "full window metrics")
        check((sustained / "diagnostics_isi.png").exists(), "full ISI plot")

        burst_events = [[] for _ in range(20)]
        burst_events[8] = [(0, "EXC"), (1, "EXC"), (2, "EXC"), (3, "INH")]
        burst_events[9] = list(burst_events[8])
        burst = write_run("burst", burst_events)
        burst_metrics = analyze(burst, "basic")
        check(burst_metrics["burst_count"] >= 1, "burst not detected")

        dominant_events = [[(0, "EXC")] for _ in range(20)]
        dominant_events[0].extend([(1, "EXC"), (2, "EXC"), (3, "INH")])
        dominant = write_run("dominant", dominant_events)
        dominant_metrics = analyze(dominant, "basic")
        check(dominant_metrics["neuron_most_active"] == 0, "dominant neuron")
        check(dominant_metrics["neuron_top_10_percent_spike_share"] > 0.5, "dominant share")

        ei = write_run("ei", [[(0, "EXC"), (3, "INH")] for _ in range(10)])
        ei_metrics = analyze(ei, "basic")
        check(ei_metrics["exc_total_spikes"] == 10, "EXC count")
        check(ei_metrics["inh_total_spikes"] == 10, "INH count")

        incomplete = write_run("incomplete", [[(0, "EXC")]], complete=False)
        analyze(incomplete, "basic")
        for filename in ("metrics.csv", "metrics_report.txt", "diagnostics_overview.png"):
            check((incomplete / filename).exists(), f"incomplete missing {filename}")
    finally:
        shutil.rmtree(TEMP_ROOT, ignore_errors=True)

    print("Run diagnostics validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
