#include <math.h>
#include <stdio.h>
#include <string.h>

#include "minisnn.h"
#ifdef MINISNN_TESTING
#include "neuron_model.h"
#endif

typedef struct
{
    MiniSNN *network;
    MiniSNNSensorSchema *sensor_schema;
    MiniSNNActionSchema *action_schema;
    MiniSNNAgentIOContext *agent_io;
    MiniSNNSensorEncoder *encoder;
    MiniSNNActionDecoder *decoder;
    MiniSNNAgentCycle *cycle;
} CycleFixture;

static int fail(const char *message)
{
    fprintf(stderr, "Agent cycle test failed: %s\n", message);
    return 0;
}

static void fixture_destroy(CycleFixture *fixture)
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

static int fixture_create(CycleFixture *fixture, MiniSNNNeuronModel model,
                          uint32_t brain_steps)
{
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorEncoderError encoder_error = MINISNN_SENSOR_ENCODER_ERROR_NONE;
    MiniSNNActionDecoderError decoder_error = MINISNN_ACTION_DECODER_ERROR_NONE;
    MiniSNNAgentCycleError cycle_error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    MiniSNNConfig config = minisnn_default_config();
    const MiniSNNSensorChannelSpec sensors[] =
    {
        {10U, "input_signal", 0.0, 1.0, 0.0}
    };
    const MiniSNNActionChannelSpec actions[] =
    {
        {20U, "output_signal", 0.0, 1.0, 0.0}
    };
    const MiniSNNSensorEncodingSpec encoding[] =
    {
        {10U, 0U, 1U, MINISNN_SENSOR_ENCODING_LINEAR_CURRENT,
         4000.0, 0.0, 0.0, 0.0, 0U}
    };
    const MiniSNNActionDecodingSpec decoding[] =
    {
        {20U, MINISNN_ACTION_DECODING_POPULATION_RATE,
         0U, 1U, 0U, 0U, 0.0, 1.0,
         0.0, 0.0, 0.0, 0U, 0.0, 0.0, 0.0, 0.0}
    };

    memset(fixture, 0, sizeof(*fixture));
    config.neuron_count = 2;
    config.neuron_model = model;
    fixture->network = minisnn_create_with_config(&config);
    fixture->sensor_schema = minisnn_sensor_schema_create(sensors, 1U, &io_error);
    fixture->action_schema = minisnn_action_schema_create(actions, 1U, &io_error);
    fixture->agent_io = minisnn_agent_io_create(fixture->sensor_schema,
                                                 fixture->action_schema, &io_error);
    fixture->encoder = minisnn_sensor_encoder_create(fixture->sensor_schema,
                                                      encoding, 1U, 2U, brain_steps,
                                                      &encoder_error);
    fixture->decoder = minisnn_action_decoder_create(fixture->action_schema,
                                                      decoding, 1U, 2U, brain_steps,
                                                      &decoder_error);
    fixture->cycle = minisnn_agent_cycle_create(fixture->network, fixture->agent_io,
                                                fixture->encoder, fixture->decoder,
                                                &cycle_error);
    if (fixture->network == NULL || fixture->sensor_schema == NULL ||
        fixture->action_schema == NULL || fixture->agent_io == NULL ||
        fixture->encoder == NULL || fixture->decoder == NULL || fixture->cycle == NULL)
    {
        fixture_destroy(fixture);
        return 0;
    }
    return minisnn_connect_delayed(fixture->network, 0, 1, 0.5, 1);
}

static int submit_sensor(CycleFixture *fixture, uint64_t tick, double value)
{
    MiniSNNSensorFrame frame = {0};
    MiniSNNAgentIOError error = MINISNN_AGENT_IO_ERROR_NONE;
    int result;

    if (!minisnn_sensor_frame_init(&frame, 1U) ||
        !minisnn_sensor_frame_set_values(&frame, tick, &value, 1U, &error))
    {
        minisnn_sensor_frame_destroy(&frame);
        return 0;
    }
    result = minisnn_agent_io_submit_sensor_frame(fixture->agent_io, &frame);
    minisnn_sensor_frame_destroy(&frame);
    return result;
}

static int consume_action(CycleFixture *fixture, uint64_t expected_tick)
{
    MiniSNNActionFrame action = {0};
    int result = 0;
    if (!minisnn_action_frame_init(&action, 1U))
        return 0;
    result = minisnn_agent_io_consume_action_frame(fixture->agent_io, &action) &&
        action.tick == expected_tick && isfinite(action.values[0]);
    minisnn_action_frame_destroy(&action);
    return result;
}

static int configure_rstdp(CycleFixture *fixture)
{
    MiniSNNPlasticityConfig plasticity = minisnn_default_plasticity_config();
    MiniSNNRewardConfig reward = minisnn_default_reward_config();

    plasticity.enabled = 1;
    plasticity.rule = MINISNN_PLASTICITY_STDP_PAIR_TRACE;
    plasticity.learning_mode = MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP;
    reward.enabled = 1;
    return minisnn_set_plasticity_config(fixture->network, &plasticity) &&
        minisnn_set_reward_config(fixture->network, &reward);
}

static int test_creation_contracts(void)
{
    CycleFixture fixture;
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNAgentCycleError cycle_error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    MiniSNNActionChannelSpec mismatched_channels[] =
    {
        {21U, "other_output", 0.0, 1.0, 0.0}
    };
    MiniSNNActionSchema *mismatch_schema;
    MiniSNNAgentIOContext *mismatch_context;
    MiniSNNAgentCycle *mismatch_cycle;

    if (!fixture_create(&fixture, MINISNN_NEURON_MODEL_LIF, 3U))
        return fail("fixture LIF nao criada");
    mismatch_schema = minisnn_action_schema_create(mismatched_channels, 1U,
                                                    &io_error);
    mismatch_context = minisnn_agent_io_create(fixture.sensor_schema,
                                               mismatch_schema, &io_error);
    mismatch_cycle = minisnn_agent_cycle_create(fixture.network, mismatch_context,
                                                fixture.encoder, fixture.decoder,
                                                &cycle_error);
    if (mismatch_schema == NULL || mismatch_context == NULL || mismatch_cycle != NULL ||
        cycle_error != MINISNN_AGENT_CYCLE_ERROR_SCHEMA_MISMATCH ||
        minisnn_agent_cycle_create(NULL, fixture.agent_io, fixture.encoder,
                                   fixture.decoder, &cycle_error) != NULL ||
        cycle_error != MINISNN_AGENT_CYCLE_ERROR_INVALID_ARGUMENT)
    {
        minisnn_agent_cycle_destroy(&mismatch_cycle);
        minisnn_agent_io_destroy(&mismatch_context);
        minisnn_action_schema_destroy(&mismatch_schema);
        fixture_destroy(&fixture);
        return fail("contratos de criacao nao validados");
    }
    minisnn_agent_io_destroy(&mismatch_context);
    minisnn_action_schema_destroy(&mismatch_schema);
    fixture_destroy(&fixture);
    return 1;
}

static int test_tick_feedback_and_reset(void)
{
    CycleFixture fixture;
    MiniSNNAgentCycleDiagnostics diagnostics;
    MiniSNNAgentFeedback feedback;
    MiniSNNConnectionInfo before;
    MiniSNNConnectionInfo after;

    if (!fixture_create(&fixture, MINISNN_NEURON_MODEL_LIF, 3U) ||
        !minisnn_get_connection(fixture.network, 0U, &before) ||
        !submit_sensor(&fixture, 0U, 1.0) ||
        !minisnn_agent_cycle_run_tick(fixture.cycle, &diagnostics) ||
        diagnostics.episode_tick != 0U || diagnostics.global_tick != 0U ||
        diagnostics.brain_steps_executed != 3U ||
        minisnn_agent_cycle_total_neural_steps(fixture.cycle) != 3U ||
        minisnn_agent_cycle_state(fixture.cycle) !=
            MINISNN_AGENT_CYCLE_STATE_ACTION_PENDING ||
        minisnn_agent_cycle_run_tick(fixture.cycle, NULL) ||
        minisnn_agent_cycle_last_error(fixture.cycle) !=
            MINISNN_AGENT_CYCLE_ERROR_ACTION_PENDING ||
        !consume_action(&fixture, 0U))
    {
        fixture_destroy(&fixture);
        return fail("tick completo ou consumo externo falhou");
    }

    feedback.source_tick = 0U;
    feedback.delivery_tick = 1U;
    feedback.reward = 0.0;
    feedback.episode_terminal = 0U;
    if (!minisnn_agent_cycle_submit_feedback(fixture.cycle, &feedback) ||
        !submit_sensor(&fixture, 1U, 0.0) ||
        !minisnn_agent_cycle_run_tick(fixture.cycle, &diagnostics) ||
        diagnostics.episode_tick != 1U || diagnostics.feedback_events_delivered != 1U ||
        diagnostics.reward_delivered != 0.0 || !consume_action(&fixture, 1U))
    {
        fixture_destroy(&fixture);
        return fail("feedback zero nao foi entregue deterministicamente");
    }

    feedback.source_tick = 1U;
    feedback.delivery_tick = 2U;
    feedback.reward = NAN;
    if (minisnn_agent_cycle_submit_feedback(fixture.cycle, &feedback) ||
        minisnn_agent_cycle_last_error(fixture.cycle) !=
            MINISNN_AGENT_CYCLE_ERROR_NONFINITE_REWARD)
    {
        fixture_destroy(&fixture);
        return fail("feedback nao finito aceito");
    }
    feedback.reward = 1.0;
    if (minisnn_agent_cycle_submit_feedback(fixture.cycle, &feedback) ||
        minisnn_agent_cycle_last_error(fixture.cycle) !=
            MINISNN_AGENT_CYCLE_ERROR_REWARD_UNAVAILABLE)
    {
        fixture_destroy(&fixture);
        return fail("reward sem R-STDP aceito");
    }

    if (!submit_sensor(&fixture, 2U, 1.0) ||
        !minisnn_agent_cycle_run_tick(fixture.cycle, NULL) ||
        minisnn_agent_cycle_reset_episode(fixture.cycle) ||
        minisnn_agent_cycle_last_error(fixture.cycle) !=
            MINISNN_AGENT_CYCLE_ERROR_RESET_WHILE_BUSY ||
        !consume_action(&fixture, 2U) ||
        !minisnn_agent_cycle_reset_episode(fixture.cycle) ||
        minisnn_agent_cycle_episode_id(fixture.cycle) != 1U ||
        minisnn_agent_cycle_episode_tick(fixture.cycle) != 0U ||
        minisnn_agent_cycle_next_global_tick(fixture.cycle) != 3U ||
        !minisnn_get_connection(fixture.network, 0U, &after) ||
        before.weight != after.weight || minisnn_current_step(fixture.network) != 9)
    {
        fixture_destroy(&fixture);
        return fail("reset transiente violou o contrato");
    }
    if (!submit_sensor(&fixture, 3U, 0.0) ||
        !minisnn_agent_cycle_run_tick(fixture.cycle, &diagnostics) ||
        diagnostics.episode_tick != 0U || diagnostics.global_tick != 3U ||
        !consume_action(&fixture, 3U) || minisnn_current_step(fixture.network) != 12)
    {
        fixture_destroy(&fixture);
        return fail("diagnostico do tick de episodio nao foi reiniciado");
    }
    fixture_destroy(&fixture);
    return 1;
}

static int test_fault_and_models(void)
{
    const MiniSNNNeuronModel models[] =
    {
        MINISNN_NEURON_MODEL_LIF,
        MINISNN_NEURON_MODEL_ADEX,
        MINISNN_NEURON_MODEL_HODGKIN_HUXLEY
    };

    for (uint32_t index = 0U; index < 3U; index++)
    {
        CycleFixture fixture;
        double voltage = 0.0;
        if (!fixture_create(&fixture, models[index], 2U) ||
            !submit_sensor(&fixture, 0U, 0.0) ||
            !minisnn_agent_cycle_run_tick(fixture.cycle, NULL) ||
            !consume_action(&fixture, 0U) ||
            !minisnn_get_voltage(fixture.network, 0, &voltage) || !isfinite(voltage))
        {
            fixture_destroy(&fixture);
            return fail("smoke de modelo falhou");
        }
        fixture_destroy(&fixture);
    }
#ifdef MINISNN_TESTING
    {
        CycleFixture fixture;
        MiniSNNAgentCycleDiagnostics diagnostics;
        if (!fixture_create(&fixture, MINISNN_NEURON_MODEL_LIF, 2U) ||
            !submit_sensor(&fixture, 0U, 1.0))
        {
            fixture_destroy(&fixture);
            return fail("fixture de falha nao criada");
        }
        neuron_model_test_fail_after_calls(0);
        if (minisnn_agent_cycle_run_tick(fixture.cycle, &diagnostics) ||
            minisnn_agent_cycle_state(fixture.cycle) !=
                MINISNN_AGENT_CYCLE_STATE_FAULTED ||
            minisnn_agent_cycle_last_error(fixture.cycle) !=
                MINISNN_AGENT_CYCLE_ERROR_NETWORK_STEP ||
            diagnostics.brain_steps_executed != 0U || diagnostics.total_spikes != 0U ||
            minisnn_agent_cycle_total_ticks(fixture.cycle) != 0U ||
            minisnn_agent_io_action_pending(fixture.agent_io) ||
            !minisnn_agent_cycle_reset_episode(fixture.cycle))
        {
            neuron_model_test_fail_after_calls(-1);
            fixture_destroy(&fixture);
            return fail("falha neural nao foi isolada pelo ciclo");
        }
        neuron_model_test_fail_after_calls(-1);
        fixture_destroy(&fixture);
    }
#endif
    return 1;
}

static int test_reward_delivery_contract(void)
{
    CycleFixture fixture;
    MiniSNNAgentFeedback feedback;
    MiniSNNAgentCycleDiagnostics diagnostics;
    double applied = 0.0;

    if (!fixture_create(&fixture, MINISNN_NEURON_MODEL_LIF, 2U))
        return fail("fixture de reward nao criada");
    if (!configure_rstdp(&fixture) ||
        !submit_sensor(&fixture, 0U, 1.0) ||
        !minisnn_agent_cycle_run_tick(fixture.cycle, NULL) ||
        !consume_action(&fixture, 0U))
    {
        fixture_destroy(&fixture);
        return fail("configuracao inicial de R-STDP falhou");
    }
    feedback.source_tick = 0U;
    feedback.delivery_tick = 1U;
    feedback.reward = 1.0;
    feedback.episode_terminal = 0U;
    if (!minisnn_agent_cycle_submit_feedback(fixture.cycle, &feedback) ||
        !submit_sensor(&fixture, 1U, 1.0) ||
        !minisnn_agent_cycle_run_tick(fixture.cycle, &diagnostics) ||
        diagnostics.feedback_events_delivered != 1U || diagnostics.reward_delivered != 1.0 ||
        !minisnn_get_last_applied_reward(fixture.network, &applied) || applied != 1.0 ||
        !consume_action(&fixture, 1U))
    {
        fixture_destroy(&fixture);
        return fail("reward positivo nao chegou antes do passo neural");
    }
    feedback.source_tick = 1U;
    feedback.delivery_tick = 2U;
    feedback.reward = -1.0;
    if (!minisnn_agent_cycle_submit_feedback(fixture.cycle, &feedback) ||
        !submit_sensor(&fixture, 2U, 1.0) ||
        !minisnn_agent_cycle_run_tick(fixture.cycle, &diagnostics) ||
        diagnostics.feedback_events_delivered != 1U || diagnostics.reward_delivered != -1.0 ||
        !minisnn_get_last_applied_reward(fixture.network, &applied) || applied != -1.0 ||
        !consume_action(&fixture, 2U))
    {
        fixture_destroy(&fixture);
        return fail("reward negativo nao chegou antes do passo neural");
    }
    feedback.source_tick = 2U;
    feedback.delivery_tick = 2U;
    feedback.reward = 0.0;
    if (minisnn_agent_cycle_submit_feedback(fixture.cycle, &feedback) ||
        minisnn_agent_cycle_last_error(fixture.cycle) !=
            MINISNN_AGENT_CYCLE_ERROR_FEEDBACK_TOO_LATE)
    {
        fixture_destroy(&fixture);
        return fail("feedback atrasado foi aceito");
    }
    feedback.source_tick = 3U;
    feedback.delivery_tick = 3U;
    if (minisnn_agent_cycle_submit_feedback(fixture.cycle, &feedback) ||
        minisnn_agent_cycle_last_error(fixture.cycle) !=
            MINISNN_AGENT_CYCLE_ERROR_UNKNOWN_SOURCE_TICK)
    {
        fixture_destroy(&fixture);
        return fail("source tick futuro foi aceito");
    }
    fixture_destroy(&fixture);
    return 1;
}

static int test_terminal_feedback_contract(void)
{
    CycleFixture fixture;
    MiniSNNAgentFeedback feedback;
    MiniSNNAgentCycleDiagnostics diagnostics;
    MiniSNNConnectionInfo before;
    MiniSNNConnectionInfo after;
    double applied = 0.0;
    double pending = 0.0;
    int step_before;

    if (!fixture_create(&fixture, MINISNN_NEURON_MODEL_LIF, 1U) ||
        !configure_rstdp(&fixture) ||
        !submit_sensor(&fixture, 0U, 1.0) ||
        !minisnn_agent_cycle_run_tick(fixture.cycle, &diagnostics) ||
        diagnostics.episode_tick != 0U ||
        !minisnn_get_connection(fixture.network, 0U, &before))
    {
        fixture_destroy(&fixture);
        return fail("preparacao do feedback terminal falhou");
    }

    feedback.source_tick = 0U;
    feedback.delivery_tick = 1U;
    feedback.reward = 1.0;
    feedback.episode_terminal = 2U;
    if (minisnn_agent_cycle_submit_feedback(fixture.cycle, &feedback) ||
        minisnn_agent_cycle_last_error(fixture.cycle) !=
            MINISNN_AGENT_CYCLE_ERROR_FEEDBACK_INVALID ||
        minisnn_agent_io_action_pending(fixture.agent_io) == 0)
    {
        fixture_destroy(&fixture);
        return fail("flag terminal invalida alterou o ciclo");
    }

    feedback.episode_terminal = 1U;
    if (minisnn_agent_cycle_submit_feedback(fixture.cycle, &feedback) ||
        minisnn_agent_cycle_last_error(fixture.cycle) !=
            MINISNN_AGENT_CYCLE_ERROR_FEEDBACK_INVALID ||
        !consume_action(&fixture, 0U))
    {
        fixture_destroy(&fixture);
        return fail("feedback terminal antes do consumo foi aceito");
    }

    step_before = minisnn_current_step(fixture.network);
    feedback.source_tick = 9U;
    if (minisnn_agent_cycle_submit_feedback(fixture.cycle, &feedback) ||
        minisnn_agent_cycle_last_error(fixture.cycle) !=
            MINISNN_AGENT_CYCLE_ERROR_FEEDBACK_INVALID)
    {
        fixture_destroy(&fixture);
        return fail("source terminal invalido foi aceito");
    }
    feedback.source_tick = 0U;
    feedback.delivery_tick = 2U;
    if (minisnn_agent_cycle_submit_feedback(fixture.cycle, &feedback) ||
        minisnn_agent_cycle_last_error(fixture.cycle) !=
            MINISNN_AGENT_CYCLE_ERROR_FEEDBACK_INVALID)
    {
        fixture_destroy(&fixture);
        return fail("delivery terminal invalido foi aceito");
    }

    feedback.delivery_tick = 1U;
    feedback.reward = 0.25;
    feedback.episode_terminal = 0U;
    if (!minisnn_agent_cycle_submit_feedback(fixture.cycle, &feedback))
    {
        fixture_destroy(&fixture);
        return fail("feedback comum de fronteira rejeitado");
    }
    feedback.reward = 0.75;
    feedback.episode_terminal = 1U;
    if (!minisnn_agent_cycle_submit_feedback(fixture.cycle, &feedback) ||
        minisnn_current_step(fixture.network) != step_before ||
        !minisnn_get_last_applied_reward(fixture.network, &applied) ||
        applied != 1.0 || !minisnn_get_pending_reward(fixture.network, &pending) ||
        pending != 0.0 || minisnn_agent_cycle_total_reward(fixture.cycle) != 1.0 ||
        minisnn_agent_cycle_episode_id(fixture.cycle) != 0U ||
        minisnn_agent_cycle_submit_feedback(fixture.cycle, &feedback) ||
        minisnn_agent_cycle_last_error(fixture.cycle) !=
            MINISNN_AGENT_CYCLE_ERROR_FEEDBACK_INVALID ||
        !minisnn_agent_cycle_reset_episode(fixture.cycle) ||
        !minisnn_get_connection(fixture.network, 0U, &after) ||
        after.weight != before.weight)
    {
        fixture_destroy(&fixture);
        return fail("feedback terminal nao foi aplicado atomicamente");
    }
    fixture_destroy(&fixture);

    if (!fixture_create(&fixture, MINISNN_NEURON_MODEL_LIF, 1U) ||
        !submit_sensor(&fixture, 0U, 0.0) ||
        !minisnn_agent_cycle_run_tick(fixture.cycle, NULL) ||
        !consume_action(&fixture, 0U))
    {
        fixture_destroy(&fixture);
        return fail("preparacao do terminal zero falhou");
    }
    feedback.source_tick = 0U;
    feedback.delivery_tick = 1U;
    feedback.reward = 1.0;
    feedback.episode_terminal = 1U;
    if (minisnn_agent_cycle_submit_feedback(fixture.cycle, &feedback) ||
        minisnn_agent_cycle_last_error(fixture.cycle) !=
            MINISNN_AGENT_CYCLE_ERROR_REWARD_UNAVAILABLE)
    {
        fixture_destroy(&fixture);
        return fail("terminal nao zero sem R-STDP foi aceito");
    }
    feedback.reward = 0.0;
    if (!minisnn_agent_cycle_submit_feedback(fixture.cycle, &feedback) ||
        minisnn_agent_cycle_total_reward(fixture.cycle) != 0.0)
    {
        fixture_destroy(&fixture);
        return fail("terminal zero sem R-STDP nao foi aceito");
    }
    fixture_destroy(&fixture);
    return 1;
}

#ifdef MINISNN_TESTING
static int test_structural_transient_reset(void)
{
    CycleFixture fixture;
    MiniSNNStructuralPlasticityConfig config =
        minisnn_default_structural_plasticity_config();
    MiniSNNStructuralStats before_stats;
    MiniSNNStructuralStats after_stats;
    MiniSNNConnectionInfo before_connection;
    MiniSNNConnectionInfo after_connection;
    uint64_t topology_before;
    uint64_t topology_after;
    size_t event_count_before;
    double trace = 0.0;
    int step_before;

    config.enabled = 1;
    config.maintenance_interval_steps = 1U;
    config.grace_period_steps = 1U;
    config.min_connections = 1U;
    config.max_connections = 1U;
    config.pruning_enabled = 0;
    config.growth_enabled = 1;
    if (!fixture_create(&fixture, MINISNN_NEURON_MODEL_LIF, 1U) ||
        !minisnn_set_structural_plasticity_config(fixture.network, &config) ||
        !submit_sensor(&fixture, 0U, 1.0) ||
        !minisnn_agent_cycle_run_tick(fixture.cycle, NULL) ||
        !consume_action(&fixture, 0U) ||
        !minisnn_test_get_structural_rate_trace(fixture.network, 0, &trace) ||
        trace <= 0.0 || !minisnn_get_structural_stats(fixture.network, &before_stats) ||
        !minisnn_get_topology_signature(fixture.network, &topology_before) ||
        !minisnn_get_connection(fixture.network, 0U, &before_connection))
    {
        fixture_destroy(&fixture);
        return fail("preparacao estrutural nao produziu trace");
    }
    event_count_before = minisnn_structural_event_count(fixture.network);
    step_before = minisnn_current_step(fixture.network);
    if (!minisnn_agent_cycle_reset_episode(fixture.cycle) ||
        minisnn_current_step(fixture.network) != step_before ||
        !minisnn_test_get_structural_rate_trace(fixture.network, 0, &trace) ||
        trace != 0.0 || !minisnn_get_structural_stats(fixture.network, &after_stats) ||
        memcmp(&before_stats, &after_stats, sizeof(before_stats)) != 0 ||
        minisnn_structural_event_count(fixture.network) != event_count_before ||
        !minisnn_get_topology_signature(fixture.network, &topology_after) ||
        topology_after != topology_before ||
        !minisnn_get_connection(fixture.network, 0U, &after_connection) ||
        after_connection.weight != before_connection.weight ||
        after_connection.delay != before_connection.delay ||
        !submit_sensor(&fixture, 1U, 0.0) ||
        !minisnn_agent_cycle_run_tick(fixture.cycle, NULL) ||
        !consume_action(&fixture, 1U) ||
        !minisnn_get_structural_stats(fixture.network, &after_stats) ||
        after_stats.maintenance_count != before_stats.maintenance_count + 1U)
    {
        fixture_destroy(&fixture);
        return fail("reset transiente alterou a cronologia estrutural");
    }
    fixture_destroy(&fixture);
    return 1;
}
#endif

int main(void)
{
    if (!test_creation_contracts() || !test_tick_feedback_and_reset() ||
        !test_fault_and_models() || !test_reward_delivery_contract() ||
        !test_terminal_feedback_contract()
#ifdef MINISNN_TESTING
        || !test_structural_transient_reset()
#endif
        )
        return 1;
    printf("Agent cycle validation OK\n");
    return 0;
}
