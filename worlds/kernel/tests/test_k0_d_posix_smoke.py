from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import sys


FLAGS = ("-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2", "-Wstrict-prototypes")


def unavailable(message: str) -> int:
    print(f"K0-D POSIX C11 smoke UNAVAILABLE: {message}")
    return 0


def run(command: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, input="", text=True, capture_output=True, check=False)


def fail(result: subprocess.CompletedProcess[str], context: str) -> int:
    print(f"K0-D POSIX C11 smoke FAILED: {context}")
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(description="Compila o app K0-D em um alvo POSIX C11 quando disponivel.")
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--kernel-root", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    compiler = shutil.which(args.compiler)
    if compiler is None:
        return unavailable("C compiler not found")
    kernel = Path(args.kernel_root).resolve()
    config = Path(args.config).resolve()
    output = Path(args.output_dir).resolve()
    probe = run([compiler, "-dM", "-E", "-"], kernel)
    if probe.returncode != 0:
        return fail(probe, "cannot query compiler target macros")
    macros = probe.stdout
    if "__unix__" not in macros and "__unix" not in macros and "__APPLE__" not in macros:
        return unavailable("no POSIX C11 compiler target available")

    shutil.rmtree(output, ignore_errors=True)
    output.mkdir(parents=True)
    runner = output / "k0_scenario_runner"
    validator = output / "k0_d_artifact_validator"
    shared_sources = [
        "src/minisnn_worlds_kernel.c",
        "app/k0_scenario_config.c",
        "app/k0_scenario_engine.c",
        "app/k0_scenario_artifacts.c",
    ]
    common = [compiler, *FLAGS, "-Iinclude", "-Iapp"]
    result = run([*common, *shared_sources, "app/k0_scenario_runner.c", "-o", str(runner)], kernel)
    if result.returncode != 0:
        return fail(result, "runner compilation")
    result = run([*common, *shared_sources, "tests/k0_d_artifact_validator.c", "-o", str(validator)], kernel)
    if result.returncode != 0:
        return fail(result, "validator compilation")
    artifacts = output / "artifacts"
    result = run([str(runner), "--config", str(config), "--output", str(artifacts)], kernel)
    if result.returncode != 0:
        return fail(result, "runner execution")
    result = run([str(validator), str(artifacts)], kernel)
    if result.returncode != 0:
        return fail(result, "validator execution")
    shutil.rmtree(output, ignore_errors=True)
    print("K0-D POSIX C11 smoke PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
