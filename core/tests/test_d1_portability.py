from __future__ import annotations

from pathlib import Path
import contextlib
import io
import os
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS))

import check_d1_b
import run_portable_sanitizers


def fail(message: str) -> int:
    print(f"D1-B portability test FAILED: {message}")
    return 1


def main() -> int:
    filesystem = (ROOT / "app" / "app_filesystem.c").read_text(encoding="utf-8")
    scenario_runner = (ROOT / "app" / "scenario_runner.c").read_text(encoding="utf-8")
    optimization = (SCRIPTS / "check_d1_optimization_determinism.py").read_text(encoding="utf-8")
    sanitizer = (SCRIPTS / "run_portable_sanitizers.py").read_text(encoding="utf-8")
    posix_smoke = (SCRIPTS / "check_d1_posix_headless.py").read_text(encoding="utf-8")

    if not filesystem.startswith("#ifndef _WIN32\n#ifndef _POSIX_C_SOURCE"):
        return fail("app_filesystem nao declara POSIX antes dos headers")
    for token in ("MINISNN_APP_POPEN", "MINISNN_APP_PCLOSE", "MINISNN_APP_NULL_REDIRECT",
                  "MINISNN_APP_OS_NAME", "operating_system=%s"):
        if token not in scenario_runner:
            return fail(f"scenario_runner sem wrapper portavel: {token}")
    if ("os.name == \"nt\" else [\"-lm\"]" not in optimization or
            "link_flags =" not in optimization or "*link_flags," not in optimization):
        return fail("harness O0/O2 sem linkagem matematica POSIX")
    if "sanitizer_environment" not in sanitizer or "compile_status" not in sanitizer:
        return fail("harness sanitizer sem separacao de fases")
    for harness in (sanitizer, posix_smoke):
        if ("def platform_compile_flags" not in harness or
                '"-D_POSIX_C_SOURCE=200809L"' not in harness or
                "*platform_compile_flags()" not in harness):
            return fail("harness POSIX sem feature macro na linha de comando")

    compiler = shutil.which("gcc") or shutil.which("clang")
    if compiler is None:
        return fail("compilador C ausente para teste de classificacao")
    broken = run_portable_sanitizers.project_test(
        compiler, "intentional_compile_error", (), ("tests/does_not_exist.c",)
    )
    if broken["compile_status"] != "FAIL" or broken["run_status"] != "NOT_RUN":
        return fail("erro de compilacao do projeto nao foi classificado como FAIL")
    probe_status, _ = run_portable_sanitizers.sanitizer_environment(compiler)
    if probe_status not in {"PASS", "UNAVAILABLE"}:
        return fail("probe de sanitizer retornou classificacao invalida")

    if os.name != "nt":
        with tempfile.TemporaryDirectory() as temporary_name:
            object_path = Path(temporary_name) / "app_filesystem.o"
            forced_include = subprocess.run(
                [
                    compiler, "-std=c11", "-Wall", "-Wextra", "-pedantic",
                    "-D_POSIX_C_SOURCE=200809L", "-DMINISNN_TESTING",
                    "-include", "tests/test_allocation.h", "-c", "app/app_filesystem.c",
                    "-Iinclude", "-Isrc", "-Iapp", "-o", str(object_path),
                ],
                cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                check=False,
            )
            if forced_include.returncode != 0:
                return fail("forced include POSIX nao declarou localtime_r")

    original_run = run_portable_sanitizers.run
    original_build = run_portable_sanitizers.BUILD
    with tempfile.TemporaryDirectory() as temporary_name:
        run_portable_sanitizers.BUILD = Path(temporary_name)
        try:
            calls = 0

            def simulated_run(command: list[str], environment: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
                nonlocal calls
                calls += 1
                return subprocess.CompletedProcess(command, 0 if calls == 1 else 1,
                                                   "" if calls == 1 else "simulated failure")

            run_portable_sanitizers.run = simulated_run
            simulated = run_portable_sanitizers.project_test(
                compiler, "intentional_runtime_failure", (), ("tests/test_LIF.c",)
            )
            if simulated["compile_status"] != "PASS" or simulated["run_status"] != "FAIL":
                return fail("falha de execucao com sanitizer nao foi classificada como FAIL")

            run_portable_sanitizers.run = lambda command, environment=None: subprocess.CompletedProcess(
                command, 1, "runtime unavailable"
            )
            unavailable, _ = run_portable_sanitizers.sanitizer_environment(compiler)
            if unavailable != "UNAVAILABLE":
                return fail("runtime sanitizer ausente nao foi classificado como UNAVAILABLE")
        finally:
            run_portable_sanitizers.run = original_run
            run_portable_sanitizers.BUILD = original_build

    artifact = ROOT / "results" / "d1_b_robustness" / "d1_b_determinism.csv"
    before = artifact.read_bytes() if artifact.is_file() else None
    missing = ROOT / "build" / "tests" / "bin" / "d1_b_missing.exe"
    result = subprocess.run(
        [sys.executable, str(SCRIPTS / "check_d1_determinism.py"), str(missing)],
        cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False,
    )
    after = artifact.read_bytes() if artifact.is_file() else None
    if result.returncode == 0 or before != after:
        return fail("artefato PASS foi criado ou alterado sem executar binario")

    with tempfile.TemporaryDirectory() as temporary_name:
        temporary = Path(temporary_name)
        old_output = check_d1_b.OUTPUT
        try:
            check_d1_b.OUTPUT = temporary
            with contextlib.redirect_stdout(io.StringIO()):
                accepted = check_d1_b.main() == 0
            if accepted:
                return fail("checker aceitou artefato stale ou incompleto")
        finally:
            check_d1_b.OUTPUT = old_output
    print("D1-B portability harness validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
