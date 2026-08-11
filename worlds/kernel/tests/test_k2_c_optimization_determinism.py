#!/usr/bin/env python3
"""Compare K2-C canonical artifacts built with -O0 and -O2."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile


def run(command: list[str]) -> None:
    completed = subprocess.run(command, text=True, capture_output=True, check=False)
    if completed.returncode != 0:
        raise SystemExit("K2-C O0/O2 FAILED:\n" + completed.stdout + completed.stderr)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--kernel-root", required=True)
    args = parser.parse_args()
    root = Path(args.kernel_root).resolve()
    sources = [
        str(root / "app" / "k2_command_replay_demo.c"),
        str(root / "app" / "k2_snapshot_file.c"),
        str(root / "app" / "k2_command_log_file.c"),
        str(root / "src" / "minisnn_worlds_kernel.c"),
        str(root / "src" / "minisnn_worlds_kernel_snapshot.c"),
        str(root / "src" / "minisnn_worlds_kernel_restore.c"),
        str(root / "src" / "minisnn_worlds_kernel_command_log.c"),
    ]
    with tempfile.TemporaryDirectory(prefix="k2_c_o") as directory:
        temporary = Path(directory)
        artifacts: list[tuple[bytes, bytes, str]] = []
        for optimization in ("-O0", "-O2"):
            executable = temporary / f"demo_{optimization[2:]}.exe"
            output = temporary / optimization[1:]
            output.mkdir()
            run([
                args.compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
                "-Wformat=2", "-Wstrict-prototypes", optimization,
                "-I", str(root / "include"), "-I", str(root / "app"),
                *sources, "-o", str(executable),
            ])
            run([str(executable), str(output)])
            artifacts.append((
                (output / "command_log.bin").read_bytes(),
                (output / "snapshot.bin").read_bytes(),
                (output / "summary.txt").read_text(encoding="utf-8"),
            ))
        if artifacts[0] != artifacts[1]:
            raise SystemExit("K2-C O0/O2 FAILED: canonical artifacts differ")
    print("K2-C O0/O2 deterministic validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())