from __future__ import annotations

from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import check_release_packages as release  # noqa: E402
from d1_c_release_common import core_package_name, package_names, studio_package_name  # noqa: E402


def expect_rejected(action, description: str) -> None:
    try:
        action()
    except release.ReleaseIntegrityError:
        return
    raise AssertionError(f"integrity gate accepted {description}")


def write_zip(path: Path, members: dict[str, bytes]) -> None:
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, content in members.items():
            archive.writestr(name, content)


def write_raw_zip_member(path: Path, name: str) -> None:
    safe_name = name.replace("\\", "/")
    write_zip(path, {safe_name: b"x"})
    encoded_safe = safe_name.encode("ascii")
    encoded_raw = name.encode("ascii")
    if len(encoded_safe) != len(encoded_raw):
        raise AssertionError("fixture ZIP requer nomes de mesmo tamanho")
    path.write_bytes(path.read_bytes().replace(encoded_safe, encoded_raw))


def write_outer(dist: Path) -> None:
    lines = [f"{release.sha256(dist / name)}  {name}" for name in sorted(package_names())]
    (dist / "SHA256SUMS.txt").write_text("\n".join(lines) + "\n", encoding="ascii")


def make_stage(directory: Path) -> None:
    (directory / "docs").mkdir(parents=True)
    (directory / "README.txt").write_text("release candidate\n", encoding="utf-8")
    (directory / "docs" / "guide.txt").write_text("guide\n", encoding="utf-8")
    files = release._distributed_files(directory)
    checksums = {name: release.sha256(path) for name, path in files.items()}
    (directory / "SHA256SUMS.txt").write_text(
        "\n".join(f"{checksums[name]}  {name}" for name in sorted(checksums)) + "\n",
        encoding="ascii",
    )
    manifest = [
        "manifest_format=minisnn_release_package_v1",
        "product=minisnn-core-dev",
        f"version={release.version_string()}",
        f"architecture={release.ARCHITECTURE}",
        "checksum_scope=all distributed content except generated MANIFEST.txt and SHA256SUMS.txt",
    ]
    manifest.extend(
        f"file={name};size={files[name].stat().st_size};sha256={checksums[name]}"
        for name in sorted(files)
    )
    (directory / "MANIFEST.txt").write_text("\n".join(manifest) + "\n", encoding="utf-8")


def copied_stage(source: Path, destination: Path) -> Path:
    shutil.copytree(source, destination)
    return destination


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="minisnn_release_integrity_") as temporary_name:
        temporary = Path(temporary_name)
        dist = temporary / "dist"
        dist.mkdir()
        for name in package_names():
            write_zip(dist / name, {"payload.txt": b"payload"})
        write_outer(dist)
        release.validate_external_checksums(dist)

        altered = dist / core_package_name()
        altered.write_bytes(altered.read_bytes() + b"append")
        expect_rejected(lambda: release.validate_external_checksums(dist), "ZIP with appended bytes")
        write_zip(altered, {"payload.txt": b"payload"})
        write_outer(dist)
        changed = bytearray(altered.read_bytes())
        changed[0] ^= 1
        altered.write_bytes(bytes(changed))
        expect_rejected(lambda: release.validate_external_checksums(dist), "ZIP with one changed byte")
        write_zip(altered, {"payload.txt": b"payload"})
        write_outer(dist)
        sums = dist / "SHA256SUMS.txt"
        sums.write_text(sums.read_text(encoding="ascii").splitlines()[0] + "\n", encoding="ascii")
        expect_rejected(lambda: release.validate_external_checksums(dist), "missing external checksum entry")
        write_outer(dist)
        sums = dist / "SHA256SUMS.txt"
        sums.write_text("0" * 64 + f"  {core_package_name()}\n" + sums.read_text(encoding="ascii"), encoding="ascii")
        expect_rejected(lambda: release.validate_external_checksums(dist), "duplicate or fake external checksum")
        write_outer(dist)
        sums.write_text(sums.read_text(encoding="ascii") + "0" * 64 + "  unexpected.zip\n", encoding="ascii")
        expect_rejected(lambda: release.validate_external_checksums(dist), "unknown external checksum entry")

        for malicious_name in ("../escape.txt", "/absolute.txt", "//server/share.txt", "C:/drive.txt", "nested\\file.txt"):
            archive = temporary / "malicious.zip"
            if "\\" in malicious_name:
                write_raw_zip_member(archive, malicious_name)
            else:
                write_zip(archive, {malicious_name: b"x"})
            expect_rejected(lambda archive=archive: release.safe_extract_package(archive, temporary / "out"), malicious_name)
        collision = temporary / "collision.zip"
        write_zip(collision, {"nested/file.txt": b"a", "nested//file.txt": b"b"})
        expect_rejected(lambda: release.safe_extract_package(collision, temporary / "collision_out"), "normalised ZIP collision")

        stage = temporary / "stage"
        make_stage(stage)
        release.validate_checksums(stage)
        release.validate_manifest(stage, "minisnn-core-dev")

        extra = copied_stage(stage, temporary / "extra")
        (extra / "unregistered.txt").write_text("x", encoding="utf-8")
        expect_rejected(lambda: release.validate_checksums(extra), "unregistered package file")
        missing = copied_stage(stage, temporary / "missing")
        (missing / "README.txt").unlink()
        expect_rejected(lambda: release.validate_checksums(missing), "checksum for missing package file")
        false_hash = copied_stage(stage, temporary / "false_hash")
        checksums = false_hash / "SHA256SUMS.txt"
        checksums.write_text("0" * 64 + "  README.txt\n" + "\n".join(checksums.read_text(encoding="ascii").splitlines()[1:]) + "\n", encoding="ascii")
        expect_rejected(lambda: release.validate_checksums(false_hash), "false internal checksum")
        false_size = copied_stage(stage, temporary / "false_size")
        manifest = false_size / "MANIFEST.txt"
        manifest.write_text(
            re.sub(r"file=README\.txt;size=\d+", "file=README.txt;size=999", manifest.read_text(encoding="utf-8")),
            encoding="utf-8",
        )
        expect_rejected(lambda: release.validate_manifest(false_size, "minisnn-core-dev"), "false manifest size")
        missing_manifest = copied_stage(stage, temporary / "missing_manifest")
        manifest = missing_manifest / "MANIFEST.txt"
        manifest.write_text("\n".join(line for line in manifest.read_text(encoding="utf-8").splitlines() if not line.startswith("file=README.txt;")) + "\n", encoding="utf-8")
        expect_rejected(lambda: release.validate_manifest(missing_manifest, "minisnn-core-dev"), "missing manifest file")
        extra_manifest = copied_stage(stage, temporary / "extra_manifest")
        manifest = extra_manifest / "MANIFEST.txt"
        manifest.write_text(manifest.read_text(encoding="utf-8") + "file=unknown.txt;size=1;sha256=" + "0" * 64 + "\n", encoding="utf-8")
        expect_rejected(lambda: release.validate_manifest(extra_manifest, "minisnn-core-dev"), "extra manifest file")

        studio = temporary / "studio"
        (studio / "bin").mkdir(parents=True)
        (studio / "bin" / "minisnn_studio.exe").write_bytes(b"MZfake")
        (studio / "Abrir miniSNN Studio.cmd").write_text("mingw32-make studio\n", encoding="ascii")
        expect_rejected(lambda: release.validate_studio_layout(studio), "incorrect Studio launcher")

        executable = temporary / "smoke.exe"
        executable.write_bytes(b"MZ")
        expect_rejected(
            lambda: release.run_studio_smoke(executable, lambda *args, **kwargs: subprocess.CompletedProcess(args[0], 1, "failed")),
            "failed Studio smoke",
        )
        def timeout_runner(*args, **kwargs):
            raise subprocess.TimeoutExpired(args[0], kwargs.get("timeout", 0))
        expect_rejected(lambda: release.run_studio_smoke(executable, timeout_runner), "timed out Studio smoke")
        expect_rejected(
            lambda: release.run_studio_runtime_smoke(
                executable,
                temporary,
                lambda *args, **kwargs: subprocess.CompletedProcess(args[0], 1, "failed"),
            ),
            "failed Studio runtime smoke",
        )
        expect_rejected(
            lambda: release.run_studio_runtime_smoke(executable, temporary, timeout_runner),
            "timed out Studio runtime smoke",
        )

    print("D1-C release integrity validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
