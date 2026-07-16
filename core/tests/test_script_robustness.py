from __future__ import annotations

from pathlib import Path
import csv
import os
import shutil
import sys

os.environ.setdefault("MPLBACKEND", "Agg")

PROJECT_ROOT = Path(__file__).resolve().parents[1]
SCRIPTS_DIR = PROJECT_ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS_DIR))

from analyze_run import analyze  # noqa: E402
from compare_runs import analyze_run as compare_analyze_run  # noqa: E402
from plot_neuron import read_neuron_csv  # noqa: E402
from plot_scenario import (  # noqa: E402
    POPULATION_COLUMNS,
    RASTER_COLUMNS,
    read_csv,
)


TEMP_ROOT = PROJECT_ROOT / "build" / "script robustness with spaces"


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def write_metadata(run: Path, steps: int = 1) -> None:
    (run / "config_used.ini").write_text(
        "[run]\nrun_name = robustness\n"
        "[network]\ntopology = chain\nneurons = 1\ninhibitory_fraction = 0\nseed = 1\n"
        f"[simulation]\nsteps = {steps}\ndt = 0.1\nv_threshold = -50\n"
        "[recording]\nrecord_neuron = 0\n"
        "[diagnostics]\nlevel = basic\n",
        encoding="utf-8",
    )
    (run / "summary.txt").write_text(
        f"run_name=robustness\nneurons=1\ninhibitory_count=0\nsteps={steps}\n",
        encoding="utf-8",
    )


def expect_analysis_error(run: Path, label: str) -> None:
    try:
        analyze(run, "basic")
    except ValueError as error:
        check("population.csv" in str(error), f"{label}: unclear error")
        return
    raise AssertionError(f"{label}: invalid run was accepted")


def test_analysis_inputs() -> None:
    missing = TEMP_ROOT / "missing population"
    missing.mkdir(parents=True)
    write_metadata(missing)
    expect_analysis_error(missing, "missing population")

    empty = TEMP_ROOT / "empty population"
    empty.mkdir()
    write_metadata(empty)
    (empty / "population.csv").write_text("", encoding="utf-8")
    expect_analysis_error(empty, "empty population")

    bad_columns = TEMP_ROOT / "bad columns"
    bad_columns.mkdir()
    write_metadata(bad_columns)
    (bad_columns / "population.csv").write_text("tempo,wrong\n0,0\n", encoding="utf-8")
    expect_analysis_error(bad_columns, "missing columns")

    nonfinite = TEMP_ROOT / "nonfinite population"
    nonfinite.mkdir()
    write_metadata(nonfinite)
    (nonfinite / "population.csv").write_text(
        "tempo,spikes_total,spikes_exc,spikes_inh,mean_potential,mean_syn_current\n"
        "0,inf,0,0,-65,0\n",
        encoding="utf-8",
    )
    expect_analysis_error(nonfinite, "nonfinite population")

    valid = TEMP_ROOT / "valid one row extra columns"
    valid.mkdir()
    write_metadata(valid)
    (valid / "population.csv").write_text(
        "extra,mean_syn_current,spikes_inh,tempo,mean_potential,spikes_total,spikes_exc\n"
        "kept,0,0,0,-65,0,0\n",
        encoding="utf-8",
    )
    (valid / "raster.csv").write_text("tipo,neuronio,tempo,extra\n", encoding="utf-8")
    (valid / "neuron_0.csv").write_text(
        "spike,corrente_sinaptica,V,tempo,corrente_externa,extra\n"
        "0,0,-65,0,0,kept\n",
        encoding="utf-8",
    )
    metrics = analyze(valid, "basic")
    check(metrics["activity_total_spikes"] == 0, "one-line silent total")
    check(metrics["diagnostic_regime"] == "silent", "one-line silent regime")

    old_metrics, _, warnings = compare_analyze_run(valid)
    check(old_metrics is not None, "legacy run without manifest/old metrics rejected")
    check(isinstance(warnings, list), "comparison warnings contract")


def test_plot_readers() -> None:
    run = TEMP_ROOT / "plot readers"
    run.mkdir()
    check(read_neuron_csv(run / "missing.csv") is None, "missing neuron CSV accepted")
    valid_neuron = run / "valid.csv"
    valid_neuron.write_text(
        "extra,corrente_sinaptica,tempo,spike,V,corrente_externa\n"
        "x,0,0,0,-65,20\n",
        encoding="utf-8",
    )
    data = read_neuron_csv(valid_neuron)
    check(data is not None and len(data) == 1, "valid reordered neuron CSV")

    invalid_neuron = run / "invalid.csv"
    invalid_neuron.write_text(
        "tempo,V,spike,corrente_externa,corrente_sinaptica\n0,inf,0,20,0\n",
        encoding="utf-8",
    )
    check(read_neuron_csv(invalid_neuron) is None, "infinite neuron value accepted")

    population = run / "population.csv"
    with population.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(["extra", *reversed(sorted(POPULATION_COLUMNS))])
        values = {
            "tempo": 0,
            "spikes_total": 0,
            "spikes_exc": 0,
            "spikes_inh": 0,
            "mean_potential": -65,
            "mean_syn_current": 0,
        }
        writer.writerow(["x", *(values[key] for key in reversed(sorted(POPULATION_COLUMNS)))])
    check(read_csv(population, POPULATION_COLUMNS) is not None, "plot population ordering")

    raster = run / "raster.csv"
    raster.write_text("tipo,extra,neuronio,tempo\n", encoding="utf-8")
    raster_data = read_csv(raster, RASTER_COLUMNS)
    check(raster_data is not None and raster_data.empty, "empty raster with header")
    check(read_csv(run / "missing_raster.csv", RASTER_COLUMNS) is None, "missing raster accepted")

    bad_population = run / "bad_population.csv"
    bad_population.write_text(
        "tempo,spikes_total,spikes_exc,spikes_inh,mean_potential,mean_syn_current\n"
        "0,NaN,0,0,-65,0\n",
        encoding="utf-8",
    )
    check(read_csv(bad_population, POPULATION_COLUMNS) is None, "NaN plot value accepted")


def main() -> int:
    shutil.rmtree(TEMP_ROOT, ignore_errors=True)
    TEMP_ROOT.mkdir(parents=True)
    try:
        test_analysis_inputs()
        test_plot_readers()
    finally:
        shutil.rmtree(TEMP_ROOT, ignore_errors=True)

    print("Python script robustness validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
