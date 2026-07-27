#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app_filesystem.h"
#include "c7_audit_common.h"
#include "minisnn.h"
#include "scenario_config.h"
#include "scenario_runner.h"

#define D1_FNV_OFFSET UINT64_C(14695981039346656037)
#define D1_FNV_PRIME UINT64_C(1099511628211)
#define TRACE_STEPS 160U

static int fail(const char *message)
{
    fprintf(stderr, "D1-B determinism test failed: %s\n", message);
    return 0;
}

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

static int run_network_trace(MiniSNNNeuronModel model, unsigned int seed,
                             int plasticity_mode, uint64_t *out_hash)
{
    MiniSNNConfig config = minisnn_default_config();
    MiniSNNPlasticityConfig plasticity = minisnn_default_plasticity_config();
    MiniSNNRewardConfig reward = minisnn_default_reward_config();
    MiniSNNStructuralPlasticityConfig structural =
        minisnn_default_structural_plasticity_config();
    MiniSNN *network;
    uint64_t hash = D1_FNV_OFFSET;

    if (out_hash == NULL)
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
        int target = (source + 1) % config.neuron_count;
        if (!minisnn_connect_delayed(network, source, target, 12.0,
                                     1 + source % config.max_synaptic_delay))
        {
            minisnn_destroy(&network);
            return 0;
        }
    }
    if (plasticity_mode >= 1)
    {
        plasticity.enabled = 1;
        plasticity.learning_mode = plasticity_mode == 2 ?
            MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP :
            MINISNN_LEARNING_MODE_DIRECT_STDP;
        if (!minisnn_set_plasticity_config(network, &plasticity))
        {
            minisnn_destroy(&network);
            return 0;
        }
    }
    if (plasticity_mode == 2)
    {
        reward.enabled = 1;
        if (!minisnn_set_reward_config(network, &reward))
        {
            minisnn_destroy(&network);
            return 0;
        }
    }
    if (plasticity_mode == 3)
    {
        structural.enabled = 1;
        structural.maintenance_interval_steps = 8U;
        structural.grace_period_steps = 8U;
        structural.max_connections = 12U;
        structural.min_connections = 1U;
        if (!minisnn_set_structural_plasticity_config(network, &structural))
        {
            minisnn_destroy(&network);
            return 0;
        }
    }

    hash = mix_u64(hash, minisnn_neuron_model_config_signature(network));
    for (uint32_t step = 0U; step < TRACE_STEPS; step++)
    {
        int spikes;
        const double drive = ((step + seed) % 7U) < 4U ?
            (model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY ? 12.0 : 1000.0) : 0.0;

        minisnn_clear_inputs(network);
        if (!minisnn_set_input(network, 0, drive))
        {
            minisnn_destroy(&network);
            return 0;
        }
        if (plasticity_mode == 2 && step % 19U == 0U &&
            !minisnn_queue_reward(network, (step & 1U) == 0U ? 0.25 : -0.10))
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
    return 1;
}

static int test_model_and_plasticity_repeats(void)
{
    const MiniSNNNeuronModel models[] =
    {
        MINISNN_NEURON_MODEL_LIF,
        MINISNN_NEURON_MODEL_ADEX,
        MINISNN_NEURON_MODEL_HODGKIN_HUXLEY
    };
    const int modes[] = {0, 1, 2, 3};

    for (size_t model_index = 0U; model_index < sizeof(models) / sizeof(models[0]);
         model_index++)
    {
        for (size_t mode_index = 0U; mode_index < sizeof(modes) / sizeof(modes[0]);
             mode_index++)
        {
            uint64_t first;
            uint64_t second;
            uint64_t third;

            if (!run_network_trace(models[model_index], 41U, modes[mode_index], &first) ||
                !run_network_trace(models[model_index], 41U, modes[mode_index], &second) ||
                !run_network_trace(models[model_index], 41U, modes[mode_index], &third) ||
                first != second || first != third)
                return fail("traces repetidos divergiram");
        }
    }
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

static int run_c7_trace(MiniSNNNeuronModel model, uint64_t *out_hash)
{
    C7AuditFixture fixture = {0};
    C7AuditFingerprint fingerprint;
    uint64_t spikes = 0U;
    uint64_t action_variations = 0U;
    double previous_action[C7_AUDIT_ACTION_COUNT] = {0};
    int have_previous_action = 0;
    const int homeostasis = model == MINISNN_NEURON_MODEL_LIF;

    if (out_hash == NULL || !c7_audit_fixture_create_calibrated(
            &fixture, model, C7_AUDIT_MIN_NEURONS, 8U, c7_input_drive(model),
            1, 1, homeostasis, 1))
        return 0;
    c7_audit_fingerprint_init(&fingerprint, &fixture, UINT64_C(77));
    for (uint64_t tick = 0U; tick < 512U; tick++)
    {
        const double input[C7_AUDIT_SENSOR_COUNT] =
        {
            (tick % 3U) == 0U ? 1.0 : 0.5,
            (tick % 3U) == 1U ? -0.5 : 0.25,
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
        spikes += diagnostics.total_spikes;
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
    c7_audit_fixture_destroy(&fixture);
    return spikes > 0U && action_variations > 0U;
}

static int test_c7_isolation(void)
{
    for (MiniSNNNeuronModel model = MINISNN_NEURON_MODEL_LIF;
         model <= MINISNN_NEURON_MODEL_HODGKIN_HUXLEY; model++)
    {
        uint64_t first;
        uint64_t second;
        uint64_t third;

        if (!run_c7_trace(model, &first) || !run_c7_trace(model, &second) ||
            !run_c7_trace(model, &third) || first == 0U || first != second ||
            first != third)
            return fail("C7 nao foi deterministico entre instancias isoladas");
    }
    return 1;
}

static int test_seeded_topology(void)
{
    ScenarioConfig config;
    ScenarioRunResult first;
    ScenarioRunResult second;
    ScenarioRunResult different;
    char error[256];

    scenario_config_default(&config);
    snprintf(config.run_name, sizeof(config.run_name), "%s", "d1_b_seed_a");
    snprintf(config.topology, sizeof(config.topology), "%s", "random_balanced");
    config.neurons = 12;
    config.connection_probability = 0.35;
    config.steps = 64;
    config.auto_unique_run = 0;
    config.history_enabled = 0;
    config.seed = 19U;
    app_filesystem_remove_tree("results/scenarios/d1_b_seed_a");
    if (!scenario_runner_execute(&config, NULL, &first, error, sizeof(error)) ||
        !scenario_runner_execute(&config, NULL, &second, error, sizeof(error)) ||
        first.topology_signature != second.topology_signature ||
        first.spikes_total != second.spikes_total)
    {
        app_filesystem_remove_tree("results/scenarios/d1_b_seed_a");
        return fail("topologia randomica nao repetiu com mesma seed");
    }
    snprintf(config.run_name, sizeof(config.run_name), "%s", "d1_b_seed_b");
    config.seed = 97U;
    app_filesystem_remove_tree("results/scenarios/d1_b_seed_b");
    if (!scenario_runner_execute(&config, NULL, &different, error, sizeof(error)) ||
        first.topology_signature == different.topology_signature)
    {
        app_filesystem_remove_tree("results/scenarios/d1_b_seed_a");
        app_filesystem_remove_tree("results/scenarios/d1_b_seed_b");
        return fail("seeds distintos nao alteraram topologia aleatoria");
    }
    app_filesystem_remove_tree("results/scenarios/d1_b_seed_a");
    app_filesystem_remove_tree("results/scenarios/d1_b_seed_b");
    return 1;
}

static int test_small_world_repeat(void)
{
    ScenarioConfig config;
    ScenarioRunResult first;
    ScenarioRunResult second;
    char error[256];

    if (!scenario_config_load_file("configs/small_world.ini", &config, error,
                                   sizeof(error)))
        return fail("configuracao small-world nao carregou");
    snprintf(config.run_name, sizeof(config.run_name), "%s", "d1_b_small_world");
    config.steps = 96;
    config.auto_unique_run = 0;
    config.history_enabled = 0;
    app_filesystem_remove_tree("results/scenarios/d1_b_small_world");
    if (!scenario_runner_execute(&config, NULL, &first, error, sizeof(error)) ||
        !scenario_runner_execute(&config, NULL, &second, error, sizeof(error)) ||
        first.topology_signature != second.topology_signature ||
        first.spikes_total != second.spikes_total)
    {
        app_filesystem_remove_tree("results/scenarios/d1_b_small_world");
        return fail("small-world nao repetiu com mesma seed");
    }
    app_filesystem_remove_tree("results/scenarios/d1_b_small_world");
    return 1;
}

int main(void)
{
    if (!test_model_and_plasticity_repeats() || !test_c7_isolation() ||
        !test_seeded_topology() || !test_small_world_repeat())
        return 1;
    printf("D1-B deterministic matrix validation OK\n");
    return 0;
}
