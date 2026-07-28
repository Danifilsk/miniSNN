from __future__ import annotations

import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import zipfile

from d1_c_release_common import ARCHITECTURE, core_package_name, studio_package_name, version_string


ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = ROOT.parent
RELEASE = REPOSITORY / "build" / "release"
DIST = REPOSITORY / "dist"

STUDIO_RUNTIME_SCRIPTS = (
    "analyze_run.py",
    "compare_neuron_models.py",
    "compare_runs.py",
    "generate_evolution_report.py",
    "generate_history_report.py",
    "generate_run_reports.py",
    "html_report_common.py",
    "metrics_common.py",
    "plot_evolution.py",
    "plot_homeostasis.py",
    "plot_neuron.py",
    "plot_plasticity.py",
    "plot_reward.py",
    "plot_scenario.py",
)

STUDIO_EXCLUDED_CONFIGS = {
    "c7_integrated_audit.ini",
    "d1_b_robustness_audit.ini",
}
def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_metadata(stage: Path, product: str, runtime_dependencies: list[str]) -> None:
    (stage / "VERSION.txt").write_text(version_string() + "\n", encoding="ascii")
    build_info = [
        f"product={product}",
        f"version={version_string()}",
        f"architecture={ARCHITECTURE}",
        "build_profile=release;-O2;-DNDEBUG",
        "content_reproducibility=stable_file_order",
        "binary_reproducibility=not_guaranteed",
        "license_status=NO_LICENSE_FILE_IN_REPOSITORY",
    ]
    build_info.extend(f"runtime_dependency={item}" for item in runtime_dependencies)
    (stage / "BUILD_INFO.txt").write_text("\n".join(build_info) + "\n", encoding="utf-8")

    content = sorted(
        path for path in stage.rglob("*")
        if path.is_file() and path.name not in {"MANIFEST.txt", "SHA256SUMS.txt"}
    )
    checksums = [f"{sha256(path)}  {path.relative_to(stage).as_posix()}" for path in content]
    (stage / "SHA256SUMS.txt").write_text("\n".join(checksums) + "\n", encoding="ascii")
    manifest = [
        "manifest_format=minisnn_release_package_v1",
        f"product={product}",
        f"version={version_string()}",
        f"architecture={ARCHITECTURE}",
        "checksum_scope=all distributed content except generated MANIFEST.txt and SHA256SUMS.txt",
    ]
    manifest.extend(
        f"file={path.relative_to(stage).as_posix()};size={path.stat().st_size};sha256={sha256(path)}"
        for path in content
    )
    (stage / "MANIFEST.txt").write_text("\n".join(manifest) + "\n", encoding="utf-8")


def write_zip(stage: Path, output: Path) -> None:
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(item for item in stage.rglob("*") if item.is_file()):
            info = zipfile.ZipInfo(path.relative_to(stage).as_posix(), date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, path.read_bytes())


def imported_dlls(executable: Path) -> list[str]:
    objdump = shutil.which("objdump")
    if objdump is None:
        return ["objdump=UNAVAILABLE"]
    result = subprocess.run([objdump, "-p", str(executable)], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
    return sorted(set(re.findall(r"DLL Name: ([^\s]+)", result.stdout, flags=re.IGNORECASE)))


def copy_runtime_dependencies(executable: Path, destination: Path) -> list[str]:
    dependencies = imported_dlls(executable)
    compiler = shutil.which("gcc")
    system = {"kernel32.dll", "user32.dll", "gdi32.dll", "shell32.dll", "comdlg32.dll", "ole32.dll", "advapi32.dll"}
    recorded: list[str] = []
    for dependency in dependencies:
        lower = dependency.lower()
        if lower in system or lower.startswith("api-ms-win-") or lower == "objdump=unavailable":
            recorded.append(f"system:{dependency}")
            continue
        candidate = ""
        if compiler is not None:
            query = subprocess.run([compiler, f"-print-file-name={dependency}"], text=True,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
            candidate = query.stdout.strip()
        source = Path(candidate) if candidate and candidate != dependency else None
        if source is None or not source.is_file():
            raise RuntimeError(f"dependencia nao padrao nao localizada: {dependency}")
        shutil.copy2(source, destination / source.name)
        recorded.append(f"bundled:{dependency};source=gcc-print-file-name")
    return recorded


def copy_runtime_executables(executables: tuple[Path, ...], destination: Path) -> list[str]:
    recorded: list[str] = []
    copied_dependencies: set[str] = set()

    for executable in executables:
        if not executable.is_file():
            raise RuntimeError(f"executavel de runtime ausente: {executable}")
        shutil.copy2(executable, destination / executable.name)
        for dependency in copy_runtime_dependencies(executable, destination):
            if dependency not in copied_dependencies:
                copied_dependencies.add(dependency)
                recorded.append(dependency)
    return recorded


def copy_studio_runtime_resources(stage: Path) -> None:
    configs = stage / "configs"
    scripts = stage / "scripts"
    results = stage / "results"

    configs.mkdir()
    scripts.mkdir()
    results.mkdir()
    for source in sorted((ROOT / "configs").glob("*.ini")):
        if source.name not in STUDIO_EXCLUDED_CONFIGS:
            shutil.copy2(source, configs / source.name)
    for name in STUDIO_RUNTIME_SCRIPTS:
        source = ROOT / "scripts" / name
        if not source.is_file():
            raise RuntimeError(f"script exigido pelo Studio ausente: {source}")
        shutil.copy2(source, scripts / name)
    (results / ".gitkeep").write_text("", encoding="ascii")


def copy_documentation(destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    documents = (
        (ROOT / "API_REFERENCE.md", "API_REFERENCE.md"),
        (ROOT / "docs" / "PUBLIC_API_MANIFEST.md", "PUBLIC_API_MANIFEST.md"),
        (ROOT / "docs" / "PUBLIC_API_BASELINE_1_0_RC.txt", "PUBLIC_API_BASELINE_1_0_RC.txt"),
        (ROOT / "docs" / "CORE_BRIDGE_API_CANDIDATE.md", "CORE_BRIDGE_API_CANDIDATE.md"),
        (ROOT / "docs" / "INSTALLING_AND_LINKING.md", "INSTALLING_AND_LINKING.md"),
        (ROOT / "docs" / "KNOWN_LIMITATIONS_1_0_0_RC1.md", "KNOWN_LIMITATIONS_1_0_0_RC1.md"),
    )
    for source, name in documents:
        shutil.copy2(source, destination / name)


def build_core_package() -> Path:
    stage = DIST / "staging_core"
    shutil.rmtree(stage, ignore_errors=True)
    (stage / "include").mkdir(parents=True)
    shutil.copytree(RELEASE / "core" / "include", stage / "include", dirs_exist_ok=True)
    shutil.copytree(RELEASE / "core" / "lib", stage / "lib")
    (stage / "examples").mkdir()
    shutil.copy2(ROOT / "examples" / "external_consumer.c", stage / "examples" / "external_consumer.c")
    copy_documentation(stage / "docs")
    (stage / "README_FIRST.txt").write_text(
        "miniSNN Core is a headless neural library.\n"
        "Compile examples/external_consumer.c with -Iinclude and lib/libminisnn_core.a.\n"
        "The public API is a release candidate and remains provisional until D2.\n"
        "No license file is currently present in the repository; public distribution requires a licensing decision.\n",
        encoding="utf-8",
    )
    write_metadata(stage, "minisnn-core-dev", [])
    output = DIST / core_package_name()
    write_zip(stage, output)
    return output


def build_studio_package() -> Path:
    stage = DIST / "staging_studio"
    shutil.rmtree(stage, ignore_errors=True)
    (stage / "bin").mkdir(parents=True)
    executables = (
        RELEASE / "studio" / "bin" / "minisnn_studio.exe",
        RELEASE / "tools" / "bin" / "minisnn_runner.exe",
        RELEASE / "tools" / "bin" / "evolution_runner.exe",
    )
    dependencies = copy_runtime_executables(executables, stage / "bin")
    copy_studio_runtime_resources(stage)
    (stage / "Abrir miniSNN Studio.cmd").write_text(
        "@echo off\r\nsetlocal\r\ncd /d \"%~dp0\"\r\n"
        "start \"\" \"%~dp0bin\\minisnn_studio.exe\" --runtime-root \"%~dp0\"\r\n",
        encoding="ascii",
        newline="",
    )
    (stage / "README_FIRST.txt").write_text(
        "miniSNN Studio is the Win32 frontend powered by miniSNN Core.\n"
        "Open Abrir miniSNN Studio.cmd. This package does not compile source code.\n"
        "Python with pandas and matplotlib remains optional for plots and HTML reports;\n"
        "basic scenario execution and neuroevolution do not require Python.\n"
        "The Core API is a release candidate and remains provisional until D2.\n"
        "No license file is currently present in the repository; public distribution requires a licensing decision.\n",
        encoding="utf-8",
    )
    write_metadata(stage, "minisnn-studio", dependencies)
    output = DIST / studio_package_name()
    write_zip(stage, output)
    return output


def main() -> int:
    if not (RELEASE / "core" / "lib" / "libminisnn_core.a").is_file() or not (RELEASE / "studio" / "bin" / "minisnn_studio.exe").is_file():
        print("Release package build FAILED: execute release-all first")
        return 1
    DIST.mkdir(parents=True, exist_ok=True)
    core = build_core_package()
    studio = build_studio_package()
    sums = [f"{sha256(path)}  {path.name}" for path in sorted((core, studio), key=lambda item: item.name)]
    (DIST / "SHA256SUMS.txt").write_text("\n".join(sums) + "\n", encoding="ascii")
    print(f"Release packages generated: {core.name}, {studio.name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
