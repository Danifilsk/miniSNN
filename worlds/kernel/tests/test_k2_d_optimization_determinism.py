#!/usr/bin/env python3
"""Compare K2-D A/B/C artifacts built with -O0 and -O2."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile


def run(command: list[str]) -> None:
    completed = subprocess.run(command, text=True, capture_output=True, check=False)
    if completed.returncode != 0:
        raise SystemExit("K2-D O0/O2 FAILED:\n" + completed.stdout + completed.stderr)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--kernel-root", required=True)
    args = parser.parse_args()
    root = Path(args.kernel_root).resolve()
    sources = [
        root / "app" / "k2_persistence_replay_demo.c",
        root / "app" / "k2_snapshot_file.c",
        root / "app" / "k2_command_log_file.c",
        root / "src" / "minisnn_worlds_kernel.c",
        root / "src" / "minisnn_worlds_kernel_snapshot.c",
        root / "src" / "minisnn_worlds_kernel_restore.c",
        root / "src" / "minisnn_worlds_kernel_command_log.c",
    ]
    with tempfile.TemporaryDirectory(prefix="k2_d_o") as directory:
        temporary = Path(directory)
        artifacts: list[tuple[bytes, ...]] = []
        for optimization in ("-O0", "-O2"):
            executable = temporary / f"demo_{optimization[2:]}.exe"
            output = temporary / optimization[1:]
            output.mkdir()
            run([
                args.compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
                "-Wformat=2", "-Wstrict-prototypes", optimization,
                "-I", str(root / "include"), "-I", str(root / "app"),
                *(str(source) for source in sources), "-o", str(executable),
            ])
            run([str(executable), str(output)])
            artifacts.append(tuple(
                (output / name).read_bytes()
                for name in (
                    "command_log.bin", "checkpoint_snapshot.bin", "final_snapshot.bin",
                    "final_continuous.bin", "final_full_replay.bin",
                    "final_restored_replay.bin", "summary.txt", "hashes.txt",
                )
            ))
        if artifacts[0] != artifacts[1]:
            raise SystemExit("K2-D O0/O2 FAILED: canonical artifacts differ")
    print("K2-D O0/O2 deterministic validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())