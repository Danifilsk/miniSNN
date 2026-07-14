#ifndef EVOLUTION_RUNNER_H
#define EVOLUTION_RUNNER_H

#include <stddef.h>

#define EVOLUTION_OUTPUT_PATH_MAX 512
#define EVOLUTION_ACTUAL_NAME_MAX 128

typedef struct
{
    const char *output_root;
    int stop_after_generations;
} EvolutionRunnerOptions;

typedef struct
{
    char output_directory[EVOLUTION_OUTPUT_PATH_MAX];
    char actual_experiment_name[EVOLUTION_ACTUAL_NAME_MAX];
    int completed;
    int generations_completed;
    size_t gene_count;
    unsigned long long best_individual_id;
    double best_fitness;
} EvolutionRunResult;

void evolution_runner_default_options(EvolutionRunnerOptions *options);

int evolution_runner_execute(
    const char *config_path,
    const EvolutionRunnerOptions *options,
    EvolutionRunResult *out_result,
    char *error_message,
    size_t error_message_size);

int evolution_runner_resume(
    const char *experiment_directory,
    const EvolutionRunnerOptions *options,
    EvolutionRunResult *out_result,
    char *error_message,
    size_t error_message_size);

#endif
