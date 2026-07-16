from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import sys

import pandas as pd


ROOT = Path(__file__).resolve().parents[1]
RUN = ROOT / "build" / "test reward plot with spaces"


def write_fixture() -> None:
    pd.DataFrame({"reward_enabled": [True]}).to_csv(
        RUN / "reward_metrics.csv", index=False
    )
    pd.DataFrame(
        {
            "step": [3, 7],
            "raw_reward": [1.0, -0.5],
            "applied_reward": [1.0, -0.5],
            "event_component_count": [1, 1],
            "active_eligibility_count": [1, 1],
            "modified_connection_count": [1, 1],
            "weight_signed_change": [0.4, -0.1],
            "weight_absolute_change": [0.4, 0.1],
            "weight_clamp_min_count": [0, 0],
            "weight_clamp_max_count": [0, 0],
        }
    ).to_csv(RUN / "reward_events.csv", index=False)
    pd.DataFrame(
        {
            "step": [0, 3, 7, 10],
            "pending_reward": [0.0, 0.0, 0.0, 0.0],
            "last_applied_reward": [0.0, 1.0, -0.5, -0.5],
            "cumulative_reward": [0.0, 1.0, 0.5, 0.5],
            "cumulative_absolute_reward": [0.0, 1.0, 1.5, 1.5],
            "eligibility_mean": [0.0, 0.4, -0.2, -0.1],
            "eligibility_min": [0.0, 0.2, -0.4, -0.2],
            "eligibility_max": [0.0, 0.6, 0.0, 0.0],
            "eligibility_mean_absolute": [0.0, 0.4, 0.2, 0.1],
            "modified_connection_count_cumulative": [0, 1, 1, 1],
            "weight_signed_change_cumulative": [0.0, 0.4, 0.3, 0.3],
            "weight_absolute_change_cumulative": [0.0, 0.4, 0.5, 0.5],
        }
    ).to_csv(RUN / "reward_history.csv", index=False)
    pd.DataFrame(
        {
            "step": [0, 3, 7, 10],
            "connection_id": [0, 0, 0, 0],
            "source": [0, 0, 0, 0],
            "target": [1, 1, 1, 1],
            "eligibility": [0.0, 0.4, -0.2, -0.1],
            "sampled": [1, 1, 1, 1],
        }
    ).to_csv(RUN / "eligibility_history.csv", index=False)
    pd.DataFrame(
        {
            "connection_id": [0],
            "net_weight_change": [0.3],
        }
    ).to_csv(RUN / "reward_connections.csv", index=False)
    (RUN / "reward_report.html").write_text(
        "<!doctype html><html><body><h1>Reward</h1></body></html>", encoding="utf-8"
    )


def run_plot() -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(ROOT / "scripts" / "plot_reward.py"), str(RUN)],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )


def main() -> int:
    shutil.rmtree(RUN, ignore_errors=True)
    RUN.mkdir(parents=True)
    write_fixture()
    originals = {path.name: path.read_bytes() for path in RUN.glob("*.csv")}

    result = run_plot()
    if result.returncode != 0:
        print(result.stdout)
        print(result.stderr, file=sys.stderr)
        return 1
    output = RUN / "reward_overview.png"
    if not output.is_file() or output.stat().st_size == 0:
        print("reward overview was not generated", file=sys.stderr)
        return 1
    if "reward_overview.png" not in (RUN / "reward_report.html").read_text(encoding="utf-8"):
        print("reward report was not updated", file=sys.stderr)
        return 1
    for name, original in originals.items():
        if (RUN / name).read_bytes() != original:
            print(f"input CSV was modified: {name}", file=sys.stderr)
            return 1

    (RUN / "reward_metrics.csv").unlink()
    missing = run_plot()
    if missing.returncode == 0 or "ausentes" not in missing.stderr:
        print("missing reward input was not rejected clearly", file=sys.stderr)
        return 1
    write_fixture()
    history = pd.read_csv(RUN / "reward_history.csv")
    history.loc[0, "eligibility_mean"] = float("inf")
    history.to_csv(RUN / "reward_history.csv", index=False)
    nonfinite = run_plot()
    if nonfinite.returncode == 0 or "nao finito" not in nonfinite.stderr:
        print("non-finite reward input was not rejected", file=sys.stderr)
        return 1

    shutil.rmtree(RUN, ignore_errors=True)
    print("Reward plot validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
