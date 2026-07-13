from __future__ import annotations

from pathlib import Path
import argparse
import math
import platform
import sys
import time

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np
    import pandas as pd
except ModuleNotFoundError as error:
    print(f"Erro: dependencia ausente: {error.name}. Instale pandas e matplotlib.")
    raise SystemExit(1)

from metrics_common import (
    basic_metrics,
    finite_number,
    integer,
    sampled_correlation_metrics,
    write_metrics,
)


def value(metrics: dict[str, object], key: str) -> str:
    item = metrics.get(key)
    if item is None or (isinstance(item, float) and not math.isfinite(item)):
        return "NA"
    if isinstance(item, float):
        return f"{item:.6g}"
    return str(item)


def plot_overview(run_path: Path, metrics: dict[str, object], context: dict[str, object]) -> Path:
    population = context["population"]
    counts = context["counts"]
    output = run_path / "diagnostics_overview.png"
    fig, axes = plt.subplots(2, 2, figsize=(12, 8))
    if population is not None and not population.empty:
        axes[0, 0].plot(population["tempo"], population["spikes_total"])
        axes[0, 0].set_title("Spikes por timestep")
        mean_potential = (
            population["mean_potential"]
            if "mean_potential" in population
            else np.zeros(len(population))
        )
        axes[0, 1].plot(population["tempo"], mean_potential)
        axes[0, 1].set_title("Potencial medio")
    else:
        axes[0, 0].text(0.5, 0.5, "Nao disponivel", ha="center")
        axes[0, 1].text(0.5, 0.5, "Nao disponivel", ha="center")
    axes[1, 0].bar(np.arange(len(counts)), counts)
    axes[1, 0].set_title("Spikes por neuronio")
    labels = ["atividade", "silencio", "burst"]
    values = [
        finite_number(metrics.get("activity_fraction")) or 0.0,
        finite_number(metrics.get("silence_fraction")) or 0.0,
        finite_number(metrics.get("burst_fraction")) or 0.0,
    ]
    axes[1, 1].bar(labels, values)
    axes[1, 1].set_ylim(0, 1)
    axes[1, 1].set_title(f"Regime: {metrics.get('diagnostic_regime', 'NA')}")
    for axis in axes.flat:
        axis.grid(True, alpha=0.25)
    fig.tight_layout()
    fig.savefig(output)
    plt.close(fig)
    return output


def write_report(run_path: Path, metrics: dict[str, object], context: dict[str, object], full_notes: list[str]) -> Path:
    warnings = list(context["warnings"]) + full_notes
    lines = [
        "MINISNN - RELATORIO DE DIAGNOSTICO",
        "",
        "1. Identificacao da execucao",
        f"run_name: {value(metrics, 'run_name')}",
        f"actual_run_name: {value(metrics, 'actual_run_name')}",
        f"nivel: {value(metrics, 'diagnostics_level')}",
        "",
        "2. Configuracao principal",
        f"topologia: {value(metrics, 'topology')}",
        f"neuronios: {value(metrics, 'network_num_neurons')}",
        f"passos: {value(metrics, 'run_steps')}",
        f"dt: {value(metrics, 'run_dt')} (unidade temporal do cenario)",
        f"seed: {value(metrics, 'run_seed')}",
        "",
        "3. Resumo da atividade",
        f"spikes totais: {value(metrics, 'activity_total_spikes')}",
        f"media por passo: {value(metrics, 'activity_mean_spikes_per_step')}",
        f"fracao de passos ativos: {value(metrics, 'activity_fraction')}",
        f"maior sequencia de silencio: {value(metrics, 'activity_longest_silence_streak')} passos",
        "",
        "4. Atividade temporal",
        f"primeiro tercio: {value(metrics, 'activity_early_total_spikes')} spikes",
        f"segundo tercio: {value(metrics, 'activity_middle_total_spikes')} spikes",
        f"terceiro tercio: {value(metrics, 'activity_late_total_spikes')} spikes",
        f"ultimo quarto: {value(metrics, 'activity_last_quarter_total_spikes')} spikes",
        "",
        "5. Bursts",
        f"threshold: media + {context['params']['burst_z_threshold']} * desvio, minimo 1 spike",
        f"duracao minima: {context['params']['min_burst_steps']} passos",
        f"bursts: {value(metrics, 'burst_count')}",
        f"fracao de spikes em bursts: {value(metrics, 'burst_spike_share')}",
        "",
        "6. Distribuicao por neuronio",
        f"neuronios ativos: {value(metrics, 'neuron_active_count')}",
        f"neuronios sem spikes nesta execucao: {value(metrics, 'neuron_inactive_count')}",
        f"top 10% concentrou: {value(metrics, 'neuron_top_10_percent_spike_share')}",
        f"Gini: {value(metrics, 'neuron_spike_gini')}",
        f"entropia normalizada: {value(metrics, 'neuron_normalized_spike_entropy')}",
        "",
        "7. Excitacao e inibicao",
        f"spikes EXC: {value(metrics, 'exc_total_spikes')}",
        f"spikes INH: {value(metrics, 'inh_total_spikes')}",
        f"razao EXC/INH: {value(metrics, 'exc_inh_total_spike_ratio')}",
        "",
        "8. Variabilidade e sincronizacao",
        f"CV populacional: {value(metrics, 'activity_population_cv')}",
        f"Fano populacional: {value(metrics, 'activity_population_fano_factor')}",
        f"proxy de sincronia: {value(metrics, 'activity_synchrony_proxy')}",
        "O proxy de sincronia e uma concentracao temporal aproximada, nao uma medida completa de sincronia neural.",
        "",
        "9. Neuronio detalhado",
        f"spikes: {value(metrics, 'neuron_detailed_spike_count')}",
        f"V medio: {value(metrics, 'voltage_mean')}",
        f"corrente sinaptica media: {value(metrics, 'current_synaptic_mean')}",
        "",
        "10. Conectividade",
        f"conexoes: {value(metrics, 'network_total_connections')}",
        f"densidade: {value(metrics, 'network_connection_density')}",
        f"indegree medio: {value(metrics, 'network_indegree_mean')}",
        f"outdegree medio: {value(metrics, 'network_outdegree_mean')}",
        "",
        "11. Plasticidade",
        f"ativa: {value(metrics, 'plasticity_enabled')}",
        f"regra: {value(metrics, 'plasticity_rule')}",
        f"origem das metricas: {value(metrics, 'plasticity_metrics_source')}",
        f"fracao de conexoes modificadas: {value(metrics, 'plasticity_modified_connection_fraction')}",
        f"peso medio inicial: {value(metrics, 'plasticity_initial_weight_mean')}",
        f"peso medio final: {value(metrics, 'plasticity_final_weight_mean')}",
        f"mudanca absoluta media: {value(metrics, 'plasticity_mean_absolute_change')}",
        f"mudanca assinada total: {value(metrics, 'plasticity_total_signed_change')}",
        f"eventos LTP: {value(metrics, 'plasticity_potentiation_events')}",
        f"eventos LTD: {value(metrics, 'plasticity_depression_events')}",
        "",
        "12. Desempenho",
        f"tempo de simulacao real: {value(metrics, 'performance_simulation_time_seconds')} s",
        f"tempo da analise: {value(metrics, 'performance_analysis_time_seconds')} s",
        f"tamanho total: {value(metrics, 'performance_total_result_size_bytes')} bytes",
        "",
        "13. Classificacao do regime",
        f"regime: {value(metrics, 'diagnostic_regime')}",
        f"confianca heuristica: {value(metrics, 'diagnostic_confidence')}",
        f"motivos: {value(metrics, 'diagnostic_reasons')}",
        f"stability_score: {value(metrics, 'diagnostic_stability_score')}",
        "O score e um indice heuristico de diagnostico, nao uma medida biologica universal.",
        "",
        "14. Avisos e limitacoes",
    ]
    lines.extend([f"- {warning}" for warning in warnings] or ["- Nenhum aviso."])
    lines.extend([
        "",
        "15. Definicoes e parametros usados",
        "Neuronio sem spikes significa apenas inativo durante esta execucao.",
        "Gini alto sugere atividade concentrada; entropia normalizada alta sugere distribuicao mais uniforme.",
        "Classificacoes e observacoes sao diagnosticas e nao estabelecem causalidade biologica.",
    ])
    if metrics.get("activity_has_late_activity"):
        lines.append("Nesta execucao, a rede permaneceu ativa no ultimo quarto.")
    lines.append(
        f"Segundo esta metrica, os 10% mais ativos concentraram {value(metrics, 'neuron_top_10_percent_spike_share')} dos spikes."
    )
    output = run_path / "metrics_report.txt"
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return output


def full_analysis(run_path: Path, metrics: dict[str, object], context: dict[str, object]) -> tuple[list[Path], list[str]]:
    outputs: list[Path] = []
    notes: list[str] = []
    counts: np.ndarray = context["counts"]
    types: list[str | None] = context["types"]
    events: pd.DataFrame | None = context["events"]
    params = context["params"]
    steps = integer(metrics.get("run_steps")) or 0
    rows: list[dict[str, object]] = []
    isi_values: list[float] = []
    isi_cvs: list[float] = []
    threshold = float(counts.mean() + 3.0 * counts.std()) if counts.size else 0.0
    for neuron_id, count in enumerate(counts):
        times = np.array([], dtype=int)
        if events is not None:
            times = np.sort(events.loc[events["neuronio"] == neuron_id, "tempo"].to_numpy(dtype=int))
        intervals = np.diff(times)
        valid_isi = count >= int(params["isi_min_spikes"]) and intervals.size > 0
        if valid_isi:
            isi_values.extend(intervals.astype(float))
            mean_isi = float(intervals.mean())
            isi_cv = float(intervals.std() / mean_isi) if mean_isi > 0 else 0.0
            isi_cvs.append(isi_cv)
        else:
            mean_isi = None
            isi_cv = None
        rows.append({
            "neuron_id": neuron_id,
            "type": types[neuron_id] if neuron_id < len(types) else "NA",
            "spikes": int(count),
            "active": int(count > 0),
            "hyperactive": int(count > threshold and counts.size > 1),
            "first_spike": int(times[0]) if times.size else -1,
            "last_spike": int(times[-1]) if times.size else -1,
            "mean_isi": mean_isi,
            "isi_cv": isi_cv,
        })
    neurons = pd.DataFrame(rows)
    neurons_path = run_path / "metrics_neurons.csv"
    neurons.to_csv(neurons_path, index=False, na_rep="NA")
    outputs.append(neurons_path)
    isi_array = np.asarray(isi_values, dtype=float)
    cv_array = np.asarray(isi_cvs, dtype=float)
    metrics.update({
        "neuron_hyperactive_threshold": threshold,
        "neuron_hyperactive_count": int(neurons["hyperactive"].sum()),
        "neuron_hyperactive_fraction": float(neurons["hyperactive"].mean()) if len(neurons) else 0.0,
        "neuron_dominant_count": int(np.sum(counts >= 0.05 * counts.sum())) if counts.sum() else 0,
        "neuron_dominant_spike_share": top_share_safe(counts, max(1, int(np.sum(counts >= 0.05 * counts.sum())))),
        "isi_neurons_with_valid_isi": len(isi_cvs),
        "isi_mean": float(isi_array.mean()) if isi_array.size else None,
        "isi_median": float(np.median(isi_array)) if isi_array.size else None,
        "isi_std": float(isi_array.std()) if isi_array.size else None,
        "isi_mean_cv": float(cv_array.mean()) if cv_array.size else None,
        "isi_median_cv": float(np.median(cv_array)) if cv_array.size else None,
        "isi_min": float(isi_array.min()) if isi_array.size else None,
        "isi_max": float(isi_array.max()) if isi_array.size else None,
    })

    population: pd.DataFrame | None = context["population"]
    bin_size = int(params["time_bin_steps"])
    windows: list[dict[str, object]] = []
    if population is not None:
        for index, start in enumerate(range(0, steps, bin_size)):
            end = min(steps, start + bin_size)
            part = population[(population["tempo"] >= start) & (population["tempo"] < end)]
            values = part["spikes_total"].to_numpy(dtype=float)
            active_neurons = 0
            if events is not None:
                active_neurons = int(events[(events["tempo"] >= start) & (events["tempo"] < end)]["neuronio"].nunique())
            mean = float(values.mean()) if values.size else 0.0
            variance = float(values.var()) if values.size else 0.0
            windows.append({
                "window_index": index, "start_step": start, "end_step": end,
                "spikes": int(values.sum()), "active_neurons": active_neurons,
                "mean_activity": mean, "variance": variance,
                "fano_factor": variance / mean if mean > 0 else 0.0,
                "exc_spikes": int(part.get("spikes_exc", pd.Series(dtype=float)).sum()),
                "inh_spikes": int(part.get("spikes_inh", pd.Series(dtype=float)).sum()),
            })
    windows_path = run_path / "metrics_windows.csv"
    pd.DataFrame(windows).to_csv(windows_path, index=False, na_rep="NA")
    outputs.append(windows_path)

    correlation_metrics, correlation_values, correlation_matrix, sampled_ids = (
        sampled_correlation_metrics(
            events,
            len(counts),
            steps,
            bin_size,
            int(params["correlation_sample_size"]),
            int(params["neuron_sample_limit"]),
            int(params["sample_stride"]),
            integer(metrics.get("run_seed")) or 0,
        )
    )
    metrics.update(correlation_metrics)

    if population is not None:
        output = run_path / "diagnostics_activity.png"
        plt.figure(figsize=(11, 5)); plt.plot(population["tempo"], population["spikes_total"]); plt.title("Atividade populacional"); plt.xlabel("tempo"); plt.ylabel("spikes"); plt.grid(True, alpha=0.25); plt.tight_layout(); plt.savefig(output); plt.close(); outputs.append(output)
        output = run_path / "diagnostics_exc_inh.png"
        exc_values = population["spikes_exc"] if "spikes_exc" in population else np.zeros(len(population))
        inh_values = population["spikes_inh"] if "spikes_inh" in population else np.zeros(len(population))
        plt.figure(figsize=(11, 5)); plt.plot(population["tempo"], exc_values, label="EXC"); plt.plot(population["tempo"], inh_values, label="INH"); plt.title("Atividade EXC/INH"); plt.legend(); plt.grid(True, alpha=0.25); plt.tight_layout(); plt.savefig(output); plt.close(); outputs.append(output)
    output = run_path / "diagnostics_distribution.png"
    plt.figure(figsize=(10, 5)); plt.bar(np.arange(len(counts)), counts); plt.title("Distribuicao de spikes por neuronio"); plt.xlabel("neuronio"); plt.ylabel("spikes"); plt.tight_layout(); plt.savefig(output); plt.close(); outputs.append(output)
    if isi_array.size:
        output = run_path / "diagnostics_isi.png"
        plt.figure(figsize=(9, 5)); plt.hist(isi_array, bins=min(40, max(5, int(math.sqrt(isi_array.size))))); plt.title("Distribuicao de intervalos entre spikes"); plt.xlabel("ISI (passos)"); plt.tight_layout(); plt.savefig(output); plt.close(); outputs.append(output)
    else:
        notes.append("Nao disponivel: diagnostics_isi.png; nenhum neuronio teve spikes suficientes.")
    if correlation_values.size:
        output = run_path / "diagnostics_correlation.png"
        plt.figure(figsize=(9, 5)); plt.hist(correlation_values, bins=30); plt.title("Correlacoes pareadas da amostra"); plt.xlabel("correlacao"); plt.tight_layout(); plt.savefig(output); plt.close(); outputs.append(output)
    else:
        notes.append("Nao disponivel: diagnostics_correlation.png; amostra sem variacao suficiente.")
    return outputs, notes


def top_share_safe(values: np.ndarray, count: int) -> float:
    total = float(values.sum())
    return float(np.sort(values)[-count:].sum() / total) if total > 0 and count > 0 else 0.0


def update_manifest(run_path: Path, level: str, outputs: list[Path]) -> None:
    path = run_path / "run_manifest.txt"
    original = path.read_text(encoding="utf-8", errors="replace") if path.exists() else ""
    original = original.split("\n[analysis]\n", 1)[0].rstrip()
    import matplotlib as mpl
    lines = [
        original,
        "",
        "[analysis]",
        f"diagnostics_level_effective={level}",
        f"python={sys.executable}",
        f"python_version={platform.python_version()}",
        f"pandas_version={pd.__version__}",
        f"matplotlib_version={mpl.__version__}",
        "analysis_files=" + ";".join(item.name for item in outputs),
    ]
    path.write_text("\n".join(lines).strip() + "\n", encoding="utf-8")


def analyze(run_directory: str | Path, level: str) -> dict[str, object]:
    run_path = Path(run_directory).resolve()
    if not run_path.is_dir():
        raise ValueError(f"pasta de execucao inexistente: {run_path}")
    started = time.perf_counter()
    metrics, context = basic_metrics(run_path, level=level, keep_events=level == "full")
    population = context.get("population")
    if population is None or population.empty:
        raise ValueError("population.csv ausente, vazio ou sem dados validos")
    outputs: list[Path] = []
    notes: list[str] = []
    if level == "off":
        update_manifest(run_path, level, outputs)
        return metrics
    if level == "full":
        full_outputs, notes = full_analysis(run_path, metrics, context)
        outputs.extend(full_outputs)
    metrics["diagnostics_level"] = level
    metrics["performance_analysis_time_seconds"] = time.perf_counter() - started
    report_path = write_report(run_path, metrics, context, notes)
    outputs.append(report_path)
    overview_path = plot_overview(run_path, metrics, context)
    outputs.append(overview_path)
    metrics["performance_analysis_time_seconds"] = time.perf_counter() - started
    metrics["performance_total_result_size_bytes"] = sum(
        item.stat().st_size for item in run_path.iterdir() if item.is_file()
    )
    metrics_path = run_path / "metrics.csv"
    write_metrics(metrics_path, metrics)
    outputs.append(metrics_path)
    update_manifest(run_path, level, outputs)
    print(f"Diagnostico {level} gerado em: {run_path}")
    for output in outputs:
        print(f"- {output.name}")
    return metrics


def main() -> int:
    parser = argparse.ArgumentParser(description="Analisa uma execucao da miniSNN.")
    parser.add_argument("run_directory", help="Pasta results/scenarios/<actual_run_name>")
    parser.add_argument("--level", choices=("off", "basic", "full"), default=None)
    args = parser.parse_args()
    run_path = Path(args.run_directory)
    level = args.level
    if level is None:
        from metrics_common import diagnostics_parameters, read_config
        warnings: list[str] = []
        level = str(diagnostics_parameters(read_config(run_path, warnings))["level"])
    try:
        analyze(run_path, level)
    except (OSError, ValueError) as error:
        print(f"Erro: {error}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
