#!/usr/bin/env python3
"""Validates canonical artifacts from the K1-C4 spatial-links integration demo."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path

REQUIRED = (
    "config_used.ini", "commands.csv", "events.csv", "entities.csv",
    "spatial_links.csv", "diagnostics.csv", "state_hash.txt", "summary.txt",
)
COMMAND_HEADER = [
    "command_id", "target_tick", "priority", "issuer", "type", "target",
    "parent", "child", "delta_x", "delta_y", "half_extent_x", "half_extent_y",
    "category_bits", "blocking_mask",
]
EVENT_HEADER = [
    "event_id", "tick", "type", "command_id", "issuer", "subject",
    "related_entity", "affected_entity", "rejection", "previous_x",
    "previous_y", "previous_orientation", "x", "y", "orientation",
    "delta_x", "delta_y", "link_parent", "link_child", "offset_x", "offset_y",
]


def fail(message: str) -> None:
    raise SystemExit(f"K1-C4 artifact validation FAILED: {message}")


def rows(path: Path, header: list[str]) -> list[dict[str, str]]:
    with path.open("r", encoding="ascii", newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != header:
            fail(f"unexpected header in {path.name}")
        return list(reader)


def values(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for raw in path.read_text(encoding="ascii").splitlines():
        key, separator, value = raw.partition("=")
        if not separator or not key or key in result:
            fail(f"invalid key/value text in {path.name}")
        result[key] = value
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", required=True)
    parser.add_argument("--golden")
    args = parser.parse_args()

    directory = Path(args.directory)
    if not directory.is_dir():
        fail("artifact directory is missing")
    for name in REQUIRED:
        if not (directory / name).is_file():
            fail(f"missing {name}")

    commands = rows(directory / "commands.csv", COMMAND_HEADER)
    events = rows(directory / "events.csv", EVENT_HEADER)
    diagnostics = rows(directory / "diagnostics.csv", [
        "completed_ticks", "state_hash_version", "state_hash", "alive_entities",
        "placed_entities", "active_occupancies", "blocking_occupancies",
        "active_spatial_links", "total_commands_submitted", "total_commands_applied",
        "total_commands_rejected", "total_events_emitted", "total_entities_moved",
        "total_movement_overflows_rejected", "total_occupancy_conflicts_rejected",
        "total_spatial_links_created", "total_spatial_links_removed",
    ])
    links = rows(directory / "spatial_links.csv", ["parent", "child", "offset_x", "offset_y"])
    state = values(directory / "state_hash.txt")
    summary = values(directory / "summary.txt")

    if len(diagnostics) != 1 or not commands or not events:
        fail("empty canonical trace")
    if [int(row["event_id"]) for row in events] != list(range(1, len(events) + 1)):
        fail("event identifiers are not consecutive")
    ordering = [(int(row["target_tick"]), int(row["priority"]), int(row["issuer"]), int(row["command_id"])) for row in commands]
    if ordering != sorted(ordering):
        fail("commands are not canonical")
    if diagnostics[0]["state_hash_version"] != "5" or state.get("state_hash_version") != "V5":
        fail("state hash V5 is absent")
    if state.get("invariants") != "PASS" or summary.get("invariants") != "PASS":
        fail("invariant status is not PASS")
    if diagnostics[0]["state_hash"] != state.get("state_hash") or state.get("state_hash") != summary.get("state_hash"):
        fail("inconsistent state hash")

    def rejection(tick: int, subject: int, reason: str, related: int, affected: int) -> bool:
        return any(
            int(row["tick"]) == tick and row["type"] == "command_rejected" and
            int(row["subject"]) == subject and row["rejection"] == reason and
            int(row["related_entity"]) == related and int(row["affected_entity"]) == affected
            for row in events
        )

    if not rejection(7, 2, "target_has_spatial_parent", 1, 0):
        fail("direct child movement rejection missing")
    if not rejection(9, 1, "occupancy_conflict", 7, 3):
        fail("canonical external collision missing")
    if not rejection(10, 5, "destination_overflow", 0, 5):
        fail("overflow rejection missing")
    if not rejection(14, 1, "target_has_spatial_links", 2, 0):
        fail("lifecycle rejection missing")
    tick5 = [row for row in events if row["tick"] == "5" and row["type"] == "entity_moved"]
    if [int(row["subject"]) for row in tick5] != [1, 2, 4, 3]:
        fail("root subtree BFS event order differs")
    if len([row for row in events if row["tick"] == "6"]) != 1:
        fail("zero displacement emitted descendant events")
    if [(row["parent"], row["child"]) for row in links] != [("2", "3"), ("5", "6")]:
        fail("final spatial forest differs")
    if diagnostics[0]["active_spatial_links"] != "2" or diagnostics[0]["total_commands_rejected"] != "4":
        fail("diagnostic counters differ")
    if args.golden:
        golden = Path(args.golden).read_text(encoding="ascii").strip()
        if state["state_hash"] != golden:
            fail(f"hash {state['state_hash']} differs from golden {golden}")

    print("K1-C4 integrated spatial links demo validation OK")


if __name__ == "__main__":
    main()