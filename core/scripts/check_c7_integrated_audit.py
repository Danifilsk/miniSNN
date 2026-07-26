"""Validate the generated, domain-neutral C7.5-B audit artifacts."""

from __future__ import annotations

import csv
import math
import re
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


def fail(message: str) -> None:
    print(f"C7 integrated audit validation FAILED\n- {message}")
    raise SystemExit(1)


def rows(path: Path) -> list[dict[str, str]]:
    try:
        with path.open(newline="", encoding="utf-8") as file:
            return list(csv.DictReader(file))
    except (OSError, csv.Error) as error:
        fail(f"CSV invalido: {path.name}: {error}")


def parse_ini(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith(("#", ";", "[")):
            continue
        if "=" not in line:
            fail(f"config_used.ini invalido: {line}")
        key, value = (part.strip() for part in line.split("=", 1))
        result[key] = value
    return result


def finite(row: dict[str, str], names: tuple[str, ...]) -> None:
    for name in names:
        try:
            value = float(row[name])
        except (KeyError, ValueError):
            fail(f"valor numerico invalido em {name}")
        if not math.isfinite(value):
            fail(f"valor nao finito em {name}")


def key_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    return values


def expected_models(config: dict[str, str]) -> set[str]:
    return {model.strip() for model in config["models"].split(",") if model.strip()}


def report_operation(report: str, name: str) -> tuple[str, str, str, str, str]:
    match = re.search(
        rf'<tr data-operation="{re.escape(name)}"><td>[^<]+</td><td>([^<]+)</td>'
        rf'<td>([^<]+)</td><td>([^<]+)</td><td>([^<]+)</td><td>([^<]+)</td></tr>',
        report,
    )
    if match is None:
        fail(f"operacao HTML ausente: {name}")
    return match.groups()


def report_effective_modules(report: str, model: str) -> list[tuple[str, str, str, str]]:
    modules: list[tuple[str, str, str, str]] = []
    for match in re.finditer(rf"<tr><td>{re.escape(model)}</td><td>\d+</td>(.*?)</tr>", report):
        cells = re.findall(r"<td>([^<]*)</td>", match.group(0))
        if len(cells) < 8:
            fail(f"linha HTML incompleta: {model}")
        modules.append(tuple(cells[4:8]))
    if not modules:
        fail(f"linha HTML da execucao ausente: {model}")
    return modules


def validate(directory: Path, source_config: Path | None = None) -> None:
    for name in REQUIRED:
        if not (directory / name).is_file():
            fail(f"artefato obrigatorio ausente: {directory / name}")

    config = parse_ini(directory / "config_used.ini")
    if source_config is not None and (directory / "config_source.ini").read_bytes() != source_config.read_bytes():
        fail("config_source.ini nao preserva os bytes fornecidos")
    enabled_models = expected_models(config)
    repeat_count = 2 if config["replay_enabled"] == "true" else 1
    expected_checkpoint_count = int(config["checkpoint_ready"] == "true") + int(
        config["checkpoint_pending"] == "true")
    run_id = key_values(directory / "c7_audit_manifest.txt").get("run_id")
    if not run_id:
        fail("manifesto sem run_id")

    run_rows = rows(directory / "c7_audit_runs.csv")
    action_rows = rows(directory / "c7_audit_actions.csv")
    feedback_rows = rows(directory / "c7_audit_feedback.csv")
    checkpoint_rows = rows(directory / "c7_audit_checkpoints.csv")
    summary_rows = rows(directory / "c7_audit_model_summary.csv")
    if not run_rows:
        fail("c7_audit_runs.csv vazio")

    grouped: dict[str, list[dict[str, str]]] = defaultdict(list)
    actions_by_run: dict[tuple[str, str], int] = defaultdict(int)
    action_values: dict[tuple[str, str], set[tuple[str, ...]]] = defaultdict(set)
    previous_tick: dict[tuple[str, str], int] = {}
    for row in run_rows:
        model = row.get("model", "")
        if model not in enabled_models:
            fail(f"modelo inesperado: {model}")
        grouped[model].append(row)
        finite(row, ("seed", "ticks", "neural_steps", "total_spikes", "actions", "resets",
                     "reward", "checkpoint_count", "resume_count", "replay_count",
                     "active_readout_channels", "nondefault_action_count",
                     "action_variation_count", "silent_ticks", "input_drive",
                     "model_config_signature", "fingerprint", "finite", "completed"))
        if row.get("final_state") != "READY" or row.get("finite") != "1" or row.get("completed") != "1":
            fail(f"run nao concluiu em READY: {model}")
        if int(row["neural_steps"]) <= 0 or int(row["actions"]) != int(row["ticks"]):
            fail(f"contagem invalida em {model}")
        if int(row["total_spikes"]) <= 0 or int(row["active_readout_channels"]) <= 0:
            fail(f"auditoria sem atividade neural real para {model}")
        if int(row["nondefault_action_count"]) <= 0 or int(row["action_variation_count"]) <= 0:
            fail(f"decoder silencioso ou sem resposta a atividade para {model}")
        if int(row["checkpoint_count"]) != expected_checkpoint_count or \
           int(row["resume_count"]) != expected_checkpoint_count:
            fail(f"contador de checkpoint/resume nao reflete operacoes reais: {model}")
        expected_replays = expected_checkpoint_count if config["replay_enabled"] == "true" else 0
        if int(row["replay_count"]) != expected_replays:
            fail(f"contador de replay invalido: {model}")

    if set(grouped) != enabled_models:
        fail("artefatos nao refletem os modelos habilitados")
    for model in enabled_models:
        model_rows = grouped[model]
        if len(model_rows) != repeat_count:
            fail(f"numero de repeticoes invalido para {model}")
        if repeat_count == 2 and len({row["fingerprint"] for row in model_rows}) != 1:
            fail(f"replay nao deterministico para {model}")

    for row in action_rows:
        key = (row.get("model", ""), row.get("repeat", ""))
        if key[0] not in enabled_models:
            fail("acao com modelo invalido")
        tick = int(row["tick"])
        if tick <= previous_tick.get(key, -1):
            fail("ticks de acao nao monotonicos")
        previous_tick[key] = tick
        finite(row, ("output_a", "output_b", "target_signal", "context_output"))
        actions_by_run[key] += 1
        action_values[key].add(tuple(row[name] for name in (
            "output_a", "output_b", "target_signal", "context_output")))

    for row in run_rows:
        key = (row["model"], row["repeat"])
        if actions_by_run[key] != int(row["actions"]):
            fail(f"acoes produzidas/consumidas divergentes para {key[0]}")
        if len(action_values[key]) < 2:
            fail(f"padroes sensoriais nao alteraram a acao de {key[0]}")

    seen_feedback: set[tuple[str, str, str]] = set()
    for row in feedback_rows:
        key = (row["model"], row["repeat"], row["source_tick"])
        if key in seen_feedback:
            fail("feedback duplicado")
        seen_feedback.add(key)
        finite(row, ("source_tick", "delivery_tick", "reward"))
        if int(row["delivery_tick"]) <= int(row["source_tick"]):
            fail("feedback nao atrasado")

    used_paths: set[str] = set()
    expected_checkpoint_rows = len(run_rows) * expected_checkpoint_count
    if len(checkpoint_rows) != expected_checkpoint_rows:
        fail("linhas de checkpoint nao correspondem aos checkpoints configurados")
    for row in checkpoint_rows:
        if row["model"] not in enabled_models or row["path"] in used_paths:
            fail("checkpoint reutilizado ou de modelo invalido")
        used_paths.add(row["path"])
        if Path(row["path"]).is_absolute() or ".." in Path(row["path"]).parts:
            fail("checkpoint persistiu caminho nao relativo")
        checkpoint = directory / row["path"]
        if not checkpoint.is_dir() or not (checkpoint / "manifest.txt").is_file():
            fail(f"checkpoint ausente: {checkpoint}")
        metadata = key_values(checkpoint / "audit_metadata.txt")
        manifest = key_values(checkpoint / "manifest.txt")
        if metadata.get("model") != row["model"] or metadata.get("repeat") != row["repeat"] or \
           metadata.get("boundary") != row["boundary"] or metadata.get("run_id") != run_id:
            fail("metadata de checkpoint nao corresponde a linha CSV")
        if manifest.get("neuron_model") != row["model"] or \
           manifest.get("checkpoint_boundary") != row["boundary"]:
            fail("manifesto de checkpoint nao corresponde a linha CSV")
        if row["save_completed"] != "1" or row["load_completed"] != "1" or \
           row["resume_completed"] != "1" or row["state_signature"] != metadata.get("state_signature"):
            fail("checkpoint nao foi salvo/carregado de forma completa")
        expected_replay_completed = "1" if config["replay_enabled"] == "true" else "0"
        if row["replay_completed"] != expected_replay_completed:
            fail("status de replay do checkpoint nao reflete a configuracao")

    if {row.get("model") for row in summary_rows} != enabled_models:
        fail("resumo por modelo nao corresponde a configuracao")
    for row in summary_rows:
        if row.get("deterministic") != "1" or row.get("finite") != "1":
            fail("resumo indica falha de determinismo ou finitude")
        if row.get("stdp") != ("1" if config["stdp_enabled"] == "true" else "0") or \
           row.get("reward") != ("1" if config["reward_enabled"] == "true" else "0") or \
           row.get("structural") != ("1" if config["structural_enabled"] == "true" else "0"):
            fail("plasticidades do resumo nao refletem config_used.ini")
        expected_homeostasis = "1" if (
            config["homeostasis_enabled"] == "true" and row["model"] == "lif") else "0"
        if row.get("homeostasis") != expected_homeostasis:
            fail("homeostase efetiva incorreta")

    report = (directory / "c7_audit_report.html").read_text(encoding="utf-8")
    if "http://" in report or "https://" in report or "C:\\" in report:
        fail("relatorio deve usar somente recursos locais relativos")
    if "D1" not in report or "Limits" not in report or "Operations" not in report or \
       "not reached" not in report:
        fail("relatorio nao declara limites e proximo passo")
    for model in enabled_models:
        if model not in report:
            fail("relatorio nao lista modelo habilitado")
    for model in {"lif", "adex", "hodgkin_huxley"} - enabled_models:
        if model in report:
            fail("relatorio contem modelo desabilitado")

    if "effective homeostasis" not in report:
        fail("relatorio nao mostra modulos efetivos por execucao")
    for model in enabled_models:
        expected_modules = (
            "on" if config["stdp_enabled"] == "true" else "off",
            "on" if config["reward_enabled"] == "true" else "off",
            "on" if config["homeostasis_enabled"] == "true" and model == "lif" else "off",
            "on" if config["structural_enabled"] == "true" else "off",
        )
        if any(modules != expected_modules
               for modules in report_effective_modules(report, model)):
            fail(f"modulos efetivos incorretos no HTML para {model}")

    expected_operations = {
        "checkpoint-ready": (
            "yes" if config["checkpoint_ready"] == "true" else "no",
            str(sum(row["boundary"] == "READY" for row in checkpoint_rows)),
        ),
        "checkpoint-action-pending": (
            "yes" if config["checkpoint_pending"] == "true" else "no",
            str(sum(row["boundary"] == "ACTION_PENDING" for row in checkpoint_rows)),
        ),
        "resume": (
            "yes" if expected_checkpoint_count else "no",
            str(sum(int(row["resume_count"]) for row in run_rows)),
        ),
        "replay": (
            "yes" if config["replay_enabled"] == "true" else "no",
            str(sum(int(row["replay_count"]) for row in run_rows)),
        ),
    }
    for operation, (configured, completed) in expected_operations.items():
        html_configured, attempted, html_completed, failed, not_reached = report_operation(
            report, operation)
        if (html_configured, attempted, html_completed, failed, not_reached) != (
            configured, completed, completed, "0", "0"):
            fail(f"totais HTML da operacao {operation} nao correspondem aos CSVs")


def write_alternate(path: Path) -> None:
    path.write_text(
        "[run]\nrun_name = c7_integrated_audit_alternate\n\n[audit]\n"
        "models = lif\nseed = 702\nepisodes = 2\nticks_per_episode = 5\n"
        "brain_steps_per_tick = 64\nlif_input_drive = 900.0\n"
        "adex_input_drive = 500.0\nhodgkin_huxley_input_drive = 12.0\n"
        "stdp_enabled = false\nreward_enabled = true\nhomeostasis_enabled = false\n"
        "structural_enabled = false\ncheckpoint_ready = true\n"
        "checkpoint_pending = true\nreplay_enabled = true\n",
        encoding="utf-8",
    )


def write_short_config(path: Path) -> None:
    path.write_text(
        "[run]\nrun_name = c7_integrated_audit_short\n\n[audit]\nmodels = lif\n"
        "seed = 1\nepisodes = 1\nticks_per_episode = 2\nbrain_steps_per_tick = 64\n"
        "lif_input_drive = 1000\nadex_input_drive = 500\n"
        "hodgkin_huxley_input_drive = 12\nstdp_enabled = false\nreward_enabled = false\n"
        "homeostasis_enabled = false\nstructural_enabled = false\ncheckpoint_ready = true\n"
        "checkpoint_pending = true\nreplay_enabled = true\n",
        encoding="utf-8",
    )


def write_stale_config(path: Path, checkpoints: bool) -> None:
    path.write_text(
        "[run]\nrun_name = c7_integrated_audit_stale\n\n[audit]\nmodels = lif\n"
        "seed = 711\nepisodes = 1\nticks_per_episode = 12\nbrain_steps_per_tick = 64\n"
        "lif_input_drive = 1000\nadex_input_drive = 500\n"
        "hodgkin_huxley_input_drive = 12\nstdp_enabled = false\nreward_enabled = false\n"
        "homeostasis_enabled = false\nstructural_enabled = false\n"
        f"checkpoint_ready = {'true' if checkpoints else 'false'}\n"
        f"checkpoint_pending = {'true' if checkpoints else 'false'}\n"
        f"replay_enabled = {'true' if checkpoints else 'false'}\n",
        encoding="utf-8",
    )


def write_no_replay_config(path: Path, run_name: str, ready_checkpoint: bool) -> None:
    path.write_text(
        f"[run]\nrun_name = {run_name}\n\n[audit]\nmodels = lif\n"
        "seed = 703\nepisodes = 1\nticks_per_episode = 6\nbrain_steps_per_tick = 64\n"
        "lif_input_drive = 1000\nadex_input_drive = 500\n"
        "hodgkin_huxley_input_drive = 12\nstdp_enabled = false\n"
        "reward_enabled = false\nhomeostasis_enabled = false\nstructural_enabled = false\n"
        f"checkpoint_ready = {'true' if ready_checkpoint else 'false'}\n"
        "checkpoint_pending = true\nreplay_enabled = false\n",
        encoding="utf-8",
    )


def validate_no_replay(directory: Path, source_config: Path, expected_checkpoints: int) -> None:
    validate(directory, source_config)
    run_rows = rows(directory / "c7_audit_runs.csv")
    action_rows = rows(directory / "c7_audit_actions.csv")
    feedback_rows = rows(directory / "c7_audit_feedback.csv")
    checkpoint_rows = rows(directory / "c7_audit_checkpoints.csv")
    if len(run_rows) != 1 or int(run_rows[0]["checkpoint_count"]) != expected_checkpoints or \
       int(run_rows[0]["resume_count"]) != expected_checkpoints or \
       int(run_rows[0]["replay_count"]) != 0:
        fail("contadores de checkpoint ACTION_PENDING sem replay invalidos")
    if len(action_rows) != 6 or len(feedback_rows) != 6 or \
       run_rows[0]["final_state"] != "READY":
        fail("acao, feedback ou estado final invalido sem replay")
    if len(checkpoint_rows) != expected_checkpoints or \
       any(row["replay_completed"] != "0" for row in checkpoint_rows):
        fail("checkpoint sem replay registrou replay indevidamente")
    if not any(row["boundary"] == "ACTION_PENDING" for row in checkpoint_rows):
        fail("checkpoint ACTION_PENDING sem replay ausente")


def run(executable: Path, config: Path, should_succeed: bool) -> None:
    completed = subprocess.run([str(executable), str(config.relative_to(ROOT))], cwd=ROOT,
                               check=False, capture_output=True, text=True)
    if (completed.returncode == 0) != should_succeed:
        fail(f"resultado inesperado para {config.name}: {completed.stdout}{completed.stderr}")


def main() -> None:
    if len(sys.argv) != 2:
        fail("uso: check_c7_integrated_audit.py <c7_integrated_audit.exe>")
    executable = Path(sys.argv[1]).resolve()
    if not executable.is_file():
        fail("executavel de auditoria ausente")
    default_dir = ROOT / "results" / "scenarios" / "c7_integrated_audit_demo"
    validate(default_dir, ROOT / "configs" / "c7_integrated_audit.ini")

    alternate_ini = ROOT / "build" / "c7_integrated_audit_alternate.ini"
    alternate_dir = ROOT / "results" / "scenarios" / "c7_integrated_audit_alternate"
    short_ini = ROOT / "build" / "c7_integrated_audit_short.ini"
    stale_ini = ROOT / "build" / "c7_integrated_audit_stale.ini"
    stale_dir = ROOT / "results" / "scenarios" / "c7_integrated_audit_stale"
    pending_no_replay_ini = ROOT / "build" / "c7_integrated_audit_pending_no_replay.ini"
    pending_no_replay_dir = ROOT / "results" / "scenarios" / "c7_integrated_audit_pending_no_replay"
    both_no_replay_ini = ROOT / "build" / "c7_integrated_audit_both_no_replay.ini"
    both_no_replay_dir = ROOT / "results" / "scenarios" / "c7_integrated_audit_both_no_replay"
    for directory in (alternate_dir, stale_dir, pending_no_replay_dir, both_no_replay_dir):
        shutil.rmtree(directory, ignore_errors=True)
    try:
        write_alternate(alternate_ini)
        run(executable, alternate_ini, True)
        validate(alternate_dir, alternate_ini)
        primary = rows(default_dir / "c7_audit_runs.csv")[0]
        alternate = rows(alternate_dir / "c7_audit_runs.csv")[0]
        if primary["fingerprint"] == alternate["fingerprint"] or \
           alternate["input_drive"] != "900":
            fail("configuracao alternativa nao alterou execucao efetiva")
        write_short_config(short_ini)
        run(executable, short_ini, False)
        write_stale_config(stale_ini, True)
        run(executable, stale_ini, True)
        if not list(stale_dir.glob("checkpoint_*")):
            fail("precondicao do teste de limpeza ausente")
        write_stale_config(stale_ini, False)
        run(executable, stale_ini, True)
        if list(stale_dir.glob("checkpoint_*")):
            fail("artefato de checkpoint antigo sobreviveu a execucao nova")
        validate(stale_dir, stale_ini)
        write_no_replay_config(pending_no_replay_ini,
                               "c7_integrated_audit_pending_no_replay", False)
        run(executable, pending_no_replay_ini, True)
        validate_no_replay(pending_no_replay_dir, pending_no_replay_ini, 1)
        write_no_replay_config(both_no_replay_ini,
                               "c7_integrated_audit_both_no_replay", True)
        run(executable, both_no_replay_ini, True)
        validate_no_replay(both_no_replay_dir, both_no_replay_ini, 2)
    finally:
        for path in (alternate_ini, short_ini, stale_ini, pending_no_replay_ini,
                     both_no_replay_ini):
            path.unlink(missing_ok=True)
        for directory in (alternate_dir, stale_dir, pending_no_replay_dir,
                          both_no_replay_dir):
            shutil.rmtree(directory, ignore_errors=True)
    print("C7 integrated audit artifact validation OK")


if __name__ == "__main__":
    main()
