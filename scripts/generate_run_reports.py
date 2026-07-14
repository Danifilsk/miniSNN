from __future__ import annotations

from pathlib import Path
import argparse

from html_report_common import (
    ReportGenerationError,
    generate_metrics_report,
    generate_reward_report,
    generate_weights_report,
)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Gera relatorios HTML locais para uma execucao da miniSNN."
    )
    parser.add_argument("run_directory", help="Pasta da execucao")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--metrics", action="store_true", help="gera metrics_report.html")
    mode.add_argument("--weights", action="store_true", help="gera weights_report.html")
    mode.add_argument("--reward", action="store_true", help="gera reward_report.html")
    mode.add_argument("--all", action="store_true", help="gera todos os relatorios disponiveis")
    args = parser.parse_args()
    run_dir = Path(args.run_directory)

    try:
        outputs = []
        if args.metrics or args.all:
            outputs.append(generate_metrics_report(run_dir))
        if args.weights or args.all:
            outputs.append(generate_weights_report(run_dir))
        if args.reward or (args.all and (run_dir / "reward_metrics.csv").is_file()):
            outputs.append(generate_reward_report(run_dir))
    except (ReportGenerationError, OSError, UnicodeError) as error:
        print(f"Erro: {error}")
        return 1

    print("Relatorios HTML gerados:")
    for output in outputs:
        print(f"- {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
