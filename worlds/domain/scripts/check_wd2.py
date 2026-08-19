#!/usr/bin/env python3
"""Validate the compact deterministic WD2 lifecycle demo artifacts."""
from __future__ import annotations

import argparse
import csv
from pathlib import Path
import sys


def fail(message: str) -> int:
    print(f"WD2 check FAIL: {message}", file=sys.stderr)
    return 1


def read_summary(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="ascii").splitlines():
        if "=" not in line:
            raise ValueError("invalid summary line")
        key, value = line.split("=", 1)
        values[key] = value
    return values


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--domain-root", required=True)
    parser.add_argument("--directory", required=True)
    args = parser.parse_args()
    directory = Path(args.directory)
    required = ["summary.txt", "events.csv", "actions.csv", "dead_snapshot_v2.bin"]
    for name in required:
        if not (directory / name).is_file():
            return fail(f"missing {name}")
    try:
        summary = read_summary(directory / "summary.txt")
    except (OSError, UnicodeError, ValueError) as error:
        return fail(str(error))
    for key in ("death_tick", "death_event_id", "death_cause", "event_count",
                "final_kernel_hash", "final_domain_hash", "dead_restore"):
        if key not in summary:
            return fail(f"summary missing {key}")
    if summary["death_cause"] != "STARVATION" or summary["dead_restore"] != "PASS":
        return fail("summary lifecycle values")
    try:
        events = list(csv.DictReader((directory / "events.csv").open(encoding="ascii", newline="")))
        actions = list(csv.DictReader((directory / "actions.csv").open(encoding="ascii", newline="")))
    except (OSError, UnicodeError, csv.Error) as error:
        return fail(str(error))
    deaths = [event for event in events if event.get("type") == "ORGANISM_DIED"]
    if len(deaths) != 1 or deaths[0].get("death_cause") != "STARVATION":
        return fail("expected exactly one starvation death event")
    if not actions or actions[-1].get("reason") != "ACTOR_DEAD" or actions[-1].get("status") != "REJECTED":
        return fail("dead action was not rejected")
    if int(summary["event_count"]) != len(events):
        return fail("event count mismatch")
    print("WD2 lifecycle validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())