from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import re

from d1_c_release_common import version_string


ROOT = Path(__file__).resolve().parents[1]
INCLUDE = ROOT / "include"
BASELINE = ROOT / "docs" / "PUBLIC_API_BASELINE_1_0_RC.txt"
MANIFEST = ROOT / "docs" / "PUBLIC_API_MANIFEST.md"

_TOKEN = re.compile(
    r'''\.\.\.|(?:0[xX][0-9A-Fa-f]+)|(?:\d+(?:\.\d*)?(?:[eE][+-]?\d+)?)|(?:[A-Za-z_]\w*)|(?:"(?:\\.|[^"\\])*")|(?:'(?:\\.|[^'\\])*')|(?:==|!=|<=|>=|->|&&|\|\||<<|>>|\+\+|--)|[{}()\[\];,.*&=+\-/%!<>?:|~^]'''
)


def digest(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def _strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", text)


def _strip_testing_blocks(text: str) -> str:
    output: list[str] = []
    depth = 0
    for line in text.splitlines(keepends=True):
        directive = line.strip()
        starts_testing = bool(re.match(r"#\s*(?:ifdef\s+MINISNN_TESTING|if\b.*\bMINISNN_TESTING\b)", directive))
        if depth:
            if re.match(r"#\s*(?:if|ifdef|ifndef)\b", directive):
                depth += 1
            elif re.match(r"#\s*endif\b", directive):
                depth -= 1
            continue
        if starts_testing:
            depth = 1
            continue
        output.append(line)
    if depth:
        raise ValueError("bloco MINISNN_TESTING sem #endif")
    return "".join(output)


def _include_guard(text: str) -> str | None:
    lines = text.splitlines()
    for index, line in enumerate(lines):
        match = re.fullmatch(r"\s*#\s*ifndef\s+([A-Za-z_]\w*)\s*", line)
        if match is None:
            continue
        for next_line in lines[index + 1:]:
            if not next_line.strip():
                continue
            define = re.fullmatch(r"\s*#\s*define\s+([A-Za-z_]\w*)\s*", next_line)
            return match.group(1) if define is not None and define.group(1) == match.group(1) else None
    return None


def _normalise_tokens(text: str) -> str:
    tokens = list(_TOKEN.finditer(text))
    cursor = 0
    for token in tokens:
        if text[cursor:token.start()].strip():
            raise ValueError(f"token publico nao reconhecido: {text[cursor:token.start()]!r}")
        cursor = token.end()
    if text[cursor:].strip():
        raise ValueError(f"token publico nao reconhecido: {text[cursor:]!r}")
    return " ".join(token.group(0) for token in tokens)


def _public_macros(text: str, guard: str | None) -> list[str]:
    logical_lines: list[str] = []
    pending = ""
    for raw_line in text.splitlines():
        line = pending + raw_line
        if line.rstrip().endswith("\\"):
            pending = line.rstrip()[:-1] + " "
            continue
        pending = ""
        logical_lines.append(line)
    if pending:
        raise ValueError("macro publico com continuacao incompleta")

    macros: list[str] = []
    for line in logical_lines:
        match = re.match(r"\s*#\s*define\s+([A-Za-z_]\w*)(.*)$", line)
        if match is None:
            continue
        name, value = match.groups()
        if not name.startswith("MINISNN_") or name == guard:
            continue
        macros.append(f"macro={_normalise_tokens(f'{name}{value}')}")
    return macros


def _declarations(text: str) -> list[str]:
    source = "\n".join(
        "" if line.lstrip().startswith("#") else line
        for line in text.splitlines()
    )
    declarations: list[str] = []
    current: list[str] = []
    brace_depth = 0
    for character in source:
        current.append(character)
        if character == "{":
            brace_depth += 1
        elif character == "}":
            brace_depth -= 1
            if brace_depth < 0:
                raise ValueError("chave publica sem abertura")
        elif character == ";" and brace_depth == 0:
            declaration = "".join(current).strip()
            current = []
            if declaration:
                declarations.append(_normalise_tokens(declaration))
    remainder = "".join(current).strip()
    if remainder:
        raise ValueError(f"declaracao publica sem ponto e virgula: {remainder[:48]!r}")
    if brace_depth:
        raise ValueError("chave publica sem fechamento")
    return declarations


def semantic_surface_from_text(text: str, header_name: str) -> list[str]:
    """Return the complete semantic public declarations for one header."""
    stripped = _strip_testing_blocks(_strip_comments(text.replace("\r\n", "\n").replace("\r", "\n")))
    guard = _include_guard(stripped)
    surface = _public_macros(stripped, guard)
    for declaration in _declarations(stripped):
        if declaration.startswith("typedef ") or declaration.startswith("struct ") or declaration.startswith("enum "):
            surface.append(f"type={declaration}")
        elif re.search(r"\bminisnn_[A-Za-z0-9_]*\s*\(", declaration):
            surface.append(f"function={declaration}")
        else:
            surface.append(f"declaration={declaration}")
    return [f"surface={header_name};{item}" for item in sorted(surface)]


def semantic_surface_from_headers(headers: list[Path]) -> list[str]:
    surface: list[str] = []
    for header in sorted(headers, key=lambda item: item.name):
        surface.extend(semantic_surface_from_text(header.read_text(encoding="utf-8"), header.name))
    return surface


def _canonical_document(text: str) -> str:
    return "\n".join(" ".join(line.split()) for line in _strip_comments(text).splitlines() if line.strip()) + "\n"


def build_baseline() -> str:
    headers = sorted(INCLUDE.glob("*.h"), key=lambda item: item.name)
    header_surfaces = {
        header.name: semantic_surface_from_text(header.read_text(encoding="utf-8"), header.name)
        for header in headers
    }
    canonical_surface = "\n".join(
        item for header in headers for item in header_surfaces[header.name]
    ) + "\n"
    manifest_hash = digest(_canonical_document(MANIFEST.read_text(encoding="utf-8")))
    lines = [
        "miniSNN Core public API baseline",
        "baseline_format=minisnn_public_api_v2",
        f"version={version_string()}",
        "stability=CANDIDATE_V1_PROVISIONAL_UNTIL_D2",
        f"semantic_surface_sha256={digest(canonical_surface)}",
        f"manifest_sha256={manifest_hash}",
        "classification=CANDIDATE_V1: minisnn.h minisnn_types.h minisnn_version.h minisnn_agent_io.h minisnn_sensor_encoder.h minisnn_action_decoder.h minisnn_agent_cycle.h",
        "classification=LEGACY_SUPPORTED: minisnn_evolution_legacy.h",
        "classification=TEST_ONLY: excluded MINISNN_TESTING declarations",
        "classification=INTERNAL: excluded src app studio tests include guards includes comments formatting",
    ]
    for header in headers:
        entries = header_surfaces[header.name]
        lines.append(f"header={header.name};semantic_sha256={digest(chr(10).join(entries) + chr(10))}")
        lines.extend(entries)
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="Valida o baseline publico candidato.")
    parser.add_argument("--write", action="store_true", help="Atualiza o baseline versionado.")
    args = parser.parse_args()
    try:
        expected = build_baseline()
    except (OSError, ValueError) as error:
        print(f"Public API baseline validation FAILED: {error}")
        return 1
    if args.write:
        BASELINE.write_text(expected, encoding="utf-8", newline="\n")
        print("Public API baseline written")
        return 0
    if not BASELINE.is_file():
        print("Public API baseline validation FAILED: baseline ausente")
        return 1
    actual = BASELINE.read_text(encoding="utf-8")
    if actual != expected:
        print("Public API baseline validation FAILED: superficie publica ou manifesto alterado")
        return 1
    if any(line.startswith("surface=") and "MINISNN_TESTING" in line for line in actual.splitlines()):
        print("Public API baseline validation FAILED: simbolo TEST_ONLY exposto")
        return 1
    print("Public API baseline validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
