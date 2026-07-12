#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scenario_config.h"
#include "scenario_runner.h"

#define HASH_OFFSET 1469598103934665603ULL
#define HASH_PRIME 1099511628211ULL

typedef struct
{
    int count;
    int self_count;
    int exc_exc;
    int exc_inh;
    int inh_exc;
    int inh_inh;
    unsigned long long signature;
} ExpectedTopology;

static int fail(const char *message)
{
    printf("FAIL: %s\n", message);
    return 0;
}

static void hash_value(unsigned long long *hash, unsigned long long value)
{
    for (int byte_index = 0; byte_index < 8; byte_index++)
    {
        *hash ^= (value >> (byte_index * 8)) & 0xffULL;
        *hash *= HASH_PRIME;
    }
}

static void expected_init(ExpectedTopology *expected)
{
    memset(expected, 0, sizeof(*expected));
    expected->signature = HASH_OFFSET;
}

static void expected_add(
    ExpectedTopology *expected,
    int source,
    int target,
    double weight,
    int delay,
    int source_is_inhibitory,
    int target_is_inhibitory)
{
    unsigned long long weight_bits = 0ULL;

    memcpy(&weight_bits, &weight, sizeof(weight_bits));
    hash_value(&expected->signature, (unsigned long long)source);
    hash_value(&expected->signature, (unsigned long long)target);
    hash_value(&expected->signature, weight_bits);
    hash_value(&expected->signature, (unsigned long long)delay);
    hash_value(&expected->signature, (unsigned long long)source_is_inhibitory);

    expected->count++;
    if (source == target)
        expected->self_count++;
    if (source_is_inhibitory && target_is_inhibitory)
        expected->inh_inh++;
    else if (source_is_inhibitory)
        expected->inh_exc++;
    else if (target_is_inhibitory)
        expected->exc_inh++;
    else
        expected->exc_exc++;
}

static void cleanup_run(const char *run_name)
{
    char command[320];
    snprintf(
        command,
        sizeof(command),
        "if exist results\\scenarios\\%s rmdir /S /Q results\\scenarios\\%s",
        run_name,
        run_name);
    system(command);
}

static void base_config(ScenarioConfig *config, const char *run_name, const char *topology)
{
    scenario_config_default(config);
    snprintf(config->run_name, sizeof(config->run_name), "%s", run_name);
    snprintf(config->topology, sizeof(config->topology), "%s", topology);
    config->neurons = 4;
    config->inhibitory_fraction = 0.0;
    config->source_count = 1;
    config->input_current = 0.0;
    config->steps = 1;
    config->record_neuron = 0;
    config->delay = 2;
    config->max_synaptic_delay = 4;
    config->excitatory_weight = 2.0;
    config->inhibitory_weight = -3.0;
    config->connection_probability = 1.0;
    config->allow_self_connections = 0;
    config->allow_inh_to_inh = 1;
    config->auto_unique_run = 0;
    config->history_enabled = 0;
    snprintf(config->diagnostics_level, sizeof(config->diagnostics_level), "off");
}

static int run_matches(
    const ScenarioConfig *config,
    const ExpectedTopology *expected)
{
    ScenarioRunResult result;
    char error[256];

    cleanup_run(config->run_name);

    if (!scenario_runner_execute(config, NULL, &result, error, sizeof(error)))
    {
        printf("Runner error for %s: %s\n", config->run_name, error);
        return 0;
    }

    cleanup_run(config->run_name);

    return result.connection_count == expected->count &&
           result.self_connection_count == expected->self_count &&
           result.excitatory_to_excitatory_count == expected->exc_exc &&
           result.excitatory_to_inhibitory_count == expected->exc_inh &&
           result.inhibitory_to_excitatory_count == expected->inh_exc &&
           result.inhibitory_to_inhibitory_count == expected->inh_inh &&
           result.topology_signature == expected->signature;
}

static int check_chain_and_ring(void)
{
    ScenarioConfig config;
    ExpectedTopology expected;

    base_config(&config, "test_runner_chain_exact", "chain");
    expected_init(&expected);
    for (int source = 0; source < config.neurons - 1; source++)
        expected_add(&expected, source, source + 1, 2.0, 2, 0, 0);
    if (!run_matches(&config, &expected))
        return fail("chain structure differs from 0->1->...->N-1");

    snprintf(config.run_name, sizeof(config.run_name), "test_runner_ring_exact");
    snprintf(config.topology, sizeof(config.topology), "ring");
    expected_add(&expected, config.neurons - 1, 0, 2.0, 2, 0, 0);
    if (!run_matches(&config, &expected))
        return fail("ring structure differs from chain plus N-1->0");

    return 1;
}

static void expected_all_pairs(
    const ScenarioConfig *config,
    ExpectedTopology *expected)
{
    int inhibitory_count =
        (int)(config->neurons * config->inhibitory_fraction + 0.5);
    int inhibitory_start = config->neurons - inhibitory_count;

    expected_init(expected);
    for (int source = 0; source < config->neurons; source++)
    {
        int source_is_inhibitory = source >= inhibitory_start;
        double weight = source_is_inhibitory ?
            config->inhibitory_weight : config->excitatory_weight;

        for (int target = 0; target < config->neurons; target++)
        {
            int target_is_inhibitory = target >= inhibitory_start;

            if (!config->allow_self_connections && source == target)
                continue;
            if (!config->allow_inh_to_inh &&
                source_is_inhibitory && target_is_inhibitory)
            {
                continue;
            }

            expected_add(
                expected,
                source,
                target,
                weight,
                config->delay,
                source_is_inhibitory,
                target_is_inhibitory);
        }
    }
}

static int check_all_to_all_and_random_extremes(void)
{
    ScenarioConfig config;
    ExpectedTopology expected;

    base_config(&config, "test_runner_all_no_self", "all_to_all");
    expected_all_pairs(&config, &expected);
    if (!run_matches(&config, &expected) || expected.count != 12)
        return fail("all_to_all without self is not N*(N-1)");

    snprintf(config.run_name, sizeof(config.run_name), "test_runner_all_self");
    config.allow_self_connections = 1;
    expected_all_pairs(&config, &expected);
    if (!run_matches(&config, &expected) || expected.count != 16)
        return fail("all_to_all with self is not N*N");

    snprintf(config.run_name, sizeof(config.run_name), "test_runner_all_no_inh_inh");
    config.allow_self_connections = 0;
    config.allow_inh_to_inh = 0;
    config.inhibitory_fraction = 0.5;
    expected_all_pairs(&config, &expected);
    if (!run_matches(&config, &expected) ||
        expected.count != 10 || expected.inh_inh != 0)
    {
        return fail("all_to_all did not respect INH->INH exclusion");
    }

    snprintf(config.run_name, sizeof(config.run_name), "test_runner_random_zero");
    snprintf(config.topology, sizeof(config.topology), "random");
    config.connection_probability = 0.0;
    expected_init(&expected);
    if (!run_matches(&config, &expected))
        return fail("random density zero created connections");

    snprintf(config.run_name, sizeof(config.run_name), "test_runner_random_one");
    config.connection_probability = 1.0;
    expected_all_pairs(&config, &expected);
    if (!run_matches(&config, &expected))
        return fail("random density one differs from all permitted pairs");

    return 1;
}

static int check_random_balanced(void)
{
    ScenarioConfig config;
    ExpectedTopology expected;

    base_config(&config, "test_runner_random_balanced", "random_balanced");
    config.neurons = 5;
    config.inhibitory_fraction = 0.4;
    config.allow_inh_to_inh = 0;
    config.connection_probability = 1.0;
    expected_all_pairs(&config, &expected);

    if (!run_matches(&config, &expected) ||
        expected.exc_inh != 6 ||
        expected.inh_exc != 6 ||
        expected.inh_inh != 0)
    {
        return fail("random_balanced types, signs or INH->INH rule differ");
    }

    return 1;
}

static int check_small_world_local_ring(void)
{
    ScenarioConfig config;
    ExpectedTopology expected;

    base_config(&config, "test_runner_small_world_local", "small_world");
    config.neurons = 6;
    config.small_world_neighbors = 2;
    config.small_world_rewire_probability = 0.0;
    expected_init(&expected);

    for (int source = 0; source < config.neurons; source++)
    {
        expected_add(
            &expected,
            source,
            (source + 1) % config.neurons,
            2.0,
            2,
            0,
            0);
        expected_add(
            &expected,
            source,
            (source - 1 + config.neurons) % config.neurons,
            2.0,
            2,
            0,
            0);
    }

    if (!run_matches(&config, &expected) || expected.count != 12)
        return fail("small_world p=0 did not preserve local circular neighbors");

    return 1;
}

static int check_feedforward(void)
{
    ScenarioConfig config;
    ExpectedTopology expected;

    base_config(&config, "test_runner_feedforward_one", "feedforward");
    config.neurons = 6;
    config.feedforward_layers = 3;
    config.connection_probability = 1.0;
    expected_init(&expected);

    for (int source = 0; source < 2; source++)
        for (int target = 2; target < 4; target++)
            expected_add(&expected, source, target, 2.0, 2, 0, 0);
    for (int source = 2; source < 4; source++)
        for (int target = 4; target < 6; target++)
            expected_add(&expected, source, target, 2.0, 2, 0, 0);

    if (!run_matches(&config, &expected) || expected.count != 8)
        return fail("feedforward density one connected outside adjacent layers");

    snprintf(config.run_name, sizeof(config.run_name), "test_runner_feedforward_zero");
    config.connection_probability = 0.0;
    expected_init(&expected);
    if (!run_matches(&config, &expected))
        return fail("feedforward density zero created connections");

    return 1;
}

int main(void)
{
    if (!check_chain_and_ring() ||
        !check_all_to_all_and_random_extremes() ||
        !check_random_balanced() ||
        !check_small_world_local_ring() ||
        !check_feedforward())
    {
        return 1;
    }

    printf("Runner topology structural validation OK\n");
    return 0;
}
