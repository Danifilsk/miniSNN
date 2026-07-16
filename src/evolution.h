#ifndef EVOLUTION_H
#define EVOLUTION_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define EVOLUTION_GENE_NAME_MAX 95
#define EVOLUTION_PARAMETER_PATH_MAX 95
#define EVOLUTION_OPERATION_MAX 31

typedef enum
{
    EVOLUTION_GENE_SCALAR_PARAMETER = 0,
    EVOLUTION_GENE_EXC_CONNECTION_WEIGHT = 1,
    EVOLUTION_GENE_INH_CONNECTION_MAGNITUDE = 2
} EvolutionGeneKind;

typedef enum
{
    EVOLUTION_INITIALIZATION_BASELINE_PLUS_MUTATION = 0,
    EVOLUTION_INITIALIZATION_UNIFORM = 1
} EvolutionInitialization;

typedef enum
{
    EVOLUTION_FITNESS_TARGET = 0,
    EVOLUTION_FITNESS_MAXIMIZE = 1,
    EVOLUTION_FITNESS_MINIMIZE = 2
} EvolutionFitnessGoal;

typedef struct
{
    uint64_t state;
    uint64_t increment;
} EvolutionPrng;

typedef struct
{
    size_t gene_index;
    char gene_name[EVOLUTION_GENE_NAME_MAX + 1];
    EvolutionGeneKind gene_kind;
    double minimum;
    double maximum;
    double baseline_value;
    double mutation_scale;
    size_t connection_id;
    int has_connection_id;
    char parameter_path[EVOLUTION_PARAMETER_PATH_MAX + 1];
} EvolutionGeneMetadata;

typedef struct
{
    size_t mutation_count;
    double mutation_absolute_sum;
    double mutation_max_absolute;
    size_t clamp_min_count;
    size_t clamp_max_count;
} EvolutionMutationStats;

typedef struct
{
    uint64_t individual_id;
    int generation;
    uint64_t parent_a_id;
    uint64_t parent_b_id;
    int has_parent_a;
    int has_parent_b;
    char operation[EVOLUTION_OPERATION_MAX + 1];

    double *genes;
    size_t gene_count;

    double fitness_selection;
    double fitness_mean;
    double fitness_std;
    double fitness_min;
    double fitness_max;
    int valid_replicates;
    int invalid_replicates;
    int evaluated;

    EvolutionMutationStats mutation;
    int crossover_applied;
    size_t genes_from_parent_a;
    size_t genes_from_parent_b;
} EvolutionIndividual;

typedef struct
{
    size_t population_size;
    size_t elite_count;
    size_t tournament_size;
    double crossover_rate;
    double mutation_rate;
    double mutation_scale;
    double initialization_scale;
    double replicate_std_penalty;
    EvolutionInitialization initialization;
    uint64_t evolution_seed;
} EvolutionEngineConfig;

typedef struct
{
    EvolutionEngineConfig config;
    EvolutionGeneMetadata *metadata;
    size_t gene_count;
    EvolutionIndividual *population;
    EvolutionPrng prng;
    uint64_t next_individual_id;
    int current_generation;

    int has_global_best;
    uint64_t global_best_individual_id;
    int global_best_generation;
    double global_best_fitness_selection;
    double global_best_fitness_mean;
    double global_best_fitness_std;
    double *global_best_genes;
} EvolutionEngine;

static inline uint32_t evolution_prng_next(EvolutionPrng *prng)
{
    uint64_t old_state;
    uint32_t xor_shifted;
    uint32_t rotation;
    if (prng == NULL)
        return 0U;
    old_state = prng->state;
    prng->state = old_state * 6364136223846793005ULL + prng->increment;
    xor_shifted = (uint32_t)(((old_state >> 18U) ^ old_state) >> 27U);
    rotation = (uint32_t)(old_state >> 59U);
    return (xor_shifted >> rotation) |
           (xor_shifted << ((0U - rotation) & 31U));
}

static inline void evolution_prng_seed(
    EvolutionPrng *prng,
    uint64_t initial_state,
    uint64_t sequence)
{
    if (prng == NULL)
        return;
    prng->state = 0U;
    prng->increment = (sequence << 1U) | 1U;
    (void)evolution_prng_next(prng);
    prng->state += initial_state;
    (void)evolution_prng_next(prng);
}

static inline double evolution_prng_unit(EvolutionPrng *prng)
{
    return (double)evolution_prng_next(prng) / 4294967296.0;
}

double evolution_fitness_score(
    EvolutionFitnessGoal goal,
    double observed,
    double target,
    double scale);

int evolution_fitness_weighted_mean(
    const double *scores,
    const double *weights,
    size_t count,
    double *out_fitness);

int evolution_aggregate_replicates(
    const double *fitness_values,
    size_t count,
    double std_penalty,
    double *out_mean,
    double *out_std,
    double *out_selection);

int evolution_engine_config_is_valid(
    const EvolutionEngineConfig *config);

int evolution_gene_metadata_is_valid(
    const EvolutionGeneMetadata *metadata,
    size_t count);

int evolution_engine_init(
    EvolutionEngine *engine,
    const EvolutionEngineConfig *config,
    const EvolutionGeneMetadata *metadata,
    size_t gene_count);

void evolution_engine_destroy(EvolutionEngine *engine);

int evolution_engine_initialize_population(EvolutionEngine *engine);

int evolution_engine_set_evaluation(
    EvolutionEngine *engine,
    size_t population_index,
    double fitness_mean,
    double fitness_std,
    double fitness_min,
    double fitness_max,
    int valid_replicates,
    int invalid_replicates);

int evolution_engine_set_evaluation_with_selection(
    EvolutionEngine *engine,
    size_t population_index,
    double fitness_mean,
    double fitness_std,
    double fitness_min,
    double fitness_max,
    double fitness_selection,
    int valid_replicates,
    int invalid_replicates);

int evolution_engine_tournament_select(
    EvolutionEngine *engine,
    size_t *out_population_index);

int evolution_engine_rank_population(
    const EvolutionEngine *engine,
    size_t *out_indices,
    size_t count);

int evolution_engine_breed_next_generation(EvolutionEngine *engine);

int evolution_engine_breed_next_generation_deferred_mutation(
    EvolutionEngine *engine);

int evolution_engine_mutate_population_individual(
    EvolutionEngine *engine,
    size_t population_index);

int evolution_engine_write_checkpoint(
    const EvolutionEngine *engine,
    FILE *file,
    const char *signature,
    int next_generation,
    int completed);

int evolution_engine_read_checkpoint(
    EvolutionEngine *engine,
    FILE *file,
    const EvolutionEngineConfig *config,
    const EvolutionGeneMetadata *metadata,
    size_t gene_count,
    const char *expected_signature,
    int *out_next_generation,
    int *out_completed);

#endif
