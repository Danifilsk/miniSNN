from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import sys


def fail(message: str) -> int:
    print(f"K0-D corruption validation FAILED: {message}")
    return 1


def invalid_config(base: str, replacement: tuple[str, str] | None = None) -> str:
    if replacement is None:
        return base
    return base.replace(replacement[0], replacement[1])


def main() -> int:
    parser = argparse.ArgumentParser(description="Valida rejeicoes do parser K0-D.")
    parser.add_argument("--runner", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    runner = Path(args.runner).resolve()
    base = Path(args.config).read_text(encoding="ascii")
    output = Path(args.output_dir).resolve()
    cases: dict[str, str] = {
        "empty": "",
        "missing_section": base.replace("[kernel]", "[kernell]", 1),
        "duplicate_section": base + "\n[scenario]\n",
        "unknown_key": base + "unknown_key=1\n",
        "duplicate_key": base + "\n[kernel]\nmaster_seed=2\n",
        "unknown_version": invalid_config(base, ("config_version=1", "config_version=2")),
        "negative": invalid_config(base, ("ticks=73", "ticks=-1")),
        "plus": invalid_config(base, ("ticks=73", "ticks=+73")),
        "overflow": invalid_config(base, ("ticks=73", "ticks=18446744073709551616")),
        "trailing": invalid_config(base, ("ticks=73", "ticks=73xyz")),
        "slash_id": invalid_config(base, ("scenario_id=k0_integrated_demo", "scenario_id=bad/name")),
        "parent_id": invalid_config(base, ("scenario_id=k0_integrated_demo", "scenario_id=..")),
        "minimum": invalid_config(base, ("minimum_alive_entities=4", "minimum_alive_entities=33")),
        "initial": invalid_config(base, ("initial_entities=10", "initial_entities=33")),
        "zero_interval": invalid_config(base, ("create_interval=5", "create_interval=0")),
        "zero_delay": invalid_config(base, ("command_delay=1", "command_delay=0")),
        "tick_limit": invalid_config(base, ("ticks=73", "ticks=1000001")),
        "isolated_cr": base.replace("ticks=73\n", "ticks=73\rX\n"),
        "line_long": base + "#" + "x" * 600 + "\n",
        "file_large": base + "#" + "x" * 66000,
    }
    shutil.rmtree(output, ignore_errors=True)
    output.mkdir(parents=True)
    try:
        for name, content in cases.items():
            config = output / f"{name}.ini"
            config.write_bytes(content.encode("ascii"))
            result = subprocess.run([str(runner), "--config", str(config), "--validate-only"], text=True, capture_output=True, check=False)
            if result.returncode == 0 or "error: config:" not in result.stderr:
                return fail(f"invalid case accepted: {name}")
    finally:
        shutil.rmtree(output, ignore_errors=True)
    print("K0-D corruption validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
