#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "evolution_config.h"
#include "evolution_runner.h"
#include "scenario_config.h"

#define TEST_ROOT "build/test_evolution_long"
#define TEST_BASE_CONFIG TEST_ROOT "/base.ini"
#define TEST_EVOLUTION_CONFIG TEST_ROOT "/evolution.ini"
#define TEST_OUTPUT_ROOT TEST_ROOT "/results"

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
        if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0)
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

static int ensure_directory(const char *path)
{
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static int count_data_lines(const char *path)
{
    FILE *file = fopen(path, "r");
    char line[4096];
    int count = -1;

    if (file == NULL)
        return -1;
    while (fgets(line, sizeof(line), file) != NULL)
        count++;
    fclose(file);
    return count;
}

static int file_contains_nonfinite(const char *path)
{
    FILE *file = fopen(path, "r");
    char line[4096];

    if (file == NULL)
        return 1;
    while (fgets(line, sizeof(line), file) != NULL)
    {
        for (char *cursor = line; *cursor != '\0'; cursor++)
            *cursor = (char)tolower((unsigned char)*cursor);
        if (strstr(line, "nan") != NULL || strstr(line, "inf") != NULL)
        {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

static int create_base_config(void)
{
    ScenarioConfig config;
    char error[256] = {0};

    scenario_config_default(&config);
    snprintf(config.run_name, sizeof(config.run_name), "evolution_long_base");
    snprintf(config.topology, sizeof(config.topology), "random_balanced");
    config.neurons = 24;
    config.inhibitory_fraction = 0.25;
    config.connection_probability = 0.50;
    config.seed = 31415U;
    config.steps = 80;
    config.source_count = 3;
    config.record_neuron = 3;
    config.auto_unique_run = 0;
    config.history_enabled = 0;
    snprintf(config.diagnostics_level, sizeof(config.diagnostics_level), "off");
    return scenario_config_save_file(TEST_BASE_CONFIG, &config, error, sizeof(error));
}

static int create_evolution_config(void)
{
    EvolutionExperimentConfig config;
    char error[256] = {0};

    evolution_config_default(&config);
    snprintf(config.experiment_name, sizeof(config.experiment_name),
             "evolution_long_validation");
    snprintf(config.base_scenario, sizeof(config.base_scenario), "%s",
             TEST_BASE_CONFIG);
    config.population_size = 20;
    config.generations = 20;
    config.elite_count = 2;
    config.tournament_size = 3;
    config.evaluation_replicates = 2;
    config.checkpoint_interval_generations = 5;
    config.save_all_genomes = 0;
    config.save_best_run = 1;
    config.auto_unique_run = 0;
    config.history_enabled = 0;
    config.evolution_seed = 20260714U;
    config.evaluation_seed_base = 9000U;
    config.evolve_exc_weights = 1;
    config.evolve_inh_magnitudes = 1;
    config.fitness_term_count = 1;
    config.fitness_terms[0].index = 0;
    snprintf(config.fitness_terms[0].metric,
             sizeof(config.fitness_terms[0].metric),
             "activity_total_spikes");
    config.fitness_terms[0].goal = EVOLUTION_FITNESS_TARGET;
    config.fitness_terms[0].target = 30.0;
    config.fitness_terms[0].scale = 20.0;
    config.fitness_terms[0].weight = 1.0;
    config.fitness_terms[0].has_neuron_id = 0;
    return evolution_config_save_file(TEST_EVOLUTION_CONFIG, &config,
                                      error, sizeof(error));
}

int main(void)
{
    EvolutionRunnerOptions options;
    EvolutionRunResult result;
    char error[512] = {0};
    char path[EVOLUTION_OUTPUT_PATH_MAX + 64];
    int ok = 1;

    remove_tree(TEST_ROOT);
    if (!ensure_directory(TEST_ROOT) || !ensure_directory(TEST_OUTPUT_ROOT) ||
        !create_base_config() || !create_evolution_config())
    {
        fprintf(stderr, "Failed to create long-test configuration.\n");
        remove_tree(TEST_ROOT);
        return 1;
    }

    evolution_runner_default_options(&options);
    options.output_root = TEST_OUTPUT_ROOT;
    if (!evolution_runner_execute(TEST_EVOLUTION_CONFIG, &options, &result,
                                  error, sizeof(error)))
    {
        fprintf(stderr, "Long evolution failed: %s\n", error);
        remove_tree(TEST_ROOT);
        return 1;
    }

    ok = ok && result.completed && result.generations_completed == 20;
    ok = ok && result.gene_count >= 100 && isfinite(result.best_fitness);

    snprintf(path, sizeof(path), "%s/generations.csv", result.output_directory);
    ok = ok && count_data_lines(path) == 20 && !file_contains_nonfinite(path);
    snprintf(path, sizeof(path), "%s/individuals.csv", result.output_directory);
    ok = ok && count_data_lines(path) == 400 && !file_contains_nonfinite(path);
    snprintf(path, sizeof(path), "%s/replicates.csv", result.output_directory);
    ok = ok && count_data_lines(path) == 800 && !file_contains_nonfinite(path);
    snprintf(path, sizeof(path), "%s/checkpoint.txt", result.output_directory);
    ok = ok && count_data_lines(path) > 0;
    snprintf(path, sizeof(path), "%s/best_run/population.csv",
             result.output_directory);
    ok = ok && count_data_lines(path) == 80 && !file_contains_nonfinite(path);

    if (!remove_tree(TEST_ROOT))
    {
        fprintf(stderr, "Failed to remove long-test artifacts.\n");
        return 1;
    }
    if (!ok)
    {
        fprintf(stderr, "Long evolution validation failed.\n");
        return 1;
    }

    printf("Evolution long validation OK\n");
    return 0;
}
