"""Shared immutable release-candidate identity helpers for D1-C scripts."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
VERSION_HEADER = ROOT / "include" / "minisnn_version.h"
ARCHITECTURE = "windows-x86_64"


@dataclass(frozen=True)
class ReleaseVersion:
    major: int
    minor: int
    patch: int
    prerelease: str
    string: str


def _macro_value(content: str, name: str) -> str:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+(.+?)\s*$", content, re.MULTILINE)
    if match is None:
        raise ValueError(f"{name} ausente em minisnn_version.h")
    return match.group(1).strip()


def _uint32_macro(value: str, name: str) -> int:
    match = re.fullmatch(r"UINT32_C\((\d+)\)", value)
    if match is None:
        raise ValueError(f"{name} deve usar UINT32_C(numero)")
    return int(match.group(1))


def _quoted(value: str, name: str) -> str:
    match = re.fullmatch(r'"([^"]*)"', value)
    if match is None:
        raise ValueError(f"{name} deve ser uma string C")
    return match.group(1)


def release_version() -> ReleaseVersion:
    content = VERSION_HEADER.read_text(encoding="utf-8")
    version = ReleaseVersion(
        major=_uint32_macro(_macro_value(content, "MINISNN_VERSION_MAJOR"), "MINISNN_VERSION_MAJOR"),
        minor=_uint32_macro(_macro_value(content, "MINISNN_VERSION_MINOR"), "MINISNN_VERSION_MINOR"),
        patch=_uint32_macro(_macro_value(content, "MINISNN_VERSION_PATCH"), "MINISNN_VERSION_PATCH"),
        prerelease=_quoted(_macro_value(content, "MINISNN_VERSION_PRERELEASE"), "MINISNN_VERSION_PRERELEASE"),
        string=_quoted(_macro_value(content, "MINISNN_VERSION_STRING"), "MINISNN_VERSION_STRING"),
    )
    expected = f"{version.major}.{version.minor}.{version.patch}"
    if version.prerelease:
        expected += f"-{version.prerelease}"
    if version.string != expected:
        raise ValueError("MINISNN_VERSION_STRING diverge dos componentes de versao")
    return version


def version_string() -> str:
    return release_version().string


def core_package_name() -> str:
    return f"minisnn-core-{version_string()}-{ARCHITECTURE}-dev.zip"


def studio_package_name() -> str:
    return f"minisnn-studio-{version_string()}-{ARCHITECTURE}.zip"


def package_names() -> tuple[str, str]:
    return core_package_name(), studio_package_name()
