from __future__ import annotations

import csv
import html
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "results" / "model_comparison"
WARNING = (
    "Correntes e pesos numericamente iguais nao possuem necessariamente "
    "significado fisico equivalente entre modelos."
)
CASES = (
    ("lif", "configs/model_comparison_lif.ini", "model_comparison_lif"),
    ("adex", "configs/model_comparison_adex.ini", "model_comparison_adex"),
    ("hodgkin_huxley", "configs/model_comparison_hh.ini", "model_comparison_hh"),
)


def read_key_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
    return values


def neuron_metrics(path: Path) -> tuple[float, float, float, float]:
    voltages: list[float] = []
    spike_steps: list[int] = []
    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            voltages.append(float(row["V"]))
            if int(row["spike"]):
                spike_steps.append(int(row["tempo"]))
    mean_isi = 0.0
    if len(spike_steps) > 1:
        mean_isi = sum(b - a for a, b in zip(spike_steps, spike_steps[1:])) / (
            len(spike_steps) - 1
        )
    return (
        min(voltages), max(voltages),
        float(spike_steps[0]) if spike_steps else -1.0,
        mean_isi,
    )


def collect_results(run_times: dict[str, float]) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for model, _, run_name in CASES:
        directory = ROOT / "results" / "scenarios" / run_name
        summary = read_key_values(directory / "summary.txt")
        v_min, v_max, latency, mean_isi = neuron_metrics(
            directory / "neuron_0.csv"
        )
        steps = int(summary["steps"])
        elapsed = run_times[model]
        rows.append({
            "model": model,
            "spikes": int(summary["spikes_total"]),
            "firing_rate": int(summary["spikes_total"]) / steps,
            "first_spike_step": latency,
            "mean_isi_steps": mean_isi,
            "voltage_min": v_min,
            "voltage_max": v_max,
            "runtime_seconds": elapsed,
            "steps_per_second": steps / elapsed if elapsed > 0 else 0.0,
            "estimated_state_bytes_per_neuron": {
                "lif": 40, "adex": 48, "hodgkin_huxley": 72
            }[model],
            "specific_state": {
                "lif": "none", "adex": "w", "hodgkin_huxley": "m,h,n"
            }[model],
        })
    return rows


def write_outputs(rows: list[dict[str, object]], output: Path = OUTPUT) -> None:
    output.mkdir(parents=True, exist_ok=True)
    fields = list(rows[0])
    with (output / "model_comparison.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)

    lines = ["miniSNN neuron model comparison", "", WARNING, ""]
    for row in rows:
        lines.append(
            f"{row['model']}: spikes={row['spikes']}, "
            f"firing_rate={float(row['firing_rate']):.6g}, "
            f"steps/s={float(row['steps_per_second']):.2f}"
        )
    (output / "model_comparison.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")

    headers = "".join(f"<th>{html.escape(field)}</th>" for field in fields)
    body = "".join(
        "<tr>" + "".join(
            f"<td>{html.escape(str(row[field]))}</td>" for field in fields
        ) + "</tr>" for row in rows
    )
    document = f"""<!doctype html><html><head><meta charset="utf-8">
<title>miniSNN model comparison</title><style>
body{{background:#111;color:#eee;font-family:system-ui;margin:2rem}}table{{border-collapse:collapse}}
th,td{{border:1px solid #555;padding:.5rem}}th{{background:#222}}</style></head>
<body><h1>Comparacao de modelos neuronais</h1><p>{html.escape(WARNING)}</p>
<table><thead><tr>{headers}</tr></thead><tbody>{body}</tbody></table></body></html>"""
    (output / "model_comparison.html").write_text(document, encoding="utf-8")

    import matplotlib.pyplot as plt
    names = [str(row["model"]) for row in rows]
    spikes = [int(row["spikes"]) for row in rows]
    plt.figure()
    plt.bar(names, spikes)
    plt.ylabel("Spikes")
    plt.title("miniSNN: comparacao descritiva de modelos")
    plt.tight_layout()
    plt.savefig(output / "model_comparison.png")
    plt.close()


def main() -> int:
    runner = ROOT / "build" / "minisnn_runner.exe"
    if not runner.exists():
        print("Erro: compile build/minisnn_runner.exe antes da comparacao.", file=sys.stderr)
        return 1
    run_times: dict[str, float] = {}
    for model, config, _ in CASES:
        start = time.perf_counter()
        completed = subprocess.run([str(runner), config], cwd=ROOT, check=False)
        run_times[model] = time.perf_counter() - start
        if completed.returncode != 0:
            print(f"Erro ao executar {config}.", file=sys.stderr)
            return 1
    rows = collect_results(run_times)
    write_outputs(rows)
    print("Comparacao gerada em results/model_comparison")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
