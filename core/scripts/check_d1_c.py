from __future__ import annotations

import argparse
from pathlib import Path
import re

from d1_c_release_common import core_package_name, studio_package_name, version_string


ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = ROOT.parent
def fail(message: str) -> int:
    print(f"D1-C release-candidate validation FAILED: {message}")
    return 1


def require(path: Path) -> str | None:
    if not path.is_file():
        return f"arquivo obrigatorio ausente: {path.relative_to(REPOSITORY)}"
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description="Valida os artefatos D1-C e o fechamento D1.")
    parser.add_argument("--automated", action="store_true",
                        help="executa somente a parte automatica do gate")
    parser.add_argument("--d1", action="store_true",
                        help="valida o fechamento integrado D1 apos os gates D1-A/B/C")
    args = parser.parse_args()
    if args.automated and args.d1:
        return fail("--automated e --d1 nao podem ser usados juntos")

    required = (
        ROOT / "include" / "minisnn_version.h",
        ROOT / "src" / "minisnn_version.c",
        ROOT / "examples" / "external_consumer.c",
        ROOT / "docs" / "PUBLIC_API_BASELINE_1_0_RC.txt",
        ROOT / "docs" / "CORE_BRIDGE_API_CANDIDATE.md",
        ROOT / "docs" / "INSTALLING_AND_LINKING.md",
        ROOT / "docs" / "KNOWN_LIMITATIONS_1_0_0_RC1.md",
        ROOT / "docs" / "RELEASE_NOTES_1_0_0_RC1.md",
        ROOT / "docs" / "RELEASE_CHECKLIST_1_0_RC.md",
        ROOT / "docs" / "D1_C_RELEASE_CANDIDATE_AUDIT.md",
        ROOT / "docs" / "D1_C_STUDIO_MANUAL_CHECKLIST.md",
        REPOSITORY / "dist" / core_package_name(),
        REPOSITORY / "dist" / studio_package_name(),
        REPOSITORY / "dist" / "SHA256SUMS.txt",
    )
    for path in required:
        error = require(path)
        if error is not None:
            return fail(error)

    roadmap = (ROOT / "docs" / "ROADMAP.md").read_text(encoding="utf-8")
    for token in (
        "D1-A — concluído",
        "D1-B — concluído",
        "D1-C — concluído",
        "D1 — concluído",
        "miniSNN Core 1.0.0-rc.1",
        "K0 — fundação determinística do Worlds Kernel",
        "D2",
        "C8",
        "Bloco E permanece pausado",
        "Pesquisa neural futura permanece uma trilha separada",
    ):
        if token not in roadmap:
            return fail(f"roadmap sem estado esperado: {token}")
    if "READY_FOR_MANUAL_VALIDATION" in roadmap or "D1-C manual - pendente" in roadmap:
        return fail("roadmap ainda declara validacao manual pendente")
    if "D1 — congelamento definitivo do Core" in roadmap:
        return fail("roadmap atribui congelamento definitivo a D1")

    documented = (
        ROOT / "docs" / "D1_C_RELEASE_CANDIDATE_AUDIT.md",
        ROOT / "docs" / "RELEASE_CHECKLIST_1_0_RC.md",
        ROOT / "docs" / "KNOWN_LIMITATIONS_1_0_0_RC1.md",
        ROOT / "docs" / "PUBLIC_API_MANIFEST.md",
        ROOT / "docs" / "COMPATIBILIDADE.md",
    )
    version_pattern = re.compile(r"\b\d+\.\d+\.\d+(?:-[A-Za-z0-9.]+)?\b")
    for document in documented:
        content = document.read_text(encoding="utf-8")
        if version_string() not in content:
            return fail(f"documento sem versao canonica: {document.name}")
        found = set(version_pattern.findall(content))
        if found - {version_string()}:
            return fail(f"documento com versao tecnica divergente: {document.name}")

    checklist = (ROOT / "docs" / "D1_C_STUDIO_MANUAL_CHECKLIST.md").read_text(
        encoding="utf-8"
    )
    manual_tokens = (
        "Validator: project user",
        "System: not recorded  Date: not recorded",
        "Version: `miniSNN Core 1.0.0-rc.1`  Result: PASS",
        "[x] Double-clicking `Abrir miniSNN Studio.cmd` opens the GUI.",
        "[x] The repository launcher opens the debug Studio.",
        "[x] The debug Studio opens directly from `build/studio/bin/`.",
        "[x] The release Studio opens directly from `build/release/studio/bin/`.",
        "[x] The extracted Studio package opens without a compiler.",
        "[x] The title identifies miniSNN Studio and miniSNN Core 1.0.0-rc.1.",
        "[x] Historical panels appear exactly once.",
        "[x] A normal scenario loads and executes.",
        "[ N/A ] Pause during execution -- it does not exist in the current batch Studio.",
        "[ N/A ] Speed control -- it does not exist in the current batch Studio.",
        "[ N/A ] Live neural one-step -- it does not exist in the current batch Studio.",
        "[x] Supported save/load works.",
        "[x] Existing reports open.",
        "[x] `NEUROEVOLUCAO` opens without a configuration error.",
        "[x] The default `evolution_weight_target_demo.ini` and its base scenario load.",
        "[x] Choosing, cancelling, and saving evolution configurations preserve working paths.",
        "[x] A short evolution executes.",
        "[x] No duplicate panel or evident visual regression is observed.",
        "[x] Closing and reopening works.",
    )
    if not args.automated:
        for token in manual_tokens:
            if token not in checklist:
                return fail(f"checklist manual sem validacao registrada: {token}")
        if "[ ]" in checklist:
            return fail("checklist manual contem item pendente")

        audit = (ROOT / "docs" / "D1_C_RELEASE_CANDIDATE_AUDIT.md").read_text(
            encoding="utf-8"
        )
        for token in (
            "D1-C automated validation: PASS",
            "D1-C manual Studio validation: PASS",
            "D1-C: CONCLUÍDO",
        ):
            if token not in audit:
                return fail(f"auditoria D1-C sem fechamento registrado: {token}")

        release_checklist = (ROOT / "docs" / "RELEASE_CHECKLIST_1_0_RC.md").read_text(
            encoding="utf-8"
        )
        if "| Studio visual checklist | PASS |" not in release_checklist:
            return fail("release checklist nao registra o PASS visual do Studio")
        if "D1: CONCLUÍDO" not in release_checklist:
            return fail("release checklist nao declara o fechamento D1")

    if args.automated:
        print("D1-C automated release-candidate validation OK")
        return 0

    if args.d1:
        print("D1 pre-Worlds audit and stabilization validation OK")
        print("miniSNN Core 1.0.0-rc.1 ready")
        return 0

    print("D1-C automated validation: PASS")
    print("D1-C manual Studio validation: PASS")
    print("D1-C: CONCLUÍDO")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
