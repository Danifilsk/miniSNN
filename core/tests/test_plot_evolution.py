from __future__ import annotations

from pathlib import Path
import csv
import shutil
import sys


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from plot_evolution import generate  # noqa: E402


TEMP = ROOT / "build" / "Evolution Plot Tests With Spaces"


GENERATION_FIELDS = [
    "generation", "population_size", "valid_individual_count",
    "invalid_individual_count", "fitness_best", "fitness_mean", "fitness_min",
    "fitness_max", "fitness_std", "replicate_fitness_mean",
    "replicate_fitness_std_mean", "best_individual_id",
    "global_best_individual_id", "global_best_fitness",
    "diversity_mean_gene_std", "diversity_mean_pair_distance", "elite_count",
    "crossover_child_count", "clone_child_count", "mutated_child_count",
    "mutation_count", "evaluation_seconds", "generation_wall_seconds",
]


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def write_csv(path: Path, fields: list[str], rows: list[dict[str, object]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def generation(index: int, best: float = 0.5) -> dict[str, object]:
    return {
        "generation": index, "population_size": 2,
        "valid_individual_count": 2, "invalid_individual_count": 0,
        "fitness_best": best, "fitness_mean": best, "fitness_min": best,
        "fitness_max": best, "fitness_std": 0,
        "replicate_fitness_mean": best, "replicate_fitness_std_mean": 0,
        "best_individual_id": index * 2, "global_best_individual_id": 0,
        "global_best_fitness": max(0.5, best),
        "diversity_mean_gene_std": 0, "diversity_mean_pair_distance": 0,
        "elite_count": 1, "crossover_child_count": 1 if index else 0,
        "clone_child_count": 0, "mutated_child_count": 1 if index else 0,
        "mutation_count": index, "evaluation_seconds": 0,
        "generation_wall_seconds": 0,
    }


GENOME_FIELDS = [
    "generation", "individual_id", "gene_index", "gene_name", "gene_kind",
    "value", "minimum", "maximum", "baseline_value", "connection_id",
    "parameter_path",
]


def write_genomes(path: Path, generations: int) -> None:
    rows = []
    for index in range(generations):
        rows.extend([
            {
                "generation": index, "individual_id": index * 2, "gene_index": 0,
                "gene_name": "plasticity.a_plus", "gene_kind": "SCALAR_PARAMETER",
                "value": 0.1 + index * 0.01, "minimum": 0, "maximum": 1,
                "baseline_value": 0.1, "connection_id": "NA",
                "parameter_path": "plasticity.a_plus",
            },
            {
                "generation": index, "individual_id": index * 2, "gene_index": 1,
                "gene_name": "exc_weight_0", "gene_kind": "EXC_CONNECTION_WEIGHT",
                "value": 200 + index, "minimum": 0, "maximum": 500,
                "baseline_value": 200, "connection_id": 0,
                "parameter_path": "NA",
            },
        ])
    write_csv(path, GENOME_FIELDS, rows)


def main() -> int:
    shutil.rmtree(TEMP, ignore_errors=True)
    TEMP.mkdir(parents=True)
    try:
        one = TEMP / "One Generation"
        one.mkdir()
        write_csv(one / "generations.csv", GENERATION_FIELDS, [generation(0)])
        write_genomes(one / "genomes.csv", 1)
        output = generate(one)
        check(output.is_file() and output.stat().st_size > 1000, "one-generation PNG")

        improved = TEMP / "Improved Run"
        improved.mkdir()
        write_csv(
            improved / "generations.csv", GENERATION_FIELDS,
            [generation(0), generation(1, 0.7), generation(2, 0.8)],
        )
        write_genomes(improved / "genomes.csv", 3)
        output = generate(improved)
        check(output.is_file() and output.stat().st_size > 1000, "improved PNG")

        partial = TEMP / "Checkpoint Partial"
        partial.mkdir()
        write_csv(partial / "generations.csv", GENERATION_FIELDS, [generation(0)])
        output = generate(partial)
        check(output.is_file(), "partial run without genomes")

        invalid = TEMP / "Invalid"
        invalid.mkdir()
        (invalid / "generations.csv").write_text(
            "generation,fitness_best\n0,nan\n", encoding="utf-8"
        )
        try:
            generate(invalid)
        except ValueError as error:
            check(str(error).strip() != "", "invalid data message")
        else:
            raise AssertionError("invalid generations accepted")
        check(not (invalid / "evolution_overview.png").exists(), "partial invalid PNG")
    finally:
        shutil.rmtree(TEMP, ignore_errors=True)
    print("Evolution plot validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
