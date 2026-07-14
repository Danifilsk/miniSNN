from pathlib import Path
import math
import sys

from html_report_common import ReportGenerationError, generate_weights_report

try:
    import matplotlib.pyplot as plt
    import pandas as pd
except ModuleNotFoundError as error:
    print(
        "Erro: dependencia Python nao encontrada: "
        f"{error.name}. Instale pandas e matplotlib."
    )
    raise SystemExit(1)


INITIAL_COLUMNS = {
    "connection_id",
    "source",
    "target",
    "weight",
    "eligible",
}
FINAL_COLUMNS = INITIAL_COLUMNS | {
    "initial_weight",
    "final_weight",
    "signed_change",
    "absolute_change",
}
HISTORY_COLUMNS = {"step", "connection_id", "source", "target", "weight"}


def read_csv_checked(path: Path, required: set[str], allow_empty: bool = False):
    try:
        data = pd.read_csv(path)
    except Exception as error:
        print(f"Erro ao ler {path}: {error}")
        return None

    missing = sorted(required.difference(data.columns))
    if missing:
        print(f"Erro: {path} nao possui colunas obrigatorias: {', '.join(missing)}")
        return None

    if data.empty and not allow_empty:
        print(f"Erro: {path} nao possui linhas de dados.")
        return None

    numeric_columns = [
        column
        for column in required
        if column not in {"source_type", "target_type"}
    ]

    try:
        for column in numeric_columns:
            data[column] = pd.to_numeric(data[column])
        if not all(
            math.isfinite(float(value))
            for column in numeric_columns
            for value in data[column]
        ):
            print(f"Erro: {path} possui valores NaN ou infinitos.")
            return None
    except Exception as error:
        print(f"Erro: {path} possui valores nao numericos: {error}")
        return None

    return data


def distributed_values(values, limit: int):
    ordered = list(dict.fromkeys(values))
    if len(ordered) <= limit:
        return ordered
    if limit <= 1:
        return ordered[:1]
    return [
        ordered[(index * (len(ordered) - 1)) // (limit - 1)]
        for index in range(limit)
    ]


def generate_plasticity_plot(run_dir: Path | str) -> Path | None:
    run_dir = Path(run_dir)
    initial_path = run_dir / "weights_initial.csv"
    final_path = run_dir / "weights_final.csv"
    metrics_path = run_dir / "plasticity_metrics.csv"
    history_path = run_dir / "weight_history.csv"
    output_path = run_dir / "plasticity_overview.png"

    if not run_dir.is_dir():
        print(f"Erro: pasta da execucao nao encontrada: {run_dir}")
        return None

    for path in (initial_path, final_path, metrics_path):
        if not path.is_file():
            print(f"Erro: arquivo obrigatorio de plasticidade nao encontrado: {path}")
            return None

    initial = read_csv_checked(initial_path, INITIAL_COLUMNS, allow_empty=True)
    final = read_csv_checked(final_path, FINAL_COLUMNS, allow_empty=True)
    metrics = read_csv_checked(
        metrics_path,
        {
            "plasticity_potentiation_events",
            "plasticity_depression_events",
            "plasticity_total_signed_change",
            "plasticity_modified_connection_fraction",
        },
    )

    if initial is None or final is None or metrics is None:
        return None

    history = None
    if history_path.is_file():
        history = read_csv_checked(history_path, HISTORY_COLUMNS, allow_empty=True)
        if history is None:
            return None

    figure, axes = plt.subplots(2, 2, figsize=(12, 8))
    figure.suptitle("Plasticidade STDP: pesos e eventos")

    if history is not None and not history.empty:
        selected_ids = distributed_values(
            sorted(history["connection_id"].unique()),
            20,
        )
        for connection_id in selected_ids:
            trajectory = history[history["connection_id"] == connection_id]
            axes[0, 0].plot(
                trajectory["step"],
                trajectory["weight"],
                label=f"c{int(connection_id)}",
            )
        if len(selected_ids) <= 10:
            axes[0, 0].legend(fontsize=7)
        axes[0, 0].set_title("Evolucao dos pesos registrados")
        axes[0, 0].set_xlabel("step")
        axes[0, 0].set_ylabel("peso")
    else:
        axes[0, 0].text(0.5, 0.5, "Historico nao registrado", ha="center")
        axes[0, 0].set_axis_off()

    if not initial.empty and not final.empty:
        axes[0, 1].hist(initial["weight"], alpha=0.6, label="inicial")
        axes[0, 1].hist(final["final_weight"], alpha=0.6, label="final")
        axes[0, 1].legend()
        axes[0, 1].set_title("Distribuicao de pesos registrados")
        axes[0, 1].set_xlabel("peso")
        axes[0, 1].set_ylabel("conexoes")
    else:
        axes[0, 1].text(0.5, 0.5, "Nenhuma conexao registrada", ha="center")
        axes[0, 1].set_axis_off()

    if not final.empty:
        selected_rows = distributed_values(list(final.index), 40)
        displayed = final.loc[selected_rows]
        axes[1, 0].bar(
            displayed["connection_id"].astype(str),
            displayed["signed_change"],
        )
        axes[1, 0].tick_params(axis="x", labelrotation=90, labelsize=7)
        axes[1, 0].axhline(0.0, linewidth=0.8)
        axes[1, 0].set_title("Mudanca assinada por conexao")
        axes[1, 0].set_xlabel("connection_id")
        axes[1, 0].set_ylabel("delta de peso")
    else:
        axes[1, 0].text(0.5, 0.5, "Nenhuma mudanca registrada", ha="center")
        axes[1, 0].set_axis_off()

    row = metrics.iloc[0]
    axes[1, 1].bar(
        ["LTP", "LTD"],
        [
            row["plasticity_potentiation_events"],
            row["plasticity_depression_events"],
        ],
    )
    axes[1, 1].set_title("Eventos STDP")
    axes[1, 1].set_ylabel("eventos")
    axes[1, 1].text(
        0.02,
        0.95,
        "fracao modificada: "
        f"{row['plasticity_modified_connection_fraction']:.3g}\n"
        "mudanca assinada total: "
        f"{row['plasticity_total_signed_change']:.3g}",
        transform=axes[1, 1].transAxes,
        va="top",
    )

    for axis in axes.flat:
        axis.grid(True, alpha=0.25)

    plt.tight_layout()
    figure.savefig(output_path)
    plt.close(figure)
    try:
        generate_weights_report(run_dir)
    except ReportGenerationError as error:
        print(f"Erro ao gerar weights_report.html: {error}")
        return None
    return output_path


def main() -> int:
    if len(sys.argv) != 2:
        print("Uso: python scripts/plot_plasticity.py <pasta_do_cenario>")
        return 1

    output = generate_plasticity_plot(sys.argv[1])
    if output is None:
        return 1

    print(f"Grafico de plasticidade gerado: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
