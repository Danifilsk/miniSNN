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
AGENT_CYCLE_HEADER = ROOT / "include" / "minisnn_agent_cycle.h"
AGENT_CYCLE_SOURCE = ROOT / "src" / "agent_cycle.c"
AGENT_CYCLE_TEST = ROOT / "tests" / "test_agent_cycle.c"
AGENT_CYCLE_DEMO = ROOT / "app" / "agent_cycle_demo.c"
AGENT_CYCLE_CONFIG_SOURCE = ROOT / "app" / "agent_cycle_demo_config.c"
AGENT_CYCLE_DEMO_TEST = ROOT / "tests" / "test_agent_cycle_demo.py"
CHECKPOINT_SOURCE = ROOT / "src" / "agent_cycle_checkpoint.c"
CHECKPOINT_INTERNAL_HEADER = ROOT / "src" / "agent_cycle_checkpoint_internal.h"
CHECKPOINT_TEST = ROOT / "tests" / "test_agent_cycle_checkpoint.c"
CHECKPOINT_DEMO = ROOT / "app" / "agent_cycle_checkpoint_demo.c"
CHECKPOINT_CONFIG_SOURCE = ROOT / "app" / "agent_cycle_checkpoint_demo_config.c"
CHECKPOINT_CONFIG_HEADER = ROOT / "app" / "agent_cycle_checkpoint_demo_config.h"
CHECKPOINT_DEMO_TEST = ROOT / "tests" / "test_agent_cycle_checkpoint_demo.py"
MAKEFILE = ROOT / "Makefile"


def fail(message: str) -> None:
    print(f"C7 validation FAILED\n- {message}")
    raise SystemExit(1)


def main() -> None:
    for path in (SOURCE, HEADER, TEST, ENCODER_SOURCE, ENCODER_HEADER, ENCODER_TEST,
                 DEMO_CONFIG_SOURCE, DEMO_CONFIG_HEADER, DEMO_TEST, DECODER_SOURCE,
                 DECODER_HEADER, DECODER_TEST, DECODER_DEMO, DECODER_CONFIG_SOURCE,
                 DECODER_CONFIG_HEADER, DECODER_DEMO_TEST, AGENT_CYCLE_HEADER,
                 AGENT_CYCLE_SOURCE, AGENT_CYCLE_TEST, AGENT_CYCLE_DEMO,
                 AGENT_CYCLE_CONFIG_SOURCE, AGENT_CYCLE_DEMO_TEST, CHECKPOINT_SOURCE,
                 CHECKPOINT_INTERNAL_HEADER, CHECKPOINT_TEST, CHECKPOINT_DEMO,
                 CHECKPOINT_CONFIG_SOURCE, CHECKPOINT_CONFIG_HEADER, CHECKPOINT_DEMO_TEST):
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
    agent_cycle_header = AGENT_CYCLE_HEADER.read_text(encoding="utf-8")
    agent_cycle_source = AGENT_CYCLE_SOURCE.read_text(encoding="utf-8")
    agent_cycle_test = AGENT_CYCLE_TEST.read_text(encoding="utf-8")
    agent_cycle_demo = AGENT_CYCLE_DEMO.read_text(encoding="utf-8")
    agent_cycle_config_source = AGENT_CYCLE_CONFIG_SOURCE.read_text(encoding="utf-8")
    agent_cycle_demo_test = AGENT_CYCLE_DEMO_TEST.read_text(encoding="utf-8")
    checkpoint_source = CHECKPOINT_SOURCE.read_text(encoding="utf-8")
    checkpoint_internal_header = CHECKPOINT_INTERNAL_HEADER.read_text(encoding="utf-8")
    checkpoint_test = CHECKPOINT_TEST.read_text(encoding="utf-8")
    checkpoint_demo = CHECKPOINT_DEMO.read_text(encoding="utf-8")
    checkpoint_config_source = CHECKPOINT_CONFIG_SOURCE.read_text(encoding="utf-8")
    checkpoint_demo_test = CHECKPOINT_DEMO_TEST.read_text(encoding="utf-8")
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
        "test_atomic_action_publication",
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

    for token in (
        "MiniSNNAgentCycle", "MINISNN_AGENT_CYCLE_STATE_FAULTED",
        "MiniSNNAgentFeedback", "MiniSNNAgentCycleDiagnostics",
        "minisnn_agent_cycle_run_tick", "minisnn_agent_cycle_submit_feedback",
        "minisnn_agent_cycle_reset_episode",
    ):
        if token not in agent_cycle_header:
            fail(f"API C7.4 ausente: {token}")

    for token in (
        "minisnn_sensor_encoder_encode_from_agent_io",
        "minisnn_neural_input_frame_apply_step", "minisnn_step(cycle->network)",
        "minisnn_neural_activity_frame_capture_step", "minisnn_action_decoder_decode",
        "minisnn_agent_io_submit_action_and_finish_tick",
        "MINISNN_AGENT_CYCLE_STATE_FAULTED", "deliver_due_feedback",
        "deliver_terminal_feedback", "minisnn_apply_pending_reward_now",
        "minisnn_reset_transient_state", "episode_terminal > 1U",
    ):
        if token not in agent_cycle_source:
            fail(f"contrato C7.4 ausente: {token}")

    cycle_code = re.sub(r"/\*.*?\*/|//[^\n]*", "", agent_cycle_source,
                        flags=re.DOTALL)
    for term in forbidden + ("lifneuron", "minisnn_agent_io_consume_action_frame"):
        if re.search(rf"\b{re.escape(term)}\b", cycle_code.lower()):
            fail(f"termo ou consumo proibido no ciclo C7.4: {term}")
    if "minisnn_step(" in encoder_source or "minisnn_step(" in decoder_source:
        fail("somente agent_cycle pode avancar a rede na camada C7")
    if "minisnn_agent_io_submit_action_and_finish_tick" not in source:
        fail("publicacao atomica de acao C7.4 ausente no AgentIO")

    for token in (
        "test_creation_contracts", "test_tick_feedback_and_reset",
        "test_fault_and_models", "test_reward_delivery_contract",
        "test_terminal_feedback_contract", "test_structural_transient_reset",
        "neuron_model_test_fail_after_calls",
        "MINISNN_AGENT_CYCLE_ERROR_REWARD_UNAVAILABLE",
    ):
        if token not in agent_cycle_test:
            fail(f"cobertura de teste C7.4 ausente: {token}")
    for token in (
        "agent_cycle_demo_config_load_file", "minisnn_agent_cycle_run_tick",
        "minisnn_agent_cycle_submit_feedback", "config_source.ini", "config_used.ini",
        "delivered_at_episode_boundary", "delivered_on_tick",
    ):
        if token not in agent_cycle_demo:
            fail(f"demo C7.4 ausente: {token}")
    if "neuron_model_from_name" not in agent_cycle_config_source:
        fail("parser do demo C7.4 nao usa conversao central de modelos")
    for token in ("agent_cycle_alternate", "config_source.ini", "agent_cycle_trace.csv",
                  "delivered_at_episode_boundary", "total_reward=1"):
        if token not in agent_cycle_demo_test:
            fail(f"teste de proveniencia C7.4 ausente: {token}")

    for token in (
        "minisnn_agent_cycle_save_checkpoint",
        "minisnn_agent_cycle_load_checkpoint",
        "MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_UNSTABLE",
        "MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_SIGNATURE",
        "MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_INCOMPATIBLE",
    ):
        if token not in agent_cycle_header:
            fail(f"API C7.5-A ausente: {token}")
    for token in (
        "CYCLE_NETWORK_VERSION", "CYCLE_FNV_OFFSET", "CYCLE_FNV_PRIME",
        "UINT64_C(14695981039346656037)", "UINT64_C(1099511628211)",
        "read_unsigned_long_long", "_Static_assert(sizeof(uint64_t) == 8",
        "write_model", "write_structural_config", "write_structural_stats",
        "write_genome", "minisnn_network_checkpoint_write",
        "minisnn_network_checkpoint_load", "pending_current", "pre_trace",
        "eligibility", "rate_traces", "initial_topology",
    ):
        if token not in checkpoint_source:
            fail(f"persistencia de rede C7.5-A ausente: {token}")
    for token in (
        "minisnn_agent_io_checkpoint_write", "minisnn_sensor_encoder_checkpoint_write",
        "minisnn_action_decoder_checkpoint_write", "minisnn_network_checkpoint_write",
    ):
        if token not in checkpoint_internal_header:
            fail(f"contrato interno C7.5-A ausente: {token}")
    for token in (
        "checkpoint_component_hashes_match", "checkpoint_contract_signature",
        "checkpoint_create_rollback", "checkpoint_restore_rollback",
        "ACTION_PENDING", "checkpoint_directory_is_safe",
        "minisnn_sensor_encoder_checkpoint_load",
    ):
        if token not in agent_cycle_source:
            fail(f"integridade ou atomicidade C7.5-A ausente: {token}")
    for token in (
        "test_ready_resume", "test_action_pending_resume_and_corruption",
        "test_unstable_save_rejected", "test_corrupt_component_is_atomic",
        "test_terminal_and_reset_after_load", "test_incompatible_destination_is_atomic",
        "test_unsigned_long_long_round_trip",
        "MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP",
        "MINISNN_SENSOR_ENCODING_DETERMINISTIC_RATE",
        "minisnn_test_sensor_encoder_phase", "minisnn_test_agent_cycle_feedback_at",
    ):
        if token not in checkpoint_test:
            fail(f"cobertura de checkpoint C7.5-A ausente: {token}")
    for token in (
        "agent_cycle_checkpoint_demo_config_load_file",
        "minisnn_agent_cycle_save_checkpoint", "minisnn_agent_cycle_load_checkpoint",
        "checkpoint_manifest_copy.txt", "ACTION_PENDING", "input_a", "output_a",
    ):
        if token not in checkpoint_demo:
            fail(f"demo C7.5-A ausente: {token}")
    if "agent_cycle_demo_config_load_file" not in checkpoint_config_source:
        fail("parser do demo C7.5-A nao reutiliza a configuracao auditada")
    for token in (
        "config_source.ini", "config_used.ini", "checkpoint_comparison.csv",
        "checkpoint_ready", "checkpoint_action_pending", "replay_equivalent=yes",
    ):
        if token not in checkpoint_demo_test:
            fail(f"teste de proveniencia C7.5-A ausente: {token}")
    checkpoint_code = re.sub(r"/\*.*?\*/|//[^\n]*", "", checkpoint_source + "\n" +
                             checkpoint_internal_header, flags=re.DOTALL).lower()
    for term in forbidden:
        if re.search(rf"\b{re.escape(term)}\b", checkpoint_code):
            fail(f"termo de dominio proibido no checkpoint C7.5-A: {term}")
    if "fwrite(&" in checkpoint_source or "fread(&" in checkpoint_source:
        fail("checkpoint C7.5-A nao pode serializar structs ou ponteiros brutos")

    for target in ("test-agent-io", "test-sensor-encoder", "scenario-sensor-encoding",
                   "test-action-decoder", "scenario-action-decoding", "test-agent-cycle",
                   "scenario-agent-cycle", "test-agent-cycle-checkpoint",
                   "scenario-agent-cycle-checkpoint", "check-c7"):
        if target not in makefile:
            fail(f"target Makefile ausente: {target}")

    print("C7.5-A checkpoint and replay validation OK")


if __name__ == "__main__":
    main()
