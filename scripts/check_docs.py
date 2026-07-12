from __future__ import annotations

from pathlib import Path
import argparse
import re
import sys
from urllib.parse import unquote


MARKDOWN_LINK = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
LOCAL_ABSOLUTE = re.compile(r"(?i)(?:[a-z]:[\\/]users[\\/]|file://)")
PROJECT_REFERENCE = re.compile(
    r"(?<![A-Za-z0-9_])"
    r"((?:app|configs|docs|examples|experiments|include|scripts|src|tests)/"
    r"[A-Za-z0-9_.\-/]+\.(?:c|h|ini|md|py))(?![A-Za-z0-9_])"
)
MAKE_COMMAND = re.compile(r"mingw32-make(?:\.exe)?\s+([A-Za-z0-9_.-]+)")
MAKE_TARGET = re.compile(r"^([A-Za-z0-9_.-]+):", re.MULTILINE)


CENTRAL_DOCUMENTS = (
    "README.md",
    "API_REFERENCE.md",
    "docs/INDICE_DA_DOCUMENTACAO.md",
    "docs/GUIA_RAPIDO.md",
    "docs/ARQUITETURA_DO_CORE.md",
    "docs/MAPA_DO_PROJETO.md",
    "docs/PRINCIPIOS_DE_DESENVOLVIMENTO.md",
    "docs/ROADMAP.md",
    "docs/GLOSSARIO.md",
    "docs/COMPATIBILIDADE.md",
    "docs/MATRIZ_DE_RASTREABILIDADE.md",
    "docs/GUIA_DO_STUDIO.md",
    "docs/GUIA_DE_CENARIOS.md",
    "docs/GUIA_DE_EXPERIMENTOS.md",
    "docs/GUIA_DE_METRICAS.md",
    "docs/GUIA_DE_DIAGNOSTICO.md",
    "docs/ORGANIZACAO_DE_RESULTADOS.md",
    "docs/MANUAL_DE_USO.md",
    "docs/AUDITORIA_DO_CORE_V02.md",
    "docs/COBERTURA_DE_TESTES.md",
    "docs/BENCHMARKS_V02.md",
    "docs/CHECKLIST_DE_VALIDACAO_DO_STUDIO.md",
)

IMPORTANT_FILES = (
    "Makefile",
    "include/minisnn.h",
    "include/minisnn_types.h",
    "app/minisnn_studio.c",
    "app/minisnn_runner.c",
    "app/scenario_config.c",
    "app/scenario_runner.c",
    "scripts/analyze_run.py",
    "scripts/metrics_common.py",
    "scripts/compare_runs.py",
    "scripts/plot_scenario.py",
    "scripts/plot_neuron.py",
    "scripts/check_docs.py",
    "configs/random.ini",
    "configs/small_world.ini",
    "tests/test_minisnn_api.c",
    "tests/test_topology.c",
    "tests/test_LIF.c",
    "tests/test_scenario_config.c",
    "tests/test_scenario_runner.c",
    "tests/test_plot_neuron.py",
    "tests/test_compare_runs.py",
    "tests/test_analyze_run.py",
    "tests/test_docs.py",
    "tests/test_metrics_common.py",
    "tests/test_runner_topologies.c",
    "tests/test_reproducibility.c",
    "tests/test_regression_baseline.py",
)

REQUIRED_TARGETS = (
    "test",
    "test-docs",
    "test-diagnostics",
    "test-plot-neuron",
    "test-compare-runs",
    "studio-build",
    "scenario-random",
    "scenario-small-world",
    "test-runner-topologies",
    "test-reproducibility",
    "test-regression",
    "test-analyzer",
    "test-sanitize",
    "test-long",
    "benchmark-v02",
    "check-v02",
)

IMPORTANT_KEYS = (
    "auto_unique_run",
    "history_enabled",
    "level",
    "time_bin_steps",
    "burst_z_threshold",
    "min_burst_steps",
    "isi_min_spikes",
    "correlation_sample_size",
    "neuron_sample_limit",
    "sample_stride",
)

STUDIO_BUTTONS = (
    "CSV NEURONIO",
    "GRAFICO NEURONIO",
    "ABRIR GRAFICO",
    "COMPARAR EXECUCOES",
    "ABRIR COMPARACAO",
    "ABRIR RESULTADOS",
    "ABRIR ULTIMA",
    "ABRIR HISTORICO",
    "GERAR DIAGNOSTICO",
    "ABRIR METRICAS",
    "ABRIR DIAGNOSTICO",
)


def markdown_files(root: Path) -> list[Path]:
    return [root / "README.md", root / "API_REFERENCE.md", *sorted((root / "docs").glob("*.md"))]


def clean_link_target(raw_target: str) -> str:
    target = raw_target.strip().split(maxsplit=1)[0].strip("<>")
    return unquote(target.split("#", 1)[0])


def validate_docs(root: Path) -> list[str]:
    errors: list[str] = []
    documents = markdown_files(root)
    texts: dict[Path, str] = {}

    for relative in CENTRAL_DOCUMENTS:
        if not (root / relative).is_file():
            errors.append(f"documento central ausente: {relative}")

    for relative in IMPORTANT_FILES:
        if not (root / relative).is_file():
            errors.append(f"arquivo importante ausente: {relative}")

    for document in documents:
        if not document.is_file():
            continue
        text = document.read_text(encoding="utf-8")
        texts[document] = text
        relative_document = document.relative_to(root)

        if LOCAL_ABSOLUTE.search(text):
            errors.append(f"caminho local absoluto em {relative_document}")

        if "Neuronio gravado" in text or "Neurônio gravado" in text:
            errors.append(f"rótulo obsoleto em {relative_document}: use Neurônio detalhado")

        for raw_target in MARKDOWN_LINK.findall(text):
            target = clean_link_target(raw_target)
            if not target or target.startswith(("http://", "https://", "mailto:")):
                continue
            if target.startswith("/") or re.match(r"^[A-Za-z]:[\\/]", target):
                errors.append(f"link local absoluto em {relative_document}: {raw_target}")
                continue
            resolved = (document.parent / target).resolve()
            if not resolved.exists():
                errors.append(f"link quebrado em {relative_document}: {raw_target}")

        for reference in PROJECT_REFERENCE.findall(text):
            if not (root / reference).exists():
                errors.append(f"referência inexistente em {relative_document}: {reference}")

    makefile = (root / "Makefile").read_text(encoding="utf-8")
    targets = set(MAKE_TARGET.findall(makefile))
    for target in REQUIRED_TARGETS:
        if target not in targets:
            errors.append(f"alvo obrigatório ausente no Makefile: {target}")

    for document, text in texts.items():
        for target in MAKE_COMMAND.findall(text):
            if target not in targets and target not in {"--version"}:
                errors.append(
                    f"alvo documentado inexistente em {document.relative_to(root)}: {target}"
                )

    parser_source = (root / "app" / "scenario_config.c").read_text(encoding="utf-8")
    scenario_guide = texts.get(root / "docs" / "GUIA_DE_CENARIOS.md", "")
    for key in IMPORTANT_KEYS:
        if f'{{"{key}",' not in parser_source:
            errors.append(f"chave documentada ausente no parser: {key}")
        if f"`{key}`" not in scenario_guide:
            errors.append(f"chave do parser ausente no guia de cenários: {key}")

    studio_source = (root / "app" / "minisnn_studio.c").read_text(encoding="utf-8")
    studio_guide = texts.get(root / "docs" / "GUIA_DO_STUDIO.md", "")
    for button in STUDIO_BUTTONS:
        if f'"{button}"' not in studio_source:
            errors.append(f"botão esperado ausente no Studio: {button}")
        if f"`{button}`" not in studio_guide:
            errors.append(f"botão do Studio ausente no guia: {button}")

    api_header = (root / "include" / "minisnn.h").read_text(encoding="utf-8")
    api_reference = texts.get(root / "API_REFERENCE.md", "")
    public_functions = set(re.findall(r"\b(minisnn_[a-z_]+)\s*\(", api_header))
    for function in public_functions:
        if f"## {function}" not in api_reference:
            errors.append(f"função pública ausente no API Reference: {function}")

    index_text = texts.get(root / "docs" / "INDICE_DA_DOCUMENTACAO.md", "")
    for relative in CENTRAL_DOCUMENTS:
        if relative == "docs/INDICE_DA_DOCUMENTACAO.md":
            continue
        expected = "../" + relative if not relative.startswith("docs/") else relative[5:]
        if expected not in index_text:
            errors.append(f"documento central não ligado pelo índice: {relative}")

    readme_text = texts.get(root / "README.md", "")
    if "docs/INDICE_DA_DOCUMENTACAO.md" not in readme_text:
        errors.append("README não aponta para o índice documental")

    return sorted(set(errors))


def main() -> int:
    parser = argparse.ArgumentParser(description="Valida a documentação da miniSNN.")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    errors = validate_docs(args.root.resolve())
    if errors:
        print("Documentation validation FAILED")
        for error in errors:
            print(f"- {error}")
        return 1
    print("Documentation validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
