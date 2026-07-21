from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def fail(message: str) -> int:
    print(f"Agent cycle checkpoint demo validation FAILED\n- {message}")
    return 1


def run(executable: Path, config: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(executable), str(config)], cwd=ROOT, text=True,
        capture_output=True, check=False,
    )


def main() -> int:
    if len(sys.argv) != 2:
        return fail("uso: test_agent_cycle_checkpoint_demo.py <agent_cycle_checkpoint_demo.exe>")

    executable = Path(sys.argv[1]).resolve()
    build = ROOT / "build"
    config = build / "agent_cycle_checkpoint_alternate.ini"
    invalid = build / "agent_cycle_checkpoint_invalid.ini"
    output = ROOT / "results" / "scenarios" / "agent_cycle_checkpoint_alternate"
    source = """# Exact source bytes are preserved.\n[run]\nrun_name = agent_cycle_checkpoint_alternate\n\n[network]\nneurons = 3\nmodel = AdEx\nbrain_steps_per_tick = 2\n\n[demo]\nticks = 5\nreset_interval = 5\ninput_current = 3500.0\n"""

    if not executable.is_file():
        return fail(f"executavel ausente: {executable}")
    build.mkdir(exist_ok=True)
    shutil.rmtree(output, ignore_errors=True)
    try:
        config.write_bytes(source.encode("ascii"))
        result = run(executable, config)
        if result.returncode != 0:
            return fail(f"demo alternativo falhou: {result.stdout}{result.stderr}")
        expected = {
            "config_source.ini",
            "config_used.ini",
            "checkpoint_manifest_copy.txt",
            "continuous_trace.csv",
            "ready_resume_trace.csv",
            "action_pending_resume_trace.csv",
            "checkpoint_comparison.csv",
            "agent_cycle_checkpoint_summary.txt",
            "agent_cycle_checkpoint_report.html",
        }
        missing = [name for name in expected if not (output / name).is_file()]
        if missing:
            return fail(f"saidas obrigatorias ausentes: {', '.join(missing)}")
        for checkpoint in ("checkpoint_ready", "checkpoint_action_pending"):
            for name in (
                "network_state.bin",
                "agent_io_state.bin",
                "sensor_encoder_state.bin",
                "action_decoder_state.bin",
                "agent_cycle_state.bin",
                "manifest.txt",
            ):
                if not (output / checkpoint / name).is_file():
                    return fail(f"componente ausente: {checkpoint}/{name}")
        if (output / "config_source.ini").read_bytes() != config.read_bytes():
            return fail("config_source.ini nao preservou o arquivo original")
        used = (output / "config_used.ini").read_text(encoding="utf-8")
        summary = (output / "agent_cycle_checkpoint_summary.txt").read_text(
            encoding="utf-8")
        comparison = (output / "checkpoint_comparison.csv").read_text(encoding="utf-8")
        report = (output / "agent_cycle_checkpoint_report.html").read_text(
            encoding="utf-8")
        manifest = (output / "checkpoint_manifest_copy.txt").read_text(encoding="utf-8")
        if "model = adex" not in used or "neurons = 3" not in used or \
                "brain_steps_per_tick = 2" not in used:
            return fail("config_used.ini nao registrou a configuracao efetiva")
        if "replay_equivalent=yes" not in summary or comparison.count("yes") != 5:
            return fail("comparacao de replay nao foi equivalente")
        for token in (
            "ACTION_PENDING",
            "checkpoint_manifest_copy.txt",
            "Boundary and ownership",
            "Persisted state",
            "component integrity",
            "deterministic-rate",
            "Limitations",
        ):
            if token not in report:
                return fail(f"relatorio nao descreveu checkpoint: {token}")
        if "format_version=1" not in manifest or "core_provenance=miniSNN_core_c7" not in manifest:
            return fail("manifesto nao registrou formato e proveniencia")

        invalid.write_text(source + "\n[demo]\nunknown_key = 1\n", encoding="ascii")
        invalid_result = run(executable, invalid)
        if invalid_result.returncode == 0 or "Erro ao carregar configuracao" not in invalid_result.stdout:
            return fail("chave desconhecida nao foi rejeitada pelo parser")
    finally:
        config.unlink(missing_ok=True)
        invalid.unlink(missing_ok=True)
        shutil.rmtree(output, ignore_errors=True)

    print("Agent cycle checkpoint demo validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
