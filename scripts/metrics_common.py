from __future__ import annotations

from configparser import ConfigParser
from pathlib import Path
import csv
import math
import time

import numpy as np
import pandas as pd


NA = None
RASTER_CHUNK_SIZE = 200_000


def finite_number(value: object) -> float | None:
    try:
        number = float(value)
    except (TypeError, ValueError):
        return None
    return number if math.isfinite(number) else None


def integer(value: object) -> int | None:
    number = finite_number(value)
    return int(round(number)) if number is not None else None


def read_key_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    if not path.exists():
        return values
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
    return values


def read_config(path: Path, warnings: list[str]) -> ConfigParser:
    parser = ConfigParser()
    config_path = path / "config_used.ini"
    if not config_path.exists():
        warnings.append("config_used.ini ausente; parte da proveniencia fica NA.")
        return parser
    try:
        parser.read(config_path, encoding="utf-8")
    except Exception as error:
        warnings.append(f"config_used.ini invalido: {error}")
    return parser


def cfg(parser: ConfigParser, section: str, key: str, default: object = None) -> object:
    return parser.get(section, key) if parser.has_option(section, key) else default


def diagnostics_parameters(parser: ConfigParser, level: str | None = None) -> dict[str, object]:
    selected = (level or str(cfg(parser, "diagnostics", "level", "off"))).lower()
    return {
        "level": selected,
        "time_bin_steps": integer(cfg(parser, "diagnostics", "time_bin_steps", 10)) or 10,
        "burst_z_threshold": finite_number(cfg(parser, "diagnostics", "burst_z_threshold", 2.0)) or 2.0,
        "min_burst_steps": integer(cfg(parser, "diagnostics", "min_burst_steps", 1)) or 1,
        "isi_min_spikes": integer(cfg(parser, "diagnostics", "isi_min_spikes", 4)) or 4,
        "correlation_sample_size": integer(cfg(parser, "diagnostics", "correlation_sample_size", 128)) or 128,
        "neuron_sample_limit": integer(cfg(parser, "diagnostics", "neuron_sample_limit", 1000)) or 1000,
        "sample_stride": integer(cfg(parser, "diagnostics", "sample_stride", 1)) or 1,
    }


def longest_streak(mask: np.ndarray) -> int:
    best = current = 0
    for value in mask.astype(bool):
        current = current + 1 if value else 0
        best = max(best, current)
    return best


def gini(values: np.ndarray) -> float:
    if values.size == 0 or float(values.sum()) <= 0.0:
        return 0.0
    ordered = np.sort(values.astype(float))
    n = ordered.size
    indices = np.arange(1, n + 1, dtype=float)
    return float((2.0 * np.sum(indices * ordered)) / (n * ordered.sum()) - (n + 1.0) / n)


def top_share(values: np.ndarray, fraction: float) -> float:
    total = float(values.sum())
    if values.size == 0 or total <= 0.0:
        return 0.0
    count = max(1, int(math.ceil(values.size * fraction)))
    return float(np.sort(values)[-count:].sum() / total)


def entropy_metrics(values: np.ndarray) -> tuple[float, float]:
    total = float(values.sum())
    if values.size == 0 or total <= 0.0:
        return 0.0, 0.0
    probabilities = values[values > 0].astype(float) / total
    entropy = float(-np.sum(probabilities * np.log2(probabilities)))
    maximum = math.log2(values.size) if values.size > 1 else 0.0
    return entropy, entropy / maximum if maximum > 0.0 else 1.0


def segment_metrics(spikes: np.ndarray, start: int, end: int) -> tuple[int, float, float]:
    part = spikes[start:end]
    if part.size == 0:
        return 0, 0.0, 0.0
    return int(part.sum()), float(part.mean()), float(np.mean(part > 0))


def burst_metrics(spikes: np.ndarray, z_threshold: float, minimum_steps: int) -> dict[str, object]:
    mean = float(spikes.mean()) if spikes.size else 0.0
    std = float(spikes.std()) if spikes.size else 0.0
    threshold = max(1.0, mean + z_threshold * std)
    above = spikes > threshold
    groups: list[tuple[int, int]] = []
    start: int | None = None
    for index, value in enumerate(above):
        if value and start is None:
            start = index
        if start is not None and (not value or index == above.size - 1):
            end = index + 1 if value and index == above.size - 1 else index
            if end - start >= minimum_steps:
                groups.append((start, end))
            start = None
    durations = [end - start for start, end in groups]
    sizes = [int(spikes[start:end].sum()) for start, end in groups]
    intervals = [groups[i][0] - groups[i - 1][1] for i in range(1, len(groups))]
    burst_steps = sum(durations)
    total = int(spikes.sum())
    burst_total = sum(sizes)
    return {
        "burst_threshold_used": threshold,
        "burst_count": len(groups),
        "burst_timesteps": burst_steps,
        "burst_fraction": burst_steps / spikes.size if spikes.size else 0.0,
        "burst_total_spikes": burst_total,
        "burst_spike_share": burst_total / total if total else 0.0,
        "burst_mean_duration_steps": float(np.mean(durations)) if durations else 0.0,
        "burst_max_duration_steps": max(durations, default=0),
        "burst_mean_size": float(np.mean(sizes)) if sizes else 0.0,
        "burst_max_size": max(sizes, default=0),
        "burst_mean_interburst_interval": float(np.mean(intervals)) if intervals else NA,
    }


def classify_regime(metrics: dict[str, object]) -> tuple[str, float, str]:
    total = integer(metrics.get("activity_total_spikes")) or 0
    steps = integer(metrics.get("run_steps")) or 0
    neurons = integer(metrics.get("network_num_neurons")) or 0
    if steps <= 0 or neurons <= 0:
        return "undetermined", 0.0, "dados insuficientes"
    if total == 0:
        return "silent", 1.0, "nenhum spike observado"

    activity = finite_number(metrics.get("activity_fraction")) or 0.0
    mean = finite_number(metrics.get("activity_mean_spikes_per_step")) or 0.0
    peak = finite_number(metrics.get("activity_max_spikes_per_step")) or 0.0
    burst_share = finite_number(metrics.get("burst_spike_share")) or 0.0
    longest_silence = integer(metrics.get("activity_longest_silence_streak")) or 0
    has_late = bool(metrics.get("activity_has_late_activity"))
    signals: list[str] = []
    if mean / neurons >= 0.50 or peak / neurons >= 0.80:
        signals.append("hyperactive")
    if burst_share >= 0.50 and (integer(metrics.get("burst_count")) or 0) > 0:
        signals.append("bursting")
    if has_late and activity >= 0.25:
        signals.append("sustained")
    if has_late and 0.05 <= activity <= 0.75 and longest_silence >= max(5, steps // 10):
        signals.append("intermittent")
    if activity < 0.10 and mean / neurons < 0.05:
        signals.append("sparse")
    if len(signals) > 1:
        return "mixed", min(1.0, 0.55 + 0.1 * len(signals)), "; ".join(signals)
    if signals:
        return signals[0], 0.75, signals[0]
    return "undetermined", 0.35, "nenhum threshold diagnostico dominante"


def stability(metrics: dict[str, object]) -> dict[str, float]:
    total = finite_number(metrics.get("activity_total_spikes")) or 0.0
    if total <= 0.0:
        return {
            "diagnostic_stability_score": 0.0,
            "stability_silence_component": 0.0,
            "stability_explosion_component": 1.0,
            "stability_persistence_component": 0.0,
            "stability_distribution_component": 0.0,
        }
    silence = 1.0 - (finite_number(metrics.get("silence_fraction")) or 0.0)
    neurons = max(1.0, finite_number(metrics.get("network_num_neurons")) or 1.0)
    peak_fraction = (finite_number(metrics.get("activity_max_spikes_per_step")) or 0.0) / neurons
    explosion = 1.0 - min(1.0, peak_fraction / 0.5)
    persistence = 1.0 if metrics.get("activity_has_late_activity") else 0.0
    distribution = finite_number(metrics.get("neuron_normalized_spike_entropy")) or 0.0
    score = max(0.0, min(1.0, (silence + explosion + persistence + distribution) / 4.0))
    return {
        "diagnostic_stability_score": score,
        "stability_silence_component": silence,
        "stability_explosion_component": explosion,
        "stability_persistence_component": persistence,
        "stability_distribution_component": distribution,
    }


def read_population(run_path: Path, warnings: list[str]) -> pd.DataFrame | None:
    path = run_path / "population.csv"
    if not path.exists():
        warnings.append("population.csv ausente; diagnostico populacional indisponivel.")
        return None
    try:
        data = pd.read_csv(path)
    except Exception as error:
        warnings.append(f"population.csv invalido: {error}")
        return None
    required = {"tempo", "spikes_total"}
    if not required.issubset(data.columns):
        warnings.append("population.csv sem colunas tempo/spikes_total.")
        return None
    for column in ("tempo", "spikes_total", "spikes_exc", "spikes_inh", "mean_potential", "mean_syn_current"):
        if column in data:
            data[column] = pd.to_numeric(data[column], errors="coerce")
    data = data.dropna(subset=["tempo", "spikes_total"])
    return data


def read_raster_counts(
    run_path: Path,
    num_neurons: int,
    warnings: list[str],
    keep_events: bool,
) -> tuple[np.ndarray, list[str | None], pd.DataFrame | None]:
    path = run_path / "raster.csv"
    counts = np.zeros(max(0, num_neurons), dtype=np.int64)
    types: list[str | None] = [None] * max(0, num_neurons)
    event_parts: list[pd.DataFrame] = []
    if not path.exists():
        warnings.append("raster.csv ausente; metricas por neuronio ficam NA.")
        return counts, types, None
    try:
        for chunk in pd.read_csv(path, chunksize=RASTER_CHUNK_SIZE):
            if not {"tempo", "neuronio"}.issubset(chunk.columns):
                warnings.append("raster.csv sem colunas tempo/neuronio.")
                return counts, types, None
            chunk["tempo"] = pd.to_numeric(chunk["tempo"], errors="coerce")
            chunk["neuronio"] = pd.to_numeric(chunk["neuronio"], errors="coerce")
            chunk = chunk.dropna(subset=["tempo", "neuronio"])
            chunk["tempo"] = chunk["tempo"].astype(int)
            chunk["neuronio"] = chunk["neuronio"].astype(int)
            valid = chunk[(chunk["neuronio"] >= 0) & (chunk["neuronio"] < num_neurons)]
            grouped = valid["neuronio"].value_counts()
            for neuron_id, value in grouped.items():
                counts[int(neuron_id)] += int(value)
            if "tipo" in valid:
                for neuron_id, group in valid.groupby("neuronio"):
                    observed = {str(value).upper() for value in group["tipo"].dropna()}
                    if len(observed) == 1 and next(iter(observed)) in {"EXC", "INH"}:
                        types[int(neuron_id)] = next(iter(observed))
            if keep_events:
                columns = ["tempo", "neuronio"] + (["tipo"] if "tipo" in valid else [])
                event_parts.append(valid[columns])
    except pd.errors.EmptyDataError:
        warnings.append("raster.csv vazio; tratado como execucao sem spikes.")
    except Exception as error:
        warnings.append(f"raster.csv invalido: {error}")
    events = pd.concat(event_parts, ignore_index=True) if event_parts else None
    return counts, types, events


def infer_types(types: list[str | None], num_neurons: int, inhibitory_count: int | None) -> list[str | None]:
    if all(value in {"EXC", "INH"} for value in types) and types:
        return types
    if inhibitory_count is None or inhibitory_count < 0 or inhibitory_count > num_neurons:
        return types
    boundary = num_neurons - inhibitory_count
    return ["INH" if neuron_id >= boundary else "EXC" for neuron_id in range(num_neurons)]


def basic_metrics(run_path: Path, level: str | None = None, keep_events: bool = False) -> tuple[dict[str, object], dict[str, object]]:
    started = time.perf_counter()
    warnings: list[str] = []
    summary = read_key_values(run_path / "summary.txt")
    config = read_config(run_path, warnings)
    params = diagnostics_parameters(config, level)
    population = read_population(run_path, warnings)
    steps = integer(summary.get("steps")) or integer(cfg(config, "simulation", "steps", 0)) or 0
    num_neurons = integer(summary.get("neurons")) or integer(cfg(config, "network", "neurons", 0)) or 0
    dt = finite_number(cfg(config, "simulation", "dt", NA))
    inhibitory_count = integer(summary.get("inhibitory_count"))
    if inhibitory_count is None:
        fraction = finite_number(cfg(config, "network", "inhibitory_fraction", NA))
        inhibitory_count = int(num_neurons * fraction + 0.5) if fraction is not None else None
    metrics: dict[str, object] = {
        "run_name": summary.get("run_name") or cfg(config, "run", "run_name", run_path.name),
        "actual_run_name": summary.get("actual_run_name") or run_path.name,
        "run_path": str(run_path),
        "timestamp": read_key_values(run_path / "run_manifest.txt").get("timestamp", "NA"),
        "topology": summary.get("topology") or cfg(config, "network", "topology", "NA"),
        "network_num_neurons": num_neurons,
        "run_steps": steps,
        "run_dt": dt,
        "run_simulation_duration": steps * dt if dt is not None else NA,
        "run_seed": integer(cfg(config, "network", "seed", NA)),
        "run_recorded_neuron": integer(cfg(config, "recording", "record_neuron", NA)),
        "diagnostics_level": params["level"],
        "network_total_connections": integer(summary.get("connection_count")),
        "network_excitatory_neuron_count": num_neurons - inhibitory_count if inhibitory_count is not None else NA,
        "network_inhibitory_neuron_count": inhibitory_count,
    }

    existing_path = run_path / "metrics.csv"
    if existing_path.exists():
        try:
            existing = pd.read_csv(existing_path).iloc[0].to_dict()
            for key, value in existing.items():
                if pd.notna(value):
                    metrics.setdefault(str(key), value)
        except Exception:
            warnings.append("metrics.csv anterior nao pode ser reutilizado; metricas foram recalculadas.")

    if population is None or population.empty:
        spikes = np.zeros(steps, dtype=float)
        times = np.arange(steps)
    else:
        spikes = population["spikes_total"].fillna(0).to_numpy(dtype=float)
        times = population["tempo"].to_numpy(dtype=int)
        if steps <= 0:
            steps = len(spikes)
            metrics["run_steps"] = steps

    total = int(spikes.sum())
    active = spikes > 0
    active_count = int(active.sum())
    first_active = int(times[np.flatnonzero(active)[0]]) if active_count else -1
    last_active = int(times[np.flatnonzero(active)[-1]]) if active_count else -1
    peak_index = int(np.argmax(spikes)) if spikes.size else -1
    sorted_spikes = np.sort(spikes)
    top_count = max(1, int(math.ceil(spikes.size * 0.10))) if spikes.size else 0
    early = segment_metrics(spikes, 0, spikes.size // 3)
    middle = segment_metrics(spikes, spikes.size // 3, 2 * spikes.size // 3)
    late = segment_metrics(spikes, 2 * spikes.size // 3, spikes.size)
    quarter_start = int(spikes.size * 0.75)
    last_quarter = segment_metrics(spikes, quarter_start, spikes.size)
    variance = float(spikes.var()) if spikes.size else 0.0
    std = float(spikes.std()) if spikes.size else 0.0
    mean = float(spikes.mean()) if spikes.size else 0.0
    metrics.update({
        "activity_total_spikes": total,
        "activity_mean_spikes_per_step": mean,
        "activity_median_spikes_per_step": float(np.median(spikes)) if spikes.size else 0.0,
        "activity_min_spikes_per_step": float(spikes.min()) if spikes.size else 0.0,
        "activity_max_spikes_per_step": float(spikes.max()) if spikes.size else 0.0,
        "activity_variance_spikes_per_step": variance,
        "activity_std_spikes_per_step": std,
        "activity_active_timesteps": active_count,
        "activity_silent_timesteps": int(spikes.size - active_count),
        "activity_fraction": active_count / spikes.size if spikes.size else 0.0,
        "silence_fraction": 1.0 - active_count / spikes.size if spikes.size else 1.0,
        "activity_first_active_step": first_active,
        "activity_last_active_step": last_active,
        "activity_span": last_active - first_active + 1 if active_count else 0,
        "activity_peak_step": int(times[peak_index]) if peak_index >= 0 else -1,
        "activity_peak_value": float(spikes[peak_index]) if peak_index >= 0 else 0.0,
        "activity_longest_activity_streak": longest_streak(active),
        "activity_longest_silence_streak": longest_streak(~active),
        "activity_has_late_activity": bool(np.any(active[quarter_start:])),
        "activity_mean_spikes_per_neuron": total / num_neurons if num_neurons else NA,
        "activity_mean_spikes_per_neuron_per_step": total / (num_neurons * spikes.size) if num_neurons and spikes.size else NA,
        "activity_spikes_per_time_unit": total / (spikes.size * dt) if spikes.size and dt else NA,
        "activity_spikes_per_neuron_per_time_unit": total / (num_neurons * spikes.size * dt) if num_neurons and spikes.size and dt else NA,
        "activity_early_total_spikes": early[0], "activity_early_mean": early[1], "activity_early_fraction": early[2],
        "activity_middle_total_spikes": middle[0], "activity_middle_mean": middle[1], "activity_middle_fraction": middle[2],
        "activity_late_total_spikes": late[0], "activity_late_mean": late[1], "activity_late_fraction": late[2],
        "activity_last_quarter_total_spikes": last_quarter[0],
        "activity_last_quarter_fraction": last_quarter[2],
        "activity_population_cv": std / mean if mean > 0 else 0.0,
        "activity_population_fano_factor": variance / mean if mean > 0 else 0.0,
        "activity_synchrony_proxy": float(spikes.max()) / total if total and spikes.size else 0.0,
        "activity_temporal_concentration": float(sorted_spikes[-top_count:].sum()) / total if total and top_count else 0.0,
    })
    metrics.update(burst_metrics(spikes, float(params["burst_z_threshold"]), int(params["min_burst_steps"])))

    counts, types, events = read_raster_counts(run_path, num_neurons, warnings, keep_events)
    types = infer_types(types, num_neurons, inhibitory_count)
    active_counts = counts[counts > 0]
    entropy, normalized_entropy = entropy_metrics(counts)
    metrics.update({
        "neuron_active_count": int(np.sum(counts > 0)),
        "neuron_inactive_count": int(np.sum(counts == 0)),
        "neuron_active_fraction": float(np.mean(counts > 0)) if counts.size else NA,
        "neuron_dead_fraction": float(np.mean(counts == 0)) if counts.size else NA,
        "neuron_mean_spikes": float(counts.mean()) if counts.size else NA,
        "neuron_median_spikes": float(np.median(counts)) if counts.size else NA,
        "neuron_std_spikes": float(counts.std()) if counts.size else NA,
        "neuron_min_spikes": int(counts.min()) if counts.size else NA,
        "neuron_max_spikes": int(counts.max()) if counts.size else NA,
        "neuron_most_active": int(np.argmax(counts)) if counts.size else NA,
        "neuron_most_active_spikes": int(counts.max()) if counts.size else NA,
        "neuron_least_active_active": int(np.flatnonzero(counts == active_counts.min())[0]) if active_counts.size else NA,
        "neuron_least_active_active_spikes": int(active_counts.min()) if active_counts.size else NA,
        "neuron_top_1_percent_spike_share": top_share(counts, 0.01),
        "neuron_top_5_percent_spike_share": top_share(counts, 0.05),
        "neuron_top_10_percent_spike_share": top_share(counts, 0.10),
        "neuron_spike_gini": gini(counts),
        "neuron_spike_entropy": entropy,
        "neuron_normalized_spike_entropy": normalized_entropy,
    })
    exc_ids = np.array([index for index, value in enumerate(types) if value == "EXC"], dtype=int)
    inh_ids = np.array([index for index, value in enumerate(types) if value == "INH"], dtype=int)
    if exc_ids.size + inh_ids.size == num_neurons:
        exc_total = int(counts[exc_ids].sum()) if exc_ids.size else 0
        inh_total = int(counts[inh_ids].sum()) if inh_ids.size else 0
        metrics.update({
            "exc_total_spikes": exc_total, "inh_total_spikes": inh_total,
            "exc_mean_spikes_per_neuron": exc_total / exc_ids.size if exc_ids.size else NA,
            "inh_mean_spikes_per_neuron": inh_total / inh_ids.size if inh_ids.size else NA,
            "exc_active_neuron_count": int(np.sum(counts[exc_ids] > 0)) if exc_ids.size else 0,
            "inh_active_neuron_count": int(np.sum(counts[inh_ids] > 0)) if inh_ids.size else 0,
            "exc_active_fraction": float(np.mean(counts[exc_ids] > 0)) if exc_ids.size else NA,
            "inh_active_fraction": float(np.mean(counts[inh_ids] > 0)) if inh_ids.size else NA,
            "exc_inh_total_spike_ratio": exc_total / inh_total if inh_total else NA,
            "exc_inh_rate_ratio": (exc_total / exc_ids.size) / (inh_total / inh_ids.size) if exc_ids.size and inh_ids.size and inh_total else NA,
        })
    else:
        warnings.append("tipos EXC/INH nao puderam ser confirmados com seguranca.")

    detailed_id = integer(metrics.get("run_recorded_neuron"))
    neuron_path = run_path / f"neuron_{detailed_id}.csv"
    if detailed_id is not None and neuron_path.exists():
        try:
            detailed = pd.read_csv(neuron_path)
            for column in ("tempo", "V", "spike", "corrente_externa", "corrente_sinaptica"):
                if column in detailed:
                    detailed[column] = pd.to_numeric(detailed[column], errors="coerce")
            spike_times = detailed.loc[detailed.get("spike", 0) == 1, "tempo"].dropna().to_numpy()
            isi = np.diff(spike_times)
            metrics.update({
                "neuron_detailed_spike_count": int(spike_times.size),
                "neuron_detailed_first_spike": int(spike_times[0]) if spike_times.size else -1,
                "neuron_detailed_last_spike": int(spike_times[-1]) if spike_times.size else -1,
                "neuron_detailed_mean_isi": float(isi.mean()) if isi.size else NA,
            })
            for source, prefix in (("V", "voltage"), ("corrente_externa", "current_external"), ("corrente_sinaptica", "current_synaptic")):
                if source in detailed:
                    values = detailed[source].dropna().to_numpy(dtype=float)
                    metrics.update({
                        f"{prefix}_mean": float(values.mean()) if values.size else NA,
                        f"{prefix}_median": float(np.median(values)) if values.size else NA,
                        f"{prefix}_min": float(values.min()) if values.size else NA,
                        f"{prefix}_max": float(values.max()) if values.size else NA,
                        f"{prefix}_std": float(values.std()) if values.size else NA,
                    })
            syn = detailed.get("corrente_sinaptica", pd.Series(dtype=float)).dropna().to_numpy(dtype=float)
            metrics["current_positive_synaptic_fraction"] = float(np.mean(syn > 0)) if syn.size else NA
            metrics["current_negative_synaptic_fraction"] = float(np.mean(syn < 0)) if syn.size else NA
            threshold = finite_number(cfg(config, "simulation", "v_threshold", NA))
            if threshold is not None and "V" in detailed:
                voltage = detailed["V"].dropna().to_numpy(dtype=float)
                metrics["voltage_time_near_threshold_fraction"] = float(np.mean(np.abs(voltage - threshold) <= 1.0)) if voltage.size else NA
        except Exception as error:
            warnings.append(f"CSV do neuronio detalhado invalido: {error}")
    else:
        warnings.append("CSV do neuronio detalhado ausente.")

    metrics.update(stability(metrics))
    regime, confidence, reasons = classify_regime(metrics)
    metrics["diagnostic_regime"] = regime
    metrics["diagnostic_confidence"] = confidence
    metrics["diagnostic_reasons"] = reasons
    metrics["performance_analysis_time_seconds"] = time.perf_counter() - started
    metrics["performance_population_csv_size_bytes"] = (run_path / "population.csv").stat().st_size if (run_path / "population.csv").exists() else 0
    metrics["performance_raster_csv_size_bytes"] = (run_path / "raster.csv").stat().st_size if (run_path / "raster.csv").exists() else 0
    metrics["performance_total_result_size_bytes"] = sum(item.stat().st_size for item in run_path.iterdir() if item.is_file())
    metrics["diagnostic_warnings"] = " | ".join(warnings)
    context = {"population": population, "counts": counts, "types": types, "events": events, "params": params, "warnings": warnings, "config": config}
    return metrics, context


def write_metrics(path: Path, metrics: dict[str, object]) -> None:
    pd.DataFrame([metrics]).to_csv(path, index=False, na_rep="NA", quoting=csv.QUOTE_MINIMAL)

