#!/usr/bin/env python3
"""Functional K1-A Worlds Kernel audit."""
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
    parser.add_argument("--space-test", required=True)
    parser.add_argument("--stress-test", required=True)
    args = parser.parse_args()
    root = pathlib.Path(args.root).resolve()
    include = root / "include"
    source = root / "src" / "minisnn_worlds_kernel.c"
    space = include / "minisnn_worlds_kernel_space.h"
    aggregate = include / "minisnn_worlds_kernel.h"
    output = root.parent.parent / "build" / "worlds" / "kernel" / "audit" / "k1_a"
    output.mkdir(parents=True, exist_ok=True)

    library = pathlib.Path(args.library)
    test_source = root / "tests" / "test_k1_a_space.c"
    if not library.exists():
        raise RuntimeError("kernel library missing")
    for token in ("malloc(", "partial_size", "larger_size",
                  "testing_scalar_add", "testing_scalar_subtract"):
        if token not in test_source.read_text(encoding="utf-8"):
            raise RuntimeError(f"missing K1-A compatibility coverage: {token}")
    symbols = run(["nm", str(library)], root)
    if "minisnn_worlds_kernel_testing_scalar_" in symbols:
        raise RuntimeError("testing-only scalar symbols leaked into the normal library")
    for token in (
        "MINISNN_WORLDS_KERNEL_SCALAR_SCALE",
        "MINISNN_WORLDS_KERNEL_ORIENTATION_FULL_TURN",
        "MiniSNNWorldsKernelTransform",
    ):
        if token not in space.read_text(encoding="utf-8"):
            raise RuntimeError(f"missing public space token: {token}")
    public = aggregate.read_text(encoding="utf-8")
    if "minisnn_worlds_kernel_space_bounds" not in public:
        raise RuntimeError("space bounds API not aggregated")
    code = source.read_text(encoding="utf-8")
    for forbidden in ("minisnn.h", "rand(", "srand(", "float ", "double "):
        if forbidden in code:
            raise RuntimeError(f"forbidden kernel dependency or spatial state token: {forbidden}")
    first = run([args.space_test], root)
    second = run([args.space_test], root)
    if first != second:
        raise RuntimeError("K1-A public test output is not deterministic")
    run([args.stress_test], root)
    demo_output = run([
        args.demo,
        "configs/k1_transform_demo.ini",
        str(output),
    ], root)
    for token in (
        "canonical_entity=3",
        "position_x=-1000",
        "position_y=0",
        "orientation=90000",
        "repeat_match=yes",
        "different_config_diverged=yes",
        "status=OK",
    ):
        if token not in demo_output:
            raise RuntimeError(f"demo did not report {token}")
    for filename in ("transform_trace.csv", "manifest.ini", "report.txt"):
        if not (output / filename).is_file():
            raise RuntimeError(f"missing observable artifact: {filename}")
    trace = (output / "transform_trace.csv").read_text(encoding="utf-8").splitlines()
    if not trace or "event_id,event_type,event_command_id" not in trace[0]:
        raise RuntimeError("transform trace header omits spatial event fields")
    if not any(line.startswith("event,2,") and ",1000,2000,0,0" in line for line in trace[1:]):
        raise RuntimeError("transform trace omits an applied spatial event payload")
    variant = output / "k1_transform_demo_variant.ini"
    variant.write_text(
        (root / "configs" / "k1_transform_demo.ini").read_text(encoding="utf-8")
        .replace("max_x_milli=10000", "max_x_milli=9000"),
        encoding="utf-8",
    )
    variant_output = output / "variant"
    variant_output.mkdir(exist_ok=True)
    run([args.demo, str(variant), str(variant_output)], root)
    first_manifest = (output / "manifest.ini").read_text(encoding="utf-8")
    second_manifest = (variant_output / "manifest.ini").read_text(encoding="utf-8")
    if "final_state_hash=0xDAA66E4FFBC693C9" not in first_manifest:
        raise RuntimeError("K1-A v2 known hash vector changed")
    if first_manifest == second_manifest:
        raise RuntimeError("changing the INI did not change the effective execution")
    invalid = output / "k1_transform_demo_invalid.ini"
    invalid.write_text(
        (root / "configs" / "k1_transform_demo.ini").read_text(encoding="utf-8")
        + "unknown_key=1\n",
        encoding="utf-8",
    )
    rejected = subprocess.run([args.demo, str(invalid), str(output)], cwd=root)
    if rejected.returncode == 0:
        raise RuntimeError("strict K1-A parser accepted an unknown key")
    print("K1-A functional checker OK")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"K1-A functional checker FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
