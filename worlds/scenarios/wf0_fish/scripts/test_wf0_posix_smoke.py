#!/usr/bin/env python3
"""Compile and run WF0 as a headless POSIX smoke test when available."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

FLAGS = ("-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2", "-Wstrict-prototypes")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scenario-root", required=True)
    args = parser.parse_args()
    if os.name == "nt":
        print("WF0 POSIX smoke: UNAVAILABLE")
        return 0
    compiler = shutil.which("cc") or shutil.which("gcc")
    scenario = Path(args.scenario_root).resolve()
    repository = scenario.parents[2]
    core = repository / "core"
    kernel = repository / "worlds" / "kernel"
    domain = repository / "worlds" / "domain"
    bridge = repository / "worlds" / "brain_bridge"
    libraries = (
        repository / "build" / "core" / "lib" / "libminisnn_core.a",
        repository / "build" / "worlds" / "domain" / "lib" / "libminisnn_worlds_domain.a",
        repository / "build" / "worlds" / "kernel" / "lib" / "libminisnn_worlds_kernel.a",
    )
    if compiler is None or not all(path.is_file() for path in libraries):
        print("WF0 POSIX smoke: UNAVAILABLE")
        return 0
    with tempfile.TemporaryDirectory(prefix="wf0_posix_") as temporary:
        executable = Path(temporary) / "test_wf0_fish"
        command = [
            compiler, *FLAGS,
            f"-I{scenario / 'include'}", f"-I{core / 'include'}",
            f"-I{kernel / 'include'}", f"-I{domain / 'include'}",
            f"-I{bridge / 'include'}",
            str(scenario / "tests" / "test_wf0_fish.c"),
            str(scenario / "src" / "wf0_fish.c"),
            str(scenario / "src" / "wf0_fish_observation.c"),
            str(bridge / "src" / "minisnn_worlds_brain_bridge.c"),
            *(str(path) for path in libraries), "-lm", "-o", str(executable),
        ]
        build = subprocess.run(command, capture_output=True, text=True, check=False)
        if build.returncode != 0:
            print("WF0 POSIX smoke: FAIL\n" + build.stdout + build.stderr)
            return 1
        result = subprocess.run([str(executable)], capture_output=True, text=True, check=False)
        if result.returncode != 0:
            print("WF0 POSIX smoke: FAIL\n" + result.stdout + result.stderr)
            return 1
    print("WF0 POSIX smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())