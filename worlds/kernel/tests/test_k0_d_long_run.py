from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess


def fail(message: str) -> int:
    print(f"K0-D long run validation FAILED: {message}")
    return 1


def final_hash(manifest: Path) -> str:
    for line in manifest.read_text(encoding="ascii").splitlines():
        if line.startswith("final_state_hash="):
            return line.split("=", 1)[1]
    return ""


def main() -> int:
    parser = argparse.ArgumentParser(description="Executa long run deterministico K0-D.")
    parser.add_argument("--runner", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    runner = Path(args.runner).resolve()
    config = Path(args.config).resolve()
    base = Path(args.output_dir).resolve()
    first = base / "one"
    second = base / "two"
    shutil.rmtree(base, ignore_errors=True)
    try:
        outputs: list[str] = []
        for destination in (first, second):
            result = subprocess.run([str(runner), "--config", str(config), "--output", str(destination)], text=True, capture_output=True, check=False)
            if result.returncode != 0 or "ticks_completed=1000000" not in result.stdout:
                return fail(f"long runner failed: {result.stdout}{result.stderr}")
            outputs.append(result.stdout)
        if final_hash(first / "manifest.ini") == "" or final_hash(first / "manifest.ini") != final_hash(second / "manifest.ini"):
            return fail("long run final hash is absent or non-deterministic")
        if (first / "trace.csv").read_bytes() != (second / "trace.csv").read_bytes() or \
                (first / "events.csv").read_bytes() != (second / "events.csv").read_bytes():
            return fail("long run artifacts diverged")
    finally:
        shutil.rmtree(base, ignore_errors=True)
    print("K0-D long run validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
