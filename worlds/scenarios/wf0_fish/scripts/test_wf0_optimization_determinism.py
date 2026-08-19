#!/usr/bin/env python3
"""Compile the WF0 Fish demo at O0/O2 and compare scientific artifacts."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ARTIFACTS = ("wf0_summary.txt", "wf0_fish_trace.csv")
FLAGS = ("-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2", "-Wstrict-prototypes")


def run(command: list[str]) -> None:
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise SystemExit("WF0 O0/O2 FAILED:\n" + result.stdout + result.stderr)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--scenario-root", required=True)
    args = parser.parse_args()
    scenario = Path(args.scenario_root).resolve()
    repository = scenario.parents[2]
    core = repository / "core"
    kernel = repository / "worlds" / "kernel"
    domain = repository / "worlds" / "domain"
    bridge = repository / "worlds" / "brain_bridge"
    core_library = repository / "build" / "core" / "lib" / "libminisnn_core.a"
    domain_library = repository / "build" / "worlds" / "domain" / "lib" / "libminisnn_worlds_domain.a"
    kernel_library = repository / "build" / "worlds" / "kernel" / "lib" / "libminisnn_worlds_kernel.a"
    math_library = ("-lm",) if os.name != "nt" else ()
    if not all(path.is_file() for path in (core_library, domain_library, kernel_library)):
        raise SystemExit("WF0 O0/O2 FAILED: prerequisite libraries unavailable")
    with tempfile.TemporaryDirectory(prefix="wf0_optimization_") as temporary:
        root = Path(temporary)
        outputs: list[Path] = []
        for optimization_level in ("-O0", "-O2"):
            executable = root / f"wf0{optimization_level[1:]}.exe"
            output = root / f"out{optimization_level[1:]}"
            output.mkdir()
            run([
                args.compiler, *FLAGS, optimization_level,
                f"-I{scenario / 'include'}", f"-I{core / 'include'}",
                f"-I{kernel / 'include'}", f"-I{domain / 'include'}",
                f"-I{bridge / 'include'}",
                str(scenario / "app" / "wf0_fish_demo.c"),
                str(scenario / "src" / "wf0_fish.c"),
                str(scenario / "src" / "wf0_fish_observation.c"),
                str(bridge / "src" / "minisnn_worlds_brain_bridge.c"),
                str(domain_library), str(kernel_library), str(core_library),
                *math_library, "-o", str(executable),
            ])
            run([str(executable), str(output)])
            outputs.append(output)
        for artifact in ARTIFACTS:
            if (outputs[0] / artifact).read_bytes() != (outputs[1] / artifact).read_bytes():
                raise SystemExit(f"WF0 O0/O2 FAILED: {artifact}")
    print("WF0 Fish O0/O2 deterministic validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())