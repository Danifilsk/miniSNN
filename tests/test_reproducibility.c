#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scenario_config.h"
#include "scenario_runner.h"

#define HASH_OFFSET 1469598103934665603ULL
#define HASH_PRIME 1099511628211ULL
#define RUN_NAME "test_reproducibility_random"

static int fail(const char *message)
{
    printf("FAIL: %s\n", message);
    return 0;
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

static unsigned long long hash_file(const char *path, int *ok)
{
    FILE *file = fopen(path, "rb");
    unsigned long long hash = HASH_OFFSET;
    int value;

    *ok = 0;
    if (file == NULL)
        return 0ULL;

    while ((value = fgetc(file)) != EOF)
    {
        hash ^= (unsigned long long)(unsigned char)value;
        hash *= HASH_PRIME;
    }

    if (ferror(file))
    {
        fclose(file);
        return 0ULL;
    }

    fclose(file);
    *ok = 1;
    return hash;
}

static void reproducible_config(
    ScenarioConfig *config,
    const char *run_name,
    const char *topology,
    unsigned int seed)
{
    scenario_config_default(config);
    snprintf(config->run_name, sizeof(config->run_name), "%s", run_name);
    snprintf(config->topology, sizeof(config->topology), "%s", topology);
    config->neurons = 8;
    config->inhibitory_fraction = 0.25;
    config->connection_probability = 0.35;
    config->seed = seed;
    config->source_count = 2;
    config->steps = 320;
    config->record_neuron = 0;
    config->auto_unique_run = 0;
    config->history_enabled = 0;
    snprintf(config->diagnostics_level, sizeof(config->diagnostics_level), "off");
}

static int check_same_seed_same_run(void)
{
    ScenarioConfig config;
    ScenarioRunResult first;
    ScenarioRunResult second;
    char error[256];
    int ok;
    unsigned long long population_hash;
    unsigned long long raster_hash;
    unsigned long long neuron_hash;

    cleanup_run(RUN_NAME);
    reproducible_config(&config, RUN_NAME, "random_balanced", 17U);

    if (!scenario_runner_execute(&config, NULL, &first, error, sizeof(error)))
    {
        printf("Runner error: %s\n", error);
        return 0;
    }

    population_hash = hash_file(
        "results/scenarios/" RUN_NAME "/population.csv",
        &ok);
    if (!ok)
        return fail("could not hash first population.csv");

    raster_hash = hash_file(
        "results/scenarios/" RUN_NAME "/raster.csv",
        &ok);
    if (!ok)
        return fail("could not hash first raster.csv");

    neuron_hash = hash_file(
        "results/scenarios/" RUN_NAME "/neuron_0.csv",
        &ok);
    if (!ok)
        return fail("could not hash first neuron CSV");

    if (!scenario_runner_execute(&config, NULL, &second, error, sizeof(error)))
    {
        printf("Runner error: %s\n", error);
        return 0;
    }

    if (first.topology_signature != second.topology_signature ||
        first.connection_count != second.connection_count ||
        first.spikes_total != second.spikes_total)
    {
        cleanup_run(RUN_NAME);
        return fail("same config and seed changed topology or total spikes");
    }

    if (population_hash != hash_file(
            "results/scenarios/" RUN_NAME "/population.csv", &ok) || !ok ||
        raster_hash != hash_file(
            "results/scenarios/" RUN_NAME "/raster.csv", &ok) || !ok ||
        neuron_hash != hash_file(
            "results/scenarios/" RUN_NAME "/neuron_0.csv", &ok) || !ok)
    {
        cleanup_run(RUN_NAME);
        return fail("same config and seed changed normalized CSV outputs");
    }

    cleanup_run(RUN_NAME);
    return 1;
}

static int check_seed_effects(void)
{
    ScenarioConfig config;
    ScenarioRunResult first;
    ScenarioRunResult second;
    char error[256];

    reproducible_config(&config, "test_repro_seed_a", "random", 7U);
    cleanup_run(config.run_name);
    if (!scenario_runner_execute(&config, NULL, &first, error, sizeof(error)))
        return fail("random seed 7 run failed");
    cleanup_run(config.run_name);

    snprintf(config.run_name, sizeof(config.run_name), "test_repro_seed_b");
    config.seed = 19U;
    cleanup_run(config.run_name);
    if (!scenario_runner_execute(&config, NULL, &second, error, sizeof(error)))
        return fail("random seed 19 run failed");
    cleanup_run(config.run_name);

    if (first.topology_signature == second.topology_signature)
        return fail("known different random seeds produced identical topology signature");

    reproducible_config(&config, "test_repro_chain_a", "chain", 1U);
    cleanup_run(config.run_name);
    if (!scenario_runner_execute(&config, NULL, &first, error, sizeof(error)))
        return fail("chain seed 1 run failed");
    cleanup_run(config.run_name);

    snprintf(config.run_name, sizeof(config.run_name), "test_repro_chain_b");
    config.seed = 999U;
    cleanup_run(config.run_name);
    if (!scenario_runner_execute(&config, NULL, &second, error, sizeof(error)))
        return fail("chain seed 999 run failed");
    cleanup_run(config.run_name);

    if (first.topology_signature != second.topology_signature)
        return fail("deterministic chain changed when only seed changed");

    return 1;
}

static int check_plasticity_reproducibility(void)
{
    ScenarioConfig config;
    ScenarioRunResult first;
    ScenarioRunResult second;
    char error[256];
    const char *files[] = {
        "weights_initial.csv",
        "weights_final.csv",
        "weight_history.csv",
        "plasticity_metrics.csv"
    };
    unsigned long long hashes[4];
    int ok;

    reproducible_config(&config, "test_reproducibility_stdp", "random_balanced", 23U);
    config.steps = 600;
    config.excitatory_weight = 120.0;
    config.plasticity_enabled = 1;
    config.plasticity_weight_max = 250.0;
    config.plasticity_record_interval_steps = 17;
    config.plasticity_record_connection_limit = 9;
    cleanup_run(config.run_name);

    if (!scenario_runner_execute(&config, NULL, &first, error, sizeof(error)))
    {
        printf("Runner STDP error: %s\n", error);
        return 0;
    }

    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++)
    {
        char path[320];
        snprintf(
            path,
            sizeof(path),
            "results/scenarios/%s/%s",
            config.run_name,
            files[i]);
        hashes[i] = hash_file(path, &ok);
        if (!ok)
        {
            cleanup_run(config.run_name);
            return fail("could not hash first STDP output");
        }
    }

    if (!scenario_runner_execute(&config, NULL, &second, error, sizeof(error)))
    {
        cleanup_run(config.run_name);
        return fail("second STDP reproducibility run failed");
    }

    if (first.topology_signature != second.topology_signature ||
        first.spikes_total != second.spikes_total)
    {
        cleanup_run(config.run_name);
        return fail("same STDP config changed topology or spikes");
    }

    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++)
    {
        char path[320];
        snprintf(
            path,
            sizeof(path),
            "results/scenarios/%s/%s",
            config.run_name,
            files[i]);
        if (hashes[i] != hash_file(path, &ok) || !ok)
        {
            cleanup_run(config.run_name);
            return fail("same STDP config changed weight outputs");
        }
    }

    cleanup_run(config.run_name);
    return 1;
}

static int check_homeostasis_reproducibility(void)
{
    ScenarioConfig config;
    ScenarioRunResult first;
    ScenarioRunResult second;
    char error[256];
    const char *files[] = {
        "population.csv",
        "raster.csv",
        "weights_final.csv",
        "homeostasis_metrics.csv",
        "homeostasis_history.csv",
        "threshold_history.csv",
        "homeostasis_neurons.csv"
    };
    unsigned long long hashes[7];
    int ok;

    reproducible_config(
        &config, "test_reproducibility_homeostasis", "random_balanced", 29U);
    config.steps = 500;
    config.plasticity_enabled = 1;
    config.plasticity_record_weights = 1;
    config.plasticity_weight_max = 400.0;
    config.homeostasis_enabled = 1;
    config.homeostasis_intrinsic_enabled = 1;
    config.homeostasis_synaptic_scaling_enabled = 1;
    config.homeostasis_inhibitory_gain_enabled = 1;
    config.homeostasis_update_interval_steps = 7;
    config.homeostasis_record_history = 1;
    config.homeostasis_record_interval_steps = 11;
    config.homeostasis_record_neuron_limit = 5;
    cleanup_run(config.run_name);

    if (!scenario_runner_execute(&config, NULL, &first, error, sizeof(error)))
    {
        printf("Runner homeostasis error: %s\n", error);
        return 0;
    }

    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++)
    {
        char path[320];
        snprintf(path, sizeof(path), "results/scenarios/%s/%s",
            config.run_name, files[i]);
        hashes[i] = hash_file(path, &ok);
        if (!ok)
        {
            cleanup_run(config.run_name);
            return fail("could not hash first homeostasis output");
        }
    }

    if (!scenario_runner_execute(&config, NULL, &second, error, sizeof(error)))
    {
        cleanup_run(config.run_name);
        return fail("second homeostasis reproducibility run failed");
    }
    if (first.topology_signature != second.topology_signature ||
        first.spikes_total != second.spikes_total)
    {
        cleanup_run(config.run_name);
        return fail("same homeostasis config changed topology or spikes");
    }

    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++)
    {
        char path[320];
        snprintf(path, sizeof(path), "results/scenarios/%s/%s",
            config.run_name, files[i]);
        if (hashes[i] != hash_file(path, &ok) || !ok)
        {
            cleanup_run(config.run_name);
            return fail("same homeostasis config changed normalized outputs");
        }
    }

    cleanup_run(config.run_name);
    return 1;
}

static int check_reward_reproducibility(void)
{
    ScenarioConfig config;
    ScenarioRunResult first;
    ScenarioRunResult second;
    char error[256];
    const char *files[] = {
        "population.csv",
        "raster.csv",
        "weights_final.csv",
        "reward_metrics.csv",
        "reward_events.csv",
        "reward_history.csv",
        "eligibility_history.csv",
        "reward_connections.csv"
    };
    unsigned long long hashes[8];
    int ok;

    if (!scenario_config_load_file(
            "configs/reward_mixed_demo.ini", &config, error, sizeof(error)))
        return fail("could not load reward reproducibility config");
    snprintf(config.run_name, sizeof(config.run_name),
        "test_reproducibility_reward");
    config.history_enabled = 0;
    cleanup_run(config.run_name);
    if (!scenario_runner_execute(&config, NULL, &first, error, sizeof(error)))
        return fail("first reward reproducibility run failed");

    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++)
    {
        char path[320];
        snprintf(path, sizeof(path), "results/scenarios/%s/%s",
            config.run_name, files[i]);
        hashes[i] = hash_file(path, &ok);
        if (!ok)
        {
            cleanup_run(config.run_name);
            return fail("could not hash first reward output");
        }
    }

    if (!scenario_runner_execute(&config, NULL, &second, error, sizeof(error)) ||
        first.topology_signature != second.topology_signature ||
        first.spikes_total != second.spikes_total)
    {
        cleanup_run(config.run_name);
        return fail("same reward config changed topology or spikes");
    }

    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++)
    {
        char path[320];
        snprintf(path, sizeof(path), "results/scenarios/%s/%s",
            config.run_name, files[i]);
        if (hashes[i] != hash_file(path, &ok) || !ok)
        {
            cleanup_run(config.run_name);
            return fail("same reward config changed normalized outputs");
        }
    }

    cleanup_run(config.run_name);
    return 1;
}

int main(void)
{
    if (!check_same_seed_same_run() ||
        !check_seed_effects() ||
        !check_plasticity_reproducibility() ||
        !check_homeostasis_reproducibility() ||
        !check_reward_reproducibility())
        return 1;

    printf("Runner reproducibility validation OK\n");
    return 0;
}
