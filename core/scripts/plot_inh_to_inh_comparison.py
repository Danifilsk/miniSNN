from pathlib import Path
import sys

import matplotlib.pyplot as plt
import pandas as pd


WINDOW_START = 270
WINDOW_END = 350

BASE_DIR = Path("results/experiments/inh_to_inh")

ALL_TO_ALL_SPIKES = BASE_DIR / "all_to_all_-400_spikes.csv"
NO_INH_TO_INH_SPIKES = BASE_DIR / "no_inh_to_inh_-400_spikes.csv"
ALL_TO_ALL_METRICS = BASE_DIR / "all_to_all_-400_metrics.csv"
NO_INH_TO_INH_METRICS = BASE_DIR / "no_inh_to_inh_-400_metrics.csv"

OUTPUT_RASTER_ALL_TO_ALL = BASE_DIR / "all_to_all_-400_raster.png"
OUTPUT_RASTER_NO_INH_TO_INH = BASE_DIR / "no_inh_to_inh_-400_raster.png"
OUTPUT_POPULATION_ACTIVITY = BASE_DIR / "inh_to_inh_population_activity.png"


def check_required_files(paths):
    missing = [str(path) for path in paths if not path.exists()]

    if missing:
        print("Erro: arquivos CSV ausentes:")
        for filename in missing:
            print(f"- {filename}")
        return False

    return True


def window_rows(data):
    return data[
        (data["tempo"] >= WINDOW_START) &
        (data["tempo"] <= WINDOW_END)
    ]


def plot_raster(csv_path, output_path, title):
    spikes = pd.read_csv(csv_path)
    spikes = window_rows(spikes)

    plt.figure()
    plt.scatter(spikes["tempo"], spikes["neuronio"], s=18)
    plt.xlabel("tempo")
    plt.ylabel("neuronio")
    plt.title(title)
    plt.yticks(range(20))
    plt.tight_layout()
    plt.savefig(output_path)


def plot_population_activity(all_to_all_path, no_inh_to_inh_path):
    all_to_all = window_rows(pd.read_csv(all_to_all_path))
    no_inh_to_inh = window_rows(pd.read_csv(no_inh_to_inh_path))

    plt.figure()
    plt.plot(
        all_to_all["tempo"],
        all_to_all["spikes_total"],
        label="all-to-all com INH -> INH",
    )
    plt.plot(
        no_inh_to_inh["tempo"],
        no_inh_to_inh["spikes_total"],
        label="sem INH -> INH",
    )
    plt.xlabel("tempo")
    plt.ylabel("Spikes por timestep")
    plt.title("Atividade populacional: efeito de remover INH -> INH")
    plt.legend()
    plt.tight_layout()
    plt.savefig(OUTPUT_POPULATION_ACTIVITY)


def main():
    required_files = [
        ALL_TO_ALL_SPIKES,
        NO_INH_TO_INH_SPIKES,
        ALL_TO_ALL_METRICS,
        NO_INH_TO_INH_METRICS,
    ]

    if not check_required_files(required_files):
        return 1

    plot_raster(
        ALL_TO_ALL_SPIKES,
        OUTPUT_RASTER_ALL_TO_ALL,
        "Raster: all-to-all com INH -> INH (W_INH = -400)",
    )
    plot_raster(
        NO_INH_TO_INH_SPIKES,
        OUTPUT_RASTER_NO_INH_TO_INH,
        "Raster: sem INH -> INH (W_INH = -400)",
    )
    plot_population_activity(ALL_TO_ALL_METRICS, NO_INH_TO_INH_METRICS)

    print("Graficos gerados:")
    print(f"- {OUTPUT_RASTER_ALL_TO_ALL}")
    print(f"- {OUTPUT_RASTER_NO_INH_TO_INH}")
    print(f"- {OUTPUT_POPULATION_ACTIVITY}")

    plt.show()
    return 0


if __name__ == "__main__":
    sys.exit(main())
