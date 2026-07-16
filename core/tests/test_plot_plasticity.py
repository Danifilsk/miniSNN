from pathlib import Path
import os
import shutil
import sys

os.environ.setdefault("MPLBACKEND", "Agg")

PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from plot_plasticity import generate_plasticity_plot  # noqa: E402


def main() -> int:
    temp_dir = PROJECT_ROOT / "build" / "test plot plasticity"

    if temp_dir.exists():
        shutil.rmtree(temp_dir)
    temp_dir.mkdir(parents=True)

    try:
        (temp_dir / "weights_initial.csv").write_text(
            "connection_id,source,target,source_type,target_type,delay,weight,eligible,sampled\n"
            "0,0,1,EXC,EXC,1,10,1,0\n",
            encoding="utf-8",
        )
        (temp_dir / "weights_final.csv").write_text(
            "connection_id,source,target,source_type,target_type,delay,weight,eligible,sampled,initial_weight,final_weight,signed_change,absolute_change\n"
            "0,0,1,EXC,EXC,1,11,1,0,10,11,1,1\n",
            encoding="utf-8",
        )
        (temp_dir / "weight_history.csv").write_text(
            "step,connection_id,source,target,weight\n"
            "0,0,0,1,10\n10,0,0,1,11\n",
            encoding="utf-8",
        )
        (temp_dir / "plasticity_metrics.csv").write_text(
            "plasticity_potentiation_events,plasticity_depression_events,plasticity_total_signed_change,plasticity_modified_connection_fraction\n"
            "2,1,1,1\n",
            encoding="utf-8",
        )

        output = generate_plasticity_plot(temp_dir)
        if output is None or not output.is_file() or output.stat().st_size == 0:
            print("FAIL: plasticity PNG was not generated")
            return 1
        report = temp_dir / "weights_report.html"
        if not report.is_file() or report.stat().st_size == 0:
            print("FAIL: weights HTML report was not generated")
            return 1

        (temp_dir / "weights_final.csv").write_text(
            "invalid\n1\n",
            encoding="utf-8",
        )
        if generate_plasticity_plot(temp_dir) is not None:
            print("FAIL: invalid plasticity input was accepted")
            return 1

        print("Plasticity plot validation OK")
        return 0
    finally:
        if temp_dir.exists():
            shutil.rmtree(temp_dir)


if __name__ == "__main__":
    raise SystemExit(main())
