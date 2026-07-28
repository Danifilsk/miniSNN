from __future__ import annotations

import hashlib
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import tempfile
import zipfile

from d1_c_release_common import ARCHITECTURE, core_package_name, package_names, studio_package_name, version_string
from build_release_packages import STUDIO_RUNTIME_SCRIPTS


ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = ROOT.parent
DIST = REPOSITORY / "dist"
_CHECKSUM_LINE = re.compile(r"^([0-9a-fA-F]{64})  (.+)$")
_MANIFEST_FILE = re.compile(r"^file=([^;]+);size=(\d+);sha256=([0-9a-fA-F]{64})$")
_SIMPLE_ZIP = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*\.zip$")
_CHECKSUM_SCOPE = "all distributed content except generated MANIFEST.txt and SHA256SUMS.txt"


class ReleaseIntegrityError(ValueError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _normalised_member_name(name: str) -> str:
    if (not name or any(ord(character) < 0x20 or ord(character) == 0x7F for character in name) or
            "\\" in name or name.startswith("/") or name.startswith("//") or
            re.match(r"^[A-Za-z]:", name)):
        raise ReleaseIntegrityError(f"caminho ZIP inseguro: {name!r}")
    path = PurePosixPath(name)
    if path.is_absolute() or any(part in {"", ".", ".."} for part in path.parts):
        raise ReleaseIntegrityError(f"caminho ZIP inseguro: {name!r}")
    return path.as_posix()


def _validate_zip_members(archive: zipfile.ZipFile) -> list[zipfile.ZipInfo]:
    members: list[zipfile.ZipInfo] = []
    seen: set[str] = set()
    for info in archive.infolist():
        raw_name = getattr(info, "orig_filename", info.filename)
        name = _normalised_member_name(raw_name)
        if info.is_dir():
            raise ReleaseIntegrityError(f"diretorio ZIP nao permitido: {raw_name!r}")
        mode = (info.external_attr >> 16) & 0xFFFF
        file_type = stat.S_IFMT(mode)
        if file_type and file_type != stat.S_IFREG:
            raise ReleaseIntegrityError(f"tipo especial ZIP nao permitido: {raw_name!r}")
        if name in seen:
            raise ReleaseIntegrityError(f"membro ZIP duplicado: {name}")
        seen.add(name)
        members.append(info)
    return members


def safe_extract_package(archive_path: Path, destination: Path) -> None:
    try:
        with zipfile.ZipFile(archive_path) as archive:
            members = _validate_zip_members(archive)
            root = destination.resolve()
            for info in members:
                name = _normalised_member_name(getattr(info, "orig_filename", info.filename))
                output = destination.joinpath(*PurePosixPath(name).parts)
                resolved = output.resolve()
                if root != resolved and root not in resolved.parents:
                    raise ReleaseIntegrityError(f"membro ZIP fora da raiz: {name}")
                output.parent.mkdir(parents=True, exist_ok=True)
                with archive.open(info, "r") as source, output.open("wb") as target:
                    shutil.copyfileobj(source, target)
    except zipfile.BadZipFile as error:
        raise ReleaseIntegrityError(f"ZIP invalido: {archive_path.name}") from error


def _parse_checksums(path: Path, *, simple_zip_names: bool = False) -> dict[str, str]:
    if not path.is_file():
        raise ReleaseIntegrityError(f"checksum ausente: {path.name}")
    values: dict[str, str] = {}
    for line_number, line in enumerate(path.read_text(encoding="ascii").splitlines(), start=1):
        match = _CHECKSUM_LINE.fullmatch(line)
        if match is None:
            raise ReleaseIntegrityError(f"linha de checksum malformada em {path.name}:{line_number}")
        checksum, raw_name = match.groups()
        if simple_zip_names:
            if not _SIMPLE_ZIP.fullmatch(raw_name):
                raise ReleaseIntegrityError(f"nome externo invalido: {raw_name!r}")
            name = raw_name
        else:
            name = _normalised_member_name(raw_name)
        if name in values:
            raise ReleaseIntegrityError(f"entrada de checksum duplicada: {name}")
        values[name] = checksum.lower()
    return values


def validate_external_checksums(dist: Path = DIST) -> None:
    expected_names = set(package_names())
    values = _parse_checksums(dist / "SHA256SUMS.txt", simple_zip_names=True)
    if set(values) != expected_names:
        missing = sorted(expected_names - set(values))
        extra = sorted(set(values) - expected_names)
        raise ReleaseIntegrityError(f"checksums externos nao correspondem aos ZIPs esperados; ausentes={missing}; extras={extra}")
    actual_zip_names = {path.name for path in dist.glob("*.zip") if path.is_file()}
    if actual_zip_names != expected_names:
        raise ReleaseIntegrityError(f"arquivos ZIP externos inesperados; encontrados={sorted(actual_zip_names)}")
    for name in sorted(expected_names):
        package = dist / name
        if not package.is_file():
            raise ReleaseIntegrityError(f"ZIP externo ausente: {name}")
        actual = sha256(package)
        if actual != values[name]:
            raise ReleaseIntegrityError(f"checksum externo divergente: {name}")


def _distributed_files(directory: Path) -> dict[str, Path]:
    files: dict[str, Path] = {}
    for path in directory.rglob("*"):
        if not path.is_file() or path.name in {"MANIFEST.txt", "SHA256SUMS.txt"}:
            continue
        name = _normalised_member_name(path.relative_to(directory).as_posix())
        if name in files:
            raise ReleaseIntegrityError(f"arquivo distribuido duplicado: {name}")
        files[name] = path
    return files


def validate_checksums(directory: Path) -> None:
    distributed = _distributed_files(directory)
    checksums = _parse_checksums(directory / "SHA256SUMS.txt")
    if set(checksums) != set(distributed):
        missing = sorted(set(distributed) - set(checksums))
        extra = sorted(set(checksums) - set(distributed))
        raise ReleaseIntegrityError(f"cobertura de checksum interno inexata; ausentes={missing}; extras={extra}")
    for name, path in distributed.items():
        if sha256(path) != checksums[name]:
            raise ReleaseIntegrityError(f"checksum interno divergente: {name}")


def _parse_manifest(path: Path) -> tuple[dict[str, str], dict[str, tuple[int, str]]]:
    if not path.is_file():
        raise ReleaseIntegrityError("MANIFEST.txt ausente")
    headers: dict[str, str] = {}
    files: dict[str, tuple[int, str]] = {}
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        if line.startswith("file="):
            match = _MANIFEST_FILE.fullmatch(line)
            if match is None:
                raise ReleaseIntegrityError(f"linha file malformada em MANIFEST.txt:{line_number}")
            raw_name, raw_size, checksum = match.groups()
            name = _normalised_member_name(raw_name)
            if name in files:
                raise ReleaseIntegrityError(f"arquivo duplicado no manifesto: {name}")
            files[name] = (int(raw_size), checksum.lower())
            continue
        match = re.fullmatch(r"([a-z_]+)=(.+)", line)
        if match is None:
            raise ReleaseIntegrityError(f"linha de manifesto malformada: {line!r}")
        key, value = match.groups()
        if key in headers:
            raise ReleaseIntegrityError(f"campo duplicado no manifesto: {key}")
        headers[key] = value
    return headers, files


def validate_manifest(directory: Path, product: str) -> None:
    headers, manifest_files = _parse_manifest(directory / "MANIFEST.txt")
    expected_headers = {
        "manifest_format": "minisnn_release_package_v1",
        "product": product,
        "version": version_string(),
        "architecture": ARCHITECTURE,
        "checksum_scope": _CHECKSUM_SCOPE,
    }
    if headers != expected_headers:
        raise ReleaseIntegrityError("cabecalho do manifesto divergente")
    distributed = _distributed_files(directory)
    checksums = _parse_checksums(directory / "SHA256SUMS.txt")
    if set(manifest_files) != set(distributed):
        raise ReleaseIntegrityError("cobertura de arquivos do manifesto inexata")
    if set(manifest_files) != set(checksums):
        raise ReleaseIntegrityError("manifesto e SHA256SUMS.txt cobrem arquivos diferentes")
    for name, path in distributed.items():
        size, expected_hash = manifest_files[name]
        actual_hash = sha256(path)
        if path.stat().st_size != size:
            raise ReleaseIntegrityError(f"tamanho do manifesto divergente: {name}")
        if actual_hash != expected_hash or checksums[name] != expected_hash:
            raise ReleaseIntegrityError(f"hash do manifesto divergente: {name}")


def validate_core_layout(core_dir: Path) -> None:
    if (core_dir / "bin").exists() or (core_dir / "tests").exists() or (core_dir / "src").exists():
        raise ReleaseIntegrityError("pacote Core contem produto interno")


def validate_studio_layout(studio_dir: Path) -> Path:
    launcher_path = studio_dir / "Abrir miniSNN Studio.cmd"
    if not launcher_path.is_file():
        raise ReleaseIntegrityError("launcher do Studio ausente")
    launcher = launcher_path.read_text(encoding="ascii")
    if ("mingw32-make" in launcher or "%~dp0bin\\minisnn_studio.exe" not in launcher or
            "--runtime-root \"%~dp0\"" not in launcher):
        raise ReleaseIntegrityError("launcher do pacote nao e relativo")
    executable = studio_dir / "bin" / "minisnn_studio.exe"
    required_executables = (
        executable,
        studio_dir / "bin" / "minisnn_runner.exe",
        studio_dir / "bin" / "evolution_runner.exe",
    )
    required_configs = (
        studio_dir / "configs" / "evolution_weight_target_demo.ini",
        studio_dir / "configs" / "evolution_weight_target_base.ini",
    )
    required_scripts = tuple(studio_dir / "scripts" / name for name in STUDIO_RUNTIME_SCRIPTS)
    if (not all(path.is_file() for path in required_executables) or
            not all(path.is_file() for path in required_configs) or
            not all(path.is_file() for path in required_scripts) or
            not (studio_dir / "results").is_dir() or
            (studio_dir / "lib").exists() or (studio_dir / "tests").exists() or
            (studio_dir / "src").exists() or (studio_dir / "include").exists()):
        raise ReleaseIntegrityError("conteudo do Studio invalido")
    if executable.read_bytes()[:2] != b"MZ":
        raise ReleaseIntegrityError("executavel do Studio nao possui cabecalho PE")
    return executable


def run_studio_smoke(executable: Path, runner=subprocess.run) -> None:
    try:
        completed = runner(
            [str(executable), "--smoke-test"],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=10,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        raise ReleaseIntegrityError("smoke do Studio excedeu o timeout") from error
    if completed.returncode != 0:
        raise ReleaseIntegrityError("smoke do Studio retornou falha")
    expected = f"miniSNN Studio smoke test OK: {version_string()}"
    if expected not in (completed.stdout or ""):
        raise ReleaseIntegrityError("smoke do Studio nao confirmou a versao canonica")


def run_studio_runtime_smoke(
    executable: Path,
    runtime_root: Path,
    runner=subprocess.run,
) -> None:
    try:
        completed = runner(
            [str(executable), "--runtime-smoke-test", "--runtime-root", str(runtime_root)],
            cwd=runtime_root,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=10,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        raise ReleaseIntegrityError("runtime smoke do Studio excedeu o timeout") from error
    if completed.returncode != 0:
        raise ReleaseIntegrityError("runtime smoke do Studio retornou falha")
    output = completed.stdout or ""
    if ("miniSNN Studio runtime smoke OK" not in output or "mode=package" not in output or
            f"version={version_string()}" not in output):
        raise ReleaseIntegrityError("runtime smoke do Studio nao confirmou o pacote")


def validate_core_consumer(core_dir: Path) -> None:
    executable = core_dir / "external_consumer.exe"
    compiled = subprocess.run(
        ["gcc", "-std=c11", "-Wall", "-Wextra", "-pedantic", "examples/external_consumer.c", "-I", "include", "lib/libminisnn_core.a", "-o", str(executable)],
        cwd=core_dir,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if compiled.returncode != 0:
        raise ReleaseIntegrityError("consumidor externo nao compila")
    executed = subprocess.run([str(executable)], cwd=core_dir, text=True, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, check=False)
    if executed.returncode != 0 or version_string() not in executed.stdout:
        raise ReleaseIntegrityError("consumidor externo nao executa")


def main() -> int:
    try:
        validate_external_checksums()
        with tempfile.TemporaryDirectory(prefix="minisnn_package_") as temporary_name:
            temporary = Path(temporary_name)
            core_dir = temporary / "core"
            studio_dir = temporary / "studio"
            safe_extract_package(DIST / core_package_name(), core_dir)
            safe_extract_package(DIST / studio_package_name(), studio_dir)
            validate_checksums(core_dir)
            validate_checksums(studio_dir)
            validate_manifest(core_dir, "minisnn-core-dev")
            validate_manifest(studio_dir, "minisnn-studio")
            validate_core_layout(core_dir)
            validate_core_consumer(core_dir)
            studio_executable = validate_studio_layout(studio_dir)
            if __import__("os").name == "nt":
                run_studio_runtime_smoke(studio_executable, studio_dir)
            else:
                print("Studio process smoke UNAVAILABLE: host non-Windows")
        print("Release package smoke validation OK")
        return 0
    except (OSError, ReleaseIntegrityError) as error:
        print(f"Release package validation FAILED: {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
