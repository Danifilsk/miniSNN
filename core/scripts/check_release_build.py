from __future__ import annotations

from pathlib import Path
import subprocess

from d1_c_release_common import version_string


ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = ROOT.parent
RELEASE = REPOSITORY / "build" / "release"


def fail(message: str) -> int:
    print(f"Release build validation FAILED: {message}")
    return 1


def main() -> int:
    required = (
        RELEASE / "core" / "lib" / "libminisnn_core.a",
        RELEASE / "core" / "include" / "minisnn.h",
        RELEASE / "core" / "include" / "minisnn_version.h",
        RELEASE / "tools" / "bin" / "minisnn_runner.exe",
        RELEASE / "studio" / "bin" / "minisnn_studio.exe",
    )
    for path in required:
        if not path.is_file():
            return fail(f"produto ausente: {path.relative_to(REPOSITORY)}")
    if any((RELEASE / "tests").rglob("*") if (RELEASE / "tests").exists() else []):
        return fail("testes foram incluidos no release")
    archive = RELEASE / "core" / "lib" / "libminisnn_core.a"
    nm = subprocess.run(["nm", str(archive)], text=True, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, check=False)
    if nm.returncode != 0 or "minisnn_test_" in nm.stdout or "minisnn_studio" in nm.stdout:
        return fail("biblioteca release contem simbolo de teste ou Studio")
    version = (RELEASE / "core" / "include" / "minisnn_version.h").read_text(encoding="utf-8")
    if f'MINISNN_VERSION_STRING "{version_string()}"' not in version:
        return fail("header release contem versao divergente")
    print("Release build validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
