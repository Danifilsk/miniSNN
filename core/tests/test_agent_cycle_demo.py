from __future__ import annotations

import shutil
import subprocess
import sys
import csv
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def fail(message: str) -> int:
    print(f"Agent cycle demo configuration FAILED\n- {message}")
    return 1


def run(executable: Path, config: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(executable), str(config)], cwd=ROOT, text=True,
        capture_output=True, check=False,
    )


def main() -> int:
    if len(sys.argv) != 2:
        return fail("uso: test_agent_cycle_demo.py <agent_cycle_demo.exe>")

    executable = Path(sys.argv[1]).resolve()
    build = ROOT / "build"
    config = build / "agent_cycle_alternate.ini"
    invalid = build / "agent_cycle_invalid.ini"
    output = ROOT / "results" / "scenarios" / "agent_cycle_alternate"
    source_text = """# Exact source bytes must be retained.\n[run]\nrun_name = agent_cycle_alternate\n\n[network]\nneurons = 3\nmodel = AdEx\nbrain_steps_per_tick = 2\n\n[demo]\nticks = 6\nreset_interval = 3\ninput_current = 3500.0\n"""

    if not executable.is_file():
        return fail(f"executavel ausente: {executable}")
    build.mkdir(exist_ok=True)
    shutil.rmtree(output, ignore_errors=True)
    try:
        config.write_bytes(source_text.encode("ascii"))
        result = run(executable, config)
        if result.returncode != 0:
            return fail(f"demo alternativo falhou: {result.stdout}{result.stderr}")
        expected = [
            "config_source.ini", "config_used.ini", "agent_cycle_trace.csv",
            "agent_cycle_feedback.csv", "agent_cycle_summary.txt",
            "agent_cycle_report.html",
        ]
        for name in expected:
            if not (output / name).is_file():
                return fail(f"saida obrigatoria ausente: {name}")
        if (output / "config_source.ini").read_bytes() != config.read_bytes():
            return fail("config_source.ini nao preservou o arquivo original")
        used = (output / "config_used.ini").read_text(encoding="utf-8")
        summary = (output / "agent_cycle_summary.txt").read_text(encoding="utf-8")
        trace = (output / "agent_cycle_trace.csv").read_text(encoding="utf-8").splitlines()
        if "model = adex" not in used or "neurons = 3" not in used or \
                "brain_steps_per_tick = 2" not in used:
            return fail("config_used.ini nao e a configuracao efetiva canonica")
        if "model=adex" not in summary or "neurons=3" not in summary or \
                "brain_steps_per_tick=2" not in summary or len(trace) != 7:
            return fail("alterar o INI nao alterou as dimensoes da execucao")
        feedback = list(csv.DictReader(
            (output / "agent_cycle_feedback.csv").open(encoding="utf-8", newline="")
        ))
        if not any(row["status"] == "queued" for row in feedback) or \
                not any(row["status"] == "delivered_on_tick" for row in feedback) or \
                not any(row["status"] == "delivered_at_episode_boundary" for row in feedback):
            return fail("CSV nao distinguiu os estados efetivos do feedback")
        if not any(row["reward"] == "0.000000" for row in feedback) or \
                not any(row["reward"] == "1.000000" for row in feedback) or \
                not any(row["reward"] == "-1.000000" for row in feedback):
            return fail("demo nao demonstrou reward positivo, negativo e zero")

        invalid.write_text(source_text + "\n[demo]\nunknown_key = 1\n", encoding="ascii")
        invalid_result = run(executable, invalid)
        if invalid_result.returncode == 0 or "Erro ao carregar configuracao" not in invalid_result.stdout:
            return fail("chave desconhecida nao foi rejeitada claramente")

        default_output = ROOT / "results" / "scenarios" / "agent_cycle_demo"
        default_result = run(executable, ROOT / "configs" / "agent_cycle_demo.ini")
        if default_result.returncode != 0:
            return fail(f"demo padrao falhou: {default_result.stdout}{default_result.stderr}")
        default_trace = list(csv.DictReader(
            (default_output / "agent_cycle_trace.csv").open(encoding="utf-8", newline="")
        ))
        default_feedback = list(csv.DictReader(
            (default_output / "agent_cycle_feedback.csv").open(encoding="utf-8", newline="")
        ))
        default_summary = (default_output / "agent_cycle_summary.txt").read_text(
            encoding="utf-8")
        expected_ticks = ["0", "1", "2", "3", "0", "1", "2", "3"]
        delivered_total = sum(
            float(row["reward"])
            for row in default_feedback
            if row["status"] in {"delivered_on_tick", "delivered_at_episode_boundary"}
        )
        if [row["episode_tick"] for row in default_trace] != expected_ticks or \
                not any(row["episode_terminal"] == "1" and row["reward"] == "1.000000" and
                        row["status"] == "delivered_at_episode_boundary"
                        for row in default_feedback) or \
                "total_reward=1" not in default_summary or delivered_total != 1.0:
            return fail("feedback terminal ou diagnosticos do demo padrao incorretos")
    finally:
        config.unlink(missing_ok=True)
        invalid.unlink(missing_ok=True)
        shutil.rmtree(output, ignore_errors=True)

    print("Agent cycle demo configuration validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
