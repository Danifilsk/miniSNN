#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "evolution_config.h"
#include "evolution_runner.h"
#include "scenario_config.h"

#define TEST_ROOT "build/test_structure_long"

static int remove_tree(const char *path)
{
    char pattern[512];
    WIN32_FIND_DATAA data;
    HANDLE find;
    if (snprintf(pattern, sizeof(pattern), "%s\\*", path) >= (int)sizeof(pattern))
        return 0;
    find = FindFirstFileA(pattern, &data);
    if (find == INVALID_HANDLE_VALUE)
        return GetLastError() == ERROR_FILE_NOT_FOUND ||
               GetLastError() == ERROR_PATH_NOT_FOUND;
    do
    {
        char child[512];
        if (strcmp(data.cFileName, ".") == 0 ||
            strcmp(data.cFileName, "..") == 0)
            continue;
        if (snprintf(child, sizeof(child), "%s\\%s", path, data.cFileName) >=
            (int)sizeof(child))
        {
            FindClose(find);
            return 0;
        }
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (!remove_tree(child))
            {
                FindClose(find);
                return 0;
            }
        }
        else if (!DeleteFileA(child))
        {
            FindClose(find);
            return 0;
        }
    } while (FindNextFileA(find, &data));
    FindClose(find);
    return RemoveDirectoryA(path) || GetLastError() == ERROR_PATH_NOT_FOUND;
}

static int write_configs(char *error, size_t error_size)
{
    ScenarioConfig scenario;
    EvolutionExperimentConfig evolution;

    scenario_config_default(&scenario);
    snprintf(scenario.run_name, sizeof(scenario.run_name), "structure_long_base");
    snprintf(scenario.topology, sizeof(scenario.topology), "random");
    scenario.neurons = 24;
    scenario.inhibitory_fraction = 0.25;
    scenario.connection_probability = 0.25;
    scenario.seed = 77001U;
    scenario.steps = 120;
    scenario.source_count = 4;
    scenario.record_neuron = 23;
    scenario.max_synaptic_delay = 8;
    scenario.allow_inh_to_inh = 1;
    scenario.auto_unique_run = 0;
    scenario.history_enabled = 0;
    snprintf(scenario.diagnostics_level,
             sizeof(scenario.diagnostics_level), "off");
    scenario.plasticity_enabled = 1;
    snprintf(scenario.plasticity_rule,
             sizeof(scenario.plasticity_rule), "stdp_pair_trace");
    snprintf(scenario.plasticity_learning_mode,
             sizeof(scenario.plasticity_learning_mode), "direct_stdp");
    scenario.homeostasis_enabled = 1;
    scenario.homeostasis_intrinsic_enabled = 1;
    scenario.structural_plasticity_enabled = 1;
    scenario.structural_maintenance_interval_steps = 20U;
    scenario.structural_grace_period_steps = 20U;
    scenario.structural_pruning_enabled = 1;
    scenario.structural_prune_weight_threshold = 0.0;
    scenario.structural_prune_activity_threshold = 0.0;
    scenario.structural_max_prunes_per_interval = 1U;
    scenario.structural_growth_enabled = 1;
    scenario.structural_growth_candidate_count = 32U;
    scenario.structural_growth_score_threshold = 0.0;
    scenario.structural_max_growth_per_interval = 1U;
    scenario.structural_growth_seed = 77002U;
    scenario.structural_new_exc_weight = 5.0;
    scenario.structural_new_inh_magnitude = 5.0;
    scenario.structural_new_delay = 1U;
    scenario.structural_min_connections = 20U;
    scenario.structural_max_connections = 200U;
    scenario.structural_record_history = 0;
    scenario.structural_record_interval_steps = 20U;
    if (!scenario_config_save_file(
            TEST_ROOT "/base.ini", &scenario, error, error_size))
        return 0;

    evolution_config_default(&evolution);
    snprintf(evolution.experiment_name,
             sizeof(evolution.experiment_name), "structure_long");
    snprintf(evolution.base_scenario,
             sizeof(evolution.base_scenario), TEST_ROOT "/base.ini");
    evolution.population_size = 20;
    evolution.generations = 20;
    evolution.elite_count = 2;
    evolution.tournament_size = 3;
    evolution.evaluation_replicates = 2;
    evolution.auto_unique_run = 0;
    evolution.history_enabled = 0;
    evolution.save_best_run = 1;
    evolution.evolve_exc_weights = 1;
    evolution.evolve_inh_magnitudes = 1;
    snprintf(evolution.genome_mode,
             sizeof(evolution.genome_mode), "structural_connections");
    evolution.structure_enabled = 1;
    evolution.structure_allow_add = 1;
    evolution.structure_allow_remove = 1;
    evolution.structure_allow_rewire = 1;
    evolution.structure_evolve_delays = 1;
    evolution.structure_add_rate = 0.20;
    evolution.structure_remove_rate = 0.20;
    evolution.structure_rewire_rate = 0.10;
    evolution.structure_delay_mutation_rate = 0.10;
    evolution.structure_max_mutations_per_child = 2;
    evolution.structure_min_connections = 20;
    evolution.structure_max_connections = 200;
    evolution.structure_allow_self_connections = 0;
    evolution.structure_allow_inh_to_inh = 1;
    evolution.structure_delay_min = 1;
    evolution.structure_delay_max = 4;
    evolution.structure_delay_mutation_max_delta = 2;
    evolution.structure_new_exc_weight_min = 5.0;
    evolution.structure_new_exc_weight_max = 200.0;
    evolution.structure_new_inh_magnitude_min = 5.0;
    evolution.structure_new_inh_magnitude_max = 200.0;
    evolution.structure_complexity_penalty = 0.01;
    evolution.fitness_term_count = 1;
    evolution.fitness_terms[0].index = 0;
    snprintf(evolution.fitness_terms[0].metric,
             sizeof(evolution.fitness_terms[0].metric),
             "activity_total_spikes");
    evolution.fitness_terms[0].goal = EVOLUTION_FITNESS_TARGET;
    evolution.fitness_terms[0].target = 40.0;
    evolution.fitness_terms[0].scale = 40.0;
    evolution.fitness_terms[0].weight = 1.0;
    return evolution_config_save_file(
        TEST_ROOT "/evolution.ini", &evolution, error, error_size);
}

int main(void)
{
    EvolutionRunnerOptions options;
    EvolutionRunResult partial;
    EvolutionRunResult resumed;
    char error[512] = {0};
    int ok;

    remove_tree(TEST_ROOT);
    if (!CreateDirectoryA(TEST_ROOT, NULL) &&
        GetLastError() != ERROR_ALREADY_EXISTS)
        return 1;
    if (!write_configs(error, sizeof(error)))
    {
        fprintf(stderr, "Structural long config FAILED: %s\n", error);
        remove_tree(TEST_ROOT);
        return 1;
    }
    evolution_runner_default_options(&options);
    options.output_root = TEST_ROOT "/results";
    options.stop_after_generations = 10;
    ok = evolution_runner_execute(
        TEST_ROOT "/evolution.ini", &options,
        &partial, error, sizeof(error));
    options.stop_after_generations = 0;
    ok = ok && !partial.completed &&
         evolution_runner_resume(
             partial.output_directory, &options,
             &resumed, error, sizeof(error)) &&
         resumed.completed && resumed.generations_completed == 20;
    if (!remove_tree(TEST_ROOT))
        ok = 0;
    if (!ok)
    {
        fprintf(stderr, "Structural long validation FAILED: %s\n", error);
        return 1;
    }
    printf("Structural long validation OK\n");
    return 0;
}
