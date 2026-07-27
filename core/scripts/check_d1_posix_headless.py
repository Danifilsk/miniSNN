from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT.parent / "build" / "audit" / "d1_b_posix"
OUTPUT = ROOT / "results" / "d1_b_robustness" / "d1_b_posix_headless.txt"
API_SOURCES = (
    "src/minisnn.c", "src/neuron.c", "src/neuron_model.c", "src/network.c",
    "src/plasticity.c", "src/reward.c", "src/homeostasis.c", "src/agent_io.c",
    "src/sensor_encoder.c", "src/action_decoder.c", "src/agent_cycle.c",
    "src/agent_cycle_checkpoint.c", "src/structure.c", "src/structural_plasticity.c",
)
CORE_LIBRARY_SOURCES = tuple(
    str(path.relative_to(ROOT)).replace("\\", "/")
    for path in sorted((ROOT / "src").glob("*.c"))
)
SCENARIO_SOURCES = (
    "app/app_filesystem.c", "app/scenario_config.c", "app/scenario_runtime.c",
    "app/scenario_runner.c", "app/working_memory.c", "app/associative_memory.c",
    "app/sequence_prediction.c",
)


def platform_compile_flags() -> list[str]:
    return [] if os.name == "nt" else ["-D_POSIX_C_SOURCE=200809L"]


def write_result(status: str, detail: str, entries: list[str]) -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    temporary = OUTPUT.with_suffix(".txt.tmp")
    temporary.write_text(
        "D1-B POSIX headless smoke\n"
        "audit_format_version=d1_b_v2\n"
        f"status={status}\n"
        f"detail={detail}\n" + "\n".join(entries) + "\n",
        encoding="ascii", errors="backslashreplace",
    )
    os.replace(temporary, OUTPUT)


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, check=False)


def compile_command(compiler: str, sources: tuple[str, ...], output: Path,
                    extra: tuple[str, ...] = ()) -> tuple[bool, str]:
    result = run([
        compiler, "-std=c11", "-Wall", "-Wextra", "-pedantic", *platform_compile_flags(),
        *extra, *sources,
        "-Iinclude", "-Isrc", "-Iapp", "-o", str(output), "-lm",
    ])
    return result.returncode == 0, result.stdout.strip()


def main() -> int:
    if os.name == "nt":
        write_result("UNAVAILABLE", "current host is Windows; no POSIX compiler contract", [])
        print("D1-B POSIX headless smoke UNAVAILABLE")
        return 0
    compiler = shutil.which(os.environ.get("CC", "")) or shutil.which("clang") or shutil.which("gcc")
    archiver = shutil.which("ar")
    if compiler is None or archiver is None:
        write_result("UNAVAILABLE", "POSIX C compiler or ar is unavailable", [])
        print("D1-B POSIX headless smoke UNAVAILABLE")
        return 0

    BUILD.mkdir(parents=True, exist_ok=True)
    objects: list[str] = []
    entries: list[str] = []
    for source in CORE_LIBRARY_SOURCES:
        object_path = BUILD / (Path(source).stem + ".o")
        result = run([compiler, "-std=c11", "-Wall", "-Wextra", "-pedantic",
                      *platform_compile_flags(), "-c", source,
                      "-Iinclude", "-Isrc", "-Iapp", "-o", str(object_path)])
        if result.returncode != 0:
            write_result("FAIL", f"Core library compile failed: {source}: {result.stdout.strip()}", entries)
            print("D1-B POSIX headless smoke FAIL")
            return 1
        objects.append(str(object_path))
    library = BUILD / "libminisnn_core_posix.a"
    archived = run([archiver, "rcs", str(library), *objects])
    if archived.returncode != 0:
        write_result("FAIL", f"Core library archive failed: {archived.stdout.strip()}", entries)
        print("D1-B POSIX headless smoke FAIL")
        return 1
    entries.append("core_library=PASS")

    smokes = (
        ("d1_optimization_runner", ("tests/d1_optimization_runner.c", "app/app_filesystem.c",
                                     "app/c7_audit_common.c", str(library)), ()),
        ("test_d1_determinism", ("tests/test_d1_determinism.c", *SCENARIO_SOURCES,
                                  "app/c7_audit_common.c", str(library)), ()),
        ("test_d1_corruption", ("tests/test_d1_corruption.c", "app/scenario_config.c",
                                "app/app_filesystem.c", str(library)), ()),
        ("test_d1_lifecycle_stress", ("tests/test_d1_lifecycle_stress.c",
                                       "tests/test_allocation.c", "app/app_filesystem.c",
                                       "app/c7_audit_common.c", *API_SOURCES),
         ("-DMINISNN_TESTING", "-include", "tests/test_allocation.h")),
    )
    for name, sources, flags in smokes:
        ok, detail = compile_command(compiler, sources, BUILD / name, flags)
        if not ok:
            write_result("FAIL", f"{name} compile failed: {detail}", entries)
            print("D1-B POSIX headless smoke FAIL")
            return 1
        entries.append(f"{name}=PASS")
    write_result("PASS", f"compiler={Path(compiler).name}", entries)
    print("D1-B POSIX headless smoke PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
