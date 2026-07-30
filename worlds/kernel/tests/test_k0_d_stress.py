from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import sys


ARTIFACTS = ("trace.csv", "events.csv", "manifest.ini", "report.txt")


def fail(message: str) -> int:
    print(f"K0-D stress validation FAILED: {message}")
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(description="Executa stress generico K0-D.")
    parser.add_argument("--runner", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    runner = Path(args.runner).resolve()
    config = Path(args.config).resolve()
    output = Path(args.output_dir).resolve()
    shutil.rmtree(output, ignore_errors=True)
    try:
        result = subprocess.run([str(runner), "--config", str(config), "--output", str(output)], text=True, capture_output=True, check=False)
        run_directory = output
        if result.returncode != 0 or "status=OK" not in result.stdout or "ticks_completed=50000" not in result.stdout:
            return fail(f"stress runner failed: {result.stdout}{result.stderr}")
        if any(not (run_directory / name).is_file() for name in ARTIFACTS) or list(run_directory.glob("*.tmp")):
            return fail("stress artifacts are incomplete or left temporary files")
        before = {name: (run_directory / name).read_bytes() for name in ARTIFACTS}
        rejected = subprocess.run([str(runner), "--config", str(config), "--output", str(run_directory)], text=True, capture_output=True, check=False)
        if rejected.returncode == 0 or before != {name: (run_directory / name).read_bytes() for name in ARTIFACTS}:
            return fail("existing artifacts changed without --overwrite")
        repeated = subprocess.run([str(runner), "--config", str(config), "--output", str(run_directory), "--overwrite"], text=True, capture_output=True, check=False)
        if repeated.returncode != 0 or before != {name: (run_directory / name).read_bytes() for name in ARTIFACTS}:
            return fail("overwrite did not reproduce canonical stress artifacts")
    finally:
        shutil.rmtree(output, ignore_errors=True)
    print("K0-D stress validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
