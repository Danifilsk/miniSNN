from __future__ import annotations

from pathlib import Path
import csv
import shutil
import sys


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from html_report_common import (  # noqa: E402
    EVOLUTION_REPORT_FILENAME,
    ReportGenerationError,
    generate_evolution_report,
)


TEMP = ROOT / "build" / "Evolution Report Tests With Spaces"


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def write_csv(path: Path, fields: list[str], rows: list[dict[str, object]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


GENERATION_FIELDS = [
    "generation", "population_size", "fitness_best", "fitness_mean",
    "fitness_min", "fitness_max", "global_best_fitness", "best_individual_id",
    "global_best_individual_id", "diversity_mean_gene_std",
    "diversity_mean_pair_distance", "mutation_count",
]
GENOME_FIELDS = [
    "individual_id", "generation", "fitness_selection", "gene_index",
    "gene_name", "gene_kind", "value", "minimum", "maximum",
    "baseline_value", "connection_id", "parameter_path",
]
LINEAGE_FIELDS = [
    "child_generation", "child_individual_id", "parent_a_generation",
    "parent_a_id", "parent_b_generation", "parent_b_id", "operation",
    "crossover_applied", "mutation_count",
]


def make_run(path: Path, generations: int = 2, hostile: bool = False) -> None:
    path.mkdir(parents=True)
    generation_rows = []
    for index in range(generations):
        value = 0.5 + index * 0.2
        generation_rows.append({
            "generation": index, "population_size": 2, "fitness_best": value,
            "fitness_mean": value - 0.1, "fitness_min": value - 0.2,
            "fitness_max": value, "global_best_fitness": value,
            "best_individual_id": index * 2,
            "global_best_individual_id": index * 2,
            "diversity_mean_gene_std": 0.1,
            "diversity_mean_pair_distance": 0.2, "mutation_count": index,
        })
    write_csv(path / "generations.csv", GENERATION_FIELDS, generation_rows)
    gene_name = "<script>alert(1)</script>" if hostile else "plasticity.a_plus"
    write_csv(path / "best_genome.csv", GENOME_FIELDS, [{
        "individual_id": (generations - 1) * 2, "generation": generations - 1,
        "fitness_selection": generation_rows[-1]["fitness_best"], "gene_index": 0,
        "gene_name": gene_name, "gene_kind": "SCALAR_PARAMETER", "value": 0.2,
        "minimum": 0, "maximum": 1, "baseline_value": 0.1,
        "connection_id": "NA", "parameter_path": "plasticity.a_plus",
    }])
    write_csv(path / "lineage.csv", LINEAGE_FIELDS, [{
        "child_generation": 0, "child_individual_id": 0,
        "parent_a_generation": "NA", "parent_a_id": "NA",
        "parent_b_generation": "NA", "parent_b_id": "NA",
        "operation": "initialization", "crossover_applied": 0,
        "mutation_count": 0,
    }])
    for filename in (
        "individuals.csv", "replicates.csv", "fitness_terms.csv", "genomes.csv",
        "best_network_initial.csv",
    ):
        (path / filename).write_text("placeholder\n", encoding="utf-8")
    (path / "evolution_manifest.txt").write_text(
        "actual_experiment_name=" + ("<b>hostile</b>" if hostile else path.name) +
        "\nreplicates=2\ninheritance=darwinian\nparallel_evaluation=disabled\n",
        encoding="utf-8",
    )
    (path / "evolution_config_used.ini").write_text(
        f"generations={max(generations, 2)}\npopulation_size=2\nbase_scenario=C:\\private\\base.ini\n",
        encoding="utf-8",
    )
    (path / "base_scenario_used.ini").write_text(
        "topology=chain\nneurons=3\n", encoding="utf-8"
    )


def main() -> int:
    shutil.rmtree(TEMP, ignore_errors=True)
    TEMP.mkdir(parents=True)
    try:
        complete = TEMP / "Complete Evolution"
        make_run(complete)
        output = generate_evolution_report(complete)
        content = output.read_text(encoding="utf-8")
        check(output.name == EVOLUTION_REPORT_FILENAME, "report filename")
        check("Melhor fitness" in content and "Melhora absoluta" in content, "cards")
        check("Plasticidade durante a vida" in content, "Darwinian section")
        check('href="generations.csv"' in content, "relative links")
        check("http://" not in content and "https://" not in content, "external URL")
        check("C:\\private" not in content, "absolute path leaked")

        hostile = TEMP / "Hostile Evolution"
        make_run(hostile, hostile=True)
        content = generate_evolution_report(hostile).read_text(encoding="utf-8")
        check("&lt;script&gt;alert(1)&lt;/script&gt;" in content, "gene not escaped")
        check("<script>alert(1)</script>" not in content, "script injection")
        check("&lt;b&gt;hostile&lt;/b&gt;" in content, "manifest not escaped")

        partial = TEMP / "Partial Evolution"
        make_run(partial, generations=1)
        content = generate_evolution_report(partial).read_text(encoding="utf-8")
        check("CHECKPOINTED" in content, "partial status")

        invalid = TEMP / "Invalid Evolution"
        invalid.mkdir()
        previous = b"PREVIOUS REPORT"
        (invalid / EVOLUTION_REPORT_FILENAME).write_bytes(previous)
        (invalid / "generations.csv").write_text("generation\n", encoding="utf-8")
        try:
            generate_evolution_report(invalid)
        except ReportGenerationError as error:
            check(str(error).strip() != "", "invalid report message")
        else:
            raise AssertionError("invalid report accepted")
        check((invalid / EVOLUTION_REPORT_FILENAME).read_bytes() == previous,
              "previous report changed")
        check(not (invalid / (EVOLUTION_REPORT_FILENAME + ".tmp")).exists(),
              "temporary report left")
    finally:
        shutil.rmtree(TEMP, ignore_errors=True)
    print("Evolution HTML report validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
