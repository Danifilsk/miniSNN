from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys


FLAGS = ("-fsanitize=address,undefined", "-fno-omit-frame-pointer")
UNAVAILABLE = ("unrecognized command-line option", "unsupported option", "cannot find -lasan", "cannot find -lubsan", "libasan", "libubsan")


def unavailable(result: subprocess.CompletedProcess[str]) -> bool:
    text = f"{result.stdout}\n{result.stderr}".lower()
    return any(marker in text for marker in UNAVAILABLE)


def main() -> int:
    parser = argparse.ArgumentParser(description="Executa sanitizers K0-D.")
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--kernel-root", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    kernel = Path(args.kernel_root).resolve()
    config = Path(args.config).resolve()
    output = Path(args.output_dir).resolve()
    output.mkdir(parents=True, exist_ok=True)
    probe = output / "probe.c"
    probe_exe = output / "probe.exe"
    probe.write_text("int main(void) { return 0; }\n", encoding="ascii")
    result = subprocess.run([args.compiler, "-std=c11", *FLAGS, str(probe), "-o", str(probe_exe)], text=True, capture_output=True, check=False)
    if result.returncode != 0:
        if unavailable(result):
            print("K0-D sanitizer validation UNAVAILABLE")
            return 0
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        return 1
    runner = output / "k0_scenario_runner_sanitize.exe"
    validator = output / "k0_d_artifact_validator_sanitize.exe"
    artifact_test = output / "test_k0_d_artifacts_sanitize.exe"
    command = [args.compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2", "-Wstrict-prototypes", *FLAGS, "-Iinclude", "-Iapp", "app/k0_scenario_runner.c", "app/k0_scenario_config.c", "app/k0_scenario_engine.c", "app/k0_scenario_artifacts.c", "src/minisnn_worlds_kernel.c", "-o", str(runner)]
    result = subprocess.run(command, cwd=kernel, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        return 1
    shared_sources = ["app/k0_scenario_config.c", "app/k0_scenario_engine.c", "app/k0_scenario_artifacts.c", "src/minisnn_worlds_kernel.c"]
    for source, executable in (("tests/k0_d_artifact_validator.c", validator), ("tests/test_k0_d_artifacts.c", artifact_test)):
        result = subprocess.run([args.compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2", "-Wstrict-prototypes", *FLAGS, "-Iinclude", "-Iapp", source, *shared_sources, "-o", str(executable)], cwd=kernel, text=True, capture_output=True, check=False)
        if result.returncode != 0:
            print(result.stdout, end="")
            print(result.stderr, end="", file=sys.stderr)
            return 1
    environment = os.environ.copy()
    environment.setdefault("ASAN_OPTIONS", "detect_leaks=1")
    integrated = output / "integrated"
    result = subprocess.run([str(runner), "--config", str(config), "--output", str(integrated)], cwd=kernel, text=True, capture_output=True, check=False, env=environment)
    if result.returncode != 0:
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        return 1
    result = subprocess.run([str(validator), str(integrated)], cwd=kernel, text=True, capture_output=True, check=False, env=environment)
    if result.returncode != 0:
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        return 1
    result = subprocess.run([str(runner), "--config", str(config), "--output", str(integrated), "--overwrite"], cwd=kernel, text=True, capture_output=True, check=False, env=environment)
    if result.returncode != 0:
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        return 1
    result = subprocess.run([str(artifact_test)], cwd=kernel, text=True, capture_output=True, check=False, env=environment)
    if result.returncode != 0:
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        return 1
    malformed = output / "bad.ini"
    malformed.write_text("[kernel]\n", encoding="ascii")
    result = subprocess.run([str(runner), "--config", str(malformed), "--validate-only"], cwd=kernel, text=True, capture_output=True, check=False, env=environment)
    if result.returncode == 0:
        return 1
    shutil.rmtree(output / "integrated", ignore_errors=True)
    print("K0-D sanitizer validation PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
