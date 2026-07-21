#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "minisnn.h"
#include "minisnn_internal.h"
#include "structural_plasticity.h"

_Static_assert(sizeof(uint64_t) == 8,
               "checkpoint round-trip requires an eight-byte uint64_t");

typedef struct
{
    MiniSNN *network;
    MiniSNNSensorSchema *sensor_schema;
    MiniSNNActionSchema *action_schema;
    MiniSNNAgentIOContext *agent_io;
    MiniSNNSensorEncoder *encoder;
    MiniSNNActionDecoder *decoder;
    MiniSNNAgentCycle *cycle;
} Fixture;

static int fail(const char *message)
{
    fprintf(stderr, "Agent cycle checkpoint test failed: %s\n", message);
    return 0;
}

static void cleanup_directory(const char *directory)
{
    static const char *const files[] =
    {
        "network_state.bin", "agent_io_state.bin", "sensor_encoder_state.bin",
        "action_decoder_state.bin", "agent_cycle_state.bin", "manifest.txt"
    };
    char path[320];
    for (size_t index = 0U; index < sizeof(files) / sizeof(files[0]); index++)
    {
        snprintf(path, sizeof(path), "%s/%s", directory, files[index]);
        remove(path);
    }
#ifdef _WIN32
    RemoveDirectoryA(directory);
#endif
}

static void fixture_destroy(Fixture *fixture)
{
    if (fixture == NULL)
        return;
    minisnn_agent_cycle_destroy(&fixture->cycle);
    minisnn_action_decoder_destroy(&fixture->decoder);
    minisnn_sensor_encoder_destroy(&fixture->encoder);
    minisnn_agent_io_destroy(&fixture->agent_io);
    minisnn_action_schema_destroy(&fixture->action_schema);
    minisnn_sensor_schema_destroy(&fixture->sensor_schema);
    minisnn_destroy(&fixture->network);
    memset(fixture, 0, sizeof(*fixture));
}

static int fixture_create_with_neuron_count(Fixture *fixture, int neuron_count)
{
    const MiniSNNSensorChannelSpec sensors[] =
    {
        {10U, "input_a", 0.0, 1.0, 0.0}
    };
    const MiniSNNActionChannelSpec actions[] =
    {
        {20U, "output_a", 0.0, 1.0, 0.0}
    };
    const MiniSNNSensorEncodingSpec encoding[] =
    {
        {10U, 0U, 1U, MINISNN_SENSOR_ENCODING_DETERMINISTIC_RATE,
         0.0, 0.0, 1200.0, 0.75, 137U}
    };
    const MiniSNNActionDecodingSpec decoding[] =
    {
        {20U, MINISNN_ACTION_DECODING_POPULATION_RATE,
         0U, 1U, 0U, 0U, 0.0, 1.0,
         0.0, 0.0, 0.0, 0U, 0.0, 0.0, 0.0, 0.0}
    };
    MiniSNNConfig config = minisnn_default_config();
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorEncoderError encoder_error = MINISNN_SENSOR_ENCODER_ERROR_NONE;
    MiniSNNActionDecoderError decoder_error = MINISNN_ACTION_DECODER_ERROR_NONE;
    MiniSNNAgentCycleError cycle_error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    MiniSNNPlasticityConfig plasticity = minisnn_default_plasticity_config();
    MiniSNNRewardConfig reward = minisnn_default_reward_config();
    MiniSNNHomeostasisConfig homeostasis = minisnn_default_homeostasis_config();
    MiniSNNStructuralPlasticityConfig structural =
        minisnn_default_structural_plasticity_config();

    memset(fixture, 0, sizeof(*fixture));
    if (neuron_count < 2)
        return 0;
    config.neuron_count = neuron_count;
    fixture->network = minisnn_create_with_config(&config);
    fixture->sensor_schema = minisnn_sensor_schema_create(sensors, 1U, &io_error);
    fixture->action_schema = minisnn_action_schema_create(actions, 1U, &io_error);
    fixture->agent_io = minisnn_agent_io_create(fixture->sensor_schema,
                                                 fixture->action_schema, &io_error);
    fixture->encoder = minisnn_sensor_encoder_create(fixture->sensor_schema, encoding, 1U,
                                                      (uint32_t)neuron_count, 3U,
                                                      &encoder_error);
    fixture->decoder = minisnn_action_decoder_create(fixture->action_schema, decoding, 1U,
                                                      (uint32_t)neuron_count, 3U,
                                                      &decoder_error);
    fixture->cycle = minisnn_agent_cycle_create(fixture->network, fixture->agent_io,
                                                fixture->encoder, fixture->decoder,
                                                &cycle_error);
    plasticity.enabled = 1;
    plasticity.learning_mode = MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP;
    reward.enabled = 1;
    homeostasis.enabled = 1;
    structural.enabled = 1;
    structural.max_connections = (size_t)neuron_count *
        (size_t)(neuron_count - 1);
    if (fixture->network == NULL || fixture->sensor_schema == NULL ||
        fixture->action_schema == NULL || fixture->agent_io == NULL ||
        fixture->encoder == NULL || fixture->decoder == NULL || fixture->cycle == NULL ||
        !minisnn_connect_delayed(fixture->network, 0, 1, 0.5, 1) ||
        !minisnn_set_plasticity_config(fixture->network, &plasticity) ||
        !minisnn_set_reward_config(fixture->network, &reward) ||
        !minisnn_set_homeostasis_config(fixture->network, &homeostasis) ||
        !minisnn_set_structural_plasticity_config(fixture->network, &structural))
    {
        fixture_destroy(fixture);
        return 0;
    }
    return 1;
}

static int fixture_create(Fixture *fixture)
{
    return fixture_create_with_neuron_count(fixture, 2);
}

static int submit_sensor(Fixture *fixture, uint64_t tick, double value)
{
    MiniSNNSensorFrame frame = {0};
    MiniSNNAgentIOError error = MINISNN_AGENT_IO_ERROR_NONE;
    int ok = minisnn_sensor_frame_init(&frame, 1U) &&
        minisnn_sensor_frame_set_values(&frame, tick, &value, 1U, &error) &&
        minisnn_agent_io_submit_sensor_frame(fixture->agent_io, &frame);
    minisnn_sensor_frame_destroy(&frame);
    return ok;
}

static int consume(Fixture *fixture, uint64_t tick, double *out_value)
{
    MiniSNNActionFrame action = {0};
    int ok = minisnn_action_frame_init(&action, 1U) &&
        minisnn_agent_io_consume_action_frame(fixture->agent_io, &action) &&
        action.tick == tick && isfinite(action.values[0]);
    if (ok && out_value != NULL)
        *out_value = action.values[0];
    minisnn_action_frame_destroy(&action);
    return ok;
}

static int run_and_consume(Fixture *fixture, uint64_t tick, double value,
                           double *out_action)
{
    return submit_sensor(fixture, tick, value) &&
        minisnn_agent_cycle_run_tick(fixture->cycle, NULL) &&
        consume(fixture, tick, out_action);
}

static int test_ready_resume(void)
{
    const char *directory = "build/agent_cycle_checkpoint_ready";
    Fixture continuous;
    Fixture resumed;
    MiniSNNAgentCycleError error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    MiniSNNAgentFeedback feedback;
    MiniSNNAgentFeedback saved_feedback = {0};
    double continuous_action = 0.0;
    double resumed_action = 0.0;
    double saved_phase = 0.0;
    double continuous_phase = 0.0;
    double resumed_phase = 0.0;
    double saved_eligibility = 0.0;
    double resumed_eligibility = 0.0;
    double saved_pending_reward = 0.0;
    double resumed_pending_reward = 0.0;
    double applied_reward = 0.0;
    int ok = fixture_create(&continuous) && fixture_create(&resumed) &&
        run_and_consume(&continuous, 0U, 1.0, NULL) &&
        minisnn_connect_delayed(resumed.network, 1, 0, 0.25, 1) &&
        minisnn_connection_count(resumed.network) == 2U &&
        minisnn_test_sensor_encoder_phase(continuous.encoder, 0U, &saved_phase) &&
        minisnn_get_connection_eligibility(continuous.network, 0U, &saved_eligibility) &&
        minisnn_queue_reward(continuous.network, 0.10) &&
        minisnn_get_pending_reward(continuous.network, &saved_pending_reward) &&
        saved_pending_reward == 0.10;
    feedback.source_tick = 0U;
    feedback.delivery_tick = 1U;
    feedback.reward = 0.25;
    feedback.episode_terminal = 0U;
    ok = ok && minisnn_agent_cycle_submit_feedback(continuous.cycle, &feedback);
    feedback.delivery_tick = 1U;
    feedback.reward = 0.50;
    ok = ok && minisnn_agent_cycle_submit_feedback(continuous.cycle, &feedback) &&
        minisnn_test_agent_cycle_feedback_count(continuous.cycle) == 2U &&
        minisnn_test_agent_cycle_feedback_at(continuous.cycle, 0U, &saved_feedback) &&
        saved_feedback.reward == 0.25 && saved_feedback.delivery_tick == 1U &&
        minisnn_test_agent_cycle_feedback_at(continuous.cycle, 1U, &saved_feedback) &&
        saved_feedback.reward == 0.50 && saved_feedback.delivery_tick == 1U &&
        minisnn_agent_cycle_save_checkpoint(continuous.cycle, directory, &error) &&
        error == MINISNN_AGENT_CYCLE_ERROR_NONE &&
        run_and_consume(&continuous, 1U, 1.0, &continuous_action) &&
        minisnn_agent_cycle_load_checkpoint(resumed.cycle, directory, &error) &&
        error == MINISNN_AGENT_CYCLE_ERROR_NONE &&
        minisnn_agent_cycle_state(resumed.cycle) == MINISNN_AGENT_CYCLE_STATE_READY &&
        minisnn_agent_cycle_next_global_tick(resumed.cycle) == 1U &&
        minisnn_test_agent_cycle_feedback_count(resumed.cycle) == 2U &&
        minisnn_test_agent_cycle_feedback_at(resumed.cycle, 0U, &saved_feedback) &&
        saved_feedback.reward == 0.25 &&
        minisnn_test_agent_cycle_feedback_at(resumed.cycle, 1U, &saved_feedback) &&
        saved_feedback.reward == 0.50 &&
        minisnn_test_sensor_encoder_phase(resumed.encoder, 0U, &resumed_phase) &&
        resumed_phase == saved_phase &&
        minisnn_get_connection_eligibility(resumed.network, 0U, &resumed_eligibility) &&
        resumed_eligibility == saved_eligibility &&
        minisnn_get_pending_reward(resumed.network, &resumed_pending_reward) &&
        resumed_pending_reward == saved_pending_reward &&
        run_and_consume(&resumed, 1U, 1.0, &resumed_action) &&
        continuous_action == resumed_action &&
        minisnn_agent_cycle_total_reward(continuous.cycle) == 0.75 &&
        minisnn_agent_cycle_total_reward(resumed.cycle) == 0.75 &&
        minisnn_get_last_applied_reward(continuous.network, &applied_reward) &&
        applied_reward == 0.85 &&
        minisnn_get_last_applied_reward(resumed.network, &applied_reward) &&
        applied_reward == 0.85 &&
        minisnn_test_sensor_encoder_phase(continuous.encoder, 0U, &continuous_phase) &&
        minisnn_test_sensor_encoder_phase(resumed.encoder, 0U, &resumed_phase) &&
        continuous_phase == resumed_phase &&
        run_and_consume(&continuous, 2U, 0.5, &continuous_action) &&
        run_and_consume(&resumed, 2U, 0.5, &resumed_action) &&
        continuous_action == resumed_action &&
        minisnn_agent_cycle_total_reward(continuous.cycle) == 0.75 &&
        minisnn_agent_cycle_total_reward(resumed.cycle) == 0.75 &&
        minisnn_current_step(continuous.network) == minisnn_current_step(resumed.network) &&
        minisnn_connection_count(resumed.network) == 1U &&
        minisnn_reset_structural_plasticity(
            resumed.network, MINISNN_STRUCTURAL_RESTORE_INITIAL_TOPOLOGY) &&
        minisnn_connection_count(resumed.network) == 1U;
    feedback.source_tick = 2U;
    feedback.delivery_tick = 3U;
    feedback.reward = 0.75;
    feedback.episode_terminal = 1U;
    ok = ok && minisnn_agent_cycle_submit_feedback(resumed.cycle, &feedback) &&
        minisnn_agent_cycle_total_reward(resumed.cycle) == 1.5 &&
        !minisnn_agent_cycle_submit_feedback(resumed.cycle, &feedback) &&
        minisnn_agent_cycle_reset_episode(resumed.cycle) &&
        minisnn_agent_cycle_episode_tick(resumed.cycle) == 0U &&
        minisnn_agent_cycle_next_global_tick(resumed.cycle) == 3U &&
        minisnn_test_sensor_encoder_phase(resumed.encoder, 0U, &resumed_phase) &&
        fabs(resumed_phase - 0.137) < 1e-12;
    if (!ok)
        fail("resume READY nao preservou a continuacao deterministica");
    fixture_destroy(&continuous);
    fixture_destroy(&resumed);
    cleanup_directory(directory);
    return ok;
}

static int test_action_pending_resume_and_corruption(void)
{
    const char *directory = "build/agent_cycle_checkpoint_pending";
    Fixture source;
    Fixture restored;
    MiniSNNAgentCycleError error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    FILE *file;
    double before = 0.0;
    double after = 0.0;
    uint64_t tick_before;
    int ok = fixture_create(&source) && fixture_create(&restored) &&
        submit_sensor(&source, 0U, 1.0) &&
        minisnn_agent_cycle_run_tick(source.cycle, NULL) &&
        minisnn_agent_cycle_save_checkpoint(source.cycle, directory, &error) &&
        error == MINISNN_AGENT_CYCLE_ERROR_NONE &&
        consume(&source, 0U, &before) &&
        minisnn_agent_cycle_load_checkpoint(restored.cycle, directory, &error) &&
        minisnn_agent_cycle_state(restored.cycle) == MINISNN_AGENT_CYCLE_STATE_ACTION_PENDING &&
        minisnn_agent_io_action_pending(restored.agent_io) &&
        !submit_sensor(&restored, 1U, 1.0) &&
        minisnn_agent_io_action_pending(restored.agent_io) &&
        consume(&restored, 0U, &after) && before == after &&
        !consume(&restored, 0U, NULL) &&
        run_and_consume(&restored, 1U, 1.0, NULL);
    if (!ok)
    {
        fail("ACTION_PENDING nao foi restaurado exatamente uma vez");
        fixture_destroy(&source);
        fixture_destroy(&restored);
        cleanup_directory(directory);
        return 0;
    }
    tick_before = minisnn_agent_cycle_next_global_tick(restored.cycle);
    {
        char path[320];
        snprintf(path, sizeof(path), "%s/manifest.txt", directory);
        file = fopen(path, "wb");
        if (file == NULL || fputs("alterado\n", file) < 0 || fclose(file) != 0 ||
            minisnn_agent_cycle_load_checkpoint(restored.cycle, directory, &error) ||
            error != MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_FORMAT ||
            minisnn_agent_cycle_next_global_tick(restored.cycle) != tick_before)
            ok = 0;
    }
    if (!ok)
        fail("checkpoint corrompido alterou o ciclo vivo");
    fixture_destroy(&source);
    fixture_destroy(&restored);
    cleanup_directory(directory);
    return ok;
}

static int test_unstable_save_rejected(void)
{
    Fixture fixture;
    MiniSNNAgentCycleError error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    int ok = fixture_create(&fixture) &&
        !minisnn_agent_cycle_save_checkpoint(fixture.cycle, "../outside", &error) &&
        error == MINISNN_AGENT_CYCLE_ERROR_INVALID_ARGUMENT &&
        submit_sensor(&fixture, 0U, 1.0) &&
        !minisnn_agent_cycle_save_checkpoint(fixture.cycle,
                                              "build/agent_cycle_checkpoint_unstable",
                                              &error) &&
        error == MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_UNSTABLE;
    if (!ok)
        fail("tick parcialmente aberto aceito para checkpoint");
    fixture_destroy(&fixture);
    cleanup_directory("build/agent_cycle_checkpoint_unstable");
    return ok;
}

static int test_corrupt_component_is_atomic(void)
{
    const char *directory = "build/agent_cycle_checkpoint_component";
    Fixture source;
    Fixture restored;
    MiniSNNAgentCycleError error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    uint64_t tick_before;
    FILE *file;
    char path[320];
    int ok = fixture_create(&source) && fixture_create(&restored) &&
        run_and_consume(&source, 0U, 1.0, NULL) &&
        minisnn_agent_cycle_save_checkpoint(source.cycle, directory, &error) &&
        error == MINISNN_AGENT_CYCLE_ERROR_NONE;
    tick_before = minisnn_agent_cycle_next_global_tick(restored.cycle);
    snprintf(path, sizeof(path), "%s/network_state.bin", directory);
    file = ok ? fopen(path, "wb") : NULL;
    if (file == NULL || fputc(0, file) == EOF || fclose(file) != 0)
        ok = 0;
    if (ok && (minisnn_agent_cycle_load_checkpoint(restored.cycle, directory, &error) ||
               error != MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_SIGNATURE ||
               minisnn_agent_cycle_next_global_tick(restored.cycle) != tick_before ||
               !run_and_consume(&restored, 0U, 1.0, NULL)))
        ok = 0;
    if (!ok)
        fail("componente corrompido alterou estado vivo ou bloqueou o ciclo");
    fixture_destroy(&source);
    fixture_destroy(&restored);
    cleanup_directory(directory);
    return ok;
}

static int test_terminal_and_reset_after_load(void)
{
    const char *directory = "build/agent_cycle_checkpoint_terminal";
    Fixture source;
    Fixture restored;
    MiniSNNAgentCycleError error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    MiniSNNAgentFeedback feedback = {0U, 1U, 0.5, 1U};
    int ok = fixture_create(&source) && fixture_create(&restored) &&
        run_and_consume(&source, 0U, 1.0, NULL) &&
        minisnn_agent_cycle_submit_feedback(source.cycle, &feedback) &&
        minisnn_agent_cycle_total_reward(source.cycle) == 0.5 &&
        minisnn_agent_cycle_save_checkpoint(source.cycle, directory, &error) &&
        minisnn_agent_cycle_load_checkpoint(restored.cycle, directory, &error) &&
        minisnn_agent_cycle_total_reward(restored.cycle) == 0.5 &&
        !minisnn_agent_cycle_submit_feedback(restored.cycle, &feedback) &&
        minisnn_agent_cycle_last_error(restored.cycle) ==
            MINISNN_AGENT_CYCLE_ERROR_FEEDBACK_INVALID;
    if (ok)
    {
        feedback.episode_terminal = 0U;
        feedback.reward = 0.25;
        feedback.delivery_tick = 1U;
        ok = minisnn_agent_cycle_submit_feedback(restored.cycle, &feedback) &&
            minisnn_test_agent_cycle_feedback_count(restored.cycle) == 1U &&
            minisnn_agent_cycle_reset_episode(restored.cycle) &&
            minisnn_test_agent_cycle_feedback_count(restored.cycle) == 0U &&
            run_and_consume(&restored, 1U, 1.0, NULL) &&
            minisnn_agent_cycle_total_reward(restored.cycle) == 0.5;
    }
    if (!ok)
        fail("feedback terminal ou reset apos load nao preservou o contrato");
    fixture_destroy(&source);
    fixture_destroy(&restored);
    cleanup_directory(directory);
    return ok;
}

static int test_incompatible_destination_is_atomic(void)
{
    const char *directory = "build/agent_cycle_checkpoint_incompatible";
    Fixture source;
    Fixture incompatible;
    MiniSNNAgentCycleError error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    int ok = fixture_create(&source) &&
        fixture_create_with_neuron_count(&incompatible, 3) &&
        run_and_consume(&source, 0U, 1.0, NULL) &&
        minisnn_agent_cycle_save_checkpoint(source.cycle, directory, &error) &&
        !minisnn_agent_cycle_load_checkpoint(incompatible.cycle, directory, &error) &&
        error == MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_INCOMPATIBLE &&
        minisnn_agent_cycle_next_global_tick(incompatible.cycle) == 0U &&
        run_and_consume(&incompatible, 0U, 1.0, NULL);
    if (!ok)
        fail("destino incompativel nao foi rejeitado atomicamente");
    fixture_destroy(&source);
    fixture_destroy(&incompatible);
    cleanup_directory(directory);
    return ok;
}

static int test_unsigned_long_long_round_trip(void)
{
    const char *directory = "build/agent_cycle_checkpoint_u64";
    Fixture source;
    Fixture restored;
    MiniSNNAgentCycleError error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    MiniSNNStructuralEvent event = {0};
    Network *source_network;
    Network *restored_network;
    int ok = fixture_create(&source) && fixture_create(&restored) &&
        run_and_consume(&source, 0U, 1.0, NULL);

    if (ok)
    {
        source_network = &source.network->net;
        if (source_network->plasticity == NULL || source_network->reward == NULL ||
            source_network->structural_plasticity == NULL ||
            source_network->structural_plasticity->connection_state_count == 0U)
            ok = 0;
        else
        {
            source_network->plasticity->stats.potentiation_events = 0ULL;
            source_network->reward->stats.reward_event_count = ULLONG_MAX;
            source_network->structural_plasticity->stats.add_attempt_count =
                (unsigned long long)UINT32_MAX + 1ULL;
            source_network->structural_plasticity->connection_states[0].birth_step =
                (unsigned long long)UINT32_MAX + 17ULL;
            source_network->structural_plasticity->connection_states[0].
                prune_candidate_count = 7ULL;
            event.step = (unsigned long long)UINT32_MAX + 99ULL;
            event.event_index = (unsigned long long)UINT32_MAX + 7ULL;
            event.type = MINISNN_TOPOLOGY_ADD;
            event.source = 0U;
            event.target = 1U;
            event.new_source = 0U;
            event.new_target = 1U;
            event.age_steps = (unsigned long long)UINT32_MAX + 3ULL;
            strcpy(event.reason, "u64_round_trip");
            ok = structural_plasticity_append_event(
                source_network->structural_plasticity, &event);
            if (ok)
                source_network->structural_plasticity->events[0].event_index =
                    event.event_index;
        }
    }
    ok = ok && minisnn_agent_cycle_save_checkpoint(source.cycle, directory, &error) &&
        minisnn_agent_cycle_load_checkpoint(restored.cycle, directory, &error);
    if (ok)
    {
        restored_network = &restored.network->net;
        ok = restored_network->plasticity->stats.potentiation_events == 0ULL &&
            restored_network->reward->stats.reward_event_count == ULLONG_MAX &&
            restored_network->structural_plasticity->stats.add_attempt_count ==
                (unsigned long long)UINT32_MAX + 1ULL &&
            restored_network->structural_plasticity->connection_states[0].birth_step ==
                (unsigned long long)UINT32_MAX + 17ULL &&
            restored_network->structural_plasticity->connection_states[0].
                prune_candidate_count == 7ULL &&
            restored_network->structural_plasticity->event_count == 1U &&
            restored_network->structural_plasticity->events[0].step ==
                (unsigned long long)UINT32_MAX + 99ULL &&
            restored_network->structural_plasticity->events[0].event_index ==
                (unsigned long long)UINT32_MAX + 7ULL &&
            restored_network->structural_plasticity->events[0].age_steps ==
                (unsigned long long)UINT32_MAX + 3ULL;
    }
    if (!ok)
        fail("round-trip de contadores unsigned long long falhou");
    fixture_destroy(&source);
    fixture_destroy(&restored);
    cleanup_directory(directory);
    return ok;
}

int main(void)
{
    if (!test_ready_resume() || !test_action_pending_resume_and_corruption() ||
        !test_unstable_save_rejected() || !test_corrupt_component_is_atomic() ||
        !test_terminal_and_reset_after_load() ||
        !test_incompatible_destination_is_atomic() ||
        !test_unsigned_long_long_round_trip())
        return 1;
    printf("Agent cycle checkpoint validation OK\n");
    return 0;
}
