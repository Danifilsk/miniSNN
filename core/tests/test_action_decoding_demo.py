from pathlib import Path
import shutil
import subprocess
import sys


CORE = Path(__file__).resolve().parents[1]


def fail(message: str) -> None:
    raise AssertionError(message)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: test_action_decoding_demo.py <demo.exe>")
        return 2

    executable = Path(sys.argv[1]).resolve()
    alternate = CORE / "build" / "action_decoding_alternate.ini"
    result_dir = CORE / "results" / "scenarios" / "action_decoding_alternate"
    invalid = CORE / "build" / "action_decoding_invalid.ini"
    alternate_text = """# Source formatting must survive byte-for-byte.\n[run]\nrun_name = action_decoding_alternate\n\n[network]\nneurons = 14\nmodel = LiF\n\n[action_decoder]\nbrain_steps_per_tick = 5\n\n[actions]\ncontinuous_signal = 0.0, 1.0, 0.2\nsigned_signal = -1.0, 1.0, 0.0\ntrigger_signal = 0.0, 1.0, 0.0\nchoice_a = 0.0, 1.0, 0.5\nchoice_b = 0.0, 1.0, 0.5\n\n[mappings]\ncontinuous_signal = population_rate,0,2,0.0,1.0\nsigned_signal = bipolar_difference,2,2,4,2\ntrigger_signal = threshold,6,2,0.5,1.0,0.0\nchoice_a = wta_member,8,2,1,0.25,0.0,1.0,0.0\nchoice_b = wta_member,10,2,1,0.25,0.0,1.0,0.0\n"""

    try:
        alternate.write_bytes(alternate_text.encode("utf-8"))
        completed = subprocess.run([str(executable), str(alternate)], cwd=CORE,
                                   text=True, capture_output=True, check=False)
        if completed.returncode != 0:
            fail(f"alternate configuration failed: {completed.stdout}\n{completed.stderr}")
        source = result_dir / "config_source.ini"
        used = result_dir / "config_used.ini"
        summary = result_dir / "action_decoding_summary.txt"
        activity = result_dir / "action_decoding_activity.csv"
        trace = result_dir / "action_decoding_trace.csv"
        decoder = result_dir / "action_decoder.txt"
        report = result_dir / "action_decoding_report.html"
        for path in (source, used, summary, activity, trace, decoder, report):
            if not path.is_file():
                fail(f"missing output: {path.name}")
        if source.read_bytes() != alternate.read_bytes():
            fail("config_source.ini did not preserve the input bytes")
        used_text = used.read_text(encoding="utf-8")
        if used_text == alternate_text or "neurons = 14" not in used_text or "model = lif" not in used_text:
            fail("config_used.ini is not canonical effective configuration")
        summary_text = summary.read_text(encoding="utf-8")
        if "neurons=14" not in summary_text or "brain_steps_per_tick=5" not in summary_text:
            fail("alternate configuration did not change execution dimensions")
        if len(activity.read_text(encoding="utf-8").splitlines()) != 1 + 4 * 5 * 14:
            fail("alternate activity dimensions were not applied")
        if "tick,action_channel,mode,raw_score,confidence,selected,decoded_value" not in trace.read_text(encoding="utf-8"):
            fail("trace schema missing")

        invalid.write_text("[run]\nrun_name = invalid\n[network]\nunknown_key = 1\n",
                           encoding="utf-8")
        invalid_run = subprocess.run([str(executable), str(invalid)], cwd=CORE,
                                     text=True, capture_output=True, check=False)
        if invalid_run.returncode == 0 or "Erro ao carregar configuracao" not in invalid_run.stdout:
            fail("unknown INI key was not rejected clearly")
    finally:
        alternate.unlink(missing_ok=True)
        invalid.unlink(missing_ok=True)
        shutil.rmtree(result_dir, ignore_errors=True)

    print("Action decoding demo configuration validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
