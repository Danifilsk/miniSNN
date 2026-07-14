#include <math.h>
#include <stdio.h>
#include <string.h>

#include "evolution.h"

#define TOLERANCE 1e-12

static int close_enough(double left, double right)
{
    return fabs(left - right) <= TOLERANCE;
}

static EvolutionEngineConfig engine_config(void)
{
    EvolutionEngineConfig config;

    memset(&config, 0, sizeof(config));
    config.population_size = 4;
    config.elite_count = 1;
    config.tournament_size = 2;
    config.crossover_rate = 0.75;
    config.mutation_rate = 0.5;
    config.mutation_scale = 0.1;
    config.initialization_scale = 0.2;
    config.replicate_std_penalty = 0.25;
    config.initialization = EVOLUTION_INITIALIZATION_BASELINE_PLUS_MUTATION;
    config.evolution_seed = 12345U;
    return config;
}

static void fill_metadata(EvolutionGeneMetadata metadata[3])
{
    memset(metadata, 0, 3 * sizeof(*metadata));

    metadata[0].gene_index = 0;
    snprintf(metadata[0].gene_name, sizeof(metadata[0].gene_name), "plasticity.a_plus");
    metadata[0].gene_kind = EVOLUTION_GENE_SCALAR_PARAMETER;
    metadata[0].minimum = 0.0;
    metadata[0].maximum = 5.0;
    metadata[0].baseline_value = 1.0;
    metadata[0].mutation_scale = 0.05;
    snprintf(
        metadata[0].parameter_path,
        sizeof(metadata[0].parameter_path),
        "plasticity.a_plus");

    metadata[1].gene_index = 1;
    snprintf(metadata[1].gene_name, sizeof(metadata[1].gene_name), "exc_weight_2");
    metadata[1].gene_kind = EVOLUTION_GENE_EXC_CONNECTION_WEIGHT;
    metadata[1].minimum = 0.0;
    metadata[1].maximum = 500.0;
    metadata[1].baseline_value = 200.0;
    metadata[1].mutation_scale = 0.0;
    metadata[1].connection_id = 2;
    metadata[1].has_connection_id = 1;

    metadata[2].gene_index = 2;
    snprintf(metadata[2].gene_name, sizeof(metadata[2].gene_name), "inh_magnitude_7");
    metadata[2].gene_kind = EVOLUTION_GENE_INH_CONNECTION_MAGNITUDE;
    metadata[2].minimum = 0.0;
    metadata[2].maximum = 600.0;
    metadata[2].baseline_value = 400.0;
    metadata[2].mutation_scale = 0.0;
    metadata[2].connection_id = 7;
    metadata[2].has_connection_id = 1;
}

static int test_prng(void)
{
    static const unsigned int expected[] = {
        0xa15c02b7U,
        0x7b47f409U,
        0xba1d3330U,
        0x83d2f293U,
        0xbfa4784bU,
        0xcbed606eU
    };
    EvolutionPrng first;
    EvolutionPrng restored;

    evolution_prng_seed(&first, 42U, 54U);
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
    {
        if (evolution_prng_next(&first) != expected[i])
            return 0;
    }

    restored = first;
    for (int i = 0; i < 100; i++)
    {
        if (evolution_prng_next(&first) != evolution_prng_next(&restored))
            return 0;
    }

    return 1;
}

static int test_fitness(void)
{
    double scores[] = {1.0, 0.5, 0.25};
    double weights[] = {2.0, 1.0, 1.0};
    double replicates[] = {0.2, 0.4, 0.6};
    double weighted;
    double mean;
    double standard_deviation;
    double selection;
    double expected_std = sqrt((0.04 + 0.0 + 0.04) / 3.0);

    return close_enough(
               evolution_fitness_score(
                   EVOLUTION_FITNESS_TARGET, 5.0, 5.0, 2.0),
               1.0) &&
           close_enough(
               evolution_fitness_score(
                   EVOLUTION_FITNESS_TARGET, 7.0, 5.0, 2.0),
               exp(-0.5)) &&
           close_enough(
               evolution_fitness_score(
                   EVOLUTION_FITNESS_MAXIMIZE, 5.0, 5.0, 1.0),
               0.5) &&
           close_enough(
               evolution_fitness_score(
                   EVOLUTION_FITNESS_MINIMIZE, 5.0, 5.0, 1.0),
               0.5) &&
           close_enough(
               evolution_fitness_score(
                   EVOLUTION_FITNESS_MAXIMIZE, 10000.0, 0.0, 1.0),
               1.0) &&
           close_enough(
               evolution_fitness_score(
                   EVOLUTION_FITNESS_MINIMIZE, 10000.0, 0.0, 1.0),
               0.0) &&
           evolution_fitness_weighted_mean(scores, weights, 3, &weighted) &&
           close_enough(weighted, 0.6875) &&
           evolution_aggregate_replicates(
               replicates,
               3,
               0.5,
               &mean,
               &standard_deviation,
               &selection) &&
           close_enough(mean, 0.4) &&
           close_enough(standard_deviation, expected_std) &&
           close_enough(selection, 0.4 - 0.5 * expected_std) &&
           evolution_fitness_score(
               EVOLUTION_FITNESS_TARGET, NAN, 0.0, 1.0) < 0.0;
}

static int test_initialization_and_bounds(void)
{
    EvolutionEngineConfig config = engine_config();
    EvolutionGeneMetadata metadata[3];
    EvolutionEngine first;
    EvolutionEngine second;
    int ok;

    fill_metadata(metadata);
    ok = evolution_engine_init(&first, &config, metadata, 3) &&
         evolution_engine_init(&second, &config, metadata, 3) &&
         evolution_engine_initialize_population(&first) &&
         evolution_engine_initialize_population(&second);

    if (!ok)
    {
        evolution_engine_destroy(&first);
        evolution_engine_destroy(&second);
        return 0;
    }

    for (size_t gene = 0; gene < 3; gene++)
    {
        if (!close_enough(first.population[0].genes[gene], metadata[gene].baseline_value))
            ok = 0;
    }

    for (size_t individual = 0; individual < config.population_size; individual++)
    {
        for (size_t gene = 0; gene < 3; gene++)
        {
            double value = first.population[individual].genes[gene];
            if (value < metadata[gene].minimum || value > metadata[gene].maximum ||
                !close_enough(value, second.population[individual].genes[gene]))
            {
                ok = 0;
            }
        }
    }

    evolution_engine_destroy(&first);
    evolution_engine_destroy(&second);

    config.initialization = EVOLUTION_INITIALIZATION_UNIFORM;
    ok = ok && evolution_engine_init(&first, &config, metadata, 3) &&
         evolution_engine_initialize_population(&first);
    if (ok)
    {
        for (size_t individual = 0; individual < config.population_size; individual++)
        {
            for (size_t gene = 0; gene < 3; gene++)
            {
                double value = first.population[individual].genes[gene];
                if (value < metadata[gene].minimum || value > metadata[gene].maximum)
                    ok = 0;
            }
        }
    }
    evolution_engine_destroy(&first);
    return ok;
}

static int evaluate_controlled_population(EvolutionEngine *engine)
{
    static const double values[] = {0.5, 0.8, 0.8, 0.1};
    static const double means[] = {0.5, 0.8, 0.8, 0.1};
    static const double deviations[] = {0.0, 0.2, 0.1, 0.0};

    for (size_t i = 0; i < 4; i++)
    {
        if (!evolution_engine_set_evaluation(
                engine,
                i,
                means[i],
                deviations[i],
                values[i],
                values[i],
                2,
                0))
        {
            return 0;
        }
    }
    return 1;
}

static int test_selection_elitism_crossover_and_mutation(void)
{
    EvolutionEngineConfig config = engine_config();
    EvolutionGeneMetadata metadata[3];
    EvolutionEngine engine;
    size_t ranking[4];
    double elite_genes[3];
    uint64_t elite_parent;
    int ok;

    fill_metadata(metadata);
    config.crossover_rate = 1.0;
    config.mutation_rate = 1.0;
    config.mutation_scale = 1.0;

    ok = evolution_engine_init(&engine, &config, metadata, 3) &&
         evolution_engine_initialize_population(&engine) &&
         evaluate_controlled_population(&engine) &&
         evolution_engine_rank_population(&engine, ranking, 4) &&
         ranking[0] == 2 && ranking[1] == 1;

    if (ok)
    {
        elite_parent = engine.population[ranking[0]].individual_id;
        memcpy(elite_genes, engine.population[ranking[0]].genes, sizeof(elite_genes));
        ok = evolution_engine_breed_next_generation(&engine);
    }

    if (ok)
    {
        EvolutionIndividual *elite = &engine.population[0];
        ok = elite->generation == 1 &&
             strcmp(elite->operation, "elite_copy") == 0 &&
             elite->has_parent_a && elite->parent_a_id == elite_parent &&
             !elite->has_parent_b && elite->mutation.mutation_count == 0;

        for (size_t gene = 0; ok && gene < 3; gene++)
            ok = close_enough(elite->genes[gene], elite_genes[gene]);

        for (size_t i = 1; ok && i < config.population_size; i++)
        {
            EvolutionIndividual *child = &engine.population[i];
            ok = child->crossover_applied &&
                 child->genes_from_parent_a + child->genes_from_parent_b == 3 &&
                 child->mutation.mutation_count == 3;

            for (size_t gene = 0; ok && gene < 3; gene++)
            {
                ok = child->genes[gene] >= metadata[gene].minimum &&
                     child->genes[gene] <= metadata[gene].maximum;
            }
        }
    }

    evolution_engine_destroy(&engine);
    return ok;
}

static int test_sign_convention_and_edge_cases(void)
{
    EvolutionEngineConfig config = engine_config();
    EvolutionGeneMetadata metadata[3];

    fill_metadata(metadata);
    config.population_size = 2;
    config.elite_count = 1;
    config.tournament_size = 2;
    config.crossover_rate = 0.0;
    config.mutation_rate = 0.0;

    if (!evolution_engine_config_is_valid(&config) ||
        !evolution_gene_metadata_is_valid(metadata, 3))
    {
        return 0;
    }

    if (metadata[1].minimum < 0.0 || metadata[2].minimum < 0.0 ||
        -metadata[2].baseline_value > 0.0)
    {
        return 0;
    }

    metadata[0].gene_index = 2;
    if (evolution_gene_metadata_is_valid(metadata, 3))
        return 0;
    metadata[0].gene_index = 0;

    config.elite_count = 2;
    if (evolution_engine_config_is_valid(&config))
        return 0;

    return 1;
}

static int test_checkpoint_round_trip(void)
{
    EvolutionEngineConfig config = engine_config();
    EvolutionGeneMetadata metadata[3];
    EvolutionEngine source;
    EvolutionEngine restored;
    FILE *file = NULL;
    int next_generation = -1;
    int completed = -1;
    int ok;

    fill_metadata(metadata);
    memset(&source, 0, sizeof(source));
    memset(&restored, 0, sizeof(restored));
    ok = evolution_engine_init(&source, &config, metadata, 3) &&
         evolution_engine_initialize_population(&source) &&
         evaluate_controlled_population(&source) &&
         evolution_engine_breed_next_generation(&source);

    if (ok)
        file = tmpfile();
    if (file == NULL)
        ok = 0;

    if (ok)
    {
        ok = evolution_engine_write_checkpoint(
                 &source,
                 file,
                 "test-signature",
                 1,
                 0) &&
             fflush(file) == 0 && fseek(file, 0, SEEK_SET) == 0 &&
             evolution_engine_read_checkpoint(
                 &restored,
                 file,
                 &config,
                 metadata,
                 3,
                 "test-signature",
                 &next_generation,
                 &completed);
    }

    if (ok)
    {
        ok = next_generation == 1 && !completed &&
             restored.prng.state == source.prng.state &&
             restored.prng.increment == source.prng.increment &&
             restored.next_individual_id == source.next_individual_id &&
             restored.current_generation == source.current_generation &&
             restored.global_best_individual_id == source.global_best_individual_id;

        for (size_t i = 0; ok && i < config.population_size; i++)
        {
            ok = restored.population[i].individual_id ==
                     source.population[i].individual_id &&
                 strcmp(restored.population[i].operation,
                        source.population[i].operation) == 0;
            for (size_t gene = 0; ok && gene < 3; gene++)
            {
                ok = close_enough(
                    restored.population[i].genes[gene],
                    source.population[i].genes[gene]);
            }
        }

        for (size_t gene = 0; ok && gene < 3; gene++)
        {
            ok = close_enough(
                restored.global_best_genes[gene],
                source.global_best_genes[gene]);
        }
    }

    if (file != NULL)
        fclose(file);
    evolution_engine_destroy(&source);
    evolution_engine_destroy(&restored);
    return ok;
}

int main(void)
{
    if (!test_prng() ||
        !test_fitness() ||
        !test_initialization_and_bounds() ||
        !test_selection_elitism_crossover_and_mutation() ||
        !test_sign_convention_and_edge_cases() ||
        !test_checkpoint_round_trip())
    {
        fprintf(stderr, "Neuroevolution numerical validation FAILED\n");
        return 1;
    }

    printf("Neuroevolution numerical validation OK\n");
    return 0;
}
