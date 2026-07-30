from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import sys


BASE_FLAGS = (
    "-std=c11",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Wformat=2",
    "-Wstrict-prototypes",
)


def fail(message: str) -> int:
    print(f"K0-C O0/O2 deterministic validation FAILED: {message}")
    return 1


def build_and_run(
    compiler: str,
    include: Path,
    source: Path,
    runner: Path,
    directory: Path,
    optimization: str,
) -> bytes | None:
    executable = directory / "k0_c_optimization_runner.exe"
    result = subprocess.run(
        [
            compiler,
            *BASE_FLAGS,
            optimization,
            f"-I{include}",
            str(runner),
            str(source),
            "-o",
            str(executable),
        ],
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        return None
    result = subprocess.run([str(executable)], capture_output=True, check=False)
    if result.returncode != 0:
        sys.stdout.buffer.write(result.stdout)
        sys.stderr.buffer.write(result.stderr)
        return None
    return result.stdout


def main() -> int:
    parser = argparse.ArgumentParser(description="Compara uma trajetoria K0-C em -O0 e -O2.")
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--include", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--runner", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    include = Path(args.include).resolve()
    source = Path(args.source).resolve()
    runner = Path(args.runner).resolve()
    output_dir = Path(args.output_dir).resolve()
    o0_dir = output_dir / "o0"
    o2_dir = output_dir / "o2"

    if "fast-math" in " ".join(BASE_FLAGS).lower():
        return fail("flags proibidas de fast-math")
    shutil.rmtree(output_dir, ignore_errors=True)
    o0_dir.mkdir(parents=True, exist_ok=True)
    o2_dir.mkdir(parents=True, exist_ok=True)
    try:
        o0_output = build_and_run(args.compiler, include, source, runner, o0_dir, "-O0")
        o2_output = build_and_run(args.compiler, include, source, runner, o2_dir, "-O2")
        if o0_output is None or o2_output is None:
            return fail("compilacao ou execucao da trajetoria")
        if o0_output != o2_output:
            return fail("saidas de -O0 e -O2 diferem")
    finally:
        shutil.rmtree(output_dir, ignore_errors=True)
    print("K0-C O0/O2 deterministic validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
