from __future__ import annotations

import argparse
from pathlib import Path

from html_report_common import (
    ReportGenerationError,
    generate_best_topology_report,
    generate_evolution_history_report,
    generate_evolution_report,
    generate_structural_events_report,
)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Gera relatorios HTML locais da neuroevolucao miniSNN."
    )
    parser.add_argument("directory", type=Path)
    report_kind = parser.add_mutually_exclusive_group()
    report_kind.add_argument(
        "--history",
        action="store_true",
        help="gera history.html a partir de index.csv",
    )
    report_kind.add_argument(
        "--structural-events",
        action="store_true",
        help="gera structural_events_report.html",
    )
    report_kind.add_argument(
        "--best-topology",
        action="store_true",
        help="gera best_topology_report.html",
    )
    args = parser.parse_args()

    try:
        if args.history:
            output = generate_evolution_history_report(args.directory)
        elif args.structural_events:
            output = generate_structural_events_report(args.directory)
        elif args.best_topology:
            output = generate_best_topology_report(args.directory)
        else:
            output = generate_evolution_report(args.directory)
    except (ReportGenerationError, OSError, UnicodeError, ValueError) as error:
        print(f"Erro ao gerar relatorio evolutivo: {error}")
        return 1

    print("Relatorio HTML gerado:")
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
