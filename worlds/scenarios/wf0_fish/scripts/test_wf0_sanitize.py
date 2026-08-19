#!/usr/bin/env python3
"""Run WF0 tests under ASan/UBSan with strict PASS/FAIL/UNAVAILABLE classification."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import tempfile

FLAGS = (
    "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2",
    "-Wstrict-prototypes", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
)


def run(command: list[str], cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, capture_output=True, text=True, check=False)


def failure(stage: str, result: subprocess.CompletedProcess[str] | None = None) -> int:
    detail = "" if result is None else result.stdout + result.stderr
    print(f"WF0 sanitizer: FAIL ({stage})\n" + detail)
    return 1


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
    with tempfile.TemporaryDirectory(prefix="wf0_sanitize_") as temporary:
        root = Path(temporary)
        probe = root / "probe.c"
        probe.write_text("int main(void) { return 0; }\n", encoding="ascii")
        probe_executable = root / "probe.exe"
        probe_build = run([args.compiler, *FLAGS, str(probe), "-o", str(probe_executable)])
        if probe_build.returncode != 0:
            print("WF0 sanitizer: UNAVAILABLE")
            return 0
        probe_run = run([str(probe_executable)], root)
        if probe_run.returncode != 0:
            print("WF0 sanitizer: UNAVAILABLE")
            return 0
        if not all(path.is_file() for path in (core_library, domain_library, kernel_library)):
            return failure("prerequisite libraries unavailable")
        executable = root / "test_wf0_fish.exe"
        build = run([
            args.compiler, *FLAGS,
            f"-I{scenario / 'include'}", f"-I{core / 'include'}",
            f"-I{kernel / 'include'}", f"-I{domain / 'include'}",
            f"-I{bridge / 'include'}",
            str(scenario / "tests" / "test_wf0_fish.c"),
            str(scenario / "src" / "wf0_fish.c"),
            str(scenario / "src" / "wf0_fish_observation.c"),
            str(bridge / "src" / "minisnn_worlds_brain_bridge.c"),
            str(domain_library), str(kernel_library), str(core_library),
            *math_library, "-o", str(executable),
        ])
        if build.returncode != 0:
            return failure("WF0 build", build)
        result = run([str(executable)], root)
        if result.returncode != 0:
            return failure("WF0 execution", result)
    print("WF0 sanitizer: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())