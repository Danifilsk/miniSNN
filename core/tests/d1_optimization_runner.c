#include <math.h>
#include <stdio.h>
#include <string.h>

#include "c7_audit_common.h"
#include "minisnn.h"

#define D1_FNV_OFFSET UINT64_C(14695981039346656037)
#define D1_FNV_PRIME UINT64_C(1099511628211)
#define D1_TRACE_STEPS 320U

static uint64_t mix_u64(uint64_t hash, uint64_t value)
{
    for (unsigned int index = 0U; index < 8U; index++)
    {
        hash ^= (value >> (index * 8U)) & UINT64_C(0xff);
        hash *= D1_FNV_PRIME;
    }
    return hash;
}

static uint64_t mix_double(uint64_t hash, double value)
{
    uint64_t bits = 0U;

    memcpy(&bits, &value, sizeof(bits));
    return mix_u64(hash, bits);
}

static int parse_case(const char *name, MiniSNNNeuronModel *out_model,
                      int *out_mode, int *out_c7)
{
    if (name == NULL || out_model == NULL || out_mode == NULL || out_c7 == NULL)
        return 0;
    *out_mode = 0;
    *out_c7 = 0;
    if (strcmp(name, "lif") == 0)
        *out_model = MINISNN_NEURON_MODEL_LIF;
    else if (strcmp(name, "adex") == 0)
        *out_model = MINISNN_NEURON_MODEL_ADEX;
    else if (strcmp(name, "hh") == 0)
        *out_model = MINISNN_NEURON_MODEL_HODGKIN_HUXLEY;
    else if (strcmp(name, "stdp") == 0)
    {
        *out_model = MINISNN_NEURON_MODEL_LIF;
        *out_mode = 1;
    }
    else if (strcmp(name, "rstdp") == 0)
    {
        *out_model = MINISNN_NEURON_MODEL_LIF;
        *out_mode = 2;
    }
    else if (strcmp(name, "structural") == 0)
    {
        *out_model = MINISNN_NEURON_MODEL_LIF;
        *out_mode = 3;
    }
    else if (strcmp(name, "c7_lif") == 0)
    {
        *out_model = MINISNN_NEURON_MODEL_LIF;
        *out_c7 = 1;
    }
    else if (strcmp(name, "c7_adex") == 0)
    {
        *out_model = MINISNN_NEURON_MODEL_ADEX;
        *out_c7 = 1;
    }
    else if (strcmp(name, "c7_hh") == 0)
    {
        *out_model = MINISNN_NEURON_MODEL_HODGKIN_HUXLEY;
        *out_c7 = 1;
    }
    else
        return 0;
    return 1;
}

static int configure_mode(MiniSNN *network, int mode)
{
    MiniSNNPlasticityConfig plasticity = minisnn_default_plasticity_config();
    MiniSNNRewardConfig reward = minisnn_default_reward_config();
    MiniSNNStructuralPlasticityConfig structural =
        minisnn_default_structural_plasticity_config();

    if (mode == 0)
        return 1;
    plasticity.enabled = 1;
    plasticity.learning_mode = mode == 2 ? MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP :
        MINISNN_LEARNING_MODE_DIRECT_STDP;
    if (!minisnn_set_plasticity_config(network, &plasticity))
        return 0;
    if (mode == 2)
    {
        reward.enabled = 1;
        return minisnn_set_reward_config(network, &reward);
    }
    if (mode == 3)
    {
        structural.enabled = 1;
        structural.maintenance_interval_steps = 8U;
        structural.grace_period_steps = 8U;
        structural.min_connections = 1U;
        structural.max_connections = 12U;
        return minisnn_set_structural_plasticity_config(network, &structural);
    }
    return 1;
}

static int run_network_case(MiniSNNNeuronModel model, int mode,
                            uint64_t *out_hash, uint64_t *out_spikes)
{
    MiniSNNConfig config = minisnn_default_config();
    MiniSNN *network;
    uint64_t hash = D1_FNV_OFFSET;
    uint64_t spikes_total = 0U;

    if (out_hash == NULL || out_spikes == NULL)
        return 0;
    config.neuron_count = 6;
    config.max_synaptic_delay = 3;
    config.neuron_model = model;
    config.adex = minisnn_adex_config_default();
    config.hodgkin_huxley = minisnn_hodgkin_huxley_config_default();
    config.dt = model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY ? 0.01 : 0.1;
    network = minisnn_create_with_config(&config);
    if (network == NULL)
        return 0;
    for (int source = 0; source < config.neuron_count; source++)
    {
        if (!minisnn_connect_delayed(network, source,
                                     (source + 1) % config.neuron_count, 12.0,
                                     1 + source % config.max_synaptic_delay))
        {
            minisnn_destroy(&network);
            return 0;
        }
    }
    if (!configure_mode(network, mode))
    {
        minisnn_destroy(&network);
        return 0;
    }
    hash = mix_u64(hash, minisnn_neuron_model_config_signature(network));
    for (uint32_t step = 0U; step < D1_TRACE_STEPS; step++)
    {
        int spikes;
        double drive = (step % 7U) < 4U ?
            (model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY ? 12.0 : 1000.0) : 0.0;

        minisnn_clear_inputs(network);
        if (!minisnn_set_input(network, 0, drive) ||
            (mode == 2 && step % 19U == 0U &&
             !minisnn_queue_reward(network, (step & 1U) == 0U ? 0.25 : -0.10)))
        {
            minisnn_destroy(&network);
            return 0;
        }
        spikes = minisnn_step(network);
        if (spikes < 0)
        {
            minisnn_destroy(&network);
            return 0;
        }
        spikes_total += (uint64_t)spikes;
        hash = mix_u64(hash, (uint64_t)(unsigned int)spikes);
        for (int neuron = 0; neuron < config.neuron_count; neuron++)
        {
            double voltage;

            if (!minisnn_get_voltage(network, neuron, &voltage) || !isfinite(voltage))
            {
                minisnn_destroy(&network);
                return 0;
            }
            hash = mix_double(hash, voltage);
        }
    }
    for (size_t index = 0U; index < minisnn_connection_count(network); index++)
    {
        MiniSNNConnectionInfo connection;

        if (!minisnn_get_connection(network, index, &connection))
        {
            minisnn_destroy(&network);
            return 0;
        }
        hash = mix_double(hash, connection.weight);
        hash = mix_u64(hash, (uint64_t)connection.delay);
    }
    minisnn_destroy(&network);
    *out_hash = hash;
    *out_spikes = spikes_total;
    return 1;
}

static double c7_input_drive(MiniSNNNeuronModel model)
{
    switch (model)
    {
        case MINISNN_NEURON_MODEL_LIF:
            return 1000.0;
        case MINISNN_NEURON_MODEL_ADEX:
            return 1000.0;
        case MINISNN_NEURON_MODEL_HODGKIN_HUXLEY:
            return 20.0;
        default:
            return 0.0;
    }
}

static int run_c7_case(MiniSNNNeuronModel model, uint64_t *out_hash,
                       uint64_t *out_spikes, uint64_t *out_action_variations)
{
    C7AuditFixture fixture = {0};
    C7AuditFingerprint fingerprint;
    uint64_t spikes_total = 0U;
    uint64_t action_variations = 0U;
    double previous_action[C7_AUDIT_ACTION_COUNT] = {0};
    int have_previous_action = 0;
    const int homeostasis = model == MINISNN_NEURON_MODEL_LIF;

    if (out_hash == NULL || out_spikes == NULL || out_action_variations == NULL ||
        !c7_audit_fixture_create_calibrated(
            &fixture, model, C7_AUDIT_MIN_NEURONS, 8U, c7_input_drive(model),
            1, 1, homeostasis, 1))
        return 0;
    c7_audit_fingerprint_init(&fingerprint, &fixture, UINT64_C(711));
    for (uint64_t tick = 0U; tick < 512U; tick++)
    {
        const double input[C7_AUDIT_SENSOR_COUNT] =
        {
            tick % 3U == 0U ? 1.0 : 0.5,
            tick % 3U == 1U ? -0.5 : 0.25,
            (tick & UINT64_C(1)) == 0U ? 1.0 : 0.0
        };
        double action[C7_AUDIT_ACTION_COUNT] = {0};
        MiniSNNAgentCycleDiagnostics diagnostics = {0};

        if (!c7_audit_run_tick(&fixture, tick, input, action, &diagnostics) ||
            !c7_audit_fixture_all_finite(&fixture))
        {
            c7_audit_fixture_destroy(&fixture);
            return 0;
        }
        spikes_total += diagnostics.total_spikes;
        if (have_previous_action &&
            memcmp(previous_action, action, sizeof(action)) != 0)
        {
            action_variations++;
        }
        memcpy(previous_action, action, sizeof(previous_action));
        have_previous_action = 1;
        c7_audit_fingerprint_tick(&fingerprint, tick, input, action,
                                  &diagnostics, &fixture);
    }
    *out_hash = c7_audit_fingerprint_value(&fingerprint);
    *out_spikes = spikes_total;
    *out_action_variations = action_variations;
    c7_audit_fixture_destroy(&fixture);
    return 1;
}

int main(int argc, char **argv)
{
    MiniSNNNeuronModel model;
    int mode;
    int c7_case;
    uint64_t hash;
    uint64_t spikes;
    uint64_t action_variations = 0U;

    if (argc != 2 || !parse_case(argv[1], &model, &mode, &c7_case) ||
        !(c7_case ? run_c7_case(model, &hash, &spikes, &action_variations) :
                    run_network_case(model, mode, &hash, &spikes)))
    {
        fprintf(stderr, "uso: d1_optimization_runner lif|adex|hh|stdp|rstdp|structural|c7_lif|c7_adex|c7_hh\n");
        return 1;
    }
    printf("case=%s;hash=%llu;spikes=%llu;action_variation_count=%llu\n", argv[1],
           (unsigned long long)hash, (unsigned long long)spikes,
           (unsigned long long)action_variations);
    return 0;
}
