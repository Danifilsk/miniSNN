from pathlib import Path
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


POPULATION_COLUMNS = {
    "tempo",
    "spikes_total",
    "spikes_exc",
    "spikes_inh",
    "mean_potential",
    "mean_syn_current",
}

RASTER_COLUMNS = {"tempo", "neuronio", "tipo"}


def require_columns(data: pd.DataFrame, columns: set[str], filename: Path) -> bool:
    missing = sorted(columns - set(data.columns))

    if missing:
        print(f"Erro: {filename} nao possui colunas obrigatorias:")
        for column in missing:
            print(f"- {column}")
        return False

    return True


def read_csv(path: Path, columns: set[str]) -> pd.DataFrame | None:
    try:
        data = pd.read_csv(path)
    except Exception as error:  # pandas pode levantar excecoes variadas.
        print(f"Erro ao ler {path}: {error}")
        return None

    if not require_columns(data, columns, path):
        return None

    return data


def plot_population_activity(population: pd.DataFrame, output_path: Path) -> None:
    plt.figure()
    plt.plot(population["tempo"], population["spikes_total"], label="total")
    plt.plot(population["tempo"], population["spikes_exc"], label="EXC")
    plt.plot(population["tempo"], population["spikes_inh"], label="INH")
    plt.title("Atividade populacional")
    plt.xlabel("tempo")
    plt.ylabel("Spikes por timestep")
    plt.legend()
    plt.tight_layout()
    plt.savefig(output_path)


def plot_mean_state(population: pd.DataFrame, output_path: Path) -> None:
    plt.figure()
    plt.plot(population["tempo"], population["mean_potential"], label="mean V")
    plt.plot(
        population["tempo"],
        population["mean_syn_current"],
        label="mean syn current",
    )
    plt.title("Estado medio da rede")
    plt.xlabel("tempo")
    plt.ylabel("Valor medio")
    plt.legend()
    plt.tight_layout()
    plt.savefig(output_path)


def plot_raster(raster: pd.DataFrame, output_path: Path) -> None:
    plt.figure()

    for neuron_type in ("EXC", "INH"):
        rows = raster[raster["tipo"] == neuron_type]
        plt.scatter(rows["tempo"], rows["neuronio"], s=12, label=neuron_type)

    plt.title("Raster de spikes")
    plt.xlabel("tempo")
    plt.ylabel("neuronio")
    plt.legend()
    plt.tight_layout()
    plt.savefig(output_path)


def main() -> int:
    if len(sys.argv) != 2:
        print("Uso: python scripts/plot_scenario.py <pasta_do_cenario>")
        return 1

    run_dir = Path(sys.argv[1])
    population_path = run_dir / "population.csv"
    raster_path = run_dir / "raster.csv"

    if not run_dir.exists() or not run_dir.is_dir():
        print(f"Erro: pasta da execucao nao encontrada: {run_dir}")
        return 1

    missing = [
        path for path in (population_path, raster_path)
        if not path.exists()
    ]

    if missing:
        print("Erro: arquivos CSV obrigatorios ausentes:")
        for path in missing:
            print(f"- {path}")
        return 1

    population = read_csv(population_path, POPULATION_COLUMNS)
    raster = read_csv(raster_path, RASTER_COLUMNS)

    if population is None or raster is None:
        return 1

    outputs = [
        run_dir / "population_activity.png",
        run_dir / "mean_state.png",
        run_dir / "raster.png",
    ]

    plot_population_activity(population, outputs[0])
    plot_mean_state(population, outputs[1])
    plot_raster(raster, outputs[2])

    print("Graficos gerados:")
    for output in outputs:
        print(f"- {output}")

    plt.show()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
