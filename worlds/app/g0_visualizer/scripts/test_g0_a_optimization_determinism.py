#!/usr/bin/env python3
import argparse
from pathlib import Path
import subprocess
import tempfile


def build(compiler, root, optimization, executable):
    command = [
        compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic",
        "-Wformat=2", "-Wstrict-prototypes", optimization,
        "-Iinclude", "-I../../../core/include", "-I../../kernel/include",
        "-I../../domain/include", "-I../../brain_bridge/include",
        "-I../../terrain/include", "-I../../scenarios/wf0_fish/include",
        "app/g0_visualizer_headless_demo.c",
        "src/g0_visualizer_assets.c", "src/g0_world_config.c", "src/g0_visualizer_runtime.c",
        "../../scenarios/wf0_fish/src/wf0_fish.c",
        "../../scenarios/wf0_fish/src/wf0_fish_observation.c",
        "../../brain_bridge/src/minisnn_worlds_brain_bridge.c",
        "../../../build/worlds/terrain/lib/libminisnn_worlds_terrain.a",
        "../../../build/worlds/domain/lib/libminisnn_worlds_domain.a",
        "../../../build/worlds/kernel/lib/libminisnn_worlds_kernel.a",
        "../../../build/core/lib/libminisnn_core.a",
        "-o", str(executable),
    ]
    subprocess.run(command, cwd=root, check=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--visualizer-root", required=True)
    args = parser.parse_args()
    root = Path(args.visualizer_root).resolve()
    repository_root = root.parents[2]
    with tempfile.TemporaryDirectory(prefix="g0_a_o0_o2_") as temporary:
        temporary = Path(temporary)
        o0 = temporary / "g0_o0.exe"
        o2 = temporary / "g0_o2.exe"
        out0 = temporary / "o0"
        out2 = temporary / "o2"
        out0.mkdir()
        out2.mkdir()
        build(args.compiler, root, "-O0", o0)
        build(args.compiler, root, "-O2", o2)
        subprocess.run([str(o0), str(out0), str(repository_root)], check=True)
        subprocess.run([str(o2), str(out2), str(repository_root)], check=True)
        for name in ("g0_trace.csv", "g0_summary.txt"):
            if (out0 / name).read_bytes() != (out2 / name).read_bytes():
                raise SystemExit(f"G0-A O0/O2 FAIL: {name}")
    print("G0-A O0/O2 determinism OK")


if __name__ == "__main__":
    main()
