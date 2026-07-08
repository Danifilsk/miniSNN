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


COLUMN_ALIASES = {
    "tempo": ("tempo", "time", "t", "step"),
    "V": ("V", "v", "voltage", "potencial", "potencial_membrana"),
    "spike": ("spike", "spikes"),
    "corrente_externa": ("corrente_externa", "external_current", "ext_current"),
    "corrente_sinaptica": (
        "corrente_sinaptica",
        "synaptic_current",
        "syn_current",
        "used_syn_current",
    ),
}


def find_column(data: pd.DataFrame, canonical_name: str) -> str | None:
    aliases = COLUMN_ALIASES[canonical_name]
    lower_columns = {column.lower(): column for column in data.columns}

    for alias in aliases:
        if alias.lower() in lower_columns:
            return lower_columns[alias.lower()]

    return None


def normalize_columns(data: pd.DataFrame, filename: Path) -> pd.DataFrame | None:
    renamed = {}

    for canonical_name in COLUMN_ALIASES:
        column = find_column(data, canonical_name)
        if column is None:
            print(f"Erro: {filename} nao possui a coluna obrigatoria '{canonical_name}'.")
            return None

        renamed[column] = canonical_name

    return data.rename(columns=renamed)


def read_neuron_csv(path: Path) -> pd.DataFrame | None:
    try:
        data = pd.read_csv(path)
    except Exception as error:  # pandas pode levantar excecoes variadas.
        print(f"Erro ao ler {path}: {error}")
        return None

    if data.empty:
        print(f"Erro: {path} nao possui linhas de dados.")
        return None

    data = normalize_columns(data, path)
    if data is None:
        return None

    try:
        return data[
            [
                "tempo",
                "V",
                "spike",
                "corrente_externa",
                "corrente_sinaptica",
            ]
        ].apply(pd.to_numeric)
    except Exception as error:
        print(f"Erro: {path} possui valores nao numericos: {error}")
        return None


def generate_neuron_plot(run_dir: Path | str, neuron_id: int | str) -> Path | None:
    run_dir = Path(run_dir)

    try:
        neuron_id = int(neuron_id)
    except ValueError:
        print(f"Erro: ID de neuronio invalido: {neuron_id}")
        return None

    if neuron_id < 0:
        print(f"Erro: ID de neuronio invalido: {neuron_id}")
        return None

    if not run_dir.exists() or not run_dir.is_dir():
        print(f"Erro: pasta da execucao nao encontrada: {run_dir}")
        return None

    csv_path = run_dir / f"neuron_{neuron_id}.csv"
    output_path = run_dir / f"neuron_{neuron_id}_detail.png"

    if not csv_path.exists():
        print(f"Erro: arquivo do neuronio nao encontrado: {csv_path}")
        return None

    data = read_neuron_csv(csv_path)
    if data is None:
        return None

    fig, axes = plt.subplots(4, 1, sharex=True, figsize=(10, 8))
    fig.suptitle(f"Neuronio {neuron_id}: sinais detalhados")

    axes[0].plot(data["tempo"], data["V"])
    axes[0].set_ylabel("V (mV)")
    axes[0].grid(True, alpha=0.3)

    axes[1].plot(data["tempo"], data["spike"], drawstyle="steps-post")
    axes[1].set_ylabel("spike")
    axes[1].set_ylim(-0.1, 1.1)
    axes[1].grid(True, alpha=0.3)

    axes[2].plot(data["tempo"], data["corrente_externa"])
    axes[2].set_ylabel("corrente externa")
    axes[2].grid(True, alpha=0.3)

    axes[3].plot(data["tempo"], data["corrente_sinaptica"])
    axes[3].set_xlabel("tempo")
    axes[3].set_ylabel("corrente sinaptica")
    axes[3].grid(True, alpha=0.3)

    plt.tight_layout()
    fig.savefig(output_path)
    plt.close(fig)

    return output_path


def main() -> int:
    if len(sys.argv) != 3:
        print("Uso: python scripts/plot_neuron.py <pasta_do_cenario> <neuron_id>")
        return 1

    output_path = generate_neuron_plot(sys.argv[1], sys.argv[2])
    if output_path is None:
        return 1

    print(f"Grafico do neuronio gerado: {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
