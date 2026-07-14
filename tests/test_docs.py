from pathlib import Path
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from check_docs import validate_docs


def main() -> int:
    for relative in (
        "scripts/generate_run_reports.py",
        "scripts/html_report_common.py",
        "tests/test_run_reports.py",
        "scripts/plot_homeostasis.py",
        "scripts/check_c15.py",
        "docs/GUIA_DE_HOMEOSTASE.md",
        "scripts/plot_reward.py",
        "scripts/run_benchmarks_c2.py",
        "scripts/check_c2.py",
        "docs/GUIA_DE_RECOMPENSA.md",
        "docs/BENCHMARKS_C2_RECOMPENSA.md",
        "tests/test_reward.c",
        "tests/test_reward_long.c",
        "tests/test_plot_reward.py",
        "scripts/generate_history_report.py",
        "tests/test_history_report.py",
    ):
        if not (PROJECT_ROOT / relative).is_file():
            print(f"Documentation validation FAILED\n- arquivo HTML ausente: {relative}")
            return 1
    errors = validate_docs(PROJECT_ROOT)
    if errors:
        print("Documentation validation FAILED")
        for error in errors:
            print(f"- {error}")
        return 1
    print("Documentation validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
