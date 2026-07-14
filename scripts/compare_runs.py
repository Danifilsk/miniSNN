from __future__ import annotations

from datetime import datetime
from pathlib import Path
import argparse
import csv
import math
import re
import sys

from metrics_common import (
    basic_metrics as shared_basic_metrics,
    read_config,
    read_plasticity_metrics,
    read_homeostasis_metrics,
)

try:
    import matplotlib.pyplot as plt
    import pandas as pd
except ModuleNotFoundError as error:
    print(
        "Erro: dependencia Python nao encontrada: "
        f"{error.name}. Instale pandas e matplotlib."
    )
    sys.exit(1)


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT_ROOT = PROJECT_ROOT / "results" / "comparisons"

SPIKE_COLUMN_ALIASES = (
    "spikes_total",
    "spikes",
    "spike_count",
    "population_spikes",
    "total_spikes_step",
)

TIME_COLUMN_ALIASES = ("tempo", "time", "t", "step")


def sanitize_name(name: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9_-]+", "_", name.strip())
    cleaned = cleaned.strip("_")
    return cleaned[:80] or automatic_comparison_name()


def automatic_comparison_name() -> str:
    return datetime.now().strftime("comparison_%Y%m%d_%H%M%S")


def timestamp_name() -> str:
    return datetime.now().strftime("%Y%m%d_%H%M%S")


def unique_output_dir(output_root: Path, comparison_name: str, overwrite: bool) -> Path:
    output_dir = output_root / comparison_name

    if overwrite or not output_dir.exists():
        return output_dir

    timestamp = timestamp_name()

    for suffix in range(1000):
        if suffix == 0:
            candidate = output_root / f"{comparison_name}_{timestamp}"
        else:
            candidate = output_root / f"{comparison_name}_{timestamp}_{suffix + 1}"

        if not candidate.exists():
            return candidate

    raise RuntimeError("nao foi possivel criar nome unico para comparacao")


def append_comparison_index(
    output_root: Path,
    comparison_name: str,
    output_dir: Path,
    run_paths: list[Path],
    run_names: list[str],
    status: str,
) -> None:
    output_root.mkdir(parents=True, exist_ok=True)
    index_path = output_root / "index.csv"
    needs_header = not index_path.exists()

    with index_path.open("a", encoding="utf-8", newline="") as file:
        writer = csv.writer(file)

        if needs_header:
            writer.writerow(
                [
                    "timestamp",
                    "comparison_name",
                    "comparison_path",
                    "run_count",
                    "run_names",
                    "run_paths",
                    "status",
                ]
            )

        writer.writerow(
            [
                datetime.now().strftime("%Y%m%d_%H%M%S"),
                comparison_name,
                str(output_dir),
                len(run_paths),
                ";".join(run_names),
                ";".join(str(path) for path in run_paths),
                status,
            ]
        )


def find_column(data: pd.DataFrame, aliases: tuple[str, ...]) -> str | None:
    lower_columns = {str(column).lower(): column for column in data.columns}

    for alias in aliases:
        if alias.lower() in lower_columns:
            return str(lower_columns[alias.lower()])

    return None


def read_population(path: Path, warnings: list[str]) -> tuple[pd.DataFrame | None, str | None, str | None]:
    population_path = path / "population.csv"

    if not population_path.exists():
        warnings.append(f"{path}: population.csv ausente; execucao ignorada.")
        return None, None, None

    try:
        data = pd.read_csv(population_path)
    except Exception as error:
        warnings.append(f"{path}: erro ao ler population.csv: {error}; execucao ignorada.")
        return None, None, None

    spike_column = find_column(data, SPIKE_COLUMN_ALIASES)
    time_column = find_column(data, TIME_COLUMN_ALIASES)

    if spike_column is None:
        warnings.append(f"{path}: population.csv sem coluna de spikes; execucao ignorada.")
        return None, None, None

    if time_column is None:
        data = data.copy()
        time_column = "tempo"
        data[time_column] = range(len(data))

    try:
        data[spike_column] = pd.to_numeric(data[spike_column])
        data[time_column] = pd.to_numeric(data[time_column])
    except Exception as error:
        warnings.append(f"{path}: population.csv possui valores invalidos: {error}; execucao ignorada.")
        return None, None, None

    finite = data[spike_column].map(math.isfinite) & data[time_column].map(math.isfinite)
    if not bool(finite.all()):
        warnings.append(f"{path}: population.csv possui NaN ou infinito; execucao ignorada.")
        return None, None, None

    return data, spike_column, time_column


def analyze_run(run_path: Path) -> tuple[dict[str, object] | None, pd.DataFrame | None, list[str]]:
    warnings: list[str] = []
    run_path = run_path.resolve()

    population, spike_column, time_column = read_population(run_path, warnings)
    if population is None or spike_column is None or time_column is None:
        return None, None, warnings

    metrics_path = run_path / "metrics.csv"
    stored: dict[str, object] = {}
    if metrics_path.exists():
        try:
            stored = pd.read_csv(metrics_path).iloc[0].to_dict()
        except Exception as error:
            warnings.append(f"{run_path}: metrics.csv invalido ({error}); usando fallback.")

    if stored and "diagnostic_regime" in stored and "activity_total_spikes" in stored:
        metrics = {key: value for key, value in stored.items() if pd.notna(value)}
        metrics["metrics_source"] = "metrics.csv"
    else:
        derived, context = shared_basic_metrics(run_path, level="basic", keep_events=False)
        warnings.extend(str(item) for item in context["warnings"])
        metrics = derived
        metrics.update({key: value for key, value in stored.items() if pd.notna(value)})
        metrics["metrics_source"] = (
            "metrics.csv (campos ausentes derivados)" if stored else "derivadas de CSVs antigos"
        )

    config = read_config(run_path, warnings)
    metrics.update(read_plasticity_metrics(run_path, config, warnings))
    metrics.update(read_homeostasis_metrics(run_path, config, warnings))
    aliases = {
        "num_neurons": "network_num_neurons",
        "steps": "run_steps",
        "dt": "run_dt",
        "duration": "run_simulation_duration",
        "seed": "run_seed",
        "recorded_neuron": "run_recorded_neuron",
        "total_connections": "network_total_connections",
        "total_spikes": "activity_total_spikes",
        "mean_spikes_per_step": "activity_mean_spikes_per_step",
        "max_spikes_per_step": "activity_max_spikes_per_step",
        "min_spikes_per_step": "activity_min_spikes_per_step",
        "std_spikes_per_step": "activity_std_spikes_per_step",
        "active_timesteps": "activity_active_timesteps",
        "silent_timesteps": "activity_silent_timesteps",
        "first_active_step": "activity_first_active_step",
        "last_active_step": "activity_last_active_step",
        "peak_activity_step": "activity_peak_step",
        "peak_activity_value": "activity_peak_value",
        "has_late_activity": "activity_has_late_activity",
        "activity_span": "activity_span",
        "longest_silence_streak": "activity_longest_silence_streak",
        "longest_activity_streak": "activity_longest_activity_streak",
        "population_fano_factor": "activity_population_fano_factor",
        "population_cv": "activity_population_cv",
        "synchrony_proxy": "activity_synchrony_proxy",
        "active_neuron_count": "neuron_active_count",
        "inactive_neuron_count": "neuron_inactive_count",
        "active_neuron_fraction": "neuron_active_fraction",
        "dead_neuron_fraction": "neuron_dead_fraction",
        "spike_gini_approx": "neuron_spike_gini",
        "stability_score": "diagnostic_stability_score",
    }
    for old_name, canonical_name in aliases.items():
        metrics[old_name] = metrics.get(canonical_name)
    metrics["warnings"] = " | ".join(warnings) if warnings else ""
    return metrics, population[[time_column, spike_column]].rename(
        columns={time_column: "tempo", spike_column: "spikes_total"}
    ), warnings


def format_value(value: object) -> str:
    if value is None:
        return "NA"

    if isinstance(value, float):
        if math.isnan(value):
            return "NA"
        return f"{value:.4f}"

    return str(value)


def write_report(
    output_dir: Path,
    summary: pd.DataFrame,
    all_warnings: list[str],
) -> Path:
    report_path = output_dir / "comparison_report.txt"
    lines: list[str] = []

    lines.append("Relatorio de comparacao miniSNN")
    lines.append("=" * 34)
    lines.append("")
    lines.append("Runs comparadas:")

    for _, row in summary.iterrows():
        lines.append(f"- {row['run_name']}: {row['run_path']}")

    lines.append("")
    lines.append("Avisos:")
    if all_warnings:
        for warning in all_warnings:
            lines.append(f"- {warning}")
    else:
        lines.append("- Nenhum aviso.")

    lines.append("")
    lines.append("Principais metricas:")

    for _, row in summary.iterrows():
        lines.append(
            "- "
            f"{row['run_name']}: spikes={format_value(row.get('total_spikes'))}, "
            f"atividade={format_value(row.get('activity_fraction'))}, "
            f"burst={format_value(row.get('burst_fraction'))}, "
            f"estabilidade={format_value(row.get('stability_score'))}, "
            f"sincronia_proxy={format_value(row.get('synchrony_proxy'))}"
        )
        lines.append(f"  origem: {row.get('metrics_source', 'NA')}")

    lines.append("")
    lines.append("Plasticidade:")
    plasticity_fields = (
        "plasticity_modified_connection_fraction",
        "plasticity_initial_weight_mean",
        "plasticity_final_weight_mean",
        "plasticity_mean_absolute_change",
        "plasticity_total_signed_change",
        "plasticity_potentiation_events",
        "plasticity_depression_events",
    )
    for _, row in summary.iterrows():
        available = [
            field
            for field in plasticity_fields
            if field in row and pd.notna(row.get(field))
        ]
        unavailable = [field for field in plasticity_fields if field not in available]
        lines.append(
            f"- {row['run_name']}: enabled={format_value(row.get('plasticity_enabled'))}; "
            f"fonte={format_value(row.get('plasticity_metrics_source'))}"
        )
        lines.append(
            "  carregados/derivados: "
            + (", ".join(available) if available else "nenhum")
        )
        lines.append(
            "  indisponiveis: "
            + (", ".join(unavailable) if unavailable else "nenhum")
        )

    lines.append("")
    lines.append("Homeostase:")
    homeostasis_fields = (
        "homeostasis_population_rate_final",
        "homeostasis_rate_error_mean_absolute",
        "homeostasis_threshold_final_mean",
        "homeostasis_scaling_events",
        "homeostasis_inhibitory_gain_final",
    )
    for _, row in summary.iterrows():
        available = [
            field for field in homeostasis_fields
            if field in row and pd.notna(row.get(field))
        ]
        lines.append(
            f"- {row['run_name']}: enabled={format_value(row.get('homeostasis_enabled'))}; "
            f"fonte={format_value(row.get('homeostasis_metrics_source'))}; "
            f"campos={', '.join(available) if available else 'nenhum'}"
        )

    def ranking(metric: str, title: str, ascending: bool = False) -> None:
        lines.append("")
        lines.append(title)
        ranked = summary.sort_values(metric, ascending=ascending, na_position="last")
        for index, (_, row) in enumerate(ranked.iterrows(), start=1):
            lines.append(f"{index}. {row['run_name']}: {format_value(row.get(metric))}")

    ranking("total_spikes", "Ranking por total de spikes:")
    ranking("last_active_step", "Ranking por atividade sustentada:")
    ranking("stability_score", "Ranking por estabilidade heuristica:")

    lines.append("")
    lines.append("Observacoes automaticas:")

    if not summary.empty:
        max_spikes = summary.sort_values("total_spikes", ascending=False).iloc[0]
        max_silence = summary.sort_values("silence_fraction", ascending=False).iloc[0]
        max_late = summary.sort_values("last_active_step", ascending=False).iloc[0]
        max_gini = summary.sort_values("spike_gini_approx", ascending=False, na_position="last").iloc[0]

        lines.append(
            f"- A execucao {max_spikes['run_name']} teve maior total de spikes nesta metrica."
        )
        lines.append(
            f"- A execucao {max_silence['run_name']} apresentou maior fracao de silencio."
        )
        lines.append(
            f"- A execucao {max_late['run_name']} manteve atividade ate mais tarde na simulacao."
        )

        if pd.notna(max_gini.get("spike_gini_approx")):
            lines.append(
                f"- A execucao {max_gini['run_name']} sugere maior concentracao de spikes por neuronio."
            )

    lines.append("")
    lines.append("Notas:")
    lines.append("- stability_score e uma heuristica entre 0 e 1, util para diagnostico.")
    lines.append("- synchrony_proxy e uma aproximacao simples, nao uma medida completa de sincronia neural.")
    lines.append("- spike_gini_approx e uma medida aproximada de desigualdade da atividade por neuronio.")

    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return report_path


def plot_comparison_metrics(output_dir: Path, summary: pd.DataFrame) -> Path:
    output_path = output_dir / "comparison_metrics.png"
    metrics = [
        "total_spikes",
        "activity_fraction",
        "burst_fraction",
        "active_neuron_fraction",
        "stability_score",
        "synchrony_proxy",
    ]
    labels = [str(name) for name in summary["run_name"]]

    fig, axes = plt.subplots(len(metrics), 1, figsize=(10, 14))

    for axis, metric in zip(axes, metrics):
        values = pd.to_numeric(summary.get(metric), errors="coerce").fillna(0.0)
        axis.bar(labels, values)
        axis.set_title(metric)
        axis.tick_params(axis="x", rotation=25)
        axis.grid(True, axis="y", alpha=0.3)

    plt.tight_layout()
    fig.savefig(output_path)
    plt.close(fig)
    return output_path


def plot_activity_overlay(
    output_dir: Path,
    activity_by_run: dict[str, pd.DataFrame],
) -> Path:
    output_path = output_dir / "comparison_activity_overlay.png"

    plt.figure(figsize=(10, 5))

    for run_name, activity in activity_by_run.items():
        plt.plot(activity["tempo"], activity["spikes_total"], label=run_name)

    plt.title("Atividade populacional comparada")
    plt.xlabel("tempo")
    plt.ylabel("spikes por timestep")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(output_path)
    plt.close()
    return output_path


def compare_runs(
    run_paths: list[str | Path],
    out_name: str | None = None,
    output_root: str | Path | None = None,
    overwrite: bool = False,
) -> Path | None:
    if len(run_paths) < 2:
        print("Erro: informe pelo menos duas pastas de execucao para comparar.")
        return None

    comparison_name = sanitize_name(out_name) if out_name else automatic_comparison_name()
    output_root_path = Path(output_root) if output_root is not None else DEFAULT_OUTPUT_ROOT
    try:
        output_dir = unique_output_dir(output_root_path, comparison_name, overwrite)
    except RuntimeError as error:
        print(f"Erro: {error}")
        return None

    rows: list[dict[str, object]] = []
    all_warnings: list[str] = []
    activity_by_run: dict[str, pd.DataFrame] = {}
    valid_run_paths: list[Path] = []
    valid_run_names: list[str] = []

    for raw_path in run_paths:
        run_path = Path(raw_path)
        metrics, activity, warnings = analyze_run(run_path)
        all_warnings.extend(warnings)

        if metrics is None or activity is None:
            continue

        run_name = str(metrics["run_name"])
        rows.append(metrics)
        activity_by_run[run_name] = activity
        valid_run_paths.append(run_path)
        valid_run_names.append(run_name)

    if len(rows) < 2:
        print("Erro: menos de duas execucoes validas puderam ser comparadas.")
        for warning in all_warnings:
            print(f"Aviso: {warning}")
        return None

    summary = pd.DataFrame(rows)
    output_dir.mkdir(parents=True, exist_ok=overwrite)
    summary_path = output_dir / "comparison_summary.csv"
    summary.to_csv(summary_path, index=False)
    report_path = write_report(output_dir, summary, all_warnings)
    metrics_plot = plot_comparison_metrics(output_dir, summary)
    overlay_plot = plot_activity_overlay(output_dir, activity_by_run)

    append_comparison_index(
        output_root_path,
        output_dir.name,
        output_dir,
        valid_run_paths,
        valid_run_names,
        "OK",
    )

    print(f"Comparacao solicitada: {comparison_name}")
    print(f"Comparacao gerada em: {output_dir}")
    print(f"Resumo CSV: {summary_path}")
    print(f"Relatorio: {report_path}")
    print(f"Grafico: {metrics_plot}")
    print(f"Atividade sobreposta: {overlay_plot}")

    return output_dir


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compara execucoes de cenarios da miniSNN."
    )
    parser.add_argument("run_dirs", nargs="+", help="Pastas em results/scenarios/<run_name>")
    parser.add_argument("--out-name", help="Nome da pasta em results/comparisons/")
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Permite reutilizar a pasta de saida quando ela ja existe.",
    )
    args = parser.parse_args()

    output_dir = compare_runs(args.run_dirs, args.out_name, overwrite=args.overwrite)
    return 0 if output_dir is not None else 1


if __name__ == "__main__":
    raise SystemExit(main())
