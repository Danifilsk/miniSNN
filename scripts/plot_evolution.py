#!/usr/bin/env python3
"""Generate a deterministic overview of a miniSNN evolution experiment."""

from __future__ import annotations

import argparse
import math
from pathlib import Path
import sys

import matplotlib.pyplot as plt
import pandas as pd


GENERATION_COLUMNS = {
    "generation", "fitness_best", "fitness_mean", "fitness_min", "fitness_max",
    "diversity_mean_gene_std", "diversity_mean_pair_distance",
    "mutation_count", "crossover_child_count",
}
GENOME_COLUMNS = {
    "generation", "individual_id", "gene_index", "gene_name", "gene_kind", "value",
}


def _require_columns(frame: pd.DataFrame, required: set[str], filename: str) -> None:
    missing = sorted(required.difference(frame.columns))
    if missing:
        raise ValueError(f"{filename}: colunas ausentes: {', '.join(missing)}")


def _finite(frame: pd.DataFrame, columns: set[str], filename: str) -> None:
    for column in columns:
        values = pd.to_numeric(frame[column], errors="raise")
        if not values.map(math.isfinite).all():
            raise ValueError(f"{filename}: valor nao finito em {column}")


def _selected_gene_names(genomes: pd.DataFrame, limit: int = 8) -> list[str]:
    metadata = genomes[["gene_index", "gene_name", "gene_kind"]].drop_duplicates()
    metadata = metadata.sort_values("gene_index", kind="stable")
    scalar = metadata[metadata["gene_kind"].eq("SCALAR_PARAMETER")]
    selected = list(scalar["gene_name"].head(limit))
    remaining = metadata[~metadata["gene_name"].isin(selected)]
    slots = limit - len(selected)
    if slots > 0 and not remaining.empty:
        if len(remaining) <= slots:
            selected.extend(remaining["gene_name"].tolist())
        else:
            positions = [round(index * (len(remaining) - 1) / (slots - 1))
                         if slots > 1 else 0 for index in range(slots)]
            selected.extend(remaining.iloc[positions]["gene_name"].tolist())
    return selected


def generate(run_directory: Path | str) -> Path:
    run_dir = Path(run_directory)
    generations_path = run_dir / "generations.csv"
    genomes_path = run_dir / "genomes.csv"
    if not generations_path.is_file():
        raise FileNotFoundError("arquivo obrigatorio ausente: generations.csv")

    generations = pd.read_csv(generations_path)
    if generations.empty:
        raise ValueError("generations.csv: nenhuma geracao")
    _require_columns(generations, GENERATION_COLUMNS, "generations.csv")
    _finite(generations, GENERATION_COLUMNS, "generations.csv")
    generations = generations.sort_values("generation", kind="stable")
    structural = (
        "connections_best" in generations.columns
        and pd.to_numeric(generations["connections_best"], errors="coerce").notna().any()
    )

    genomes: pd.DataFrame | None = None
    if genomes_path.is_file():
        candidate = pd.read_csv(genomes_path)
        if not candidate.empty:
            _require_columns(candidate, GENOME_COLUMNS, "genomes.csv")
            _finite(candidate, {"generation", "individual_id", "gene_index", "value"},
                    "genomes.csv")
            genomes = candidate

    figure, axes = plt.subplots(2, 2, figsize=(13, 9))
    generation = generations["generation"]
    axes[0, 0].fill_between(
        generation,
        generations["fitness_min"],
        generations["fitness_max"],
        alpha=0.2,
        label="Faixa",
    )
    axes[0, 0].plot(generation, generations["fitness_best"], label="Melhor")
    axes[0, 0].plot(generation, generations["fitness_mean"], label="Media")
    axes[0, 0].set_title("Fitness por geracao")
    axes[0, 0].set_xlabel("Geracao")
    axes[0, 0].set_ylabel("Fitness")
    axes[0, 0].legend()

    if structural:
        axes[0, 1].plot(generation, generations["topology_diversity_mean_distance"],
                        label="Distancia Jaccard media")
        axes[0, 1].plot(generation, generations["topology_unique_count"],
                        label="Topologias unicas")
        axes[0, 1].set_title("Diversidade topologica")
    else:
        axes[0, 1].plot(generation, generations["diversity_mean_gene_std"],
                        label="Media do desvio por gene")
        axes[0, 1].plot(generation, generations["diversity_mean_pair_distance"],
                        label="Distancia media entre pares")
        axes[0, 1].set_title("Diversidade genetica")
    axes[0, 1].set_xlabel("Geracao")
    axes[0, 1].legend()

    if structural:
        for column, label in [
            ("add_count", "Add"), ("remove_count", "Remove"),
            ("rewire_count", "Rewire"),
            ("delay_mutation_count", "Delay"),
        ]:
            axes[1, 0].plot(generation, generations[column], label=label)
    else:
        axes[1, 0].plot(generation, generations["mutation_count"], label="Mutacoes")
        axes[1, 0].plot(generation, generations["crossover_child_count"],
                        label="Filhos com crossover")
    axes[1, 0].set_title("Operadores evolutivos")
    axes[1, 0].set_xlabel("Geracao")
    axes[1, 0].set_ylabel("Quantidade")
    axes[1, 0].legend()

    if structural:
        axes[1, 1].plot(generation, generations["connections_best"],
                        label="Melhor")
        axes[1, 1].plot(generation, generations["connections_mean"],
                        label="Media")
        axes[1, 1].fill_between(
            generation, generations["connections_min"],
            generations["connections_max"], alpha=0.2, label="Faixa",
        )
        axes[1, 1].legend(fontsize="small")
        axes[1, 1].set_title("Conexoes por geracao")
        axes[1, 1].set_ylabel("Conexoes")
    elif genomes is None:
        axes[1, 1].text(0.5, 0.5, "Genomas parciais ou indisponiveis",
                        ha="center", va="center", transform=axes[1, 1].transAxes)
    else:
        best_ids = dict(zip(generations["generation"],
                            generations.get("best_individual_id",
                                            pd.Series(dtype=float))))
        selected = _selected_gene_names(genomes)
        for gene_name in selected:
            rows = genomes[genomes["gene_name"].eq(gene_name)].copy()
            if best_ids:
                rows = rows[rows.apply(
                    lambda row: best_ids.get(row["generation"]) == row["individual_id"],
                    axis=1,
                )]
            rows = rows.sort_values("generation", kind="stable").drop_duplicates(
                "generation", keep="last"
            )
            if not rows.empty:
                axes[1, 1].plot(rows["generation"], rows["value"], label=gene_name)
        if selected:
            axes[1, 1].legend(fontsize="small")
    if not structural:
        axes[1, 1].set_title("Genes selecionados do melhor da geracao")
    axes[1, 1].set_xlabel("Geracao")
    axes[1, 1].set_ylabel("Valor")

    figure.suptitle("miniSNN: panorama de neuroevolucao")
    figure.tight_layout()
    output = run_dir / "evolution_overview.png"
    figure.savefig(output, dpi=150)
    plt.close(figure)
    return output


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("experiment_directory", type=Path)
    args = parser.parse_args()
    try:
        output = generate(args.experiment_directory.resolve())
    except (OSError, ValueError, pd.errors.ParserError) as error:
        print(f"Erro ao gerar grafico evolutivo: {error}", file=sys.stderr)
        return 1
    print(f"Grafico evolutivo gerado: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
