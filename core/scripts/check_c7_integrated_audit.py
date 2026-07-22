"""Validate the generated, domain-neutral C7 integrated-audit artifacts."""

from __future__ import annotations

import csv
import math
import shutil
import subprocess
import sys
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REQUIRED = (
    "config_source.ini", "config_used.ini", "c7_audit_manifest.txt",
    "c7_audit_runs.csv", "c7_audit_actions.csv", "c7_audit_feedback.csv",
    "c7_audit_checkpoints.csv", "c7_audit_model_summary.csv",
    "c7_audit_summary.txt", "c7_audit_report.html",
)
MODELS = {"lif", "adex", "hodgkin_huxley"}


def fail(message: str) -> None:
    print(f"C7 integrated audit validation FAILED\n- {message}")
    raise SystemExit(1)


def rows(path: Path) -> list[dict[str, str]]:
    try:
        with path.open(newline="", encoding="utf-8") as file:
            return list(csv.DictReader(file))
    except (OSError, csv.Error) as error:
        fail(f"CSV invalido: {path.name}: {error}")


def finite(row: dict[str, str], names: tuple[str, ...]) -> None:
    for name in names:
        try:
            value = float(row[name])
        except (KeyError, ValueError):
            fail(f"valor numerico invalido em {name}")
        if not math.isfinite(value):
            fail(f"valor nao finito em {name}")


def validate(directory: Path, expected_models: set[str]) -> None:
    for name in REQUIRED:
        if not (directory / name).is_file():
            fail(f"artefato obrigatorio ausente: {directory / name}")

    run_rows = rows(directory / "c7_audit_runs.csv")
    action_rows = rows(directory / "c7_audit_actions.csv")
    feedback_rows = rows(directory / "c7_audit_feedback.csv")
    checkpoint_rows = rows(directory / "c7_audit_checkpoints.csv")
    summary_rows = rows(directory / "c7_audit_model_summary.csv")
    if not run_rows:
        fail("c7_audit_runs.csv vazio")

    grouped: dict[str, list[dict[str, str]]] = defaultdict(list)
    actions_by_run: dict[tuple[str, str], int] = defaultdict(int)
    previous_tick: dict[tuple[str, str], int] = {}
    for row in run_rows:
        model = row.get("model", "")
        if model not in expected_models:
            fail(f"modelo inesperado: {model}")
        grouped[model].append(row)
        finite(row, ("seed", "ticks", "neural_steps", "spikes", "actions", "resets",
                     "reward", "checkpoints", "resumes", "fingerprint", "finite", "completed"))
        if row.get("final_state") != "READY" or row.get("finite") != "1" or row.get("completed") != "1":
            fail(f"run nao concluiu em READY: {model}")
        if int(row["neural_steps"]) <= 0 or int(row["actions"]) != int(row["ticks"]):
            fail(f"contagem invalida em {model}")

    for model in expected_models:
        model_rows = grouped.get(model, [])
        if len(model_rows) != 2:
            fail(f"matriz de replay incompleta para {model}")
        if len({row["fingerprint"] for row in model_rows}) != 1:
            fail(f"replay nao deterministico para {model}")

    for row in action_rows:
        key = (row.get("model", ""), row.get("repeat", ""))
        if key[0] not in expected_models:
            fail("acao com modelo invalido")
        tick = int(row["tick"])
        if tick <= previous_tick.get(key, -1):
            fail("ticks de acao nao monotônicos")
        previous_tick[key] = tick
        finite(row, ("output_a", "output_b", "target_signal", "context_output"))
        actions_by_run[key] += 1

    for row in run_rows:
        key = (row["model"], row["repeat"])
        if actions_by_run[key] != int(row["actions"]):
            fail(f"acoes produzidas/consumidas divergentes para {key[0]}")

    seen_feedback: set[tuple[str, str, str]] = set()
    for row in feedback_rows:
        key = (row["model"], row["repeat"], row["source_tick"])
        if key in seen_feedback:
            fail("feedback duplicado")
        seen_feedback.add(key)
        finite(row, ("source_tick", "delivery_tick", "reward"))
        if int(row["delivery_tick"]) <= int(row["source_tick"]):
            fail("feedback nao atrasado")

    boundaries = {row.get("boundary") for row in checkpoint_rows if row.get("status") == "ok"}
    if {"READY", "ACTION_PENDING"} - boundaries:
        fail("fronteiras de checkpoint ausentes")
    for row in checkpoint_rows:
        checkpoint = Path(row["path"])
        if not checkpoint.is_dir():
            fail(f"checkpoint ausente: {checkpoint}")

    if {row.get("model") for row in summary_rows} != expected_models:
        fail("resumo por modelo incompleto")
    for row in summary_rows:
        if row.get("deterministic") != "1" or row.get("finite") != "1":
            fail("resumo indica falha de determinismo ou finitude")
        if row.get("actions_produced") != row.get("actions_consumed"):
            fail("resumo possui acao nao consumida")

    report = (directory / "c7_audit_report.html").read_text(encoding="utf-8")
    if "http://" in report or "https://" in report or "C:\\" in report:
        fail("relatorio deve usar somente recursos locais relativos")
    if "D1" not in report or "Limits" not in report:
        fail("relatorio nao declara limites e proximo passo")


def write_alternate(path: Path) -> None:
    path.write_text(
        "[run]\nrun_name = c7_integrated_audit_alternate\n\n[audit]\n"
        "models = lif\nseed = 702\nepisodes = 2\nticks_per_episode = 5\n"
        "brain_steps_per_tick = 3\nstdp_enabled = false\nreward_enabled = true\n"
        "homeostasis_enabled = false\nstructural_enabled = false\n"
        "checkpoint_ready = true\ncheckpoint_pending = true\nreplay_enabled = true\n",
        encoding="utf-8",
    )


def main() -> None:
    if len(sys.argv) != 2:
        fail("uso: check_c7_integrated_audit.py <c7_integrated_audit.exe>")
    executable = Path(sys.argv[1]).resolve()
    if not executable.is_file():
        fail("executavel de auditoria ausente")
    default_dir = ROOT / "results" / "scenarios" / "c7_integrated_audit_demo"
    validate(default_dir, MODELS)

    alternate_ini = ROOT / "build" / "c7_integrated_audit_alternate.ini"
    alternate_dir = ROOT / "results" / "scenarios" / "c7_integrated_audit_alternate"
    shutil.rmtree(alternate_dir, ignore_errors=True)
    write_alternate(alternate_ini)
    try:
        completed = subprocess.run([str(executable), str(alternate_ini.relative_to(ROOT))],
                                   cwd=ROOT, check=False, capture_output=True, text=True)
        if completed.returncode != 0:
            fail(f"configuracao alternativa falhou: {completed.stdout}{completed.stderr}")
        validate(alternate_dir, {"lif"})
        primary = rows(default_dir / "c7_audit_runs.csv")[0]
        alternate = rows(alternate_dir / "c7_audit_runs.csv")[0]
        if primary["fingerprint"] == alternate["fingerprint"]:
            fail("configuracao alternativa nao alterou o fingerprint")
    finally:
        alternate_ini.unlink(missing_ok=True)
        shutil.rmtree(alternate_dir, ignore_errors=True)
    print("C7 integrated audit artifact validation OK")


if __name__ == "__main__":
    main()
