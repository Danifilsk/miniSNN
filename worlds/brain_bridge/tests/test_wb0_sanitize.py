#!/usr/bin/env python3
"""Run focused WB0 coverage under ASan/UBSan when the toolchain supports it."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile

FLAGS = (
    "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wformat=2", "-Wstrict-prototypes",
    "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
)


def run(command: list[str], cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, text=True, capture_output=True, check=False, cwd=cwd)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--bridge-root", required=True)
    args = parser.parse_args()
    bridge = Path(args.bridge_root).resolve()
    repository = bridge.parent.parent
    core = repository / "core"
    domain = repository / "worlds" / "domain"
    kernel = repository / "worlds" / "kernel"
    core_library = repository / "build" / "core" / "lib" / "libminisnn_core.a"
    kernel_sources = (
        kernel / "src" / "minisnn_worlds_kernel.c",
        kernel / "src" / "minisnn_worlds_kernel_snapshot.c",
        kernel / "src" / "minisnn_worlds_kernel_command_log.c",
    )

    with tempfile.TemporaryDirectory(prefix="wb0_sanitize_") as directory:
        root = Path(directory)
        probe = root / "probe.c"
        probe.write_text("int main(void) { return 0; }\n", encoding="ascii")
        probe_executable = root / "probe.exe"
        build = run([args.compiler, *FLAGS, str(probe), "-o", str(probe_executable)])
        if build.returncode != 0 or run([str(probe_executable)], root).returncode != 0:
            print("WB0 sanitizer: UNAVAILABLE")
            return 0
        if not core_library.is_file():
            print("WB0 sanitizer: UNAVAILABLE")
            return 0

        executable = root / "wb0_test.exe"
        build = run([
            args.compiler, *FLAGS, f"-I{bridge / 'include'}", f"-I{core / 'include'}",
            f"-I{domain / 'include'}", f"-I{kernel / 'include'}",
            str(bridge / "tests" / "test_wb0_brain_bridge.c"),
            str(bridge / "src" / "minisnn_worlds_brain_bridge.c"),
            str(domain / "src" / "minisnn_worlds_domain.c"),
            *(str(source) for source in kernel_sources),
            str(core_library), "-o", str(executable),
        ])
        if build.returncode != 0:
            print("WB0 sanitizer: FAIL\n" + build.stdout + build.stderr)
            return 1
        result = run([str(executable)], root)
        if result.returncode != 0:
            print("WB0 sanitizer: FAIL\n" + result.stdout + result.stderr)
            return 1
        demo = root / "wb0_demo.exe"
        results = root / "results"
        results.mkdir()
        build = run([
            args.compiler, *FLAGS, f"-I{bridge / 'include'}", f"-I{core / 'include'}",
            f"-I{domain / 'include'}", f"-I{kernel / 'include'}",
            str(bridge / "app" / "wb0_brain_bridge_demo.c"),
            str(bridge / "src" / "minisnn_worlds_brain_bridge.c"),
            str(domain / "src" / "minisnn_worlds_domain.c"),
            *(str(source) for source in kernel_sources),
            str(core_library), "-o", str(demo),
        ])
        if build.returncode != 0 or run([str(demo), str(results)], root).returncode != 0:
            print("WB0 sanitizer: FAIL\n" + build.stdout + build.stderr)
            return 1
    print("WB0 sanitizer: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())