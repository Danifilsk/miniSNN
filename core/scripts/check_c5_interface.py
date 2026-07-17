from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
GENERIC_FILES = (
    "src/plasticity.c", "src/plasticity.h", "src/reward.c", "src/reward.h",
    "src/homeostasis.c", "src/homeostasis.h", "src/structure.c", "src/structure.h",
    "app/evolution_runner.c",
)
MODEL_BOUNDARY_FILES = (
    "app/scenario_config.c", "app/scenario_runtime.c",
    "app/scenario_runner.c", "app/evolution_runner.c",
)


def main() -> int:
    offenders = [
        name for name in GENERIC_FILES
        if re.search(r"\b(?:const\s+)?LIFNeuron\s*\*", (ROOT / name).read_text(encoding="utf-8"))
    ]
    if offenders:
        print("C5 interface validation FAILED: generic LIFNeuron signature in " +
              ", ".join(offenders))
        return 1
    duplicate_converters = []
    for name in MODEL_BOUNDARY_FILES:
        text = (ROOT / name).read_text(encoding="utf-8")
        if "_stricmp" in text or re.search(
            r"static\s+.*(?:model_from_name|parse_neuron_model|model_name)\s*\(",
            text,
        ):
            duplicate_converters.append(name)
    if duplicate_converters:
        print("C5 interface validation FAILED: local model-name conversion in " +
              ", ".join(duplicate_converters))
        return 1
    runtime = (ROOT / "app/scenario_runtime.c").read_text(encoding="utf-8")
    if "out_config->neuron_model = config->neuron_model" not in runtime:
        print("C5 interface validation FAILED: runtime ignores ScenarioConfig.neuron_model")
        return 1
    forced_lif = re.search(
        r"(?:scenario_runtime|scenario_runner|evolution_runner)[\s\S]*?"
        r"neuron_model\s*=\s*MINISNN_NEURON_MODEL_LIF",
        "\n".join((ROOT / name).read_text(encoding="utf-8")
                  for name in MODEL_BOUNDARY_FILES[1:]),
    )
    if forced_lif:
        print("C5 interface validation FAILED: runner forces LIF")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
