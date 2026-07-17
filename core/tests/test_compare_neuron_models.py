import csv
import tempfile
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from compare_neuron_models import WARNING, write_outputs


def main() -> int:
    rows = []
    for index, model in enumerate(("lif", "adex", "hodgkin_huxley")):
        rows.append({
            "model": model,
            "spikes": index + 1,
            "firing_rate": 0.1,
            "first_spike_step": 2.0,
            "mean_isi_steps": 3.0,
            "voltage_min": -70.0,
            "voltage_max": 20.0,
            "runtime_seconds": 0.1,
            "steps_per_second": 100.0,
            "estimated_state_bytes_per_neuron": 40,
            "specific_state": "<script>alert(1)</script>" if index == 0 else "state",
        })
    with tempfile.TemporaryDirectory(prefix="minisnn model comparison ") as temp:
        output = Path(temp)
        write_outputs(rows, output)
        expected = {
            "model_comparison.csv", "model_comparison.txt",
            "model_comparison.html", "model_comparison.png",
        }
        if {path.name for path in output.iterdir()} != expected:
            return 1
        document = (output / "model_comparison.html").read_text(encoding="utf-8")
        if "<script>alert(1)</script>" in document or "https://" in document or WARNING not in document:
            return 1
        with (output / "model_comparison.csv").open(newline="", encoding="utf-8") as handle:
            if len(list(csv.DictReader(handle))) != 3:
                return 1
    print("Neuron model comparison validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
