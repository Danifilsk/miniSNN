#!/usr/bin/env python3
"""Focused K1-C1 audit for directed spatial links and state hash V5."""
from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys


def run(command: list[str], cwd: pathlib.Path) -> str:
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    if result.returncode != 0:
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
    parser.add_argument("--spatial-test", required=True)
    args = parser.parse_args()
    root = pathlib.Path(args.root).resolve()
    include = root / "include"
    source = root / "src" / "minisnn_worlds_kernel.c"
    test = root / "tests" / "test_k1_c1_spatial_links.c"
    required = (
        include / "minisnn_worlds_kernel.h",
        include / "minisnn_worlds_kernel_spatial_link.h",
        include / "minisnn_worlds_kernel_command.h",
        include / "minisnn_worlds_kernel_event.h",
        include / "minisnn_worlds_kernel_diagnostics.h",
        include / "minisnn_worlds_kernel_entity.h",
        include / "minisnn_worlds_kernel_hash.h",
        source, test, pathlib.Path(args.library),
    )
    documents = (
        root / "docs" / "SPATIAL_LINKS_CONTRACT_PROPOSAL.md",
        root / "docs" / "K1_C1_SPATIAL_LINKS_AUDIT.md",
        root / "docs" / "STATE_HASH_CONTRACT.md",
    )
    for path in required:
        if not path.is_file():
            raise RuntimeError(f"required K1-C1 file missing: {path.name}")
    for path in documents:
        if not path.is_file():
            raise RuntimeError(f"required K1-C1 document missing: {path.name}")
    proposal = documents[0].read_text(encoding="utf-8")
    audit = documents[1].read_text(encoding="utf-8")
    state_hash = documents[2].read_text(encoding="utf-8")
    require_tokens(proposal, (
        "target_entity = parent", "affected_entity", "V1-V5", "K1-C2",
    ), "K1-C1 contract documentation")
    require_tokens(audit, (
        "Status:", "atomic", "K1-C2", "0xF6C92E0389E076CD",
    ), "K1-C1 audit documentation")
    require_tokens(state_hash, (
        "State hash V5", "affected_entity", "0xF6C92E0389E076CD",
    ), "V5 hash documentation")
    public = "".join(path.read_text(encoding="utf-8") for path in required[0:7])
    code = source.read_text(encoding="utf-8")
    coverage = test.read_text(encoding="utf-8")
    require_tokens(public, (
        "MiniSNNWorldsKernelSpatialLink",
        "MINISNN_WORLDS_KERNEL_COMMAND_CREATE_SPATIAL_LINK",
        "MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_SPATIAL_LINK",
        "has_spatial_link_endpoints",
        "MINISNN_WORLDS_KERNEL_EVENT_SPATIAL_LINK_CREATED",
        "has_spatial_link",
        "affected_entity",
        "SPATIAL_LINK_CYCLE",
        "TARGET_HAS_SPATIAL_LINKS",
        "MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V5",
        "minisnn_worlds_kernel_spatial_link_at",
    ), "public K1-C1 API")
    require_tokens(code, (
        "spatial_link_would_create_cycle",
        "insert_spatial_link",
        "remove_spatial_link",
        "COMMAND_CREATE_SPATIAL_LINK",
        "COMMAND_REMOVE_SPATIAL_LINK",
        "TARGET_HAS_SPATIAL_PARENT",
        "compute_state_hash_v5",
        "state_is_k1c_hash_compatible",
    ), "K1-C1 implementation")
    require_tokens(coverage, (
        "test_public_contract_and_simple_link",
        "test_submission_and_forest_rejections",
        "test_lifecycle_move_and_remove",
        "test_v5_and_atomicity",
        "test_canonical_order_offsets_and_scale",
        "test_offset_overflow_rejections",
        "testing_fail_allocation_after",
    ), "K1-C1 coverage")
    # C2 adds subtree translation; C1 remains the structural-link foundation.
    for forbidden in ("reparent", "link_id", "domain", "pathfinding", "physics"):
        if forbidden in code.lower():
            raise RuntimeError(f"K1-C1 implementation contains future-scope token: {forbidden}")
    symbols = run(["nm", args.library], root)
    if "minisnn_worlds_kernel_testing_" in symbols:
        raise RuntimeError("testing symbols leaked into normal library")
    first = run([args.spatial_test], root)
    second = run([args.spatial_test], root)
    if first != second:
        raise RuntimeError("K1-C1 test output is not deterministic")
    require_tokens(first, ("K1-C1 spatial link, lifecycle, atomicity and V5 validation OK",),
                   "K1-C1 test output")
    print("K1-C1 spatial links and V5 audit OK")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"K1-C1 spatial links and V5 audit FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)