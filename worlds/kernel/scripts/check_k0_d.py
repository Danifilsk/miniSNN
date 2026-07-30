from __future__ import annotations

import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys


REQUIRED = (
    "app/k0_scenario_config.h",
    "app/k0_scenario_config.c",
    "app/k0_scenario_engine.h",
    "app/k0_scenario_engine.c",
    "app/k0_scenario_artifacts.h",
    "app/k0_scenario_artifacts.c",
    "app/k0_scenario_runner.c",
    "configs/k0_integrated_demo.ini",
    "configs/k0_stress.ini",
    "configs/k0_long_run.ini",
    "tests/test_k0_d_config.c",
    "tests/test_k0_d_artifacts.c",
    "tests/test_k0_d_artifact_corruption.py",
    "tests/test_k0_d_determinism.py",
    "tests/test_k0_d_corruption.py",
    "tests/test_k0_d_stress.py",
    "tests/test_k0_d_long_run.py",
    "tests/test_k0_d_optimization_determinism.py",
    "tests/test_k0_d_posix_smoke.py",
    "tests/test_k0_d_sanitize.py",
    "tests/k0_external_consumer.c",
    "docs/SCENARIO_CONFIG_CONTRACT.md",
    "docs/ARTIFACT_CONTRACT.md",
    "docs/K0_D_INTEGRATED_SCENARIO_STRESS_AUDIT.md",
    "docs/K0_COMPLETION_AUDIT.md",
)


def fail(message: str) -> None:
    print(f"K0-D Worlds Kernel validation FAILED: {message}")
    raise SystemExit(1)


def run(command: list[str], cwd: Path) -> str:
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        fail(f"command failed: {' '.join(command)}\n{result.stdout}{result.stderr}")
    return result.stdout


def main() -> int:
    parser = argparse.ArgumentParser(description="Audita cenario e artefatos K0-D.")
    parser.add_argument("--root", required=True)
    parser.add_argument("--library", required=True)
    parser.add_argument("--runner", required=True)
    parser.add_argument("--validator", required=True)
    args = parser.parse_args()

    root = Path(args.root).resolve()
    repository = root.parents[1]
    library = (root / args.library).resolve()
    runner = (root / args.runner).resolve()
    validator = (root / args.validator).resolve()
    for relative in REQUIRED:
        if not (root / relative).is_file():
            fail(f"required K0-D file missing: {relative}")
    if not library.is_file() or not runner.is_file() or not validator.is_file():
        fail("library, runner or artifact validator is missing")

    runner_source = (root / "app/k0_scenario_runner.c").read_text(encoding="utf-8")
    app_sources = "\n".join(path.read_text(encoding="utf-8") for path in (root / "app").glob("k0_scenario_*.c"))
    if '#include "minisnn_worlds_kernel.h"' not in app_sources:
        fail("runner tool does not use the aggregate public Kernel API")
    if re.search(r'#include\s*["<].*(?:src/|minisnn_worlds_kernel\.c)', app_sources):
        fail("runner tool includes private Kernel implementation")
    for required in ("--config", "--output", "--overwrite", "--validate-only", "status=OK"):
        if required not in runner_source:
            fail(f"runner CLI contract missing: {required}")

    config_source = (root / "app/k0_scenario_config.c").read_text(encoding="utf-8")
    for required in ("K0_SCENARIO_CONFIG_FILE_MAX_SIZE", "unknown or duplicate", "scenario_id_is_valid", "k0_scenario_config_signature"):
        if required not in config_source:
            fail(f"strict parser or signature contract missing: {required}")
    artifact_source = (root / "app/k0_scenario_artifacts.c").read_text(encoding="utf-8")
    for required in ("trace.csv", "events.csv", "manifest.ini", "report.txt", "validate_set", "read_manifest", "read_report", "report_matches_manifest", "publish_paths", "trace_file_signature", "S_ISDIR"):
        if required not in artifact_source:
            fail(f"artifact contract missing: {required}")
    if "S_IFDIR" in artifact_source:
        fail("artifact directory detection is not portable C11")

    nm = shutil.which("nm")
    if nm is None:
        fail("nm unavailable for symbol audit")
    symbols = run([nm, "-g", "--defined-only", str(library)], repository)
    if any("k0_scenario" in line or "fopen" in line or "mkdir" in line for line in symbols.splitlines()):
        fail("tool, parser or filesystem symbol leaked into library")

    output = repository / "build" / "worlds" / "kernel" / "audit" / "k0_d_checker"
    shutil.rmtree(output, ignore_errors=True)
    try:
        console = run([str(runner), "--config", "configs/k0_integrated_demo.ini", "--output", str(output)], root)
        if "status=OK" not in console or "same_seed_repeat_match=yes" not in console or "different_seed_diverged=yes" not in console:
            fail("integrated runner did not report deterministic success")
        run([str(validator), str(output)], root)
        if any(not (output / name).is_file() for name in ("trace.csv", "events.csv", "manifest.ini", "report.txt")):
            fail("integrated artifact set is incomplete")
    finally:
        shutil.rmtree(output, ignore_errors=True)
    print("K0-D Worlds Kernel integrated scenario and stress validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
