from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src" / "agent_io.c"
HEADER = ROOT / "include" / "minisnn_agent_io.h"
TEST = ROOT / "tests" / "test_agent_io.c"
MAKEFILE = ROOT / "Makefile"


def fail(message: str) -> None:
    print(f"C7 validation FAILED\n- {message}")
    raise SystemExit(1)


def main() -> None:
    for path in (SOURCE, HEADER, TEST):
        if not path.is_file():
            fail(f"arquivo obrigatorio ausente: {path.relative_to(ROOT)}")

    source = SOURCE.read_text(encoding="utf-8")
    header = HEADER.read_text(encoding="utf-8")
    test = TEST.read_text(encoding="utf-8")
    makefile = MAKEFILE.read_text(encoding="utf-8")

    for token in (
        "MiniSNNSensorSchema",
        "MiniSNNActionSchema",
        "MiniSNNAgentIOContext",
        "minisnn_agent_io_submit_sensor_frame",
        "minisnn_agent_io_consume_sensor_frame",
        "minisnn_agent_io_submit_action_frame",
        "minisnn_agent_io_finish_tick",
        "minisnn_agent_io_consume_action_frame",
        "minisnn_agent_io_contract_signature",
    ):
        if token not in header:
            fail(f"API C7.1 ausente: {token}")

    for token in (
        "AGENT_IO_SENSOR_SCHEMA_VERSION",
        "UINT64_C(14695981039346656037)",
        "UINT64_C(1099511628211)",
        "schema_signature",
        "write_schema_file",
        "read_schema_file",
        "frame_matches_schema",
        "MINISNN_AGENT_IO_ERROR_SENSOR_NOT_CONSUMED",
        "MINISNN_AGENT_IO_ERROR_PREVIOUS_ACTION_NOT_CONSUMED",
    ):
        if token not in source:
            fail(f"contrato C7.1 ausente: {token}")

    forbidden = (
        "world",
        "creature",
        "body",
        "food",
        "hunger",
        "position",
        "velocity",
        "species",
        "inventory",
        "movement",
        "map",
    )
    lowered = (source + "\n" + header).lower()
    for term in forbidden:
        if term in lowered:
            fail(f"termo de dominio proibido no codigo C7: {term}")

    if "isalnum(" in source or "#include <ctype.h>" in source:
        fail("serializacao de nomes C7 deve usar ASCII explicito, nao locale")

    for token in (
        "test_schema_contracts",
        "test_signatures_and_serialization",
        "test_frames_and_context",
        "MINISNN_AGENT_IO_ERROR_TICK_REPEATED",
        "MINISNN_AGENT_IO_ERROR_ACTION_BEFORE_SENSOR",
        "MINISNN_AGENT_IO_ERROR_SENSOR_ALREADY_CONSUMED",
        "MINISNN_AGENT_IO_ERROR_ACTION_ALREADY_CONSUMED",
        "MINISNN_AGENT_IO_ERROR_PREVIOUS_ACTION_NOT_CONSUMED",
        "test_ascii_names_and_reader_errors",
        "test_frame_public_errors",
        "UINT64_C(12815672321792322842)",
    ):
        if token not in test:
            fail(f"cobertura de teste C7.1 ausente: {token}")

    for target in ("test-agent-io", "check-c7"):
        if target not in makefile:
            fail(f"target Makefile ausente: {target}")

    print("C7.1 agent I/O contracts validation OK")


if __name__ == "__main__":
    main()
