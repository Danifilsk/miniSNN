from __future__ import annotations

import argparse
from pathlib import Path

from html_report_common import (
    ReportGenerationError,
    generate_evolution_history_report,
    generate_evolution_report,
)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Gera relatorios HTML locais da neuroevolucao miniSNN."
    )
    parser.add_argument("directory", type=Path)
    parser.add_argument(
        "--history",
        action="store_true",
        help="gera history.html a partir de index.csv",
    )
    args = parser.parse_args()

    try:
        output = (
            generate_evolution_history_report(args.directory)
            if args.history else
            generate_evolution_report(args.directory)
        )
    except (ReportGenerationError, OSError, UnicodeError, ValueError) as error:
        print(f"Erro ao gerar relatorio evolutivo: {error}")
        return 1

    print("Relatorio evolutivo gerado:")
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
