from pathlib import Path
import os
import shutil
import sys

os.environ.setdefault("MPLBACKEND", "Agg")

PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from plot_neuron import generate_neuron_plot  # noqa: E402


def main() -> int:
    temp_dir = PROJECT_ROOT / "build" / "test_plot_neuron_temp"

    if temp_dir.exists():
        shutil.rmtree(temp_dir)

    temp_dir.mkdir(parents=True)

    try:
        (temp_dir / "neuron_0.csv").write_text(
            "tempo,V,spike,corrente_externa,corrente_sinaptica\n"
            "0,-65.00,0,20.00,0.00\n"
            "1,-64.90,0,20.00,0.00\n"
            "2,-50.00,1,20.00,10.00\n"
            "3,-65.00,0,20.00,5.00\n"
            "4,-64.80,0,20.00,2.50\n",
            encoding="utf-8",
        )

        output_path = generate_neuron_plot(temp_dir, 0)

        if output_path is None or not output_path.exists():
            print("FAIL: neuron detail plot was not generated")
            return 1

        print("Neuron plot validation OK")
        return 0
    finally:
        if temp_dir.exists():
            shutil.rmtree(temp_dir)


if __name__ == "__main__":
    raise SystemExit(main())
