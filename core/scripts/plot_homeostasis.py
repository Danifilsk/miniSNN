#!/usr/bin/env python3
"""Generate the local homeostasis overview from runner CSV artifacts."""

from __future__ import annotations

import argparse
import math
from pathlib import Path
import sys

import matplotlib.pyplot as plt
import pandas as pd


REQUIRED_FILES = (
    "homeostasis_metrics.csv",
    "homeostasis_history.csv",
    "threshold_history.csv",
    "homeostasis_neurons.csv",
)


def require_columns(frame: pd.DataFrame, columns: set[str], filename: str) -> None:
    missing = sorted(columns.difference(frame.columns))
    if missing:
        raise ValueError(f"{filename}: colunas ausentes: {', '.join(missing)}")


def require_finite(frame: pd.DataFrame, columns: set[str], filename: str) -> None:
    for column in columns:
        values = pd.to_numeric(frame[column], errors="raise")
        if not values.map(math.isfinite).all():
            raise ValueError(f"{filename}: valor nao finito em {column}")


def update_html(run_dir: Path) -> None:
    report_path = run_dir / "homeostasis_report.html"
    if not report_path.exists():
        return
    text = report_path.read_text(encoding="utf-8")
    marker = '<img src="homeostasis_overview.png" alt="Panorama homeostatico">'
    if marker not in text:
        text = text.replace("</body>", f"<h2>Panorama</h2>{marker}</body>")
        report_path.write_text(text, encoding="utf-8")


def generate(run_dir: Path) -> Path:
    missing = [name for name in REQUIRED_FILES if not (run_dir / name).is_file()]
    if missing:
        raise FileNotFoundError(
            "dados homeostaticos ausentes: " + ", ".join(missing)
        )

    history = pd.read_csv(run_dir / "homeostasis_history.csv")
    thresholds = pd.read_csv(run_dir / "threshold_history.csv")
    neurons = pd.read_csv(run_dir / "homeostasis_neurons.csv")
    metrics = pd.read_csv(run_dir / "homeostasis_metrics.csv")

    history_columns = {
        "step",
        "population_rate",
        "target_rate",
        "rate_error",
        "threshold_mean",
        "inhibitory_gain",
        "incoming_exc_sum_mean",
    }
    threshold_columns = {"step", "neuron_id", "effective_threshold"}
    neuron_columns = {"initial_incoming_exc_sum", "final_incoming_exc_sum"}
    require_columns(history, history_columns, "homeostasis_history.csv")
    require_columns(thresholds, threshold_columns, "threshold_history.csv")
    require_columns(neurons, neuron_columns, "homeostasis_neurons.csv")
    if metrics.empty:
        raise ValueError("homeostasis_metrics.csv: nenhuma linha")
    require_finite(history, history_columns, "homeostasis_history.csv")
    require_finite(thresholds, threshold_columns, "threshold_history.csv")
    require_finite(neurons, neuron_columns, "homeostasis_neurons.csv")

    figure, axes = plt.subplots(2, 2, figsize=(12, 8))

    axes[0, 0].plot(history["step"], history["population_rate"], label="Taxa observada")
    axes[0, 0].plot(history["step"], history["target_rate"], label="Taxa-alvo")
    axes[0, 0].set_title("Taxa populacional")
    axes[0, 0].set_xlabel("Step")
    axes[0, 0].set_ylabel("Taxa")
    axes[0, 0].legend()

    axes[0, 1].plot(history["step"], history["threshold_mean"], label="Media")
    for neuron_id, group in thresholds.groupby("neuron_id", sort=True):
        axes[0, 1].plot(
            group["step"],
            group["effective_threshold"],
            alpha=0.35,
            label=f"N{int(neuron_id)}" if neuron_id < 4 else None,
        )
    axes[0, 1].set_title("Threshold efetivo")
    axes[0, 1].set_xlabel("Step")
    axes[0, 1].set_ylabel("Threshold")

    axes[1, 0].plot(history["step"], history["inhibitory_gain"], label="Ganho INH")
    axes[1, 0].plot(history["step"], history["rate_error"], label="Erro de taxa")
    axes[1, 0].axhline(0.0, linewidth=0.8)
    axes[1, 0].set_title("Controle global")
    axes[1, 0].set_xlabel("Step")
    axes[1, 0].legend()

    neuron_ids = neurons["neuron_id"] if "neuron_id" in neurons else neurons.index
    axes[1, 1].plot(
        neuron_ids,
        neurons["initial_incoming_exc_sum"],
        marker="o",
        label="Inicial",
    )
    axes[1, 1].plot(
        neuron_ids,
        neurons["final_incoming_exc_sum"],
        marker="x",
        label="Final",
    )
    axes[1, 1].set_title("Soma de entrada EXC")
    axes[1, 1].set_xlabel("Neuronio")
    axes[1, 1].legend()

    figure.suptitle("miniSNN: panorama de homeostase")
    figure.tight_layout()
    output_path = run_dir / "homeostasis_overview.png"
    figure.savefig(output_path, dpi=150)
    plt.close(figure)
    update_html(run_dir)
    return output_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_directory", type=Path)
    args = parser.parse_args()

    try:
        output = generate(args.run_directory.resolve())
    except (OSError, ValueError, pd.errors.ParserError) as error:
        print(f"Erro ao gerar grafico de homeostase: {error}", file=sys.stderr)
        return 1

    print(f"Grafico homeostatico gerado: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
