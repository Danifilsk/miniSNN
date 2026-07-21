from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src" / "agent_io.c"
HEADER = ROOT / "include" / "minisnn_agent_io.h"
TEST = ROOT / "tests" / "test_agent_io.c"
ENCODER_SOURCE = ROOT / "src" / "sensor_encoder.c"
ENCODER_HEADER = ROOT / "include" / "minisnn_sensor_encoder.h"
ENCODER_TEST = ROOT / "tests" / "test_sensor_encoder.c"
DEMO_CONFIG_SOURCE = ROOT / "app" / "sensor_encoding_demo_config.c"
DEMO_CONFIG_HEADER = ROOT / "app" / "sensor_encoding_demo_config.h"
DEMO_TEST = ROOT / "tests" / "test_sensor_encoding_demo.py"
DECODER_SOURCE = ROOT / "src" / "action_decoder.c"
DECODER_HEADER = ROOT / "include" / "minisnn_action_decoder.h"
DECODER_TEST = ROOT / "tests" / "test_action_decoder.c"
DECODER_DEMO = ROOT / "app" / "action_decoding_demo.c"
DECODER_CONFIG_SOURCE = ROOT / "app" / "action_decoding_demo_config.c"
DECODER_CONFIG_HEADER = ROOT / "app" / "action_decoding_demo_config.h"
DECODER_DEMO_TEST = ROOT / "tests" / "test_action_decoding_demo.py"
MAKEFILE = ROOT / "Makefile"


def fail(message: str) -> None:
    print(f"C7 validation FAILED\n- {message}")
    raise SystemExit(1)


def main() -> None:
    for path in (SOURCE, HEADER, TEST, ENCODER_SOURCE, ENCODER_HEADER, ENCODER_TEST,
                 DEMO_CONFIG_SOURCE, DEMO_CONFIG_HEADER, DEMO_TEST, DECODER_SOURCE,
                 DECODER_HEADER, DECODER_TEST, DECODER_DEMO, DECODER_CONFIG_SOURCE,
                 DECODER_CONFIG_HEADER, DECODER_DEMO_TEST):
        if not path.is_file():
            fail(f"arquivo obrigatorio ausente: {path.relative_to(ROOT)}")

    source = SOURCE.read_text(encoding="utf-8")
    header = HEADER.read_text(encoding="utf-8")
    test = TEST.read_text(encoding="utf-8")
    encoder_source = ENCODER_SOURCE.read_text(encoding="utf-8")
    encoder_header = ENCODER_HEADER.read_text(encoding="utf-8")
    encoder_test = ENCODER_TEST.read_text(encoding="utf-8")
    demo_config_source = DEMO_CONFIG_SOURCE.read_text(encoding="utf-8")
    demo_test = DEMO_TEST.read_text(encoding="utf-8")
    decoder_source = DECODER_SOURCE.read_text(encoding="utf-8")
    decoder_header = DECODER_HEADER.read_text(encoding="utf-8")
    decoder_test = DECODER_TEST.read_text(encoding="utf-8")
    decoder_demo = DECODER_DEMO.read_text(encoding="utf-8")
    decoder_config_source = DECODER_CONFIG_SOURCE.read_text(encoding="utf-8")
    decoder_demo_test = DECODER_DEMO_TEST.read_text(encoding="utf-8")
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
        "MiniSNNSensorEncoder",
        "MiniSNNSensorEncodingSpec",
        "MiniSNNNeuralInputFrame",
        "MINISNN_SENSOR_ENCODING_LINEAR_CURRENT",
        "MINISNN_SENSOR_ENCODING_BIPOLAR_CURRENT",
        "MINISNN_SENSOR_ENCODING_DETERMINISTIC_RATE",
        "minisnn_sensor_encoder_encode_frame",
        "minisnn_sensor_encoder_encode_from_agent_io",
        "minisnn_neural_input_frame_apply_step",
        "MiniSNNSensorEncoderError *out_error",
        "minisnn_sensor_encoder_write_file",
        "minisnn_sensor_encoder_read_file",
    ):
        if token not in encoder_header:
            fail(f"API C7.2 ausente: {token}")

    for token in (
        "SENSOR_ENCODER_FNV_OFFSET",
        "UINT64_C(14695981039346656037)",
        "UINT64_C(1099511628211)",
        "scratch_currents",
        "next_phases",
        "minisnn_clear_inputs",
        "minisnn_set_input",
        "minisnn_agent_io_consume_sensor_frame",
        "mapping_signature",
        "contract_signature",
        "spec->phase_offset >= SENSOR_ENCODER_PHASE_SCALE",
    ):
        if token not in encoder_source:
            fail(f"contrato C7.2 ausente: {token}")

    encoder_forbidden = forbidden + (
        "lifneuron",
        "minisnn_step(",
        "reward",
        "action",
    )
    lowered_encoder = (encoder_source + "\n" + encoder_header).lower()
    for term in encoder_forbidden:
        if re.search(rf"\b{re.escape(term)}\b", lowered_encoder):
            fail(f"termo proibido no encoder C7.2: {term}")
    if ".name" in encoder_source or "channel_name" in encoder_source:
        fail("encoder C7.2 nao pode depender do nome de canal")
    if "phase_offset % SENSOR_ENCODER_PHASE_SCALE" in encoder_source:
        fail("phase_offset C7.2 nao pode aceitar aliases por modulo")

    for token in (
        "sensor_encoding_demo_config_load_file",
        "sensor_encoding_demo_config_write_file",
        "neuron_model_from_name",
        "sensor_channel_id = config->sensors[sensor_index].id",
        "chave ou valor de network invalido",
    ):
        if token not in demo_config_source:
            fail(f"parser efetivo do demo ausente: {token}")

    for token in (
        "test_linear_bipolar_and_ranges",
        "test_rate_reset_atomicity_and_names",
        "test_agent_io_apply_and_serialization",
        "test_constant_channel_and_models",
        "test_phase_offset_and_apply_errors",
        "1000U",
        "UINT64_C(7996300235072591673)",
        "UINT64_C(8203056402127860223)",
    ):
        if token not in encoder_test:
            fail(f"cobertura de teste C7.2 ausente: {token}")

    for token in (
        "config_source.ini",
        "config_used.ini",
        "sensor_encoding_alternate",
        "unknown_key",
    ):
        if token not in demo_test:
            fail(f"cobertura de configuracao C7.2 ausente: {token}")

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

    for token in (
        "MiniSNNNeuralActivityFrame",
        "MiniSNNActionDecodingSpec",
        "MINISNN_ACTION_DECODING_POPULATION_RATE",
        "MINISNN_ACTION_DECODING_BIPOLAR_DIFFERENCE",
        "MINISNN_ACTION_DECODING_THRESHOLD",
        "MINISNN_ACTION_DECODING_WTA_MEMBER",
        "minisnn_neural_activity_frame_capture_step",
        "minisnn_action_decoder_decode",
        "minisnn_action_decoder_decode_to_agent_io",
        "minisnn_action_decoder_write_file",
        "minisnn_action_decoder_read_file",
    ):
        if token not in decoder_header:
            fail(f"API C7.3 ausente: {token}")
    if "minisnn_agent_io_action_schema_signature" not in header:
        fail("accessor de assinatura de action schema C7.3 ausente")

    for token in (
        "ACTION_DECODER_FNV_OFFSET",
        "UINT64_C(14695981039346656037)",
        "population_rate",
        "minimum_confidence",
        "minisnn_get_spike",
        "minisnn_agent_io_action_schema_signature",
        "minisnn_agent_io_submit_action_frame",
        "ACTION_DECODER_TEXT_VERSION",
        "fgetc(file) != EOF",
    ):
        if token not in decoder_source:
            fail(f"contrato C7.3 ausente: {token}")

    decoder_forbidden = tuple(term for term in forbidden if term != "map") + (
        "lifneuron", "minisnn_step(", "reward", "sensor_encoder", "finish_tick(",
    )
    lowered_decoder = (decoder_source + "\n" + decoder_header).lower()
    for term in decoder_forbidden:
        if re.search(rf"\b{re.escape(term)}", lowered_decoder):
            fail(f"termo proibido no decoder C7.3: {term}")
    if ".name" in decoder_source or "channel_name" in decoder_source:
        fail("decoder C7.3 nao pode depender do nome de canal")

    for token in (
        "action_decoding_demo_config_load_file",
        "action_decoding_demo_config_write_file",
        "neuron_model_from_name",
        "spec->action_channel_id = config->actions[action_index].id",
    ):
        if token not in decoder_config_source:
            fail(f"parser efetivo do demo C7.3 ausente: {token}")
    for token in (
        "test_activity_frame", "test_modes_atomicity_and_signatures",
        "test_defaults_agent_io_and_capture", "test_creation_contracts_and_signatures",
        "test_wta_contracts", "test_agent_io_schema_signature_contract",
        "test_file_rejections", "UINT64_C(1630198257262049785)",
    ):
        if token not in decoder_test:
            fail(f"cobertura de teste C7.3 ausente: {token}")
    for token in ("config_source.ini", "config_used.ini", "action_decoding_alternate"):
        if token not in decoder_demo_test:
            fail(f"proveniencia de demo C7.3 ausente: {token}")
    if "minisnn_step(network)" not in decoder_demo:
        fail("demo C7.3 nao produz atividade pela API publica")

    for target in ("test-agent-io", "test-sensor-encoder", "scenario-sensor-encoding",
                   "test-action-decoder", "scenario-action-decoding", "check-c7"):
        if target not in makefile:
            fail(f"target Makefile ausente: {target}")

    print("C7.3 action decoding validation OK")


if __name__ == "__main__":
    main()
