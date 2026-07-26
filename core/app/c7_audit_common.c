#include "c7_audit_common.h"

#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static uint64_t fnv1a_mix(uint64_t hash, uint64_t value)
{
    for (unsigned int index = 0U; index < 8U; index++)
    {
        hash ^= (value >> (index * 8U)) & UINT64_C(0xff);
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t double_bits(double value)
{
    uint64_t bits = 0U;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static double default_input_drive(MiniSNNNeuronModel model)
{
    switch (model)
    {
        case MINISNN_NEURON_MODEL_LIF:
            return 1000.0;
        case MINISNN_NEURON_MODEL_ADEX:
            return 500.0;
        case MINISNN_NEURON_MODEL_HODGKIN_HUXLEY:
            return 12.0;
        default:
            return 0.0;
    }
}

int c7_audit_ensure_directory(const char *directory)
{
#ifdef _WIN32
    DWORD attributes;
    if (directory == NULL || directory[0] == '\0')
        return 0;
    if (CreateDirectoryA(directory, NULL) == 0 && GetLastError() != ERROR_ALREADY_EXISTS)
        return 0;
    attributes = GetFileAttributesA(directory);
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U;
#else
    struct stat state;
    if (directory == NULL || directory[0] == '\0')
        return 0;
    if (mkdir(directory, 0777) != 0 && errno != EEXIST)
        return 0;
    return stat(directory, &state) == 0 && S_ISDIR(state.st_mode);
#endif
}

void c7_audit_remove_checkpoint_directory(const char *directory)
{
    static const char *const names[] =
    {
        "network_state.bin", "agent_io_state.bin", "sensor_encoder_state.bin",
        "action_decoder_state.bin", "agent_cycle_state.bin", "manifest.txt",
        "audit_metadata.txt"
    };
    char path[512];
    if (directory == NULL || directory[0] == '\0')
        return;
    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]); index++)
    {
        if (snprintf(path, sizeof(path), "%s/%s", directory, names[index]) >= 0)
            remove(path);
    }
#ifdef _WIN32
    _rmdir(directory);
#else
    rmdir(directory);
#endif
}

static int configure_optional_modules(
    MiniSNN *network,
    int enable_plasticity,
    int enable_reward,
    int enable_homeostasis,
    int enable_structural,
    uint32_t neuron_count)
{
    MiniSNNPlasticityConfig plasticity = minisnn_default_plasticity_config();
    MiniSNNRewardConfig reward = minisnn_default_reward_config();
    MiniSNNHomeostasisConfig homeostasis = minisnn_default_homeostasis_config();
    MiniSNNStructuralPlasticityConfig structural =
        minisnn_default_structural_plasticity_config();

    if (!enable_plasticity && !enable_reward && !enable_homeostasis && !enable_structural)
        return 1;
    plasticity.enabled = enable_plasticity || enable_reward;
    plasticity.learning_mode = enable_reward ? MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP :
        MINISNN_LEARNING_MODE_DIRECT_STDP;
    reward.enabled = enable_reward;
    homeostasis.enabled = enable_homeostasis;
    structural.enabled = enable_structural;
    structural.maintenance_interval_steps = 8U;
    structural.grace_period_steps = 8U;
    structural.max_connections = (size_t)neuron_count * (size_t)(neuron_count - 1U);
    structural.min_connections = 1U;

    if (plasticity.enabled && !minisnn_set_plasticity_config(network, &plasticity))
        return 0;
    if (reward.enabled && !minisnn_set_reward_config(network, &reward))
        return 0;
    if (homeostasis.enabled && !minisnn_set_homeostasis_config(network, &homeostasis))
        return 0;
    if (structural.enabled && !minisnn_set_structural_plasticity_config(network, &structural))
        return 0;
    return 1;
}

void c7_audit_fixture_destroy(C7AuditFixture *fixture)
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

int c7_audit_fixture_create_calibrated(
    C7AuditFixture *fixture,
    MiniSNNNeuronModel model,
    uint32_t neuron_count,
    uint32_t brain_steps_per_tick,
    double drive,
    int enable_plasticity,
    int enable_reward,
    int enable_homeostasis,
    int enable_structural)
{
    const MiniSNNSensorChannelSpec sensors[C7_AUDIT_SENSOR_COUNT] =
    {
        {10U, "input_a", 0.0, 1.0, 0.0},
        {20U, "input_b", -1.0, 1.0, 0.0},
        {30U, "context_signal", 0.0, 1.0, 0.0}
    };
    const MiniSNNActionChannelSpec actions[C7_AUDIT_ACTION_COUNT] =
    {
        {100U, "output_a", 0.0, 1.0, 0.0},
        {110U, "output_b", 0.0, 1.0, 0.0},
        {120U, "target_signal", 0.0, 1.0, 0.0},
        {130U, "context_output", 0.0, 1.0, 0.0}
    };
    MiniSNNSensorEncodingSpec encoding[C7_AUDIT_SENSOR_COUNT];
    const MiniSNNActionDecodingSpec decoding[C7_AUDIT_ACTION_COUNT] =
    {
        {100U, MINISNN_ACTION_DECODING_POPULATION_RATE, 0U, 3U, 0U, 0U,
         0.01, 1.0, 0.0, 0.0, 0.0, 0U, 0.0, 0.0, 0.0, 0.0},
        {110U, MINISNN_ACTION_DECODING_THRESHOLD, 3U, 3U, 0U, 0U,
         0.0, 1.0, 0.01, 1.0, 0.0, 0U, 0.0, 0.0, 0.0, 0.0},
        {120U, MINISNN_ACTION_DECODING_WTA_MEMBER, 6U, 3U, 0U, 0U,
         0.0, 1.0, 0.0, 0.0, 0.0, 1U, 0.01, 0.01, 1.0, 0.0},
        {130U, MINISNN_ACTION_DECODING_WTA_MEMBER, 9U, 3U, 0U, 0U,
         0.0, 1.0, 0.0, 0.0, 0.0, 1U, 0.01, 0.01, 1.0, 0.0}
    };
    MiniSNNConfig config = minisnn_default_config();
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorEncoderError encoder_error = MINISNN_SENSOR_ENCODER_ERROR_NONE;
    MiniSNNActionDecoderError decoder_error = MINISNN_ACTION_DECODER_ERROR_NONE;
    MiniSNNAgentCycleError cycle_error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    if (fixture == NULL || neuron_count < C7_AUDIT_MIN_NEURONS ||
        brain_steps_per_tick == 0U || !isfinite(drive) || drive <= 0.0)
        return 0;
    memset(fixture, 0, sizeof(*fixture));
    encoding[0] = (MiniSNNSensorEncodingSpec){10U, 0U, 3U,
        MINISNN_SENSOR_ENCODING_LINEAR_CURRENT, drive, 0.0, 0.0, 0.0, 0U};
    encoding[1] = (MiniSNNSensorEncodingSpec){20U, 3U, 3U,
        MINISNN_SENSOR_ENCODING_BIPOLAR_CURRENT, drive, 0.0, 0.0, 0.0, 0U};
    encoding[2] = (MiniSNNSensorEncodingSpec){30U, 6U, 3U,
        MINISNN_SENSOR_ENCODING_DETERMINISTIC_RATE, 0.0, 0.0, drive,
        0.75, 137U};
    config.neuron_count = (int)neuron_count;
    config.neuron_model = model;
    config.adex = minisnn_adex_config_default();
    config.hodgkin_huxley = minisnn_hodgkin_huxley_config_default();
    /* The public MiniSNNConfig timestep is authoritative for every model. */
    config.dt = model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY ? 0.01 : 0.1;
    fixture->network = minisnn_create_with_config(&config);
    fixture->sensor_schema = minisnn_sensor_schema_create(sensors, C7_AUDIT_SENSOR_COUNT,
                                                          &io_error);
    fixture->action_schema = minisnn_action_schema_create(actions, C7_AUDIT_ACTION_COUNT,
                                                          &io_error);
    fixture->agent_io = minisnn_agent_io_create(fixture->sensor_schema,
                                                fixture->action_schema, &io_error);
    fixture->encoder = minisnn_sensor_encoder_create(fixture->sensor_schema, encoding,
                                                     C7_AUDIT_SENSOR_COUNT, neuron_count,
                                                     brain_steps_per_tick, &encoder_error);
    fixture->decoder = minisnn_action_decoder_create(fixture->action_schema, decoding,
                                                     C7_AUDIT_ACTION_COUNT, neuron_count,
                                                     brain_steps_per_tick, &decoder_error);
    fixture->cycle = minisnn_agent_cycle_create(fixture->network, fixture->agent_io,
                                                fixture->encoder, fixture->decoder,
                                                &cycle_error);
    for (uint32_t neuron = 0U; neuron < neuron_count && fixture->network != NULL; neuron++)
    {
        const int target = (int)((neuron + 1U) % neuron_count);
        if (!minisnn_set_neuron_type(fixture->network, (int)neuron,
                                     MINISNN_NEURON_EXCITATORY) ||
            !minisnn_connect_delayed(fixture->network, (int)neuron, target,
                                     0.5, 1))
        {
            c7_audit_fixture_destroy(fixture);
            return 0;
        }
    }
    if (fixture->network == NULL || fixture->sensor_schema == NULL ||
        fixture->action_schema == NULL || fixture->agent_io == NULL || fixture->encoder == NULL ||
        fixture->decoder == NULL || fixture->cycle == NULL ||
        !configure_optional_modules(fixture->network, enable_plasticity, enable_reward,
                                    enable_homeostasis, enable_structural, neuron_count))
    {
        c7_audit_fixture_destroy(fixture);
        return 0;
    }
    fixture->model = model;
    fixture->brain_steps_per_tick = brain_steps_per_tick;
    fixture->input_drive = drive;
    return 1;
}

int c7_audit_fixture_create(
    C7AuditFixture *fixture,
    MiniSNNNeuronModel model,
    uint32_t neuron_count,
    uint32_t brain_steps_per_tick,
    int enable_plasticity,
    int enable_reward,
    int enable_homeostasis,
    int enable_structural)
{
    return c7_audit_fixture_create_calibrated(
        fixture, model, neuron_count, brain_steps_per_tick,
        default_input_drive(model), enable_plasticity, enable_reward,
        enable_homeostasis, enable_structural);
}

int c7_audit_submit_sensor(
    C7AuditFixture *fixture,
    uint64_t tick,
    const double values[C7_AUDIT_SENSOR_COUNT])
{
    MiniSNNSensorFrame frame = {0};
    MiniSNNAgentIOError error = MINISNN_AGENT_IO_ERROR_NONE;
    int ok = fixture != NULL && values != NULL &&
        minisnn_sensor_frame_init(&frame, C7_AUDIT_SENSOR_COUNT) &&
        minisnn_sensor_frame_set_values(&frame, tick, values, C7_AUDIT_SENSOR_COUNT,
                                        &error) &&
        minisnn_agent_io_submit_sensor_frame(fixture->agent_io, &frame);
    minisnn_sensor_frame_destroy(&frame);
    return ok;
}

int c7_audit_run_pending(
    C7AuditFixture *fixture,
    MiniSNNAgentCycleDiagnostics *out_diagnostics)
{
    return fixture != NULL && minisnn_agent_cycle_run_tick(fixture->cycle, out_diagnostics);
}

int c7_audit_consume_action(
    C7AuditFixture *fixture,
    uint64_t expected_tick,
    double out_values[C7_AUDIT_ACTION_COUNT])
{
    MiniSNNActionFrame frame = {0};
    int ok = fixture != NULL && out_values != NULL &&
        minisnn_action_frame_init(&frame, C7_AUDIT_ACTION_COUNT) &&
        minisnn_agent_io_consume_action_frame(fixture->agent_io, &frame) &&
        frame.tick == expected_tick;
    if (ok)
    {
        for (uint32_t index = 0U; index < C7_AUDIT_ACTION_COUNT; index++)
        {
            if (!isfinite(frame.values[index]))
            {
                ok = 0;
                break;
            }
            out_values[index] = frame.values[index];
        }
    }
    minisnn_action_frame_destroy(&frame);
    return ok;
}

int c7_audit_run_tick(
    C7AuditFixture *fixture,
    uint64_t tick,
    const double sensor_values[C7_AUDIT_SENSOR_COUNT],
    double out_actions[C7_AUDIT_ACTION_COUNT],
    MiniSNNAgentCycleDiagnostics *out_diagnostics)
{
    return c7_audit_submit_sensor(fixture, tick, sensor_values) &&
        c7_audit_run_pending(fixture, out_diagnostics) &&
        c7_audit_consume_action(fixture, tick, out_actions);
}

int c7_audit_submit_feedback(
    C7AuditFixture *fixture,
    uint64_t source_tick,
    uint64_t delivery_tick,
    double reward,
    int episode_terminal)
{
    MiniSNNAgentFeedback feedback;
    if (fixture == NULL)
        return 0;
    feedback.source_tick = source_tick;
    feedback.delivery_tick = delivery_tick;
    feedback.reward = reward;
    feedback.episode_terminal = episode_terminal ? 1U : 0U;
    return minisnn_agent_cycle_submit_feedback(fixture->cycle, &feedback);
}

int c7_audit_fixture_all_finite(const C7AuditFixture *fixture)
{
    if (fixture == NULL || fixture->network == NULL)
        return 0;
    for (int neuron = 0; neuron < minisnn_neuron_count(fixture->network); neuron++)
    {
        double voltage;
        double current;
        int spike;
        if (!minisnn_get_voltage(fixture->network, neuron, &voltage) ||
            !minisnn_get_synaptic_current(fixture->network, neuron, &current) ||
            !minisnn_get_spike(fixture->network, neuron, &spike) ||
            !isfinite(voltage) || !isfinite(current) || (spike != 0 && spike != 1))
            return 0;
        if (fixture->model == MINISNN_NEURON_MODEL_ADEX)
        {
            MiniSNNAdExState state;
            if (!minisnn_get_adex_state(fixture->network, neuron, &state) ||
                !isfinite(state.voltage) || !isfinite(state.adaptation))
                return 0;
        }
        if (fixture->model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY)
        {
            MiniSNNHodgkinHuxleyState state;
            if (!minisnn_get_hodgkin_huxley_state(fixture->network, neuron, &state) ||
                !isfinite(state.voltage) || !isfinite(state.m) || !isfinite(state.h) ||
                !isfinite(state.n))
                return 0;
        }
    }
    return 1;
}

uint64_t c7_audit_fixture_state_signature(const C7AuditFixture *fixture)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    uint64_t topology = 0U;
    if (fixture == NULL || fixture->network == NULL || fixture->agent_io == NULL ||
        fixture->encoder == NULL || fixture->decoder == NULL || fixture->cycle == NULL ||
        !minisnn_get_topology_signature(fixture->network, &topology))
        return 0U;
    hash = fnv1a_mix(hash, (uint64_t)fixture->model);
    hash = fnv1a_mix(hash, minisnn_neuron_model_config_signature(fixture->network));
    hash = fnv1a_mix(hash, topology);
    hash = fnv1a_mix(hash, minisnn_agent_io_contract_signature(fixture->agent_io));
    hash = fnv1a_mix(hash, minisnn_sensor_encoder_contract_signature(fixture->encoder));
    hash = fnv1a_mix(hash, minisnn_action_decoder_contract_signature(fixture->decoder));
    hash = fnv1a_mix(hash, (uint64_t)minisnn_current_step(fixture->network));
    hash = fnv1a_mix(hash, minisnn_agent_cycle_episode_id(fixture->cycle));
    hash = fnv1a_mix(hash, minisnn_agent_cycle_episode_tick(fixture->cycle));
    hash = fnv1a_mix(hash, minisnn_agent_cycle_next_global_tick(fixture->cycle));
    hash = fnv1a_mix(hash, minisnn_agent_cycle_total_ticks(fixture->cycle));
    hash = fnv1a_mix(hash, minisnn_agent_cycle_total_neural_steps(fixture->cycle));
    hash = fnv1a_mix(hash, minisnn_agent_cycle_total_actions(fixture->cycle));
    return hash;
}

void c7_audit_fingerprint_init(
    C7AuditFingerprint *fingerprint,
    const C7AuditFixture *fixture,
    uint64_t seed)
{
    if (fingerprint == NULL || fixture == NULL)
        return;
    memset(fingerprint, 0, sizeof(*fingerprint));
    fingerprint->value = UINT64_C(14695981039346656037);
    fingerprint->value = fnv1a_mix(fingerprint->value, (uint64_t)fixture->model);
    fingerprint->value = fnv1a_mix(fingerprint->value,
                                   minisnn_neuron_model_config_signature(fixture->network));
    fingerprint->value = fnv1a_mix(fingerprint->value,
                                   minisnn_agent_io_contract_signature(fixture->agent_io));
    fingerprint->value = fnv1a_mix(fingerprint->value,
                                   minisnn_sensor_encoder_contract_signature(fixture->encoder));
    fingerprint->value = fnv1a_mix(fingerprint->value,
                                   minisnn_action_decoder_contract_signature(fixture->decoder));
    fingerprint->value = fnv1a_mix(fingerprint->value, seed);
}

void c7_audit_fingerprint_tick(
    C7AuditFingerprint *fingerprint,
    uint64_t tick,
    const double sensor_values[C7_AUDIT_SENSOR_COUNT],
    const double action_values[C7_AUDIT_ACTION_COUNT],
    const MiniSNNAgentCycleDiagnostics *diagnostics,
    const C7AuditFixture *fixture)
{
    uint64_t topology_signature = 0U;
    if (fingerprint == NULL || sensor_values == NULL || action_values == NULL ||
        diagnostics == NULL || fixture == NULL)
        return;
    fingerprint->value = fnv1a_mix(fingerprint->value, tick);
    for (uint32_t index = 0U; index < C7_AUDIT_SENSOR_COUNT; index++)
        fingerprint->value = fnv1a_mix(fingerprint->value, double_bits(sensor_values[index]));
    for (uint32_t index = 0U; index < C7_AUDIT_ACTION_COUNT; index++)
        fingerprint->value = fnv1a_mix(fingerprint->value, double_bits(action_values[index]));
    fingerprint->value = fnv1a_mix(fingerprint->value, diagnostics->total_spikes);
    fingerprint->value = fnv1a_mix(fingerprint->value, diagnostics->global_tick);
    fingerprint->value = fnv1a_mix(fingerprint->value, diagnostics->episode_tick);
    fingerprint->value = fnv1a_mix(fingerprint->value,
                                   (uint64_t)diagnostics->feedback_events_delivered);
    fingerprint->value = fnv1a_mix(fingerprint->value,
                                   double_bits(diagnostics->reward_delivered));
    if (minisnn_get_topology_signature(fixture->network, &topology_signature))
        fingerprint->value = fnv1a_mix(fingerprint->value, topology_signature);
    fingerprint->neural_steps += diagnostics->brain_steps_executed;
    fingerprint->actions++;
    fingerprint->rewards += diagnostics->feedback_events_delivered;
}

uint64_t c7_audit_fingerprint_value(const C7AuditFingerprint *fingerprint)
{
    return fingerprint == NULL ? 0U : fingerprint->value;
}
