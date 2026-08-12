#!/usr/bin/env python3
"""Focused K1-C2 audit for atomic rigid subtree translation."""
from __future__ import annotations
import argparse
import pathlib
import subprocess
import sys


def require_tokens(text: str, tokens: tuple[str, ...], label: str) -> None:
    for token in tokens:
        if token not in text:
            raise RuntimeError(f"{label} missing: {token}")


def run(command: list[str], cwd: pathlib.Path) -> str:
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout + result.stderr)
        raise RuntimeError("command returned an unexpected status")
    return result.stdout


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--library", required=True)
    parser.add_argument("--subtree-test", required=True)
    args = parser.parse_args()
    root = pathlib.Path(args.root).resolve()
    source = root / "src" / "minisnn_worlds_kernel.c"
    test = root / "tests" / "test_k1_c2_subtree_movement.c"
    audit = root / "docs" / "K1_C2_SUBTREE_TRANSLATION_AUDIT.md"
    state_hash = root / "docs" / "STATE_HASH_CONTRACT.md"
    for path in (source, test, audit, state_hash, pathlib.Path(args.library)):
        if not path.is_file():
            raise RuntimeError(f"required K1-C2 file missing: {path.name}")
    code = source.read_text(encoding="utf-8")
    coverage = test.read_text(encoding="utf-8")
    require_tokens(code, (
        "collect_spatial_subtree", "calculate_subtree_destinations",
        "find_subtree_external_occupancy_conflict", "reserve_step_events",
        "next_event_count", "TARGET_HAS_SPATIAL_PARENT", "affected_entity",
    ), "K1-C2 implementation")
    require_tokens(coverage, (
        "test_rigid_tree_events_and_zero_delta", "test_child_guard_and_external_conflict",
        "test_overflow_ordering_and_rollback", "test_same_tick_link_and_move_and_scale",
        "test_internal_collision_and_canonical_external_conflict",
        "test_same_tick_link_move_ordering_and_two_roots", "test_internal_failure_rollbacks",
        "testing_fail_allocation_after", "testing_set_observability_counters",
    ), "K1-C2 coverage")
    require_tokens(audit.read_text(encoding="utf-8"), (
        "Status:", "canonical", "atomic", "affected_entity", "K1-C3",
    ), "K1-C2 audit documentation")
    require_tokens(state_hash.read_text(encoding="utf-8"), (
        "State hash V5", "subtree", "affected_entity",
    ), "V5 hash documentation")
    for forbidden in ("reparent", "link_id", "pathfinding", "physics"):
        if forbidden in code.lower():
            raise RuntimeError(f"K1-C2 implementation contains future-scope token: {forbidden}")
    if "minisnn_worlds_kernel_testing_" in run(["nm", args.library], root):
        raise RuntimeError("testing symbols leaked into normal library")
    first = run([args.subtree_test], root)
    second = run([args.subtree_test], root)
    if first != second:
        raise RuntimeError("K1-C2 test output is not deterministic")
    require_tokens(first, ("K1-C2 subtree translation, events, atomicity and V5 validation OK",),
                   "K1-C2 test output")
    print("K1-C2 subtree translation and V5 audit OK")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"K1-C2 subtree translation and V5 audit FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)