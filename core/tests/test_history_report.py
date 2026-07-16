from __future__ import annotations

from pathlib import Path
import csv
import re
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from html_report_common import (  # noqa: E402
    EVOLUTION_HISTORY_COLUMNS,
    SCENARIO_HISTORY_COLUMNS,
    generate_evolution_history_report,
)


TEMP = ROOT / "build" / "test_history_report"
SCRIPT = ROOT / "scripts" / "generate_history_report.py"


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def scenarios_directory(name: str) -> Path:
    path = TEMP / name / "results" / "scenarios"
    path.mkdir(parents=True, exist_ok=True)
    return path


def row(
    run_name: str,
    timestamp: str,
    run_path: str,
    topology: str = "chain",
    status: str = "OK",
) -> dict[str, str]:
    return {
        "timestamp": timestamp,
        "run_name": run_name,
        "actual_run_name": run_name + "_actual",
        "run_path": run_path,
        "config_path": f"configs/{run_name}.ini",
        "topology": topology,
        "num_neurons": "5",
        "steps": "100",
        "dt": "0.1",
        "seed": "7",
        "recorded_neuron": "0",
        "total_connections": "20",
        "total_spikes": "12",
        "first_active_step": "10",
        "last_active_step": "90",
        "status": status,
    }


def write_index(
    directory: Path,
    rows: list[dict[str, str]],
    extra_columns: tuple[str, ...] = (),
) -> bytes:
    path = directory / "index.csv"
    fieldnames = [*SCENARIO_HISTORY_COLUMNS, *extra_columns]
    with path.open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        writer.writeheader()
        for item in rows:
            complete = dict(item)
            for name in extra_columns:
                complete[name] = "future-value"
            writer.writerow(complete)
    return path.read_bytes()


def run_report(directory: Path, output: str = "history.html") -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), str(directory), "--output", output],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def test_basic_history() -> None:
    directory = scenarios_directory("basic")
    run_old = directory / "run_old"
    run_new = directory / "run_new"
    run_middle = directory / "Test Run With Spaces"
    for run_dir in (run_old, run_new, run_middle):
        run_dir.mkdir()

    for filename in (
        "summary.txt",
        "metrics_report.html",
        "run_manifest.txt",
        "weights_report.html",
        "reward_report.html",
        "homeostasis_report.html",
    ):
        (run_new / filename).write_text(filename, encoding="utf-8")
    (run_old / "summary.txt").write_text("old", encoding="utf-8")
    (run_middle / "summary.txt").write_text("spaces", encoding="utf-8")

    original = write_index(
        directory,
        [
            row("run_old", "20260101_010101", str(run_old), "chain", "OK"),
            row("run_new", "20260301_010101", str(run_new), "all_to_all", "OK"),
            row("run_middle", "20260201_010101", str(run_middle), "chain", "ERROR"),
        ],
        ("future_column",),
    )
    completed = run_report(directory)
    check(completed.returncode == 0, completed.stdout)
    output = directory / "history.html"
    check(output.is_file() and output.stat().st_size > 0, "history.html missing")
    content = output.read_text(encoding="utf-8")
    check((directory / "index.csv").read_bytes() == original, "index.csv changed")
    check(content.count('class="history-row"') == 3, "wrong history row count")
    check("Total de execucoes" in content and '<div class="value">3</div>' in content, "wrong total card")
    check("Execucoes OK" in content and "Execucoes com erro" in content, "status cards missing")
    check(content.index("run_new") < content.index("run_middle") < content.index("run_old"), "history is not newest first")
    check("BUSCAR EXECUCOES" in content and 'id="history-search"' in content, "search missing")
    check('id="history-status"' in content and 'id="history-topology"' in content, "filters missing")
    check("ERRO: ERROR" in content, "error status lacks textual distinction")
    check("run_new/metrics_report.html" in content, "existing metrics link missing")
    check("run_new/reward_report.html" in content, "existing reward link missing")
    check("run_new/plasticity_overview.png" not in content, "missing artifact received a link")
    check("Test%20Run%20With%20Spaces/summary.txt" in content, "space path not encoded")
    check('href="index.csv"' in content, "raw index link missing")
    check("http://" not in content and "https://" not in content, "external dependency found")


def test_empty_history() -> None:
    directory = scenarios_directory("empty")
    original = write_index(directory, [])
    completed = run_report(directory)
    check(completed.returncode == 0, completed.stdout)
    content = (directory / "history.html").read_text(encoding="utf-8")
    check("Nenhuma execucao foi registrada ainda" in content, "empty message missing")
    check('<div class="value">0</div>' in content, "empty total is not zero")
    check((directory / "index.csv").read_bytes() == original, "empty index changed")


def test_removed_and_hostile_runs() -> None:
    directory = scenarios_directory("security")
    outside = TEMP / "outside"
    outside.mkdir(parents=True, exist_ok=True)
    (outside / "summary.txt").write_text("outside", encoding="utf-8")
    malicious_name = "<script>alert(1)</script>"
    malicious_actual = "<img src=x onerror=alert(1)>"
    hostile = row(
        malicious_name,
        "20260401_010101",
        "../../outside",
        "ring",
        "FAILED",
    )
    hostile["actual_run_name"] = malicious_actual
    removed = row(
        "removed_run",
        "20260301_010101",
        str(directory / "removed_run"),
    )
    write_index(directory, [hostile, removed])
    completed = run_report(directory)
    check(completed.returncode == 0, completed.stdout)
    content = (directory / "history.html").read_text(encoding="utf-8")
    check("&lt;script&gt;alert(1)&lt;/script&gt;" in content, "script text not escaped")
    check("&lt;img src=x onerror=alert(1)&gt;" in content, "image text not escaped")
    check("<script>alert(1)</script>" not in content, "script injection present")
    check("<img src=x onerror=alert(1)>" not in content, "attribute injection present")
    check(content.count("arquivos indisponiveis") == 2, "unavailable runs not retained")
    check("../" not in content and "..\\" not in content, "path traversal leaked")
    check(str(outside) not in content, "absolute outside path leaked")
    check("file:///" not in content.lower(), "file URI leaked")
    check(not re.search(r"[A-Za-z]:[\\/]", content), "absolute Windows path leaked")


def test_invalid_csv_preserves_previous_report() -> None:
    directory = scenarios_directory("invalid")
    index = directory / "index.csv"
    output = directory / "history.html"
    previous = b"PREVIOUS HISTORY"

    cases: list[tuple[str, bytes | None]] = [
        ("missing", None),
        ("empty", b""),
        ("header_absent", b"x,y\n1,2\n"),
        ("duplicate_header", b"timestamp,timestamp\n1,2\n"),
        ("missing_column", b"timestamp,run_name\n1,a\n"),
        (
            "incomplete_row",
            (",".join(SCENARIO_HISTORY_COLUMNS) + "\n20260101,run\n").encode("utf-8"),
        ),
    ]
    for name, payload in cases:
        index.unlink(missing_ok=True)
        output.write_bytes(previous)
        if payload is not None:
            index.write_bytes(payload)
        completed = run_report(directory)
        check(completed.returncode != 0, f"invalid case accepted: {name}")
        check("Erro ao gerar historico HTML:" in completed.stdout, f"unclear error: {name}")
        check("Traceback" not in completed.stdout, f"traceback shown: {name}")
        check(output.read_bytes() == previous, f"previous HTML changed: {name}")
        check(not (directory / "history.html.tmp").exists(), f"temporary file left: {name}")

    write_index(directory, [])
    completed = run_report(directory, "index.csv")
    check(completed.returncode != 0, "index.csv accepted as HTML output")


def test_large_history() -> None:
    directory = scenarios_directory("large")
    rows = [
        row(
            f"synthetic_{index:04d}",
            f"2026{(index % 12) + 1:02d}{(index % 28) + 1:02d}_{index % 24:02d}0000",
            str(directory / f"missing_{index:04d}"),
            "random" if index % 2 else "chain",
            "OK" if index % 3 else "ERROR",
        )
        for index in range(1000)
    ]
    write_index(directory, rows)
    completed = run_report(directory)
    check(completed.returncode == 0, completed.stdout)
    output = directory / "history.html"
    content = output.read_text(encoding="utf-8")
    check(content.count('class="history-row"') == 1000, "large history lost rows")
    check("synthetic_0000" in content and "synthetic_0999" in content, "large history endpoints missing")
    check(output.stat().st_size < 5_000_000, "large history output is unreasonable")
    check("querySelectorAll" in content, "local search script missing")


def test_evolution_history() -> None:
    directory = TEMP / "evolution history" / "results" / "evolution"
    directory.mkdir(parents=True)
    valid_run = directory / "experiment with spaces"
    valid_run.mkdir()
    for filename in (
        "evolution_report.html", "evolution_overview.png",
        "evolution_manifest.txt", "best_genome.csv",
    ):
        (valid_run / filename).write_text(filename, encoding="utf-8")

    rows = []
    for index in range(1000):
        rows.append({
            "timestamp": f"2026{(index % 12) + 1:02d}{(index % 28) + 1:02d}_{index % 24:02d}0000",
            "experiment_name": "<script>alert(1)</script>" if index == 0 else f"experiment_{index:04d}",
            "actual_experiment_name": f"actual_{index:04d}",
            "experiment_path": (
                str(valid_run) if index == 1 else
                "../../outside" if index == 0 else
                str(directory / f"missing_{index:04d}")
            ),
            "config_path": f"configs/evolution_{index}.ini",
            "base_scenario": "configs/base.ini",
            "population_size": "20", "generations": "10", "gene_count": "8",
            "best_fitness": "0.75", "best_individual_id": str(index),
            "status": "OK" if index % 3 else "ERROR",
        })
    index_path = directory / "index.csv"
    with index_path.open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=EVOLUTION_HISTORY_COLUMNS)
        writer.writeheader()
        writer.writerows(rows)
    original = index_path.read_bytes()

    output = generate_evolution_history_report(directory)
    content = output.read_text(encoding="utf-8")
    check(index_path.read_bytes() == original, "evolution index changed")
    check(content.count('class="history-row"') == 1000,
          "evolution history lost rows")
    check("BUSCAR EXPERIMENTOS" in content, "evolution search missing")
    check("experiment%20with%20spaces/evolution_report.html" in content,
          "evolution relative report link missing")
    check("&lt;script&gt;alert(1)&lt;/script&gt;" in content,
          "evolution hostile text not escaped")
    check("<script>alert(1)</script>" not in content,
          "evolution script injection")
    check("../" not in content and "file:///" not in content.lower(),
          "evolution path traversal leaked")
    check("http://" not in content and "https://" not in content,
          "evolution external dependency")


def main() -> int:
    shutil.rmtree(TEMP, ignore_errors=True)
    TEMP.mkdir(parents=True)
    try:
        test_basic_history()
        test_empty_history()
        test_removed_and_hostile_runs()
        test_invalid_csv_preserves_previous_report()
        test_large_history()
        test_evolution_history()
    finally:
        shutil.rmtree(TEMP, ignore_errors=True)
    print("History HTML report validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
