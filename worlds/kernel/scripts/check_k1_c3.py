#!/usr/bin/env python3
"""Focused K1-C3 audit for canonical ordering and structural hardening."""
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
    parser.add_argument("--ordering-test", required=True)
    parser.add_argument("--invariants-test", required=True)
    parser.add_argument("--limits-test", required=True)
    parser.add_argument("--long-run-test", required=True)
    args = parser.parse_args()
    root = pathlib.Path(args.root).resolve()
    required = (
        root / "src" / "minisnn_worlds_kernel.c",
        root / "tests" / "test_k1_c3_ordering.c",
        root / "tests" / "test_k1_c3_invariants.c",
        root / "tests" / "test_k1_c3_limits.c",
        root / "tests" / "test_k1_c3_long_run.c",
        root / "docs" / "K1_C3_ORDERING_AND_HARDENING_AUDIT.md",
        root / "docs" / "COMMAND_AND_EVENT_CONTRACT.md",
        root / "docs" / "STATE_HASH_CONTRACT.md",
        pathlib.Path(args.library),
    )
    for path in required:
        if not path.is_file():
            raise RuntimeError(f"required K1-C3 file missing: {path.name}")
    source = required[0].read_text(encoding="utf-8")
    ordering = required[1].read_text(encoding="utf-8")
    invariants = required[2].read_text(encoding="utf-8")
    limits = required[3].read_text(encoding="utf-8")
    long_run = required[4].read_text(encoding="utf-8")
    audit = required[5].read_text(encoding="utf-8")
    require_tokens(source, (
        "command_compare", "validate_step_counter_promotion",
        "minisnn_worlds_kernel_internal_validate_invariants", "validate_planned_invariants",
        "MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION", "uint64_addition_fits",
        "MINISNN_WORLDS_KERNEL_C3_INVARIANT_TESTING",
    ), "K1-C3 implementation")
    require_tokens(ordering, (
        "test_create_move_ordering", "test_remove_move_and_repeated_root_ordering",
        "test_lifecycle_and_link_precedence", "test_occupancy_link_and_conflict_ordering",
        "test_link_conflicts_and_canonical_queries",
        "test_additional_lifecycle_placement_and_link_ordering",
        "test_additional_movement_churn_and_occupancy_ordering",
    ), "K1-C3 ordering coverage")
    require_tokens(invariants, (
        "test_validator_detects_each_corruption",
        "CORRUPTION_SPATIAL_LINK_CYCLE", "CORRUPTION_COUNTERS",
        "test_causal_events_are_grouped_and_queries_do_not_mutate",
    ), "K1-C3 invariant coverage")
    require_tokens(limits, (
        "test_identifier_limits", "test_counter_promotion_is_atomic",
        "test_allocation_failure_matrix", "test_link_and_queue_capacity",
    ), "K1-C3 limit coverage")
    require_tokens(long_run, ("run_workload", "LONG_RUN_TICKS", "K1-C3 long run deterministic OK"), "K1-C3 long-run coverage")
    require_tokens(audit, (
        "Status:", "target_tick", "priority", "issuer", "CommandId",
        "K1-C4", "V5", "atomic", "O0", "O2", "Complexidade observada",
    ), "K1-C3 audit documentation")
    if "minisnn_worlds_kernel_testing_" in run(["nm", args.library], root):
        raise RuntimeError("testing symbols leaked into normal library")
    outputs = []
    for executable in (args.ordering_test, args.invariants_test, args.limits_test, args.long_run_test):
        first = run([executable], root)
        second = run([executable], root)
        if first != second:
            raise RuntimeError(f"nondeterministic output: {pathlib.Path(executable).name}")
        outputs.append(first)
    require_tokens("".join(outputs), (
        "K1-C3 ordering", "K1-C3 planned and official invariant",
        "K1-C3 limits", "K1-C3 long run deterministic OK",
    ), "K1-C3 test outputs")
    print("K1-C3 ordering, hardening and V5 audit OK")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"K1-C3 ordering, hardening and V5 audit FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)