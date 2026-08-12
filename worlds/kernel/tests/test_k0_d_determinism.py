from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import sys


ARTIFACTS = ("trace.csv", "events.csv", "manifest.ini", "report.txt")


def run(command: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, text=True, capture_output=True, check=False)


def fail(message: str) -> int:
    print(f"K0-D deterministic scenario validation FAILED: {message}")
    return 1


def artifact_bytes(directory: Path) -> tuple[bytes, ...]:
    return tuple((directory / name).read_bytes() for name in ARTIFACTS)


def write_equivalent_config(path: Path) -> None:
    path.write_bytes(
        b"; semantic variation\r\n"
        b"[scenario]\r\nrandom_draws_per_tick = 1\r\nrandom_stream = 1\r\n"
        b"random_namespace = 1\r\ntrace_interval = 1\r\npriority_base = 100\r\n"
        b"command_delay = 1\r\ndestroy_batch = 1\r\ndestroy_interval = 7\r\n"
        b"create_batch = 2\r\ncreate_interval = 5\r\nmaximum_alive_entities = 32\r\n"
        b"minimum_alive_entities = 4\r\ninitial_entities = 10\r\nticks = 73\r\n"
        b"scenario_id = k0_integrated_demo\r\nscenario_version = 1\r\n\r\n"
        b"[kernel]\r\nmaster_seed = 12345\r\nconfig_version = 1"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Valida determinismo integrado K0-D.")
    parser.add_argument("--runner", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--repository-root", required=True)
    args = parser.parse_args()

    runner = Path(args.runner).resolve()
    config = Path(args.config).resolve()
    output = Path(args.output_dir).resolve()
    repository = Path(args.repository_root).resolve()
    kernel = config.parents[1]
    equivalent = output / "equivalent.ini"
    different = output / "different.ini"
    outputs = [output / name for name in ("one", "two", "three", "equivalent", "different")]

    shutil.rmtree(output, ignore_errors=True)
    output.mkdir(parents=True)
    write_equivalent_config(equivalent)
    different.write_text(config.read_text(encoding="ascii").replace("master_seed=12345", "master_seed=12346"), encoding="ascii")
    try:
        invocations = (
            ("configs/k0_integrated_demo.ini", outputs[0], kernel),
            (str(config), outputs[1], repository),
            (str(config), outputs[2], kernel),
            (str(equivalent), outputs[3], repository),
        )
        console_outputs: list[str] = []
        for config_argument, destination, cwd in invocations:
            result = run([str(runner), "--config", config_argument, "--output", str(destination)], cwd)
            if result.returncode != 0 or "status=OK" not in result.stdout:
                return fail(f"runner failed: {result.stdout}{result.stderr}")
            console_outputs.append(result.stdout)
        baseline = artifact_bytes(outputs[0])
        if any(artifact_bytes(destination) != baseline for destination in outputs[1:4]):
            return fail("same semantic config produced different canonical artifacts")
        if any("same_seed_repeat_match=yes" not in text or "different_seed_diverged=yes" not in text for text in console_outputs):
            return fail("runner did not confirm deterministic repetition")
        result = run([str(runner), "--config", str(different), "--output", str(outputs[4])], kernel)
        if result.returncode != 0 or artifact_bytes(outputs[4]) == baseline:
            return fail("different seed did not diverge")
    finally:
        shutil.rmtree(output, ignore_errors=True)
    print("K0-D deterministic scenario validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
