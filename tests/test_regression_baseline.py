from __future__ import annotations

from pathlib import Path
import configparser
import shutil
import subprocess
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from capture_baseline import GOLDEN_PATH, capture  # noqa: E402


RUNNER = PROJECT_ROOT / "build" / "minisnn_runner.exe"
TEMP_DIR = PROJECT_ROOT / "build" / "known_results_regression"


def summary_value(path: Path, key: str) -> str:
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith(key + "="):
            return line.split("=", 1)[1]
    raise AssertionError(f"missing summary key {key}")


def check_known_result(config_name: str, temporary_name: str, expected: int) -> None:
    source = PROJECT_ROOT / "configs" / config_name
    parser = configparser.ConfigParser()
    parser.read(source, encoding="utf-8")
    parser.set("run", "run_name", temporary_name)
    if not parser.has_section("output"):
        parser.add_section("output")
    parser.set("output", "auto_unique_run", "false")
    parser.set("output", "history_enabled", "false")
    config_path = TEMP_DIR / config_name
    with config_path.open("w", encoding="utf-8") as file:
        parser.write(file)

    run_dir = PROJECT_ROOT / "results" / "scenarios" / temporary_name
    shutil.rmtree(run_dir, ignore_errors=True)
    completed = subprocess.run(
        [str(RUNNER), str(config_path)],
        cwd=PROJECT_ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if completed.returncode != 0:
        raise AssertionError(f"known scenario failed:\n{completed.stdout}")
    actual = int(summary_value(run_dir / "summary.txt", "spikes_total"))
    shutil.rmtree(run_dir, ignore_errors=True)
    if actual != expected:
        raise AssertionError(f"{config_name}: expected {expected} spikes, got {actual}")


def main() -> int:
    import json

    expected = json.loads(GOLDEN_PATH.read_text(encoding="utf-8"))
    actual = capture(RUNNER)
    if actual != expected:
        raise AssertionError("small topology golden baseline changed")

    shutil.rmtree(TEMP_DIR, ignore_errors=True)
    TEMP_DIR.mkdir(parents=True)
    try:
        check_known_result("random.ini", "regression_known_random", 6757)
        check_known_result("small_world.ini", "regression_known_small_world", 15045)
    finally:
        shutil.rmtree(TEMP_DIR, ignore_errors=True)

    print("Regression baseline and known results OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
