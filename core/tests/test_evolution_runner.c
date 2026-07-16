#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "evolution_config.h"
#include "evolution_runner.h"
#include "scenario_config.h"
#include "scenario_runner.h"

#define TEST_ROOT "build/test_evolution_runner"
#define TEST_TOLERANCE 1e-12

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
    return CreateDirectoryA(path, NULL) ||
           GetLastError() == ERROR_ALREADY_EXISTS;
}

static int file_exists(const char *path)
{
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static int files_equal(const char *left_path, const char *right_path)
{
    FILE *left = fopen(left_path, "rb");
    FILE *right = fopen(right_path, "rb");
    int equal = 1;

    if (left == NULL || right == NULL)
    {
        if (left != NULL)
            fclose(left);
        if (right != NULL)
            fclose(right);
        return 0;
    }

    for (;;)
    {
        unsigned char left_buffer[4096];
        unsigned char right_buffer[4096];
        size_t left_count = fread(left_buffer, 1, sizeof(left_buffer), left);
        size_t right_count = fread(right_buffer, 1, sizeof(right_buffer), right);
        if (left_count != right_count ||
            memcmp(left_buffer, right_buffer, left_count) != 0)
        {
            equal = 0;
            break;
        }
        if (left_count == 0)
            break;
    }

    fclose(left);
    fclose(right);
    return equal;
}

static int file_contains_nonfinite(const char *path)
{
    FILE *file = fopen(path, "r");
    char line[2048];

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

static int configure_base(const char *path, int plasticity_enabled)
{
    ScenarioConfig config;
    char error[256] = {0};

    scenario_config_default(&config);
    snprintf(config.run_name, sizeof(config.run_name), "evolution_runner_test_base");
    snprintf(config.topology, sizeof(config.topology), "random");
    config.neurons = 4;
    config.inhibitory_fraction = 0.25;
    config.connection_probability = 0.75;
    config.seed = 41;
    config.steps = plasticity_enabled ? 400 : 120;
    config.source_count = 1;
    config.record_neuron = 2;
    config.excitatory_weight = 150.0;
    config.inhibitory_weight = -200.0;
    config.auto_unique_run = 0;
    config.history_enabled = 0;
    snprintf(config.diagnostics_level, sizeof(config.diagnostics_level), "off");
    config.plasticity_enabled = plasticity_enabled;
    if (plasticity_enabled)
    {
        snprintf(config.topology, sizeof(config.topology), "chain");
        config.neurons = 2;
        config.inhibitory_fraction = 0.0;
        config.connection_probability = 1.0;
        config.record_neuron = 1;
        snprintf(config.plasticity_rule, sizeof(config.plasticity_rule),
                 "stdp_pair_trace");
        snprintf(config.plasticity_learning_mode,
                 sizeof(config.plasticity_learning_mode), "direct_stdp");
        config.plasticity_a_plus = 2.0;
        config.plasticity_a_minus = 0.5;
        config.plasticity_weight_min = 0.0;
        config.plasticity_weight_max = 500.0;
    }
    return scenario_config_save_file(path, &config, error, sizeof(error));
}

static void configure_evolution(
    EvolutionExperimentConfig *config,
    const char *experiment_name,
    const char *base_path,
    int plasticity_enabled)
{
    evolution_config_default(config);
    snprintf(config->experiment_name, sizeof(config->experiment_name), "%s",
             experiment_name);
    snprintf(config->base_scenario, sizeof(config->base_scenario), "%s", base_path);
    config->population_size = 4;
    config->generations = 4;
    config->elite_count = 1;
    config->tournament_size = 2;
    config->crossover_rate = 0.8;
    config->mutation_rate = plasticity_enabled ? 0.0 : 0.4;
    config->mutation_scale = 0.15;
    config->initialization_scale = 0.25;
    config->evolution_seed = 9981U;
    config->evaluation_replicates = 2;
    config->evaluation_seed_base = 7000U;
    config->replicate_std_penalty = 0.1;
    config->checkpoint_interval_generations = 1;
    config->save_all_genomes = 1;
    config->save_best_run = 1;
    config->auto_unique_run = 0;
    config->history_enabled = 0;
    config->evolve_exc_weights = 1;
    config->exc_weight_min = 0.0;
    config->exc_weight_max = 500.0;
    config->evolve_inh_magnitudes = 1;
    config->inh_magnitude_min = 0.0;
    config->inh_magnitude_max = 500.0;
    config->scalar_gene_count = 0;
    config->fitness_term_count = 2;
    config->fitness_terms[0].index = 0;
    snprintf(config->fitness_terms[0].metric,
             sizeof(config->fitness_terms[0].metric), "neuron_spikes:2");
    config->fitness_terms[0].goal = EVOLUTION_FITNESS_TARGET;
    config->fitness_terms[0].target = 2.0;
    config->fitness_terms[0].scale = 2.0;
    config->fitness_terms[0].weight = 1.0;
    config->fitness_terms[0].has_neuron_id = 1;
    config->fitness_terms[0].neuron_id = 2;
    config->fitness_terms[1].index = 1;
    snprintf(config->fitness_terms[1].metric,
             sizeof(config->fitness_terms[1].metric), "activity_total_spikes");
    config->fitness_terms[1].goal = EVOLUTION_FITNESS_MINIMIZE;
    config->fitness_terms[1].target = 20.0;
    config->fitness_terms[1].scale = 20.0;
    config->fitness_terms[1].weight = 0.25;
}

static int test_config_parser(
    const char *base_path,
    const char *config_path)
{
    EvolutionExperimentConfig config;
    EvolutionExperimentConfig loaded;
    ScenarioConfig base;
    char error[256] = {0};
    char invalid_path[256];
    FILE *file;
    int ok;

    configure_evolution(&config, "parser_test", base_path, 0);
    config.scalar_gene_count = 0;
    ok = evolution_config_save_file(config_path, &config, error, sizeof(error)) &&
         evolution_config_load_file(config_path, &loaded, &base,
                                    error, sizeof(error)) &&
         loaded.population_size == config.population_size &&
         loaded.fitness_term_count == 2;

    snprintf(invalid_path, sizeof(invalid_path), "%s/unknown.ini", TEST_ROOT);
    file = fopen(invalid_path, "w");
    if (file == NULL)
        return 0;
    fprintf(file,
            "[evolution]\nenabled=true\nunknown_key=1\n"
            "[genome]\nevolve_exc_weights=true\n"
            "[fitness]\nterm_0=activity_total_spikes,minimize,0,1,1\n");
    fclose(file);
    ok = ok && !evolution_config_load_file(
        invalid_path, &loaded, &base, error, sizeof(error));

    snprintf(invalid_path, sizeof(invalid_path), "%s/duplicate.ini", TEST_ROOT);
    file = fopen(invalid_path, "w");
    if (file == NULL)
        return 0;
    fprintf(file,
            "[evolution]\nenabled=true\nenabled=true\n"
            "[genome]\nevolve_exc_weights=true\n"
            "[fitness]\nterm_0=activity_total_spikes,minimize,0,1,1\n");
    fclose(file);
    ok = ok && !evolution_config_load_file(
        invalid_path, &loaded, &base, error, sizeof(error));

    config.population_size = 1;
    ok = ok && !evolution_config_validate(&config, &base, error, sizeof(error));
    config.population_size = 4;
    config.mutation_scale = NAN;
    ok = ok && !evolution_config_validate(&config, &base, error, sizeof(error));
    config.mutation_scale = 0.1;
    snprintf(config.selection, sizeof(config.selection), "roulette");
    ok = ok && !evolution_config_validate(&config, &base, error, sizeof(error));
    return ok;
}

static int test_fixed_blueprint(const ScenarioConfig *base)
{
    ScenarioBlueprint first;
    ScenarioBlueprint second;
    char error[256] = {0};
    int ok;

    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    ok = scenario_runner_capture_blueprint(base, &first, error, sizeof(error)) &&
         scenario_runner_capture_blueprint(base, &second, error, sizeof(error)) &&
         first.neuron_count == second.neuron_count &&
         first.inhibitory_count == second.inhibitory_count &&
         first.connection_count == second.connection_count &&
         first.topology_signature == second.topology_signature;
    for (size_t i = 0; ok && i < first.connection_count; i++)
    {
        ok = first.connections[i].source == second.connections[i].source &&
             first.connections[i].target == second.connections[i].target &&
             first.connections[i].delay == second.connections[i].delay &&
             first.connections[i].source_type == second.connections[i].source_type &&
             first.connections[i].target_type == second.connections[i].target_type;
    }
    scenario_blueprint_destroy(&first);
    scenario_blueprint_destroy(&second);
    return ok;
}

static int test_runner_and_resume(
    const char *config_path,
    const ScenarioConfig *base)
{
    EvolutionRunnerOptions options;
    EvolutionRunResult full;
    EvolutionRunResult partial;
    EvolutionRunResult resumed;
    char error[512] = {0};
    char full_dir[512];
    char resume_dir[512];
    char left[1024];
    char right[1024];
    const char *deterministic_files[] = {
        "best_genome.csv", "best_network_initial.csv", "checkpoint.txt",
        "fitness_terms.csv", "genomes.csv", "lineage.csv"
    };
    int ok;

    evolution_runner_default_options(&options);
    options.output_root = TEST_ROOT "/full";
    ok = evolution_runner_execute(config_path, &options, &full,
                                  error, sizeof(error)) && full.completed &&
         full.generations_completed == 4 && full.gene_count > 0;
    snprintf(full_dir, sizeof(full_dir), "%s", full.output_directory);

    options.output_root = TEST_ROOT "/resume";
    options.stop_after_generations = 2;
    ok = ok && evolution_runner_execute(config_path, &options, &partial,
                                        error, sizeof(error)) &&
         !partial.completed && partial.generations_completed == 2;
    snprintf(resume_dir, sizeof(resume_dir), "%s", partial.output_directory);
    options.stop_after_generations = 0;
    ok = ok && evolution_runner_resume(resume_dir, &options, &resumed,
                                       error, sizeof(error)) &&
         resumed.completed && resumed.generations_completed == 4 &&
         resumed.best_individual_id == full.best_individual_id &&
         fabs(resumed.best_fitness - full.best_fitness) <= TEST_TOLERANCE;

    for (size_t i = 0; ok &&
         i < sizeof(deterministic_files) / sizeof(deterministic_files[0]); i++)
    {
        snprintf(left, sizeof(left), "%s/%s", full_dir, deterministic_files[i]);
        snprintf(right, sizeof(right), "%s/%s", resume_dir, deterministic_files[i]);
        ok = files_equal(left, right) && !file_contains_nonfinite(left);
    }

    snprintf(left, sizeof(left), "%s/best_run/weights_initial.csv", full_dir);
    snprintf(right, sizeof(right), "%s/best_run/weights_final.csv", full_dir);
    ok = ok && file_exists(left) && file_exists(right);
    ok = ok && test_fixed_blueprint(base);
    return ok;
}

static int read_first_weight(const char *path, double *out_weight)
{
    FILE *file = fopen(path, "r");
    char line[512];
    size_t id;
    size_t source;
    size_t target;
    char source_type[8];
    char target_type[8];
    unsigned int delay;
    double weight;

    if (file == NULL || fgets(line, sizeof(line), file) == NULL ||
        fgets(line, sizeof(line), file) == NULL)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    fclose(file);
    if (sscanf(line, "%zu,%zu,%zu,%7[^,],%7[^,],%u,%lf",
               &id, &source, &target, source_type, target_type,
               &delay, &weight) != 7 || !isfinite(weight))
        return 0;
    *out_weight = weight;
    return 1;
}

static int test_darwinian_replay(
    const char *base_path,
    const char *config_path)
{
    EvolutionExperimentConfig config;
    EvolutionRunnerOptions options;
    EvolutionRunResult result;
    char error[512] = {0};
    char initial_path[1024];
    char final_path[1024];
    double initial_weight;
    double final_weight;

    configure_evolution(&config, "darwinian_test", base_path, 1);
    config.population_size = 2;
    config.generations = 2;
    config.elite_count = 1;
    config.tournament_size = 2;
    config.evaluation_replicates = 1;
    config.mutation_rate = 0.0;
    config.crossover_rate = 0.0;
    snprintf(config.fitness_terms[0].metric,
             sizeof(config.fitness_terms[0].metric), "neuron_spikes:1");
    config.fitness_terms[0].neuron_id = 1;
    if (!evolution_config_save_file(config_path, &config,
                                    error, sizeof(error)))
        return 0;

    evolution_runner_default_options(&options);
    options.output_root = TEST_ROOT "/darwinian";
    if (!evolution_runner_execute(config_path, &options, &result,
                                 error, sizeof(error)) || !result.completed)
        return 0;
    snprintf(initial_path, sizeof(initial_path),
             "%s/best_run/weights_initial.csv", result.output_directory);
    snprintf(final_path, sizeof(final_path),
             "%s/best_run/weights_final.csv", result.output_directory);
    return read_first_weight(initial_path, &initial_weight) &&
           read_first_weight(final_path, &final_weight) &&
           fabs(initial_weight - final_weight) > 1e-9;
}

int main(void)
{
    const char *base_path = TEST_ROOT "/base.ini";
    const char *plasticity_base_path = TEST_ROOT "/plasticity_base.ini";
    const char *config_path = TEST_ROOT "/evolution.ini";
    const char *darwinian_config_path = TEST_ROOT "/darwinian.ini";
    EvolutionExperimentConfig config;
    ScenarioConfig loaded_base;
    char error[512] = {0};
    int ok;

    remove_tree(TEST_ROOT);
    if (!ensure_directory("build") || !ensure_directory(TEST_ROOT) ||
        !configure_base(base_path, 0) ||
        !configure_base(plasticity_base_path, 1) ||
        !test_config_parser(base_path, config_path))
    {
        fprintf(stderr, "Falha na preparacao do teste evolutivo.\n");
        remove_tree(TEST_ROOT);
        return 1;
    }

    configure_evolution(&config, "runner_test", base_path, 0);
    ok = evolution_config_save_file(config_path, &config,
                                    error, sizeof(error));
    if (!ok)
        fprintf(stderr, "Etapa save config falhou: %s\n", error);
    if (ok)
    {
        ok = evolution_config_load_file(config_path, &config, &loaded_base,
                                        error, sizeof(error));
        if (!ok)
            fprintf(stderr, "Etapa load config falhou: %s\n", error);
    }
    if (ok)
    {
        ok = test_runner_and_resume(config_path, &loaded_base);
        if (!ok)
            fprintf(stderr, "Etapa runner/resume falhou.\n");
    }
    if (ok)
    {
        ok = test_darwinian_replay(plasticity_base_path, darwinian_config_path);
        if (!ok)
            fprintf(stderr, "Etapa darwiniana falhou.\n");
    }

    if (!remove_tree(TEST_ROOT))
        ok = 0;
    if (!ok)
    {
        fprintf(stderr, "Evolution runner validation FAILED: %s\n", error);
        return 1;
    }

    printf("Evolution runner validation OK\n");
    return 0;
}
