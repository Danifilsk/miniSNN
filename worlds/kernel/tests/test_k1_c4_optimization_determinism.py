#!/usr/bin/env python3
import argparse
import filecmp
import subprocess
import sys
import tempfile
from pathlib import Path

ARTIFACTS = (
    "config_used.ini", "commands.csv", "events.csv", "entities.csv",
    "spatial_links.csv", "diagnostics.csv", "state_hash.txt", "summary.txt",
)

def run(command):
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode:
        raise SystemExit("K1-C4 O0/O2 FAILED:\n" + result.stdout + result.stderr)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--kernel-root", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--golden", required=True)
    args = parser.parse_args()
    root = Path(args.kernel_root).resolve()
    with tempfile.TemporaryDirectory(prefix="k1_c4_o") as temporary:
        base = Path(temporary)
        outputs = []
        for optimization in ("-O0", "-O2"):
            executable = base / ("demo" + optimization[-1] + ".exe")
            output = base / ("out" + optimization[-1])
            output.mkdir()
            run([args.compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
                 "-Wformat=2", "-DMINISNN_WORLDS_KERNEL_TESTING",
                 "-DMINISNN_WORLDS_KERNEL_C3_INVARIANT_TESTING", optimization,
                 "-I", str(root / "include"), "-I", str(root / "app"),
                 str(root / "app" / "k1_spatial_links_demo.c"),
                 str(root / "app" / "k1_c4_config.c"),
                 str(root / "src" / "minisnn_worlds_kernel.c"),
                 "-o", str(executable)])
            run([str(executable), str(root / args.config), str(output)])
            run([sys.executable, str(root / "scripts" / "check_k1_c4.py"),
                 "--directory", str(output), "--golden", str(root / args.golden)])
            outputs.append(output)
        for artifact in ARTIFACTS:
            if not filecmp.cmp(outputs[0] / artifact, outputs[1] / artifact, shallow=False):
                raise SystemExit("K1-C4 O0/O2 FAILED: " + artifact)
    print("K1-C4 O0/O2 deterministic validation OK")

if __name__ == "__main__":
    main()