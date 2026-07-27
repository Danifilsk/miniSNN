from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
LIBRARY = ROOT.parent / "build" / "core" / "lib" / "libminisnn_core.a"
OUTPUT = ROOT / "results" / "d1_b_robustness" / "d1_b_symbols.txt"


def main() -> int:
    nm = shutil.which("nm")
    if nm is None or not LIBRARY.is_file():
        print("D1-B symbol audit FAILED: nm or core library unavailable")
        return 1
    result = subprocess.run([nm, "-g", "--defined-only", str(LIBRARY)], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            check=False)
    if result.returncode != 0:
        print(f"D1-B symbol audit FAILED:\n{result.stdout.strip()}")
        return 1
    symbols = result.stdout
    forbidden = ("minisnn_studio", "CreateWindow", "WinMain", "minisnn_test_")
    found = [name for name in forbidden if name in symbols]
    if found:
        print(f"D1-B symbol audit FAILED: symbols prohibited in core library: {', '.join(found)}")
        return 1
    if not (ROOT / "include" / "minisnn_evolution_legacy.h").is_file():
        print("D1-B symbol audit FAILED: legacy evolution compatibility header absent")
        return 1
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    temporary = OUTPUT.with_suffix(".txt.tmp")
    temporary.write_text(
        "D1-B symbol and ABI audit\n"
        "audit_format_version=d1_b_v2\n"
        "status=PASS\n"
        "api=source_candidate_v1\n"
        "binary_abi=cross_toolchain_not_guaranteed\n"
        "legacy_evolution_header=present\n"
        "studio_symbols=absent\n"
        "test_symbols=absent\n"
        f"defined_symbol_lines={len([line for line in symbols.splitlines() if line.strip()])}\n",
        encoding="ascii",
    )
    os.replace(temporary, OUTPUT)
    print("D1-B core symbol and ABI audit OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
