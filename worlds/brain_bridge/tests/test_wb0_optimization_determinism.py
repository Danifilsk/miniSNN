#!/usr/bin/env python3
"""Compile the WB0 demo around the same Core library in O0 and O2 and compare artifacts."""
from __future__ import annotations

import argparse
import filecmp
from pathlib import Path
import subprocess
import tempfile

ARTIFACTS = (
    "summary.txt",
    "sensor_frames.csv",
    "neural_outputs.csv",
    "decisions.csv",
    "world_hashes.csv",
)
FLAGS = ("-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2", "-Wstrict-prototypes")


def run(command: list[str]) -> None:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        raise SystemExit("WB0 O0/O2 FAILED:\n" + result.stdout + result.stderr)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--bridge-root", required=True)
    args = parser.parse_args()
    bridge = Path(args.bridge_root).resolve()
    repository = bridge.parent.parent
    core = repository / "core"
    domain = repository / "worlds" / "domain"
    kernel = repository / "worlds" / "kernel"
    core_library = repository / "build" / "core" / "lib" / "libminisnn_core.a"

    if not core_library.is_file():
        raise SystemExit("WB0 O0/O2 FAILED: Core library unavailable")

    with tempfile.TemporaryDirectory(prefix="wb0_optimization_") as directory:
        root = Path(directory)
        outputs: list[Path] = []
        for optimization in ("-O0", "-O2"):
            executable = root / f"wb0{optimization[1:]}.exe"
            output = root / f"out{optimization[1:]}"
            output.mkdir()
            run([
                args.compiler, *FLAGS, optimization,
                f"-I{bridge / 'include'}", f"-I{core / 'include'}",
                f"-I{domain / 'include'}", f"-I{kernel / 'include'}",
                str(bridge / "app" / "wb0_brain_bridge_demo.c"),
                str(bridge / "src" / "minisnn_worlds_brain_bridge.c"),
                str(domain / "src" / "minisnn_worlds_domain.c"),
                str(kernel / "src" / "minisnn_worlds_kernel.c"),
                str(kernel / "src" / "minisnn_worlds_kernel_snapshot.c"),
                str(kernel / "src" / "minisnn_worlds_kernel_command_log.c"),
                str(core_library), "-o", str(executable),
            ])
            run([str(executable), str(output)])
            outputs.append(output)
        for artifact in ARTIFACTS:
            if not filecmp.cmp(outputs[0] / artifact, outputs[1] / artifact, shallow=False):
                raise SystemExit("WB0 O0/O2 FAILED: " + artifact)
    print("WB0 O0/O2 deterministic validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())