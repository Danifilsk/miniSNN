from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def fail(message: str) -> int:
    print(f"Sensor encoding demo configuration FAILED\n- {message}")
    return 1


def run_demo(executable: Path, config: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(executable), str(config)],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )


def main() -> int:
    if len(sys.argv) != 2:
        return fail("uso: test_sensor_encoding_demo.py <sensor_encoding_demo.exe>")

    executable = Path(sys.argv[1]).resolve()
    build_dir = ROOT / "build"
    alt_config = build_dir / "sensor_encoding_alternate.ini"
    invalid_config = build_dir / "sensor_encoding_invalid.ini"
    invalid_phase_config = build_dir / "sensor_encoding_invalid_phase.ini"
    output_dir = ROOT / "results" / "scenarios" / "sensor_encoding_alternate"
    alternate = """# This source file intentionally has comments and spacing.\n\n[run]\nrun_name = sensor_encoding_alternate\n\n[network]\nneurons = 7\nmodel = AdEx\n\n[sensor_encoder]\nbrain_steps_per_tick = 2\nrate_unit = pulses_per_neural_step\n\n[sensors]\nsignal_a = 0.0, 1.0, 0.25\nsignal_b = -1.0, 1.0, 0.0\nsignal_c = 0.0, 1.0, 0.0\n\n[mappings]\nsignal_a = linear_current, 0, 2, 40.0, 0.0\nsignal_b = bipolar_current, 2, 2, 30.0, 0.0\nsignal_c = deterministic_rate, 4, 2, 50.0, 0.5, 999\n"""

    if not executable.is_file():
        return fail(f"executavel ausente: {executable}")
    build_dir.mkdir(exist_ok=True)
    shutil.rmtree(output_dir, ignore_errors=True)
    try:
        alt_config.write_bytes(alternate.encode("ascii"))
        result = run_demo(executable, alt_config)
        if result.returncode != 0:
            return fail(f"demo alternativo falhou: {result.stdout}{result.stderr}")

        source = output_dir / "config_source.ini"
        used = output_dir / "config_used.ini"
        summary = output_dir / "sensor_encoding_summary.txt"
        trace = output_dir / "sensor_encoding_trace.csv"
        report = output_dir / "sensor_encoding_report.html"
        for path in (source, used, summary, trace, report):
            if not path.is_file():
                return fail(f"saida obrigatoria ausente: {path.name}")
        if source.read_bytes() != alt_config.read_bytes():
            return fail("config_source.ini nao preservou os bytes da fonte")
        used_text = used.read_text(encoding="utf-8")
        if used.read_bytes() == source.read_bytes() or "model = adex" not in used_text or \
                "neurons = 7" not in used_text or "phase_offset" in used_text:
            return fail("config_used.ini nao e a configuracao canonica efetiva")
        summary_text = summary.read_text(encoding="utf-8")
        if "model=adex" not in summary_text or "neurons=7" not in summary_text or \
                "brain_steps_per_tick=2" not in summary_text:
            return fail("configuracao alternativa nao mudou a execucao")
        if "2,signal_a,1.000000,1.000000,linear_current,0,0,40.000000" not in \
                trace.read_text(encoding="utf-8"):
            return fail("corrente do mapping alternativo nao foi aplicada")

        invalid_config.write_text(alternate + "\n[network]\nunknown_key = 1\n", encoding="ascii")
        invalid_result = run_demo(executable, invalid_config)
        if invalid_result.returncode == 0 or "Erro ao carregar configuracao" not in invalid_result.stdout:
            return fail("chave desconhecida nao foi rejeitada claramente")
        invalid_phase_config.write_text(
            alternate.replace("50.0, 0.5, 999", "50.0, 0.5, 1000"), encoding="ascii"
        )
        invalid_phase_result = run_demo(executable, invalid_phase_config)
        if invalid_phase_result.returncode == 0 or "Erro ao carregar configuracao" not in \
                invalid_phase_result.stdout:
            return fail("phase_offset invalido nao foi rejeitado claramente")
    finally:
        alt_config.unlink(missing_ok=True)
        invalid_config.unlink(missing_ok=True)
        invalid_phase_config.unlink(missing_ok=True)
        shutil.rmtree(output_dir, ignore_errors=True)

    print("Sensor encoding demo configuration validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
