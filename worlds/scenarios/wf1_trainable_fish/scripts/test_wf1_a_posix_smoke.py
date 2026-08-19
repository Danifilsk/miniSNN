#!/usr/bin/env python3
"""Compile and run the WF1-A runner natively on POSIX when available."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

FLAGS = (
    "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2",
    "-Wstrict-prototypes",
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scenario-root", required=True)
    args = parser.parse_args()

    if os.name == "nt":
        print("WF1-A POSIX smoke UNAVAILABLE")
        return 0

    compiler = shutil.which("cc") or shutil.which("gcc")
    if compiler is None:
        print("WF1-A POSIX smoke UNAVAILABLE")
        return 0

    scenario = Path(args.scenario_root).resolve()
    repository = scenario.parents[2]
    core = repository / "core"
    kernel = repository / "worlds" / "kernel"
    domain = repository / "worlds" / "domain"
    bridge = repository / "worlds" / "brain_bridge"
    core_sources = sorted((core / "src").glob("*.c"))
    kernel_sources = (
        kernel / "src" / "minisnn_worlds_kernel.c",
        kernel / "src" / "minisnn_worlds_kernel_snapshot.c",
        kernel / "src" / "minisnn_worlds_kernel_command_log.c",
    )

    with tempfile.TemporaryDirectory(prefix="wf1_a_posix_") as temporary:
        root = Path(temporary)
        executable = root / "wf1_trainable_fish_experiment"
        command = [
            compiler, *FLAGS,
            f"-I{scenario / 'include'}", f"-I{core / 'include'}",
            f"-I{kernel / 'include'}", f"-I{domain / 'include'}",
            f"-I{bridge / 'include'}",
            str(scenario / "app" / "wf1_trainable_fish_experiment.c"),
            str(scenario / "src" / "wf1_trainable_fish.c"),
            str(bridge / "src" / "minisnn_worlds_brain_bridge.c"),
            str(bridge / "src" / "minisnn_worlds_trainable_brain.c"),
            str(domain / "src" / "minisnn_worlds_domain.c"),
            *(str(path) for path in kernel_sources),
            *(str(path) for path in core_sources),
            "-lm", "-o", str(executable),
        ]
        build = subprocess.run(command, cwd=root, capture_output=True, text=True,
                               check=False)
        if build.returncode != 0:
            print("WF1-A POSIX smoke FAIL\n" + build.stdout + build.stderr)
            return 1
        result = subprocess.run([str(executable), "results"], cwd=root,
                                capture_output=True, text=True, check=False)
        if result.returncode != 0:
            print("WF1-A POSIX smoke FAIL\n" + result.stdout + result.stderr)
            return 1

    print("WF1-A POSIX smoke PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())