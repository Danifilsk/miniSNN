#ifndef EVOLUTION_CONFIG_H
#define EVOLUTION_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#include "evolution.h"
#include "scenario_config.h"

#define EVOLUTION_EXPERIMENT_NAME_MAX 64
#define EVOLUTION_BASE_SCENARIO_MAX 260
#define EVOLUTION_METHOD_NAME_MAX 32
#define EVOLUTION_MAX_SCALAR_GENES 16
#define EVOLUTION_MAX_FITNESS_TERMS 32
#define EVOLUTION_METRIC_NAME_MAX 95
#define EVOLUTION_GENOME_MODE_MAX 31
#define EVOLUTION_MAX_REQUIRED_NEURONS 256

typedef struct
{
    int index;
    char parameter_path[EVOLUTION_PARAMETER_PATH_MAX + 1];
    double minimum;
    double maximum;
    double mutation_scale;
} EvolutionScalarGeneConfig;

typedef struct
{
    int index;
    char metric[EVOLUTION_METRIC_NAME_MAX + 1];
    EvolutionFitnessGoal goal;
    double target;
    double scale;
    double weight;
    int neuron_id;
    int has_neuron_id;
} EvolutionFitnessTermConfig;

typedef struct
{
    int enabled;
    char experiment_name[EVOLUTION_EXPERIMENT_NAME_MAX + 1];
    char base_scenario[EVOLUTION_BASE_SCENARIO_MAX + 1];

    int population_size;
    int generations;
    int elite_count;
    char selection[EVOLUTION_METHOD_NAME_MAX];
    int tournament_size;
    char crossover[EVOLUTION_METHOD_NAME_MAX];
    double crossover_rate;
    char mutation[EVOLUTION_METHOD_NAME_MAX];
    double mutation_rate;
    double mutation_scale;
    char initialization[EVOLUTION_METHOD_NAME_MAX];
    double initialization_scale;
    uint64_t evolution_seed;

    int evaluation_replicates;
    uint64_t evaluation_seed_base;
    double replicate_std_penalty;

    int checkpoint_interval_generations;
    int save_all_genomes;
    int save_best_run;
    int auto_unique_run;
    int history_enabled;

    char genome_mode[EVOLUTION_GENOME_MODE_MAX + 1];

    int evolve_exc_weights;
    double exc_weight_min;
    double exc_weight_max;
    int evolve_inh_magnitudes;
    double inh_magnitude_min;
    double inh_magnitude_max;

    int structure_enabled;
    int structure_allow_add;
    int structure_allow_remove;
    int structure_allow_rewire;
    int structure_evolve_delays;
    double structure_add_rate;
    double structure_remove_rate;
    double structure_rewire_rate;
    double structure_delay_mutation_rate;
    int structure_max_mutations_per_child;
    int structure_min_connections;
    int structure_max_connections;
    int structure_allow_self_connections;
    int structure_allow_inh_to_inh;
    int structure_delay_min;
    int structure_delay_max;
    int structure_delay_mutation_max_delta;
    double structure_new_exc_weight_min;
    double structure_new_exc_weight_max;
    double structure_new_inh_magnitude_min;
    double structure_new_inh_magnitude_max;
    double structure_complexity_penalty;
    int structure_preserve_required_reachability;
    int structure_required_input_count;
    int structure_required_input_neurons[EVOLUTION_MAX_REQUIRED_NEURONS];
    int structure_required_output_count;
    int structure_required_output_neurons[EVOLUTION_MAX_REQUIRED_NEURONS];

    int scalar_gene_count;
    EvolutionScalarGeneConfig scalar_genes[EVOLUTION_MAX_SCALAR_GENES];
    int fitness_term_count;
    EvolutionFitnessTermConfig fitness_terms[EVOLUTION_MAX_FITNESS_TERMS];
} EvolutionExperimentConfig;

void evolution_config_default(EvolutionExperimentConfig *config);

int evolution_config_load_file(
    const char *filename,
    EvolutionExperimentConfig *out_config,
    ScenarioConfig *out_base_scenario,
    char *error_message,
    size_t error_message_size);

int evolution_config_validate(
    const EvolutionExperimentConfig *config,
    const ScenarioConfig *base_scenario,
    char *error_message,
    size_t error_message_size);

int evolution_config_save_file(
    const char *filename,
    const EvolutionExperimentConfig *config,
    char *error_message,
    size_t error_message_size);

int evolution_config_scalar_baseline(
    const ScenarioConfig *base_scenario,
    const char *parameter_path,
    double *out_value);

const char *evolution_fitness_goal_name(EvolutionFitnessGoal goal);

#endif
