#!/usr/bin/env python3
import argparse
from pathlib import Path
import subprocess
import tempfile

def build(compiler, root, optimization, output):
    command = [
        compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
        "-Wformat=2", "-Wstrict-prototypes", optimization,
        "-Iinclude", "-I../kernel/include",
        "app/wt0_terrain_demo.c", "src/minisnn_worlds_terrain.c",
        "../kernel/src/minisnn_worlds_kernel.c", "../kernel/src/minisnn_worlds_kernel_snapshot.c", "../kernel/src/minisnn_worlds_kernel_restore.c", "../kernel/src/minisnn_worlds_kernel_command_log.c", "-o", str(output)
    ]
    subprocess.run(command, cwd=root, check=True)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--terrain-root", required=True)
    args = parser.parse_args()
    root = Path(args.terrain_root)
    with tempfile.TemporaryDirectory(prefix="wt0_o0_o2_") as temporary:
        temporary = Path(temporary)
        o0 = temporary / "demo_o0.exe"
        o2 = temporary / "demo_o2.exe"
        out0 = temporary / "o0"
        out2 = temporary / "o2"
        out0.mkdir()
        out2.mkdir()
        build(args.compiler, root, "-O0", o0)
        build(args.compiler, root, "-O2", o2)
        subprocess.run([str(o0), str(out0)], check=True)
        subprocess.run([str(o2), str(out2)], check=True)
        for name in ("wt0_map.txt", "wt0_summary.txt"):
            if (out0 / name).read_bytes() != (out2 / name).read_bytes():
                raise SystemExit(f"WT0 O0/O2 FAIL: {name}")
    print("WT0 O0/O2 determinism OK")

if __name__ == "__main__":
    main()
