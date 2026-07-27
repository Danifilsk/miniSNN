from __future__ import annotations

import csv
import os
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIRECTORY = ROOT / "results" / "d1_b_robustness"


def execute_test_and_write_artifact(
    artifact_name: str,
    success_marker: str,
    description: str,
) -> int:
    if len(sys.argv) != 2:
        print(f"D1-B {description} FAILED: expected one test executable path")
        return 1

    test_binary_argument = sys.argv[1]
    test_binary = Path(test_binary_argument)
    if not test_binary.is_file():
        print(f"D1-B {description} FAILED: test executable absent: {test_binary_argument}")
        return 1

    result = subprocess.run(
        [str(test_binary)], cwd=ROOT, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, check=False,
    )
    evidence = " ".join(result.stdout.split())
    if result.returncode != 0 or success_marker not in result.stdout:
        print(f"D1-B {description} FAILED: test executable did not provide success evidence")
        if evidence:
            print(evidence)
        return 1

    OUTPUT_DIRECTORY.mkdir(parents=True, exist_ok=True)
    destination = OUTPUT_DIRECTORY / artifact_name
    temporary = destination.with_suffix(destination.suffix + ".tmp")
    with temporary.open("w", newline="", encoding="ascii", errors="backslashreplace") as file:
        writer = csv.DictWriter(
            file,
            fieldnames=("audit_format_version", "test_binary", "test_status", "evidence", "status"),
        )
        writer.writeheader()
        writer.writerow({
            "audit_format_version": "d1_b_v2",
            "test_binary": test_binary_argument.replace("\\", "/"),
            "test_status": "PASS",
            "evidence": evidence,
            "status": "PASS",
        })
    os.replace(temporary, destination)
    print(f"D1-B {description} artifact validation OK")
    return 0
