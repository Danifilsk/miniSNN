from __future__ import annotations

from pathlib import Path
import json
import re
import subprocess
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
ROOT_ARTIFACT_SUFFIXES = {".csv", ".png", ".exe", ".o", ".obj"}
FORBIDDEN_LOCAL_PATH = re.compile(r"(?i)(?:[a-z]:[\\/]users[\\/]|file://)")

EXPECTED_FILES = (
    "include/minisnn.h",
    "src/minisnn.c",
    "src/network.c",
    "src/neuron.c",
    "app/scenario_config.c",
    "app/scenario_runner.c",
    "app/minisnn_runner.c",
    "app/minisnn_studio.c",
    "configs/random.ini",
    "configs/small_world.ini",
    "tests/test_LIF.c",
    "tests/test_runner_topologies.c",
    "tests/test_reproducibility.c",
    "tests/test_metrics_common.py",
    "tests/test_regression_baseline.py",
    "tests/golden/core_v02_baseline.json",
    "docs/AUDITORIA_DO_CORE_V02.md",
    "docs/COBERTURA_DE_TESTES.md",
    "docs/BENCHMARKS_V02.md",
    "docs/CHECKLIST_DE_VALIDACAO_DO_STUDIO.md",
    "results/benchmarks/.gitkeep",
)

REQUIRED_TARGETS = (
    "test",
    "test-lif",
    "test-runner-topologies",
    "test-reproducibility",
    "test-diagnostics",
    "test-regression",
    "test-analyzer",
    "test-sanitize",
    "test-long",
    "benchmark-v02",
    "test-docs",
    "check-v02",
)

GOLDEN_TOPOLOGIES = {
    "chain",
    "ring",
    "all_to_all",
    "random",
    "random_balanced",
    "small_world",
    "feedforward",
}


def git_status() -> list[str]:
    completed = subprocess.run(
        ["git", "status", "--short"],
        cwd=PROJECT_ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if completed.returncode != 0:
        return [f"ERROR: {completed.stdout.strip()}"]
    return completed.stdout.splitlines()


def check_local_paths(errors: list[str]) -> None:
    allowed_suffixes = {".c", ".h", ".py", ".md", ".ini"}
    validator_paths = {
        Path("scripts/check_docs.py"),
        Path("scripts/check_release_v02.py"),
    }
    for path in PROJECT_ROOT.rglob("*"):
        if not path.is_file():
            continue
        relative = path.relative_to(PROJECT_ROOT)
        if relative in validator_paths:
            continue
        if relative.parts[0] in {".git", "build", "results"}:
            continue
        if path.suffix.lower() not in allowed_suffixes and path.name not in {"Makefile", ".gitignore"}:
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        if FORBIDDEN_LOCAL_PATH.search(text):
            errors.append(f"forbidden local absolute path: {relative}")


def main() -> int:
    errors: list[str] = []
    status = git_status()
    print(f"Working tree: {'clean' if not status else 'dirty (' + str(len(status)) + ' entries)'}")
    for line in status:
        print(f"  {line}")

    for relative in EXPECTED_FILES:
        if not (PROJECT_ROOT / relative).is_file():
            errors.append(f"missing expected file: {relative}")

    root_artifacts = [
        path.name
        for path in PROJECT_ROOT.iterdir()
        if path.is_file() and path.suffix.lower() in ROOT_ARTIFACT_SUFFIXES
    ]
    if root_artifacts:
        errors.append("generated artifacts in project root: " + ", ".join(sorted(root_artifacts)))

    runner_source = (PROJECT_ROOT / "app" / "scenario_runner.c").read_text(encoding="utf-8")
    if '#define MINISNN_VERSION "0.2"' not in runner_source:
        errors.append("scenario runner version is not 0.2")

    makefile = (PROJECT_ROOT / "Makefile").read_text(encoding="utf-8")
    targets = set(re.findall(r"^([A-Za-z0-9_.-]+):", makefile, re.MULTILINE))
    for target in REQUIRED_TARGETS:
        if target not in targets:
            errors.append(f"missing Make target: {target}")

    golden_path = PROJECT_ROOT / "tests" / "golden" / "core_v02_baseline.json"
    if golden_path.exists():
        try:
            golden = json.loads(golden_path.read_text(encoding="utf-8"))
            if set(golden) != GOLDEN_TOPOLOGIES:
                errors.append("golden topology set is incomplete")
            for name, data in golden.items():
                for key in ("topology_signature", "connection_count", "spikes_per_timestep", "raster"):
                    if key not in data:
                        errors.append(f"golden {name} missing {key}")
        except (OSError, ValueError) as error:
            errors.append(f"invalid golden baseline: {error}")

    check_local_paths(errors)

    sys.path.insert(0, str(PROJECT_ROOT / "scripts"))
    from check_docs import validate_docs

    errors.extend(validate_docs(PROJECT_ROOT))

    if errors:
        print("Core v0.2 readiness FAILED")
        for error in sorted(set(errors)):
            print(f"- {error}")
        return 1

    print("Core v0.2 automatic readiness checks OK")
    print("Studio manual validation: PENDING")
    print("No commit, tag, push, or Git index change was performed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
