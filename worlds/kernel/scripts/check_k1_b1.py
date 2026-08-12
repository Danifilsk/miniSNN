#!/usr/bin/env python3
"""Functional K1-B1 occupancy and generic barrier audit."""
from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys


def run(command: list[str], cwd: pathlib.Path) -> str:
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout + result.stderr)
        raise RuntimeError("command failed")
    return result.stdout


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--library", required=True)
    parser.add_argument("--demo", required=True)
    parser.add_argument("--occupancy-test", required=True)
    parser.add_argument("--stress-test", required=True)
    args = parser.parse_args()
    root = pathlib.Path(args.root).resolve()
    include = root / "include"
    source = root / "src" / "minisnn_worlds_kernel.c"
    occupancy_header = include / "minisnn_worlds_kernel_occupancy.h"
    aggregate = include / "minisnn_worlds_kernel.h"
    command_header = include / "minisnn_worlds_kernel_command.h"
    entity_header = include / "minisnn_worlds_kernel_entity.h"
    test_source = root / "tests" / "test_k1_b1_occupancy.c"
    output = root.parent.parent / "build" / "worlds" / "kernel" / "audit" / "k1_b1"
    output.mkdir(parents=True, exist_ok=True)

    if not pathlib.Path(args.library).is_file():
        raise RuntimeError("kernel library missing")
    for path in (occupancy_header, aggregate, test_source):
        if not path.is_file():
            raise RuntimeError(f"required K1-B1 file missing: {path.name}")
    public = (aggregate.read_text(encoding="utf-8") +
              command_header.read_text(encoding="utf-8") +
              entity_header.read_text(encoding="utf-8"))
    occupancy = occupancy_header.read_text(encoding="utf-8")
    code = source.read_text(encoding="utf-8")
    tests = test_source.read_text(encoding="utf-8")
    for token in (
        "MiniSNNWorldsKernelOccupancy",
        "half_extent_x",
        "half_extent_y",
        "category_bits",
        "blocking_mask",
    ):
        if token not in occupancy:
            raise RuntimeError(f"occupancy descriptor missing: {token}")
    for token in (
        "queue_set_occupancy",
        "queue_clear_occupancy",
        "entity_has_occupancy",
        "active_occupancy_count",
    ):
        if token not in public:
            raise RuntimeError(f"aggregate API missing: {token}")
    for token in (
        "aabb_has_positive_overlap",
        "occupancies_block_each_other",
        "find_occupancy_conflict",
        "MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V3",
        "OCCUPANCY_CONFLICT",
        "related_entity",
    ):
        if token not in code:
            raise RuntimeError(f"kernel implementation missing: {token}")
    for forbidden in (
        "minisnn.h",
        "rand(",
        "srand(",
        "float ",
        "double ",
        "fish",
        "shark",
        "sprite",
        "pathfinding",
    ):
        if forbidden in code.lower():
            raise RuntimeError(f"forbidden kernel token: {forbidden}")
    for token in (
        "INVALID_OCCUPANCY",
        "OCCUPANCY_OUT_OF_BOUNDS",
        "OCCUPANCY_CONFLICT",
        "ENTITY_HAS_NO_OCCUPANCY",
        "testing_fail_next_allocation",
    ):
        if token not in tests:
            raise RuntimeError(f"missing public contract coverage: {token}")
    symbols = run(["nm", args.library], root)
    if "minisnn_worlds_kernel_testing_" in symbols:
        raise RuntimeError("testing symbols leaked into normal library")
    first = run([args.occupancy_test], root)
    second = run([args.occupancy_test], root)
    if first != second:
        raise RuntimeError("occupancy test output is not deterministic")
    stress_output = run([args.stress_test], root)
    for token in (
        "occupancy_conflicts_rejected=1000",
        "repeat_match=yes",
    ):
        if token not in stress_output:
            raise RuntimeError(f"stress did not report {token}")
    demo_output = run([
        args.demo,
        "configs/k1_occupancy_demo.ini",
        str(output),
    ], root)
    for token in (
        "conflict_rejected=yes",
        "edge_contact_allowed=yes",
        "nonblocking_overlap_allowed=yes",
        "barrier_cleared=yes",
        "placement_after_clear=yes",
        "repeat_match=yes",
        "different_mask_diverged=yes",
        "state_hash_version=3",
        "final_state_hash=0x9D8B5539282DF587",
        "status=OK",
    ):
        if token not in demo_output:
            raise RuntimeError(f"demo did not report {token}")
    for filename in ("trace.csv", "events.csv", "manifest.ini", "report.txt"):
        if not (output / filename).is_file():
            raise RuntimeError(f"missing observable artifact: {filename}")
    events = (output / "events.csv").read_text(encoding="utf-8").splitlines()
    if not events or "related_entity" not in events[0] or "category_bits" not in events[0]:
        raise RuntimeError("event artifact omits occupancy observability")
    manifest = (output / "manifest.ini").read_text(encoding="utf-8")
    if "state_hash_version=3" not in manifest or \
            "final_state_hash=0x9D8B5539282DF587" not in manifest:
        raise RuntimeError("K1-B1 v3 golden changed")
    print("K1-B1 functional checker OK")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"K1-B1 functional checker FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
