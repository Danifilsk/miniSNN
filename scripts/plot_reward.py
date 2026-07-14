#!/usr/bin/env python3
"""Generate a local overview of reward, punishment, and eligibility."""

from __future__ import annotations

import argparse
import math
from pathlib import Path
import sys

import matplotlib.pyplot as plt
import pandas as pd


REQUIRED_FILES = (
    "reward_metrics.csv",
    "reward_events.csv",
    "reward_history.csv",
    "eligibility_history.csv",
    "reward_connections.csv",
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
    report_path = run_dir / "reward_report.html"
    if not report_path.is_file():
        return
    text = report_path.read_text(encoding="utf-8")
    marker = '<img src="reward_overview.png" alt="Panorama de recompensa">'
    if marker not in text:
        text = text.replace("</body>", f"<h2>Panorama</h2>{marker}</body>")
        report_path.write_text(text, encoding="utf-8", newline="\n")


def generate(run_dir: Path) -> Path:
    missing = [name for name in REQUIRED_FILES if not (run_dir / name).is_file()]
    if missing:
        raise FileNotFoundError("dados de reward ausentes: " + ", ".join(missing))

    metrics = pd.read_csv(run_dir / "reward_metrics.csv")
    events = pd.read_csv(run_dir / "reward_events.csv")
    history = pd.read_csv(run_dir / "reward_history.csv")
    eligibility = pd.read_csv(run_dir / "eligibility_history.csv")
    connections = pd.read_csv(run_dir / "reward_connections.csv")

    if metrics.empty:
        raise ValueError("reward_metrics.csv: nenhuma linha")
    event_columns = {"step", "applied_reward", "weight_signed_change"}
    history_columns = {
        "step",
        "eligibility_mean",
        "eligibility_min",
        "eligibility_max",
        "weight_signed_change_cumulative",
        "weight_absolute_change_cumulative",
    }
    eligibility_columns = {"step", "connection_id", "eligibility"}
    connection_columns = {"connection_id", "net_weight_change"}
    require_columns(events, event_columns, "reward_events.csv")
    require_columns(history, history_columns, "reward_history.csv")
    require_columns(eligibility, eligibility_columns, "eligibility_history.csv")
    require_columns(connections, connection_columns, "reward_connections.csv")
    require_finite(events, event_columns, "reward_events.csv")
    require_finite(history, history_columns, "reward_history.csv")
    require_finite(eligibility, eligibility_columns, "eligibility_history.csv")
    require_finite(connections, connection_columns, "reward_connections.csv")

    figure, axes = plt.subplots(2, 2, figsize=(12, 8))

    if events.empty:
        axes[0, 0].axhline(0.0, linewidth=0.8)
        axes[0, 0].text(0.5, 0.5, "Nenhum reward aplicado", ha="center", va="center")
    else:
        axes[0, 0].stem(events["step"], events["applied_reward"], basefmt=" ")
        axes[0, 0].axhline(0.0, linewidth=0.8)
    axes[0, 0].set_title("Reward aplicado")
    axes[0, 0].set_xlabel("Step")
    axes[0, 0].set_ylabel("Sinal")

    axes[0, 1].plot(history["step"], history["eligibility_mean"], label="Media")
    axes[0, 1].plot(history["step"], history["eligibility_min"], label="Minima")
    axes[0, 1].plot(history["step"], history["eligibility_max"], label="Maxima")
    axes[0, 1].axhline(0.0, linewidth=0.8)
    axes[0, 1].set_title("Elegibilidade")
    axes[0, 1].set_xlabel("Step")
    axes[0, 1].legend()

    axes[1, 0].plot(
        history["step"],
        history["weight_signed_change_cumulative"],
        label="Assinada",
    )
    axes[1, 0].plot(
        history["step"],
        history["weight_absolute_change_cumulative"],
        label="Absoluta",
    )
    axes[1, 0].axhline(0.0, linewidth=0.8)
    axes[1, 0].set_title("Mudanca acumulada de pesos")
    axes[1, 0].set_xlabel("Step")
    axes[1, 0].legend()

    ranked = connections.reindex(
        connections["net_weight_change"].abs().sort_values(ascending=False).index
    ).head(20)
    if ranked.empty:
        axes[1, 1].text(0.5, 0.5, "Nenhuma conexao", ha="center", va="center")
    else:
        axes[1, 1].bar(
            [str(int(value)) for value in ranked["connection_id"]],
            ranked["net_weight_change"],
        )
    axes[1, 1].axhline(0.0, linewidth=0.8)
    axes[1, 1].set_title("Conexoes mais modificadas")
    axes[1, 1].set_xlabel("Connection ID")
    axes[1, 1].set_ylabel("Delta de peso")

    figure.suptitle("miniSNN: recompensa, punicao e elegibilidade")
    figure.tight_layout()
    output_path = run_dir / "reward_overview.png"
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
        print(f"Erro ao gerar grafico de reward: {error}", file=sys.stderr)
        return 1
    print(f"Grafico de reward gerado: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
