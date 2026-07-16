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
    BEST_TOPOLOGY_REPORT_FILENAME,
    STRUCTURAL_EVENTS_REPORT_FILENAME,
    generate_best_topology_report,
    generate_structural_events_report,
)


TEMP = ROOT / "build" / "Structural HTML Tests With Spaces"
EVENT_FIELDS = [
    "generation", "child_individual_id", "event_index", "event_type",
    "old_source", "old_target", "new_source", "new_target",
    "old_magnitude", "new_magnitude", "old_delay", "new_delay",
    "connection_key_before", "connection_key_after", "status", "reason",
]
TOPOLOGY_FIELDS = [
    "connection_key", "source", "target", "source_type", "target_type",
    "magnitude", "applied_weight", "delay", "topology_role",
]


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def write_csv(path: Path, fields: list[str], rows: list[dict[str, object]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def make_run(path: Path, *, empty: bool = False) -> tuple[Path, Path]:
    path.mkdir(parents=True)
    events_path = path / "structural_events.csv"
    topology_path = path / "best_topology.csv"
    event_rows: list[dict[str, object]] = []
    topology_rows: list[dict[str, object]] = []
    if not empty:
        event_rows = [{
            "generation": 4, "child_individual_id": 11, "event_index": 0,
            "event_type": "rewire", "old_source": 0, "old_target": 1,
            "new_source": 1, "new_target": 3, "old_magnitude": 200,
            "new_magnitude": 240, "old_delay": 1, "new_delay": 3,
            "connection_key_before": 1, "connection_key_after": 8,
            "status": "applied",
            "reason": "../../outside.html <script>alert(1)</script>",
        }, {
            "generation": 5, "child_individual_id": 12, "event_index": 0,
            "event_type": "mutation_skipped", "old_source": "NA",
            "old_target": "NA", "new_source": "NA", "new_target": "NA",
            "old_magnitude": "NA", "new_magnitude": "NA", "old_delay": "NA",
            "new_delay": "NA", "connection_key_before": "NA",
            "connection_key_after": "NA", "status": "applied",
            "reason": "no_valid_candidate",
        }]
        topology_rows = [{
            "connection_key": 8, "source": 1, "target": 3, "source_type": "EXC",
            "target_type": "INH", "magnitude": 240, "applied_weight": 240,
            "delay": 3, "topology_role": "<b>heritable</b>",
        }, {
            "connection_key": 9, "source": 16, "target": 3, "source_type": "INH",
            "target_type": "EXC", "magnitude": 400, "applied_weight": -400,
            "delay": 1, "topology_role": "heritable_genome",
        }]
    write_csv(events_path, EVENT_FIELDS, event_rows)
    write_csv(topology_path, TOPOLOGY_FIELDS, topology_rows)
    (path / "evolution_manifest.txt").write_text(
        "best_topology_signature=signature_123\n", encoding="utf-8"
    )
    return events_path, topology_path


def check_relative_links(content: str) -> None:
    for href in re.findall(r'href="([^"]+)"', content):
        check(not href.startswith(("http:", "https:", "file:", "/")),
              f"external or absolute link: {href}")
        check(".." not in href and ":" not in href and "\\" not in href,
              f"unsafe relative link: {href}")


def main() -> int:
    shutil.rmtree(TEMP, ignore_errors=True)
    TEMP.mkdir(parents=True)
    try:
        run = TEMP / "experiment safe with spaces"
        events_path, topology_path = make_run(run)
        events_before = events_path.read_bytes()
        topology_before = topology_path.read_bytes()
        (run / "evolution_report.html").write_text("existing", encoding="utf-8")
        (run / "best_genome.csv").write_text("gene\n", encoding="utf-8")
        (run / STRUCTURAL_EVENTS_REPORT_FILENAME).write_text("old", encoding="utf-8")

        event_report = generate_structural_events_report(run)
        topology_report = generate_best_topology_report(run)
        event_html = event_report.read_text(encoding="utf-8")
        topology_html = topology_report.read_text(encoding="utf-8")

        check(event_report.name == STRUCTURAL_EVENTS_REPORT_FILENAME, "event filename")
        check(topology_report.name == BEST_TOPOLOGY_REPORT_FILENAME, "topology filename")
        check("Total de eventos" in event_html and "Reconexoes" in event_html,
              "event cards")
        check("Eventos rejeitados/pulados" in event_html, "skipped card")
        check("structural-event-search" in event_html, "event local search")
        check("A tabela preserva a ordem original" in event_html, "event ordering")
        check('href="structural_events.csv"' in event_html, "event CSV link")
        check("&lt;script&gt;alert(1)&lt;/script&gt;" in event_html,
              "event text escaped")
        check("<script>alert(1)</script>" not in event_html, "event script injection")
        check("../../outside.html" in event_html,
              "path traversal text not preserved as data")
        check('href="../../outside.html"' not in event_html,
              "path traversal emitted as link")
        check("Conexoes EXC" in topology_html and "Conexoes INH" in topology_html,
              "topology cards")
        check("best-topology-search" in topology_html, "topology local search")
        check("topology_role" not in topology_html, "normalized origin header")
        check("&lt;b&gt;heritable&lt;/b&gt;" in topology_html, "topology text escaped")
        check("<b>heritable</b>" not in topology_html, "topology markup injection")
        check('href="best_topology.csv"' in topology_html, "topology CSV link")
        check('href="evolution_report.html"' in topology_html, "existing evolution link")
        check('href="best_genome.csv"' in topology_html, "existing genome link")
        check('href="best_topology_lifetime_final.csv"' not in topology_html,
              "missing optional link emitted")
        check('href="best_run/topology_report.html"' not in topology_html,
              "missing nested optional link emitted")
        check("topologia herdavel inicial" in topology_html.lower(), "initial note")
        check("apos a plasticidade durante a vida" in topology_html.lower(), "lifetime note")
        check_relative_links(event_html)
        check_relative_links(topology_html)
        check("http://" not in event_html and "https://" not in event_html,
              "event external resource")
        check("http://" not in topology_html and "https://" not in topology_html,
              "topology external resource")
        check(str(run.resolve()) not in event_html + topology_html,
              "absolute directory leaked")
        check("C:\\" not in event_html + topology_html, "absolute Windows path leaked")
        check(events_path.read_bytes() == events_before, "event CSV changed")
        check(topology_path.read_bytes() == topology_before, "topology CSV changed")
        check(event_html != "old", "atomic replacement did not occur")
        check(not (run / (STRUCTURAL_EVENTS_REPORT_FILENAME + ".tmp")).exists(),
              "event temporary file remains")
        check(not (run / (BEST_TOPOLOGY_REPORT_FILENAME + ".tmp")).exists(),
              "topology temporary file remains")

        cli_events = subprocess.run(
            [sys.executable, str(ROOT / "scripts" / "generate_evolution_report.py"),
             str(run), "--structural-events"],
            capture_output=True, text=True, check=False,
        )
        cli_topology = subprocess.run(
            [sys.executable, str(ROOT / "scripts" / "generate_evolution_report.py"),
             str(run), "--best-topology"],
            capture_output=True, text=True, check=False,
        )
        check(cli_events.returncode == 0, "event CLI failed")
        check(cli_topology.returncode == 0, "topology CLI failed")

        empty = TEMP / "empty csv"
        empty_events, empty_topology = make_run(empty, empty=True)
        empty_events_before = empty_events.read_bytes()
        empty_topology_before = empty_topology.read_bytes()
        empty_event_html = generate_structural_events_report(empty).read_text(encoding="utf-8")
        empty_topology_html = generate_best_topology_report(empty).read_text(encoding="utf-8")
        check("Nenhum evento estrutural registrado." in empty_event_html,
              "empty events message")
        check("Nenhuma conexao na topologia herdavel." in empty_topology_html,
              "empty topology message")
        check(empty_events.read_bytes() == empty_events_before, "empty event CSV changed")
        check(empty_topology.read_bytes() == empty_topology_before, "empty topology CSV changed")
        check_relative_links(empty_event_html)
        check_relative_links(empty_topology_html)
    finally:
        shutil.rmtree(TEMP, ignore_errors=True)
    print("Structural HTML report validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
