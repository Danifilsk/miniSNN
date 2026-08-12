#!/usr/bin/env python3
"""Regression contracts for K1-C3 sanitizer coverage and classification."""
from __future__ import annotations

import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
from sanitizer_support import classify_unavailable, has_sanitizer_failure, project_has_failed


EXPECTED_OUTPUTS = {
    "ordering": "K1-C3 ordering, lifecycle, occupancy, links and canonical query validation OK",
    "invariants": "K1-C3 planned and official invariant validation OK",
    "limits": "K1-C3 limits, identifiers, counters and allocation atomicity OK",
    "long_run": "K1-C3 long run deterministic OK",
}


def completed(command: list[str], returncode: int, stdout: str = "", stderr: str = "") -> subprocess.CompletedProcess[str]:
    return subprocess.CompletedProcess(command, returncode, stdout, stderr)


def load_harness():
    spec = importlib.util.spec_from_file_location(
        "test_k1_c3_sanitize_harness", SCRIPT_DIR / "test_k1_c3_sanitize.py"
    )
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load K1-C3 sanitizer harness")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run_controlled_harness(mode: str) -> tuple[int, str, list[str], list[str]]:
    with tempfile.TemporaryDirectory(prefix="k1_c3_sanitizer_classification_") as directory:
        root = Path(directory)
        include = root / "include"
        include.mkdir()
        source = root / "kernel.c"
        source.write_text("int kernel_placeholder;\n", encoding="ascii")
        tests = {}
        for label in EXPECTED_OUTPUTS:
            path = root / f"{label}.c"
            path.write_text("int main(void) { return 0; }\n", encoding="ascii")
            tests[label] = path

        harness = load_harness()
        compiled: list[str] = []
        executed: list[str] = []

        def fake_probe(compiler: str, output: Path, label: str):
            if mode == "unavailable":
                print(f"{label} sanitizer UNAVAILABLE: simulated probe")
                return None
            return True

        def fake_run(command: list[str], environment=None):
            if command[0] == "fake-cc":
                label = Path(command[-4]).stem
                compiled.append(label)
                if mode == "compile_failure" and label == "limits":
                    return completed(command, 1, stderr="simulated compilation failure")
                return completed(command, 0)
            label = Path(command[0]).name.removesuffix("_sanitize.exe")
            label = label.removeprefix("k1_c3_")
            executed.append(label)
            if mode == "execution_failure" and label == "invariants":
                return completed(command, 1, stderr="simulated execution failure")
            return completed(command, 0, EXPECTED_OUTPUTS[label])

        harness.probe_toolchain = fake_probe
        harness.run = fake_run
        previous_argv = sys.argv
        sys.argv = [
            "test_k1_c3_sanitize.py",
            "--compiler", "fake-cc",
            "--include", str(include),
            "--source", str(source),
            "--ordering-test", str(tests["ordering"]),
            "--invariants-test", str(tests["invariants"]),
            "--limits-test", str(tests["limits"]),
            "--long-run-test", str(tests["long_run"]),
            "--output-dir", str(root / "output"),
        ]
        try:
            captured = io.StringIO()
            with contextlib.redirect_stdout(captured), contextlib.redirect_stderr(captured):
                result = harness.main()
        finally:
            sys.argv = previous_argv
        return result, captured.getvalue(), compiled, executed


def main() -> int:
    unavailable_probe = completed(["cc"], 1, stderr="ld: cannot find -lasan")
    invalid_project = completed(["cc"], 1, stderr="fatal error: missing_project_source.c: No such file")
    runtime_report = completed(["cc"], 1, stderr="ERROR: AddressSanitizer: heap-use-after-free")
    if not classify_unavailable(unavailable_probe):
        raise SystemExit("sanitizer availability regression: missing libasan not unavailable")
    if classify_unavailable(invalid_project) or not project_has_failed(invalid_project):
        raise SystemExit("sanitizer availability regression: invalid project misclassified")
    if not has_sanitizer_failure(runtime_report):
        raise SystemExit("sanitizer availability regression: runtime report not detected")

    result, output, compiled, executed = run_controlled_harness("unavailable")
    if result != 0 or compiled or executed or "UNAVAILABLE" not in output:
        raise SystemExit("sanitizer availability regression: probe unavailability contract failed")

    result, output, compiled, executed = run_controlled_harness("pass")
    expected = ["ordering", "invariants", "limits", "long_run"]
    if result != 0 or compiled != expected or executed != expected:
        raise SystemExit("sanitizer coverage regression: successful probe did not build and run all C3 tests")
    for label in expected:
        if f"K1-C3 sanitizer {label}: PASS" not in output:
            raise SystemExit(f"sanitizer coverage regression: missing PASS output for {label}")

    result, output, compiled, executed = run_controlled_harness("compile_failure")
    if result == 0 or compiled != ["ordering", "invariants", "limits"] or executed != ["ordering", "invariants"]:
        raise SystemExit("sanitizer classification regression: compilation failure was not fatal")
    if "K1-C3 sanitizer limits: FAIL" not in output:
        raise SystemExit("sanitizer classification regression: limits compilation failure was not identified")

    result, output, compiled, executed = run_controlled_harness("execution_failure")
    if result == 0 or compiled != ["ordering", "invariants"] or executed != ["ordering", "invariants"]:
        raise SystemExit("sanitizer classification regression: execution failure was not fatal")
    if "K1-C3 sanitizer invariants: FAIL" not in output:
        raise SystemExit("sanitizer classification regression: invariants execution failure was not identified")

    print("K1-C3 sanitizer classification regression OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())