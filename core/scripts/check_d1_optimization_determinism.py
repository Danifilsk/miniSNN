from __future__ import annotations

import csv
import os
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT.parent / "build" / "audit" / "d1_b_optimization"
RESULTS = ROOT / "results" / "d1_b_robustness"
CASES = ("lif", "adex", "hh", "stdp", "rstdp", "structural", "c7_lif", "c7_adex", "c7_hh")
SOURCES = (
    "tests/d1_optimization_runner.c",
    "app/app_filesystem.c",
    "app/c7_audit_common.c",
    "src/minisnn.c",
    "src/neuron.c",
    "src/neuron_model.c",
    "src/network.c",
    "src/plasticity.c",
    "src/reward.c",
    "src/homeostasis.c",
    "src/agent_io.c",
    "src/sensor_encoder.c",
    "src/action_decoder.c",
    "src/agent_cycle.c",
    "src/agent_cycle_checkpoint.c",
    "src/structure.c",
    "src/structural_plasticity.c",
)


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, check=False)


def write_csv_atomically(path: Path, rows: list[dict[str, str]]) -> None:
    temporary = path.with_suffix(path.suffix + ".tmp")
    with temporary.open("w", newline="", encoding="ascii") as file:
        writer = csv.DictWriter(
            file,
            fieldnames=("audit_format_version", "case", "o0_output", "o2_output", "spikes",
                        "action_variation_count", "status"),
        )
        writer.writeheader()
        writer.writerows(rows)
    os.replace(temporary, path)


def compile_variant(compiler: str, optimization: str, output: Path) -> str | None:
    link_flags = [] if os.name == "nt" else ["-lm"]
    result = run([
        compiler, "-std=c11", "-Wall", "-Wextra", "-pedantic", optimization,
        *SOURCES, "-Iinclude", "-Isrc", "-Iapp", "-o", str(output), *link_flags,
    ])
    return None if result.returncode == 0 else result.stdout


def parse_output(output: str, case: str) -> dict[str, int] | None:
    fields: dict[str, str] = {}
    for item in output.split(";"):
        key, separator, value = item.partition("=")
        if not separator:
            return None
        fields[key] = value
    if fields.get("case") != case:
        return None
    try:
        return {
            "spikes": int(fields["spikes"]),
            "action_variation_count": int(fields["action_variation_count"]),
        }
    except (KeyError, ValueError):
        return None


def main() -> int:
    compiler = os.environ.get("CC", "gcc")
    if shutil.which(compiler) is None:
        print(f"D1-B optimization determinism FAILED: compiler unavailable: {compiler}")
        return 1
    BUILD.mkdir(parents=True, exist_ok=True)
    RESULTS.mkdir(parents=True, exist_ok=True)
    outputs = {"-O0": BUILD / "d1_o0.exe", "-O2": BUILD / "d1_o2.exe"}
    for optimization, executable in outputs.items():
        error = compile_variant(compiler, optimization, executable)
        if error is not None:
            print(f"D1-B optimization determinism FAILED while compiling {optimization}:\n{error.strip()}")
            return 1

    rows: list[dict[str, str]] = []
    for case in CASES:
        case_outputs: dict[str, str] = {}
        for optimization, executable in outputs.items():
            result = run([str(executable), case])
            if result.returncode != 0:
                print(f"D1-B optimization determinism FAILED for {case} {optimization}:\n"
                      f"{result.stdout.strip()}")
                return 1
            output = result.stdout.strip()
            parsed = parse_output(output, case)
            if parsed is None:
                print(f"D1-B optimization determinism FAILED: malformed output for {case} {optimization}")
                return 1
            case_outputs[optimization] = output
        parsed_o0 = parse_output(case_outputs["-O0"], case)
        assert parsed_o0 is not None
        active_c7_case = case.startswith("c7_")
        status = "PASS" if case_outputs["-O0"] == case_outputs["-O2"] else "FAIL"
        if active_c7_case and (parsed_o0["spikes"] == 0 or
                               parsed_o0["action_variation_count"] == 0):
            status = "FAIL"
        rows.append({
            "audit_format_version": "d1_b_v2",
            "case": case,
            "o0_output": case_outputs["-O0"],
            "o2_output": case_outputs["-O2"],
            "spikes": str(parsed_o0["spikes"]),
            "action_variation_count": str(parsed_o0["action_variation_count"]),
            "status": status,
        })
    write_csv_atomically(RESULTS / "d1_b_optimization_determinism.csv", rows)
    if any(row["status"] != "PASS" for row in rows):
        print("D1-B optimization determinism FAILED: O0/O2 traces diverged")
        return 1
    print("D1-B O0/O2 deterministic trace validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
