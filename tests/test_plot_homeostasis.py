from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import sys

import pandas as pd


ROOT = Path(__file__).resolve().parents[1]
RUN = ROOT / "build" / "test homeostasis plot"


def main() -> int:
    shutil.rmtree(RUN, ignore_errors=True)
    RUN.mkdir(parents=True)

    pd.DataFrame(
        {
            "step": [0, 10, 20],
            "population_rate": [0.0, 0.03, 0.05],
            "target_rate": [0.05, 0.05, 0.05],
            "rate_error": [-0.05, -0.02, 0.0],
            "threshold_mean": [-50.0, -50.1, -50.15],
            "threshold_min": [-50.0, -50.2, -50.3],
            "threshold_max": [-50.0, -50.0, -50.0],
            "inhibitory_gain": [1.0, 1.0, 1.0],
            "incoming_exc_sum_mean": [100.0, 100.0, 100.0],
            "incoming_exc_sum_error_mean": [0.0, 0.0, 0.0],
            "scaling_factor_mean": [1.0, 1.0, 1.0],
            "scaling_events_cumulative": [0, 1, 2],
        }
    ).to_csv(RUN / "homeostasis_history.csv", index=False)
    pd.DataFrame(
        {
            "step": [0, 10, 20],
            "neuron_id": [0, 0, 0],
            "rate_trace": [0.0, 0.03, 0.05],
            "effective_threshold": [-50.0, -50.1, -50.15],
            "initial_threshold": [-50.0, -50.0, -50.0],
            "sampled": [1, 1, 1],
        }
    ).to_csv(RUN / "threshold_history.csv", index=False)
    pd.DataFrame(
        {
            "neuron_id": [0],
            "initial_incoming_exc_sum": [100.0],
            "final_incoming_exc_sum": [99.0],
        }
    ).to_csv(RUN / "homeostasis_neurons.csv", index=False)
    pd.DataFrame({"homeostasis_enabled": [True]}).to_csv(
        RUN / "homeostasis_metrics.csv", index=False
    )
    (RUN / "homeostasis_report.html").write_text(
        "<!doctype html><html><body><h1>Homeostase</h1></body></html>",
        encoding="utf-8",
    )

    input_bytes = {
        name: (RUN / name).read_bytes()
        for name in (
            "homeostasis_history.csv",
            "threshold_history.csv",
            "homeostasis_neurons.csv",
            "homeostasis_metrics.csv",
        )
    }

    result = subprocess.run(
        [sys.executable, str(ROOT / "scripts" / "plot_homeostasis.py"), str(RUN)],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        print(result.stdout)
        print(result.stderr, file=sys.stderr)
        return 1
    output = RUN / "homeostasis_overview.png"
    if not output.is_file() or output.stat().st_size == 0:
        print("homeostasis overview was not generated", file=sys.stderr)
        return 1
    html = (RUN / "homeostasis_report.html").read_text(encoding="utf-8")
    if "homeostasis_overview.png" not in html:
        print("homeostasis report was not updated", file=sys.stderr)
        return 1
    for name, original in input_bytes.items():
        if (RUN / name).read_bytes() != original:
            print(f"input CSV was modified: {name}", file=sys.stderr)
            return 1

    (RUN / "homeostasis_metrics.csv").unlink()
    missing = subprocess.run(
        [sys.executable, str(ROOT / "scripts" / "plot_homeostasis.py"), str(RUN)],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )
    if missing.returncode == 0 or "ausentes" not in missing.stderr:
        print("missing homeostasis input was not rejected clearly", file=sys.stderr)
        return 1
    (RUN / "homeostasis_metrics.csv").write_bytes(input_bytes["homeostasis_metrics.csv"])

    invalid_history = pd.read_csv(RUN / "homeostasis_history.csv")
    invalid_history.loc[1, "population_rate"] = float("inf")
    invalid_history.to_csv(RUN / "homeostasis_history.csv", index=False)
    nonfinite = subprocess.run(
        [sys.executable, str(ROOT / "scripts" / "plot_homeostasis.py"), str(RUN)],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )
    if nonfinite.returncode == 0 or "nao finito" not in nonfinite.stderr:
        print("non-finite homeostasis input was not rejected", file=sys.stderr)
        return 1

    shutil.rmtree(RUN, ignore_errors=True)
    print("Homeostasis plot validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
