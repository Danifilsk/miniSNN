from __future__ import annotations

import csv
import os
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "results" / "d1_b_robustness"
REQUIRED = (
    "config_source.ini", "config_used.ini", "d1_b_determinism.csv",
    "d1_b_optimization_determinism.csv", "d1_b_corruption.csv", "d1_b_stress.csv",
    "d1_b_long_runs.csv", "d1_b_performance.csv", "d1_b_sanitizers.txt",
    "d1_b_posix_headless.txt", "d1_b_symbols.txt", "d1_b_summary.txt",
    "d1_b_manifest.txt", "d1_b_report.html",
)
CONFIG_KEYS = (
    "neuron_count", "seed", "lif_steps", "adex_steps", "hodgkin_huxley_steps",
    "structural_steps", "c7_ticks", "brain_steps_per_tick",
)
FNV_OFFSET = 14695981039346656037
FNV_PRIME = 1099511628211


def fail(message: str) -> int:
    print(f"D1-B validation FAILED: {message}")
    return 1


def rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8", errors="replace") as file:
        return list(csv.DictReader(file))


def key_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        key, separator, value = line.partition("=")
        if separator:
            values[key.strip()] = value.strip()
    return values


def config_signature(values: dict[str, str]) -> int | None:
    try:
        numbers = [int(values[key]) for key in CONFIG_KEYS]
    except (KeyError, ValueError):
        return None
    signature = FNV_OFFSET
    for number in numbers:
        if number < 0 or number >= 1 << 64:
            return None
        for byte_index in range(8):
            signature ^= (number >> (byte_index * 8)) & 0xff
            signature = (signature * FNV_PRIME) & ((1 << 64) - 1)
    return signature


def status_from_text(path: Path) -> str | None:
    return key_values(path).get("status")


def validate_executed_artifact(name: str) -> str | None:
    values = rows(OUTPUT / name)
    required_columns = {"audit_format_version", "test_binary", "test_status", "evidence", "status"}
    if not values or not required_columns <= set(values[0]):
        return f"artifact is not execution-derived: {name}"
    if any(row.get("audit_format_version") != "d1_b_v2" or
           row.get("test_status") != "PASS" or row.get("status") != "PASS" or
           not row.get("evidence") for row in values):
        return f"artifact without complete PASS execution: {name}"
    return None


def update_manifest(values: dict[str, str], optimization_status: str,
                    posix_status: str, sanitizer_status: str) -> None:
    values["optimization_matrix_status"] = optimization_status
    values["posix_headless_status"] = posix_status
    values["sanitizer_status"] = sanitizer_status
    values["artifact_completion"] = "PASS"
    destination = OUTPUT / "d1_b_manifest.txt"
    temporary = destination.with_suffix(".txt.tmp")
    ordered = (
        "audit", "audit_format_version", "git_commit", "git_status", "config_signature",
        "config_source", "config_used", "long_runs", "source_api", "studio_dependency",
        "checkpoint_resume", "optimization_matrix_status", "posix_headless_status",
        "sanitizer_status", "artifact_completion",
    )
    text = "".join(f"{key}={values.get(key, 'NA')}\n" for key in ordered)
    temporary.write_text(text, encoding="ascii", errors="backslashreplace")
    os.replace(temporary, destination)


def main() -> int:
    missing = [name for name in REQUIRED if not (OUTPUT / name).is_file()]
    if missing:
        return fail(f"artifacts absent: {', '.join(missing)}")
    for name in ("d1_b_determinism.csv", "d1_b_corruption.csv", "d1_b_stress.csv"):
        problem = validate_executed_artifact(name)
        if problem is not None:
            return fail(problem)

    optimization = rows(OUTPUT / "d1_b_optimization_determinism.csv")
    if (len(optimization) != 9 or any(row.get("audit_format_version") != "d1_b_v2" or
                                      row.get("status") != "PASS" for row in optimization)):
        return fail("optimization matrix incomplete")
    for row in optimization:
        if row.get("case", "").startswith("c7_"):
            try:
                if int(row["spikes"]) <= 0 or int(row["action_variation_count"]) <= 0:
                    return fail(f"inactive C7 optimization case: {row.get('case')}")
            except (KeyError, ValueError):
                return fail("optimization matrix lacks C7 activity evidence")

    long_runs = rows(OUTPUT / "d1_b_long_runs.csv")
    if len(long_runs) < 7:
        return fail("long run matrix incomplete")
    for run in long_runs:
        try:
            complete = int(run["completed_steps"]) == int(run["declared_steps"])
        except (KeyError, ValueError):
            complete = False
        if (run.get("audit_format_version") != "d1_b_v2" or
                run.get("status") != "PASS" or not complete):
            return fail(f"long run incomplete: {run}")

    performance = rows(OUTPUT / "d1_b_performance.csv")
    metrics = {row.get("metric") for row in performance}
    required_metrics = {"neural_steps", "structural_steps", "c7_ticks", "checkpoint_save", "checkpoint_load"}
    if not required_metrics <= metrics or any(row.get("audit_format_version") != "d1_b_v2" for row in performance):
        return fail("benchmark metrics incomplete")
    try:
        if any(float(row["throughput"]) <= 0.0 for row in performance):
            return fail("benchmark throughput invalid")
    except (KeyError, ValueError):
        return fail("benchmark contains invalid numeric data")

    sanitizer_text = (OUTPUT / "d1_b_sanitizers.txt").read_text(encoding="utf-8", errors="replace")
    sanitizer_status = status_from_text(OUTPUT / "d1_b_sanitizers.txt")
    if ("audit_format_version=d1_b_v2" not in sanitizer_text or
            sanitizer_status not in {"PASS", "UNAVAILABLE"}):
        return fail("sanitizer status is FAIL or incomplete")
    if sanitizer_status == "PASS" and "compile_status=PASS;run_status=PASS;sanitizer_status=PASS" not in sanitizer_text:
        return fail("sanitizer PASS lacks per-test evidence")

    posix_status = status_from_text(OUTPUT / "d1_b_posix_headless.txt")
    posix_text = (OUTPUT / "d1_b_posix_headless.txt").read_text(encoding="utf-8", errors="replace")
    if ("audit_format_version=d1_b_v2" not in posix_text or
            posix_status not in {"PASS", "UNAVAILABLE"}):
        return fail("POSIX headless smoke failed or is incomplete")
    symbols = (OUTPUT / "d1_b_symbols.txt").read_text(encoding="utf-8", errors="replace")
    if "audit_format_version=d1_b_v2" not in symbols or "status=PASS" not in symbols:
        return fail("symbol audit incomplete")

    source_values = key_values(OUTPUT / "config_source.ini")
    used_values = key_values(OUTPUT / "config_used.ini")
    if any(source_values.get(key) != used_values.get(key) for key in CONFIG_KEYS):
        return fail("config_source and config_used do not describe the same audit")
    signature = config_signature(used_values)
    manifest = key_values(OUTPUT / "d1_b_manifest.txt")
    if (signature is None or manifest.get("audit_format_version") != "d1_b_v2" or
            manifest.get("config_signature") != str(signature) or
            manifest.get("studio_dependency") != "none" or
            "git_commit" not in manifest or "git_status" not in manifest):
        return fail("manifest provenance is stale or incomplete")
    update_manifest(manifest, "PASS", posix_status, sanitizer_status)
    print("D1-B robustness, determinism, stress and performance validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
