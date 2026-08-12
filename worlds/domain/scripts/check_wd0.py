#!/usr/bin/env python3
"""Validate real WD0 demo artifacts, not just the demo exit code."""
from __future__ import annotations

import argparse
import csv
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit("WD0 checker: " + message)


def rows(path: Path) -> list[dict[str, str]]:
    require(path.is_file(), f"missing {path.name}")
    with path.open("r", encoding="utf-8", newline="") as file:
        return list(csv.DictReader(file))


def summary(path: Path) -> dict[str, str]:
    require(path.is_file(), "missing summary.txt")
    return dict(line.split("=", 1) for line in path.read_text(encoding="utf-8").splitlines() if "=" in line)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--domain-root", required=True)
    parser.add_argument("--directory", required=True)
    args = parser.parse_args()
    domain = Path(args.domain_root).resolve()
    output = Path(args.directory)
    require((domain / "include" / "minisnn_worlds_domain.h").is_file(), "public header missing")
    require((domain / "src" / "minisnn_worlds_domain.c").is_file(), "Domain source missing")
    entity_rows = rows(output / "domain_entities.csv")
    action_rows = rows(output / "domain_actions.csv")
    event_rows = rows(output / "domain_events.csv")
    perception_rows = rows(output / "perceptions.csv")
    kernel_hashes = rows(output / "kernel_hashes.csv")
    domain_hashes = rows(output / "domain_hashes.csv")
    values = summary(output / "summary.txt")
    require(sum(row["kind"] == "ORGANISM" for row in entity_rows) == 2, "organism count changed")
    require(sum(row["kind"] == "FOOD" for row in entity_rows) == 1, "remaining food count changed")
    require(any(row["action"] == "MOVE" and row["status"] == "APPLIED" for row in action_rows), "accepted MOVE missing")
    require(any(row["action"] == "MOVE" and row["reason"] == "KERNEL_REJECTED" for row in action_rows), "blocked MOVE missing")
    require(any(row["action"] == "EAT" and row["status"] == "APPLIED" for row in action_rows), "accepted EAT missing")
    require(any(row["action"] == "EAT" and row["reason"] == "TARGET_NOT_AVAILABLE" for row in action_rows), "unavailable EAT missing")
    require(any(row["type"] == "FOOD_CONSUMED" for row in event_rows), "food event missing")
    require(any(row["type"] == "ENERGY_CHANGED" for row in event_rows), "energy event missing")
    require(any(row["nearest_present"] == "1" for row in perception_rows), "nearest-food perception missing")
    require(len(kernel_hashes) >= 2 and len(domain_hashes) >= 2, "hash history incomplete")
    require(values.get("food_consumed") == "2", "food counter mismatch")
    require(int(values.get("energy_spent", "0")) > 0 and int(values.get("energy_gained", "0")) > 0,
            "metabolism or nutrition missing")
    print("WD0 minimal Domain validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())