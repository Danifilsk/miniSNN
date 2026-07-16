#!/usr/bin/env python3
"""Generate a deterministic adaptive-topology overview for a miniSNN run."""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path
import sys

import matplotlib.pyplot as plt


def _read_rows(path: Path, required: set[str]) -> list[dict[str, str]]:
    if not path.is_file():
        raise FileNotFoundError(f"arquivo obrigatorio ausente: {path.name}")
    with path.open("r", encoding="utf-8", newline="") as file:
        reader = csv.DictReader(file)
        fields = set(reader.fieldnames or [])
        missing = sorted(required.difference(fields))
        if missing:
            raise ValueError(
                f"{path.name}: colunas ausentes: {', '.join(missing)}"
            )
        return list(reader)


def _number(row: dict[str, str], field: str, filename: str) -> float:
    try:
        value = float(row[field])
    except (KeyError, ValueError) as error:
        raise ValueError(f"{filename}: valor invalido em {field}") from error
    if not math.isfinite(value):
        raise ValueError(f"{filename}: valor nao finito em {field}")
    return value


def _edges(rows: list[dict[str, str]], filename: str) -> list[tuple[int, int, int]]:
    result = []
    for row in rows:
        source = int(_number(row, "source", filename))
        target = int(_number(row, "target", filename))
        delay = int(_number(row, "delay", filename))
        result.append((source, target, delay))
    return result


def _draw_graph(
    axis: plt.Axes,
    initial: list[tuple[int, int, int]],
    final: list[tuple[int, int, int]],
) -> None:
    all_edges = initial + final
    neuron_count = max((max(source, target) for source, target, _ in all_edges),
                       default=-1) + 1
    if neuron_count <= 32:
        count = max(neuron_count, 1)
        points = [
            (math.cos(2.0 * math.pi * index / count),
             math.sin(2.0 * math.pi * index / count))
            for index in range(count)
        ]
        for source, target, _ in initial:
            x = [points[source][0], points[target][0]]
            y = [points[source][1], points[target][1]]
            axis.plot(x, y, linestyle=":", linewidth=0.8, alpha=0.45)
        for source, target, _ in final:
            x = [points[source][0], points[target][0]]
            y = [points[source][1], points[target][1]]
            axis.annotate(
                "", xy=(x[1], y[1]), xytext=(x[0], y[0]),
                arrowprops={"arrowstyle": "->", "linewidth": 0.8, "alpha": 0.65},
            )
        axis.scatter([point[0] for point in points],
                     [point[1] for point in points], s=42, zorder=3)
        for index, (x, y) in enumerate(points):
            axis.text(x, y, str(index), ha="center", va="center", fontsize=7)
        axis.set_aspect("equal")
        axis.set_axis_off()
        axis.set_title("Grafo: inicial pontilhada, final com setas")
        return

    sampled = final[:2000]
    axis.scatter([source for source, _, _ in sampled],
                 [target for _, target, _ in sampled], s=7, alpha=0.55)
    axis.set_xlabel("Origem")
    axis.set_ylabel("Destino")
    axis.set_title(
        f"Matriz esparsa final (amostra {len(sampled)}/{len(final)} arestas)"
    )


def generate(run_directory: Path | str) -> Path:
    run_dir = Path(run_directory)
    initial_rows = _read_rows(
        run_dir / "topology_initial.csv", {"source", "target", "delay"}
    )
    final_rows = _read_rows(
        run_dir / "topology_final.csv", {"source", "target", "delay"}
    )
    history = _read_rows(
        run_dir / "topology_history.csv",
        {"step", "connection_count", "added_cumulative", "removed_cumulative"},
    )
    events = _read_rows(
        run_dir / "structural_plasticity_events.csv",
        {"step", "event_type", "status"},
    )
    initial = _edges(initial_rows, "topology_initial.csv")
    final = _edges(final_rows, "topology_final.csv")

    figure, axes = plt.subplots(2, 2, figsize=(13, 9))
    steps = [_number(row, "step", "topology_history.csv") for row in history]
    axes[0, 0].plot(
        steps,
        [_number(row, "connection_count", "topology_history.csv") for row in history],
        label="Conexoes",
    )
    axes[0, 0].set_title("Complexidade estrutural ao longo do tempo")
    axes[0, 0].set_xlabel("Timestep")
    axes[0, 0].set_ylabel("Conexoes")
    axes[0, 0].legend()

    applied = [row for row in events if row["status"] == "applied"]
    event_names = sorted({row["event_type"] for row in applied})
    for index, event_name in enumerate(event_names):
        selected = [row for row in applied if row["event_type"] == event_name]
        axes[0, 1].scatter(
            [_number(row, "step", "structural_plasticity_events.csv")
             for row in selected],
            [index] * len(selected),
            label=event_name,
        )
    axes[0, 1].set_yticks(range(len(event_names)), event_names)
    axes[0, 1].set_title("Eventos estruturais aplicados")
    axes[0, 1].set_xlabel("Timestep")

    delays = [delay for _, _, delay in final]
    bins = range(min(delays, default=1), max(delays, default=1) + 2)
    axes[1, 0].hist(delays, bins=bins, align="left", rwidth=0.8)
    axes[1, 0].set_title("Distribuicao de delays finais")
    axes[1, 0].set_xlabel("Delay")
    axes[1, 0].set_ylabel("Conexoes")

    _draw_graph(axes[1, 1], initial, final)
    figure.suptitle(
        f"Topologia adaptativa: {len(initial)} conexoes iniciais, "
        f"{len(final)} finais"
    )
    figure.tight_layout()
    output = run_dir / "topology_overview.png"
    figure.savefig(output, dpi=150)
    plt.close(figure)
    return output


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_directory", type=Path)
    args = parser.parse_args(argv)
    try:
        output = generate(args.run_directory)
    except (FileNotFoundError, ValueError, OSError) as error:
        print(f"Erro ao gerar grafico de topologia: {error}", file=sys.stderr)
        return 1
    print(f"Grafico de topologia gerado: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
