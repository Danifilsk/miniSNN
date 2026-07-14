from __future__ import annotations

from pathlib import Path
import argparse

from html_report_common import (
    HISTORY_REPORT_FILENAME,
    ReportGenerationError,
    generate_scenario_history_report,
)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Gera o historico HTML local das execucoes miniSNN."
    )
    parser.add_argument(
        "scenarios_directory",
        type=Path,
        help="Pasta que contem index.csv e as pastas das execucoes.",
    )
    parser.add_argument(
        "--output",
        default=HISTORY_REPORT_FILENAME,
        help="Nome do HTML dentro da pasta de cenarios.",
    )
    args = parser.parse_args()

    try:
        output = generate_scenario_history_report(
            args.scenarios_directory,
            args.output,
        )
    except (ReportGenerationError, OSError, UnicodeError, ValueError) as error:
        print(f"Erro ao gerar historico HTML: {error}")
        return 1

    print("Historico HTML gerado:")
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
