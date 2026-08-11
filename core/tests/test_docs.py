from pathlib import Path
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from check_docs import validate_docs


def main() -> int:
    for relative in (
        "scripts/generate_run_reports.py",
        "scripts/html_report_common.py",
        "tests/test_run_reports.py",
        "scripts/plot_homeostasis.py",
        "scripts/check_c15.py",
        "docs/GUIA_DE_HOMEOSTASE.md",
        "scripts/plot_reward.py",
        "scripts/run_benchmarks_c2.py",
        "scripts/check_c2.py",
        "docs/GUIA_DE_RECOMPENSA.md",
        "docs/BENCHMARKS_C2_RECOMPENSA.md",
        "tests/test_reward.c",
        "tests/test_reward_long.c",
        "tests/test_plot_reward.py",
        "scripts/generate_history_report.py",
        "tests/test_history_report.py",
        "src/evolution.c",
        "app/evolution_config.c",
        "app/evolution_runner.c",
        "scripts/plot_evolution.py",
        "scripts/generate_evolution_report.py",
        "scripts/run_benchmarks_c3.py",
        "scripts/check_c3.py",
        "docs/GUIA_DE_NEUROEVOLUCAO.md",
        "docs/BENCHMARKS_C3_NEUROEVOLUCAO.md",
        "tests/test_evolution.c",
        "tests/test_evolution_runner.c",
        "tests/test_evolution_long.c",
        "tests/test_plot_evolution.py",
        "tests/test_evolution_report.py",
        "src/structure.c",
        "src/structural_plasticity.c",
        "scripts/plot_topology.py",
        "scripts/run_benchmarks_c4.py",
        "scripts/check_c4.py",
        "docs/GUIA_DE_TOPOLOGIA_ADAPTATIVA.md",
        "docs/GUIA_DE_MEMORIA_DE_TRABALHO.md",
        "docs/GUIA_DE_MEMORIA_DE_TRABALHO.md",
        "docs/GUIA_DA_INTERFACE_CEREBRO_AGENTE.md",
        "docs/AUDITORIA_C7_INTERFACE_CEREBRO_AGENTE.md",
        "docs/D1_B_ROBUSTNESS_AUDIT.md",
        "docs/DETERMINISM_CONTRACT.md",
        "docs/ROBUSTNESS_AND_FAULT_MODEL.md",
        "docs/PERFORMANCE_BASELINE.md",
        "docs/CORE_BRIDGE_API_CANDIDATE.md",
        "docs/INSTALLING_AND_LINKING.md",
        "docs/KNOWN_LIMITATIONS_1_0_0_RC1.md",
        "docs/RELEASE_NOTES_1_0_0_RC1.md",
        "docs/RELEASE_CHECKLIST_1_0_RC.md",
        "docs/D1_C_RELEASE_CANDIDATE_AUDIT.md",
        "docs/D1_C_STUDIO_MANUAL_CHECKLIST.md",
        "docs/PUBLIC_API_BASELINE_1_0_RC.txt",
        "scripts/check_d1_posix_headless.py",
        "tests/test_d1_portability.py",
        "include/minisnn_sensor_encoder.h",
        "src/sensor_encoder.c",
        "tests/test_sensor_encoder.c",
        "tests/test_sensor_encoding_demo.py",
        "app/sensor_encoding_demo_config.c",
        "app/sensor_encoding_demo_config.h",
        "include/minisnn_action_decoder.h",
        "src/action_decoder.c",
        "tests/test_action_decoder.c",
        "tests/test_action_decoding_demo.py",
        "app/action_decoding_demo_config.c",
        "app/action_decoding_demo_config.h",
        "docs/BENCHMARKS_C4_TOPOLOGIA.md",
        "tests/test_structure.c",
        "tests/test_structural_plasticity.c",
        "tests/test_structure_resume.c",
        "tests/test_structure_long.c",
        "tests/test_plot_topology.py",
    ):
        if not (PROJECT_ROOT / relative).is_file():
            print(f"Documentation validation FAILED\n- arquivo HTML ausente: {relative}")
            return 1
    errors = validate_docs(PROJECT_ROOT)
    roadmap = (PROJECT_ROOT / "docs" / "ROADMAP.md").read_text(encoding="utf-8")
    for token in (
        "D1-A — concluído",
        "D1-B — concluído",
        "D1-C — concluído",
        "D1 — concluído",
        "miniSNN Core 1.0.0-rc.1",
        "K0 - fundacao deterministica do Worlds Kernel",
        "K1 - fundacao espacial: K1-A, K1-B1/B2 e K1-C1..C4 concluidos",
        "K2-A - snapshot canonico em memoria do Worlds Kernel",
        "K2-B - restore + save/load do snapshot canonico",
        "K2-C - command log canonico e replay deterministico",
        "[x] K2-D - auditoria final de persistencia e replay",
        "D2 - auditoria pos-integracao",
        "congelada definitivamente",
    ):
        if token not in roadmap:
            errors.append(f"roadmap sem status oficial: {token}")
    if "D1 — congelamento definitivo do Core" in roadmap:
        errors.append("roadmap atribui congelamento definitivo a D1")
    if "READY_FOR_MANUAL_VALIDATION" in roadmap or "D1-C manual - pendente" in roadmap:
        errors.append("roadmap ainda declara validacao manual D1 pendente")
    if errors:
        print("Documentation validation FAILED")
        for error in errors:
            print(f"- {error}")
        return 1
    print("Documentation validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
