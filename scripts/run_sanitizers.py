from __future__ import annotations

from pathlib import Path
import os
import subprocess
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = PROJECT_ROOT / "build"
COMMON_FLAGS = [
    "-std=c11",
    "-Wall",
    "-Wextra",
    "-pedantic",
    "-fsanitize=address,undefined",
    "-fno-omit-frame-pointer",
]
INCLUDES = ["-Iinclude", "-Isrc", "-Iapp"]

TESTS = [
    ("LIF", ["tests/test_LIF.c", "src/neuron.c"]),
    (
        "public API",
        ["tests/test_minisnn_api.c", "src/minisnn.c", "src/neuron.c", "src/network.c"],
    ),
    (
        "core topology",
        [
            "tests/test_topology.c",
            "src/neuron.c",
            "src/network.c",
            "src/topology.c",
            "src/stimulus.c",
            "src/recorder.c",
        ],
    ),
    (
        "scenario config",
        ["tests/test_scenario_config.c", "app/scenario_config.c"],
    ),
    (
        "scenario runner",
        [
            "tests/test_scenario_runner.c",
            "app/scenario_config.c",
            "app/scenario_runner.c",
            "src/minisnn.c",
            "src/neuron.c",
            "src/network.c",
        ],
    ),
]


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=PROJECT_ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def compile_test(name: str, sources: list[str], output: Path) -> subprocess.CompletedProcess[str]:
    return run(["gcc", *COMMON_FLAGS, *sources, *INCLUDES, "-o", str(output)])


def main() -> int:
    BUILD_DIR.mkdir(exist_ok=True)
    probe = BUILD_DIR / "sanitize_probe.exe"
    probe_result = compile_test("probe", TESTS[0][1], probe)

    if probe_result.returncode != 0:
        output = probe_result.stdout.strip()
        unsupported_markers = (
            "cannot find -lasan",
            "cannot find -lubsan",
            "unrecognized command-line option",
        )
        if any(marker in output for marker in unsupported_markers):
            print("Sanitizers NOT SUPPORTED by the installed MinGW toolchain.")
            print("The compiler probe failed before execution:")
            print(output)
            print("Static -fanalyzer validation remains available via test-analyzer.")
            return 0

        print("Sanitizer probe FAILED unexpectedly:")
        print(output)
        return 1

    probe_run = run([str(probe)])
    if probe_run.returncode != 0:
        print("Sanitizer runtime probe FAILED:")
        print(probe_run.stdout.strip())
        return 1

    for index, (name, sources) in enumerate(TESTS):
        output = BUILD_DIR / f"sanitize_{index}.exe"
        compiled = compile_test(name, sources, output)
        if compiled.returncode != 0:
            print(f"Sanitizer compilation FAILED for {name}:")
            print(compiled.stdout.strip())
            return 1

        executed = run([str(output)])
        print(f"[{name}] {executed.stdout.strip()}")
        if executed.returncode != 0:
            print(f"Sanitizer execution FAILED for {name}.")
            return 1

    print("AddressSanitizer and UndefinedBehaviorSanitizer validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
