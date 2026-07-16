from __future__ import annotations

import csv
from pathlib import Path
import shutil
import sys


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from plot_topology import generate  # noqa: E402


TEMP = ROOT / "build" / "Topology Plot Tests With Spaces"


def write_csv(path: Path, fields: list[str], rows: list[dict[str, object]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    shutil.rmtree(TEMP, ignore_errors=True)
    TEMP.mkdir(parents=True)
    try:
        topology_fields = [
            "connection_key", "source", "target", "source_type", "target_type",
            "weight", "magnitude", "delay", "birth_step",
        ]
        initial = [
            {"connection_key": 1, "source": 0, "target": 1,
             "source_type": "EXC", "target_type": "EXC", "weight": 10,
             "magnitude": 10, "delay": 1, "birth_step": 0},
        ]
        final = initial + [
            {"connection_key": 2, "source": 0, "target": 2,
             "source_type": "EXC", "target_type": "EXC", "weight": 5,
             "magnitude": 5, "delay": 2, "birth_step": 10},
        ]
        write_csv(TEMP / "topology_initial.csv", topology_fields, initial)
        write_csv(TEMP / "topology_final.csv", topology_fields, final)
        write_csv(
            TEMP / "topology_history.csv",
            ["step", "connection_count", "added_cumulative", "removed_cumulative"],
            [
                {"step": 0, "connection_count": 1,
                 "added_cumulative": 0, "removed_cumulative": 0},
                {"step": 10, "connection_count": 2,
                 "added_cumulative": 1, "removed_cumulative": 0},
            ],
        )
        write_csv(
            TEMP / "structural_plasticity_events.csv",
            ["step", "event_type", "status"],
            [{"step": 10, "event_type": "add", "status": "applied"}],
        )
        output = generate(TEMP)
        if not output.is_file() or output.stat().st_size <= 1000:
            raise AssertionError("topology_overview.png invalido")
        (TEMP / "topology_final.csv").write_text(
            "source,target\n0,1\n", encoding="utf-8"
        )
        try:
            generate(TEMP)
        except ValueError:
            pass
        else:
            raise AssertionError("CSV invalido foi aceito")
    finally:
        shutil.rmtree(TEMP, ignore_errors=True)
    print("Topology plot validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
