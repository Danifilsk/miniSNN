#!/usr/bin/env python3
"""Functional K1-B2 audit for deterministic atomic movement and displacement."""
from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys


def run(command: list[str], cwd: pathlib.Path, expect_success: bool = True) -> str:
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    if (result.returncode == 0) != expect_success:
        sys.stderr.write(result.stdout + result.stderr)
        raise RuntimeError("command returned an unexpected status")
    return result.stdout


def require_tokens(text: str, tokens: tuple[str, ...], label: str) -> None:
    for token in tokens:
        if token not in text:
            raise RuntimeError(f"{label} missing: {token}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--library", required=True)
    parser.add_argument("--demo", required=True)
    parser.add_argument("--movement-test", required=True)
    parser.add_argument("--stress-test", required=True)
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    include = root / "include"
    source = root / "src" / "minisnn_worlds_kernel.c"
    output = root.parent.parent / "build" / "worlds" / "kernel" / "audit" / "k1_b2"
    output.mkdir(parents=True, exist_ok=True)
    command_header = include / "minisnn_worlds_kernel_command.h"
    event_header = include / "minisnn_worlds_kernel_event.h"
    diagnostics_header = include / "minisnn_worlds_kernel_diagnostics.h"
    hash_header = include / "minisnn_worlds_kernel_hash.h"
    aggregate = include / "minisnn_worlds_kernel.h"
    test_source = root / "tests" / "test_k1_b2_movement.c"
    stress_source = root / "tests" / "test_k1_b2_stress.c"

    if not pathlib.Path(args.library).is_file():
        raise RuntimeError("kernel library missing")
    for path in (command_header, event_header, diagnostics_header, hash_header,
                 aggregate, source, test_source, stress_source):
        if not path.is_file():
            raise RuntimeError(f"required K1-B2 file missing: {path.name}")

    public = "".join(path.read_text(encoding="utf-8") for path in (
        aggregate, command_header, event_header, diagnostics_header, hash_header,
    ))
    code = source.read_text(encoding="utf-8")
    tests = test_source.read_text(encoding="utf-8")
    require_tokens(public, (
        "MINISNN_WORLDS_KERNEL_COMMAND_MOVE_ENTITY",
        "minisnn_worlds_kernel_queue_move_entity",
        "has_displacement",
        "displacement",
        "MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED",
        "has_previous_transform",
        "previous_transform",
        "DESTINATION_OVERFLOW",
        "total_movement_commands_processed",
        "total_entities_moved",
        "total_movement_overflows_rejected",
        "MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V4",
    ), "public movement API")
    require_tokens(code, (
        "scalar_add_checked",
        "find_occupancy_conflict",
        "COMMAND_MOVE_ENTITY",
        "EVENT_ENTITY_MOVED",
        "DESTINATION_OVERFLOW",
        "compute_state_hash_v4",
        "state_is_k1b1_hash_compatible",
        "total_movement_commands_processed",
    ), "kernel movement implementation")
    require_tokens(tests, (
        "test_basic_motion_event_and_noop",
        "test_occupancy_conflict_ordering_and_preservation",
        "test_planned_ordering_and_global_atomicity",
        "test_overflow_and_destination_aabb",
        "testing_fail_next_allocation",
        "STATE_HASH_VERSION_V3",
        "STATE_HASH_VERSION_V4",
    ), "movement public coverage")
    for forbidden in (
        "minisnn.h", "rand(", "srand(", "float ", "double ", "fish",
        "shark", "sprite", "pathfinding", "velocity", "physics",
    ):
        if forbidden in code.lower():
            raise RuntimeError(f"forbidden kernel token: {forbidden}")

    symbols = run(["nm", args.library], root)
    if "minisnn_worlds_kernel_testing_" in symbols:
        raise RuntimeError("testing symbols leaked into normal library")

    first = run([args.movement_test], root)
    second = run([args.movement_test], root)
    if first != second:
        raise RuntimeError("movement test output is not deterministic")
    require_tokens(first, (
        "K1-B2 atomic movement, occupancy, overflow, ordering and hash validation OK",
        "deterministic_hash=",
    ), "movement test output")

    stress = run([args.stress_test], root)
    require_tokens(stress, (
        "movement_successes=1000",
        "movement_conflicts_rejected=1000",
        "repeat_match=yes",
    ), "movement stress output")

    demo_output = run([
        args.demo, "configs/k1_movement_demo.ini", str(output),
    ], root)
    require_tokens(demo_output, (
        "free_move=yes", "nonblocking_overlap=yes", "blocking_rejected=yes",
        "canonical_related_entity=yes", "overflow_rejected=yes", "zero_move=yes",
        "move_after_clear=yes", "removal_then_move_rejected=yes",
        "repeat_match=yes", "different_seed_diverged=yes",
        "state_hash_version=4", "final_state_hash=0xA0AF3078F4B0E869", "status=OK",
    ), "movement demo output")

    config = (root / "configs" / "k1_movement_demo.ini").read_text(encoding="utf-8")
    malformed = output / "malformed.ini"
    malformed.write_text(config + "\nunknown_key=1\n", encoding="utf-8")
    run([args.demo, str(malformed), str(output / "malformed")], root,
        expect_success=False)

    for filename in ("trace.csv", "events.csv", "positions.csv", "manifest.ini", "report.txt"):
        if not (output / filename).is_file():
            raise RuntimeError(f"missing observable artifact: {filename}")
    events = (output / "events.csv").read_text(encoding="utf-8").splitlines()
    if not events or "previous_x" not in events[0] or "orientation" not in events[0]:
        raise RuntimeError("event artifact omits movement origin/destination")
    manifest = (output / "manifest.ini").read_text(encoding="utf-8")
    require_tokens(manifest, (
        "state_hash_version=4", "final_state_hash=0xA0AF3078F4B0E869",
        "overflow_rejected=yes", "blocking_rejected=yes",
    ), "movement manifest")
    print("K1-B2 functional checker OK")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"K1-B2 functional checker FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
