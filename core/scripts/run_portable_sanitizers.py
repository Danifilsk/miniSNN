from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "results" / "d1_b_robustness" / "d1_b_sanitizers.txt"
BUILD = ROOT.parent / "build" / "audit" / "d1_b_sanitizers"
SANITIZER_FLAGS = ("-fsanitize=address,undefined", "-fno-omit-frame-pointer")
API_SOURCES = (
    "src/minisnn.c", "src/neuron.c", "src/neuron_model.c", "src/network.c",
    "src/plasticity.c", "src/reward.c", "src/homeostasis.c", "src/agent_io.c",
    "src/sensor_encoder.c", "src/action_decoder.c", "src/agent_cycle.c",
    "src/agent_cycle_checkpoint.c", "src/structure.c", "src/structural_plasticity.c",
)
TESTS = (
    ("test_d1_lifecycle_stress", ("-DMINISNN_TESTING", "-include", "tests/test_allocation.h"),
     ("tests/test_d1_lifecycle_stress.c", "tests/test_allocation.c", "app/app_filesystem.c",
      "app/c7_audit_common.c", *API_SOURCES)),
    ("test_d1_corruption", (),
     ("tests/test_d1_corruption.c", "app/scenario_config.c", "app/app_filesystem.c", *API_SOURCES)),
    ("test_agent_cycle", ("-DMINISNN_TESTING",), ("tests/test_agent_cycle.c", *API_SOURCES)),
    ("test_agent_cycle_checkpoint", ("-DMINISNN_TESTING",),
     ("tests/test_agent_cycle_checkpoint.c", "app/app_filesystem.c", *API_SOURCES)),
    ("test_c7_integration", ("-DMINISNN_TESTING",),
     ("tests/test_c7_integration.c", "app/app_filesystem.c", "app/c7_audit_common.c", *API_SOURCES)),
    ("test_structural_plasticity", (), ("tests/test_structural_plasticity.c", *API_SOURCES)),
    ("test_reward", (), ("tests/test_reward.c", *API_SOURCES)),
)


def executable(name: str) -> str | None:
    return shutil.which(name) or shutil.which(f"{name}.exe")


def run(command: list[str], environment: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, env=environment, check=False)


def link_flags() -> list[str]:
    return [] if os.name == "nt" else ["-lm"]


def platform_compile_flags() -> list[str]:
    return [] if os.name == "nt" else ["-D_POSIX_C_SOURCE=200809L"]


def sanitizer_environment(compiler: str) -> tuple[str, str]:
    BUILD.mkdir(parents=True, exist_ok=True)
    source = BUILD / "sanitizer_probe.c"
    output = BUILD / "sanitizer_probe.exe"
    source.write_text(
        "#include <stdlib.h>\n"
        "int main(void) { void *pointer = malloc(1U); free(pointer); return 0; }\n",
        encoding="ascii",
    )
    compiled = run([
        compiler, "-std=c11", "-Wall", "-Wextra", "-pedantic", *SANITIZER_FLAGS,
        str(source), "-o", str(output), *link_flags(),
    ])
    if compiled.returncode != 0:
        return "UNAVAILABLE", f"sanitizer probe compile failed: {compiled.stdout.strip()}"
    environment = os.environ.copy()
    environment.setdefault("ASAN_OPTIONS", "detect_leaks=1")
    executed = run([str(output)], environment)
    if executed.returncode != 0:
        return "UNAVAILABLE", f"sanitizer probe run failed: {executed.stdout.strip()}"
    return "PASS", "standalone sanitizer probe compiled and ran"


def project_test(compiler: str, name: str, extra_flags: tuple[str, ...],
                 sources: tuple[str, ...]) -> dict[str, str]:
    output = BUILD / f"{name}.exe"
    compiled = run([
        compiler, "-std=c11", "-Wall", "-Wextra", "-pedantic", *SANITIZER_FLAGS,
        *platform_compile_flags(), *extra_flags, *sources, "-Iinclude", "-Isrc", "-Iapp",
        "-o", str(output),
        *link_flags(),
    ])
    if compiled.returncode != 0:
        return {
            "name": name,
            "compile_status": "FAIL",
            "run_status": "NOT_RUN",
            "sanitizer_status": "NOT_RUN",
            "detail": compiled.stdout.strip(),
        }
    environment = os.environ.copy()
    environment.setdefault("ASAN_OPTIONS", "detect_leaks=1")
    executed = run([str(output)], environment)
    sanitizer_terms = ("AddressSanitizer", "UndefinedBehaviorSanitizer", "runtime error:")
    sanitizer_found = any(term in executed.stdout for term in sanitizer_terms)
    return {
        "name": name,
        "compile_status": "PASS",
        "run_status": "PASS" if executed.returncode == 0 else "FAIL",
        "sanitizer_status": "FAIL" if sanitizer_found else "PASS",
        "detail": executed.stdout.strip(),
    }


def write_report(status: str, toolchain: str, probe_detail: str,
                 tests: list[dict[str, str]]) -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    temporary = OUTPUT.with_suffix(".txt.tmp")
    lines = [
        "D1-B portable sanitizer audit",
        "audit_format_version=d1_b_v2",
        f"status={status}",
        f"toolchain={toolchain}",
        "requested=address,undefined,leak_when_supported",
        f"probe={probe_detail}",
    ]
    for test in tests:
        record = dict(test)
        record["detail"] = " ".join(record["detail"].split())
        lines.append(
            "test={name};compile_status={compile_status};run_status={run_status};"
            "sanitizer_status={sanitizer_status};detail={detail}".format(**record)
        )
    temporary.write_text("\n".join(lines) + "\n", encoding="ascii", errors="backslashreplace")
    os.replace(temporary, OUTPUT)


def main() -> int:
    candidates = [candidate for candidate in (executable("clang"), executable("gcc")) if candidate]
    probe_detail = "no supported sanitizer toolchain was found"
    last_toolchain = "none"
    for compiler in candidates:
        last_toolchain = Path(compiler).name
        probe_status, probe_detail = sanitizer_environment(compiler)
        if probe_status == "UNAVAILABLE":
            continue
        tests = [project_test(compiler, name, flags, sources)
                 for name, flags, sources in TESTS]
        failed = [test for test in tests if test["compile_status"] != "PASS" or
                  test["run_status"] != "PASS" or test["sanitizer_status"] != "PASS"]
        status = "FAIL" if failed else "PASS"
        write_report(status, Path(compiler).name, probe_detail, tests)
        print(f"D1-B portable sanitizers {status}")
        return 1 if status == "FAIL" else 0

    if os.name == "nt" and executable("wsl"):
        probe_detail += "; WSL detected but is not invoked automatically because workspace translation is external"
    write_report("UNAVAILABLE", last_toolchain, probe_detail, [])
    print("D1-B portable sanitizers UNAVAILABLE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
