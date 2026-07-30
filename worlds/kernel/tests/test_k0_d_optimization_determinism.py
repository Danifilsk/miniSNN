from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import sys


FLAGS = ("-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2", "-Wstrict-prototypes")
ARTIFACTS = ("trace.csv", "events.csv", "manifest.ini", "report.txt")


def fail(message: str) -> int:
    print(f"K0-D O0/O2 deterministic validation FAILED: {message}")
    return 1


def build(compiler: str, kernel: Path, directory: Path, optimization: str) -> Path | None:
    object_file = directory / "minisnn_worlds_kernel.o"
    library = directory / "libminisnn_worlds_kernel.a"
    runner = directory / "k0_scenario_runner.exe"
    directory.mkdir(parents=True, exist_ok=True)
    commands = (
        [compiler, *FLAGS, optimization, "-Iinclude", "-c", "src/minisnn_worlds_kernel.c", "-o", str(object_file)],
        ["ar", "rcs", str(library), str(object_file)],
        [compiler, *FLAGS, optimization, "-Iinclude", "-Iapp", "app/k0_scenario_runner.c", "app/k0_scenario_config.c", "app/k0_scenario_engine.c", "app/k0_scenario_artifacts.c", str(library), "-o", str(runner)],
    )
    for command in commands:
        result = subprocess.run(command, cwd=kernel, text=True, capture_output=True, check=False)
        if result.returncode != 0:
            print(result.stdout, end="")
            print(result.stderr, end="", file=sys.stderr)
            return None
    return runner


def main() -> int:
    parser = argparse.ArgumentParser(description="Compara o runner K0-D em -O0 e -O2.")
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--kernel-root", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    kernel = Path(args.kernel_root).resolve()
    config = Path(args.config).resolve()
    output = Path(args.output_dir).resolve()
    if "fast-math" in " ".join(FLAGS):
        return fail("fast-math is prohibited")
    shutil.rmtree(output, ignore_errors=True)
    try:
        o0 = build(args.compiler, kernel, output / "k0_d_o0", "-O0")
        o2 = build(args.compiler, kernel, output / "k0_d_o2", "-O2")
        if o0 is None or o2 is None:
            return fail("build failed")
        completed: list[subprocess.CompletedProcess[str]] = []
        for runner, destination in ((o0, output / "out_o0"), (o2, output / "out_o2")):
            result = subprocess.run([str(runner), "--config", str(config), "--output", str(destination)], cwd=kernel, text=True, capture_output=True, check=False)
            if result.returncode != 0:
                return fail(f"runner failed: {result.stdout}{result.stderr}")
            completed.append(result)
        if completed[0].stdout != completed[1].stdout:
            return fail("runner summary differs")
        if any((output / "out_o0" / item).read_bytes() != (output / "out_o2" / item).read_bytes() for item in ARTIFACTS):
            return fail("canonical artifacts differ")
    finally:
        shutil.rmtree(output, ignore_errors=True)
    print("K0-D O0/O2 deterministic validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
