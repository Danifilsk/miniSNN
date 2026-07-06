from pathlib import Path
import re
import sys

try:
    import matplotlib.pyplot as plt
    import pandas as pd
except ModuleNotFoundError as error:
    print(
        "Erro: dependencia Python nao encontrada: "
        f"{error.name}. Instale pandas e matplotlib."
    )
    sys.exit(1)


CSV_FILES = {
    "exc_neuron": Path("ei_exc_only_neuron2.csv"),
    "exc_population": Path("ei_exc_only_population.csv"),
    "balanced_neuron": Path("ei_balanced_neuron2.csv"),
    "balanced_population": Path("ei_balanced_population.csv"),
}

OUTPUT_FILES = [
    "ei_compare_voltage.png",
    "ei_compare_syn_current.png",
    "ei_compare_population_spikes.png",
]


def require_csv_files() -> None:
    missing = [str(path) for path in CSV_FILES.values() if not path.exists()]

    if missing:
        print("Erro: os seguintes arquivos CSV nao foram encontrados:")
        for filename in missing:
            print(f"- {filename}")
        sys.exit(1)


def read_v_thresh() -> float:
    config_path = Path("config.h")

    if not config_path.exists():
        print("Erro: config.h nao encontrado para ler V_THRESH.")
        sys.exit(1)

    pattern = re.compile(r"^\s*#define\s+V_THRESH\s+([-+]?\d+(?:\.\d+)?)")

    for line in config_path.read_text(encoding="utf-8", errors="ignore").splitlines():
        match = pattern.match(line)

        if match:
            return float(match.group(1))

    print("Erro: V_THRESH nao encontrado em config.h.")
    sys.exit(1)


def plot_voltage(exc_neuron: pd.DataFrame, balanced_neuron: pd.DataFrame) -> None:
    v_thresh = read_v_thresh()

    plt.figure()
    plt.plot(exc_neuron["tempo"], exc_neuron["V"], label="EXC-only")
    plt.plot(balanced_neuron["tempo"], balanced_neuron["V"], label="EXC/INH")
    plt.axhline(v_thresh, linestyle="--", label="Limiar")
    plt.title("Neurônio alvo: EXC-only vs EXC/INH")
    plt.xlabel("tempo")
    plt.ylabel("Potencial de membrana (mV)")
    plt.legend()
    plt.tight_layout()
    plt.savefig("ei_compare_voltage.png")


def plot_syn_current(exc_neuron: pd.DataFrame, balanced_neuron: pd.DataFrame) -> None:
    plt.figure()
    plt.plot(
        exc_neuron["tempo"],
        exc_neuron["corrente_sinaptica"],
        label="EXC-only",
    )
    plt.plot(
        balanced_neuron["tempo"],
        balanced_neuron["corrente_sinaptica"],
        label="EXC/INH",
    )
    plt.axhline(0.0, linestyle="--", label="Zero")
    plt.title("Corrente sináptica no neurônio alvo")
    plt.xlabel("tempo")
    plt.ylabel("Corrente sináptica")
    plt.legend()
    plt.tight_layout()
    plt.savefig("ei_compare_syn_current.png")


def plot_population_spikes(
    exc_population: pd.DataFrame,
    balanced_population: pd.DataFrame,
) -> None:
    plt.figure()
    plt.plot(
        exc_population["tempo"],
        exc_population["spikes_total"],
        label="EXC-only",
    )
    plt.plot(
        balanced_population["tempo"],
        balanced_population["spikes_total"],
        label="EXC/INH",
    )
    plt.title("Atividade total da população")
    plt.xlabel("tempo")
    plt.ylabel("Spikes por timestep")
    plt.legend()
    plt.tight_layout()
    plt.savefig("ei_compare_population_spikes.png")


def main() -> int:
    require_csv_files()

    exc_neuron = pd.read_csv(CSV_FILES["exc_neuron"])
    balanced_neuron = pd.read_csv(CSV_FILES["balanced_neuron"])
    exc_population = pd.read_csv(CSV_FILES["exc_population"])
    balanced_population = pd.read_csv(CSV_FILES["balanced_population"])

    plot_voltage(exc_neuron, balanced_neuron)
    plot_syn_current(exc_neuron, balanced_neuron)
    plot_population_spikes(exc_population, balanced_population)

    print("Graficos gerados:")
    for filename in OUTPUT_FILES:
        print(f"- {filename}")

    plt.show()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
