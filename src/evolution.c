#include "evolution.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define EVOLUTION_CHECKPOINT_MAGIC "MINISNN_EVOLUTION_CHECKPOINT_V1"

static double clamp_value(double value, double minimum, double maximum)
{
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

static void individual_reset(EvolutionIndividual *individual)
{
    if (individual == NULL)
        return;

    free(individual->genes);
    memset(individual, 0, sizeof(*individual));
}

static void population_destroy(
    EvolutionIndividual *population,
    size_t population_size)
{
    if (population == NULL)
        return;

    for (size_t i = 0; i < population_size; i++)
        individual_reset(&population[i]);

    free(population);
}

static int individual_allocate(
    EvolutionIndividual *individual,
    size_t gene_count)
{
    memset(individual, 0, sizeof(*individual));
    individual->gene_count = gene_count;
    individual->fitness_min = 0.0;
    individual->fitness_max = 0.0;

    if (gene_count == 0)
        return 1;

    individual->genes = calloc(gene_count, sizeof(*individual->genes));
    return individual->genes != NULL;
}

static int individual_copy_genes(
    EvolutionIndividual *destination,
    const EvolutionIndividual *source)
{
    if (destination == NULL || source == NULL ||
        destination->gene_count != source->gene_count)
    {
        return 0;
    }

    if (source->gene_count == 0)
        return 1;
    if (destination->genes == NULL || source->genes == NULL)
        return 0;

    memcpy(
        destination->genes,
        source->genes,
        source->gene_count * sizeof(*source->genes));
    return 1;
}

static int individual_is_better(
    const EvolutionIndividual *left,
    const EvolutionIndividual *right)
{
    if (left->fitness_selection != right->fitness_selection)
        return left->fitness_selection > right->fitness_selection;
    if (left->fitness_mean != right->fitness_mean)
        return left->fitness_mean > right->fitness_mean;
    if (left->fitness_std != right->fitness_std)
        return left->fitness_std < right->fitness_std;
    return left->individual_id < right->individual_id;
}

static int update_global_best(
    EvolutionEngine *engine,
    const EvolutionIndividual *individual)
{
    int better = 0;

    if (!engine->has_global_best)
    {
        better = 1;
    }
    else if (individual->fitness_selection >
             engine->global_best_fitness_selection)
    {
        better = 1;
    }
    else if (individual->fitness_selection ==
             engine->global_best_fitness_selection)
    {
        if (individual->fitness_mean > engine->global_best_fitness_mean)
            better = 1;
        else if (individual->fitness_mean == engine->global_best_fitness_mean &&
                 individual->fitness_std < engine->global_best_fitness_std)
            better = 1;
        else if (individual->fitness_mean == engine->global_best_fitness_mean &&
                 individual->fitness_std == engine->global_best_fitness_std &&
                 individual->individual_id < engine->global_best_individual_id)
            better = 1;
    }

    if (!better)
        return 1;

    if (engine->gene_count > 0)
    {
        memcpy(
            engine->global_best_genes,
            individual->genes,
            engine->gene_count * sizeof(*individual->genes));
    }
    engine->has_global_best = 1;
    engine->global_best_individual_id = individual->individual_id;
    engine->global_best_generation = individual->generation;
    engine->global_best_fitness_selection = individual->fitness_selection;
    engine->global_best_fitness_mean = individual->fitness_mean;
    engine->global_best_fitness_std = individual->fitness_std;
    return 1;
}

static double stable_sigmoid(double value)
{
    if (value >= 0.0)
    {
        double exponential = exp(-value);
        return 1.0 / (1.0 + exponential);
    }

    {
        double exponential = exp(value);
        return exponential / (1.0 + exponential);
    }
}

double evolution_fitness_score(
    EvolutionFitnessGoal goal,
    double observed,
    double target,
    double scale)
{
    double z;
    double score;

    if (!isfinite(observed) || !isfinite(target) ||
        !isfinite(scale) || scale <= 0.0)
    {
        return -1.0;
    }

    z = (observed - target) / scale;

    if (goal == EVOLUTION_FITNESS_TARGET)
    {
        score = exp(-0.5 * z * z);
    }
    else if (goal == EVOLUTION_FITNESS_MAXIMIZE)
    {
        score = stable_sigmoid(z);
    }
    else if (goal == EVOLUTION_FITNESS_MINIMIZE)
    {
        score = stable_sigmoid(-z);
    }
    else
    {
        return -1.0;
    }

    if (!isfinite(score))
        return 0.0;
    return clamp_value(score, 0.0, 1.0);
}

int evolution_fitness_weighted_mean(
    const double *scores,
    const double *weights,
    size_t count,
    double *out_fitness)
{
    double weighted_sum = 0.0;
    double weight_sum = 0.0;

    if (scores == NULL || weights == NULL || out_fitness == NULL || count == 0)
        return 0;

    for (size_t i = 0; i < count; i++)
    {
        if (!isfinite(scores[i]) || scores[i] < 0.0 || scores[i] > 1.0 ||
            !isfinite(weights[i]) || weights[i] <= 0.0)
        {
            return 0;
        }

        weighted_sum += scores[i] * weights[i];
        weight_sum += weights[i];
    }

    if (!isfinite(weighted_sum) || !isfinite(weight_sum) || weight_sum <= 0.0)
        return 0;

    *out_fitness = clamp_value(weighted_sum / weight_sum, 0.0, 1.0);
    return 1;
}

int evolution_aggregate_replicates(
    const double *fitness_values,
    size_t count,
    double std_penalty,
    double *out_mean,
    double *out_std,
    double *out_selection)
{
    double sum = 0.0;
    double variance_sum = 0.0;
    double mean;

    if (fitness_values == NULL || count == 0 ||
        !isfinite(std_penalty) || std_penalty < 0.0 ||
        out_mean == NULL || out_std == NULL || out_selection == NULL)
    {
        return 0;
    }

    for (size_t i = 0; i < count; i++)
    {
        if (!isfinite(fitness_values[i]) ||
            fitness_values[i] < 0.0 || fitness_values[i] > 1.0)
        {
            return 0;
        }
        sum += fitness_values[i];
    }

    mean = sum / (double)count;
    for (size_t i = 0; i < count; i++)
    {
        double difference = fitness_values[i] - mean;
        variance_sum += difference * difference;
    }

    *out_mean = mean;
    *out_std = sqrt(variance_sum / (double)count);
    *out_selection = clamp_value(
        *out_mean - std_penalty * *out_std,
        0.0,
        1.0);
    return isfinite(*out_std) && isfinite(*out_selection);
}

int evolution_engine_config_is_valid(
    const EvolutionEngineConfig *config)
{
    return config != NULL &&
           config->population_size >= 2 &&
           config->elite_count >= 1 &&
           config->elite_count < config->population_size &&
           config->tournament_size >= 2 &&
           config->tournament_size <= config->population_size &&
           isfinite(config->crossover_rate) &&
           config->crossover_rate >= 0.0 && config->crossover_rate <= 1.0 &&
           isfinite(config->mutation_rate) &&
           config->mutation_rate >= 0.0 && config->mutation_rate <= 1.0 &&
           isfinite(config->mutation_scale) &&
           config->mutation_scale > 0.0 && config->mutation_scale <= 1.0 &&
           isfinite(config->initialization_scale) &&
           config->initialization_scale >= 0.0 &&
           config->initialization_scale <= 1.0 &&
           isfinite(config->replicate_std_penalty) &&
           config->replicate_std_penalty >= 0.0 &&
           (config->initialization ==
                EVOLUTION_INITIALIZATION_BASELINE_PLUS_MUTATION ||
            config->initialization == EVOLUTION_INITIALIZATION_UNIFORM);
}

int evolution_gene_metadata_is_valid(
    const EvolutionGeneMetadata *metadata,
    size_t count)
{
    if (count == 0)
        return metadata == NULL;
    if (metadata == NULL)
        return 0;

    for (size_t i = 0; i < count; i++)
    {
        if (metadata[i].gene_index != i ||
            metadata[i].gene_name[0] == '\0' ||
            metadata[i].gene_kind < EVOLUTION_GENE_SCALAR_PARAMETER ||
            metadata[i].gene_kind > EVOLUTION_GENE_INH_CONNECTION_MAGNITUDE ||
            !isfinite(metadata[i].minimum) ||
            !isfinite(metadata[i].maximum) ||
            metadata[i].minimum >= metadata[i].maximum ||
            !isfinite(metadata[i].baseline_value) ||
            metadata[i].baseline_value < metadata[i].minimum ||
            metadata[i].baseline_value > metadata[i].maximum ||
            !isfinite(metadata[i].mutation_scale) ||
            metadata[i].mutation_scale < 0.0 ||
            metadata[i].mutation_scale > 1.0)
        {
            return 0;
        }

        if (metadata[i].gene_kind == EVOLUTION_GENE_SCALAR_PARAMETER)
        {
            if (metadata[i].parameter_path[0] == '\0' ||
                metadata[i].has_connection_id)
            {
                return 0;
            }
        }
        else if (!metadata[i].has_connection_id)
        {
            return 0;
        }
    }

    return 1;
}

int evolution_engine_init(
    EvolutionEngine *engine,
    const EvolutionEngineConfig *config,
    const EvolutionGeneMetadata *metadata,
    size_t gene_count)
{
    if (engine == NULL ||
        !evolution_engine_config_is_valid(config) ||
        !evolution_gene_metadata_is_valid(metadata, gene_count))
    {
        return 0;
    }

    memset(engine, 0, sizeof(*engine));
    engine->config = *config;
    engine->gene_count = gene_count;
    if (gene_count > 0)
    {
        engine->metadata = malloc(gene_count * sizeof(*engine->metadata));
        engine->global_best_genes = calloc(
            gene_count,
            sizeof(*engine->global_best_genes));
    }

    if (gene_count > 0 &&
        (engine->metadata == NULL || engine->global_best_genes == NULL))
    {
        evolution_engine_destroy(engine);
        return 0;
    }

    if (gene_count > 0)
        memcpy(engine->metadata, metadata, gene_count * sizeof(*metadata));
    evolution_prng_seed(&engine->prng, config->evolution_seed, 54U);
    engine->next_individual_id = 0U;
    engine->current_generation = 0;
    return 1;
}

void evolution_engine_destroy(EvolutionEngine *engine)
{
    if (engine == NULL)
        return;

    population_destroy(engine->population, engine->config.population_size);
    free(engine->metadata);
    free(engine->global_best_genes);
    memset(engine, 0, sizeof(*engine));
}

static int allocate_population(EvolutionEngine *engine)
{
    EvolutionIndividual *population = calloc(
        engine->config.population_size,
        sizeof(*population));

    if (population == NULL)
        return 0;

    for (size_t i = 0; i < engine->config.population_size; i++)
    {
        if (!individual_allocate(&population[i], engine->gene_count))
        {
            population_destroy(population, engine->config.population_size);
            return 0;
        }
    }

    engine->population = population;
    return 1;
}

int evolution_engine_initialize_population(EvolutionEngine *engine)
{
    if (engine == NULL ||
        (engine->gene_count > 0 && engine->metadata == NULL) ||
        engine->population != NULL)
        return 0;

    if (!allocate_population(engine))
        return 0;

    for (size_t individual_index = 0;
         individual_index < engine->config.population_size;
         individual_index++)
    {
        EvolutionIndividual *individual = &engine->population[individual_index];

        individual->individual_id = engine->next_individual_id++;
        individual->generation = 0;
        snprintf(individual->operation, sizeof(individual->operation), "initialization");

        for (size_t gene_index = 0; gene_index < engine->gene_count; gene_index++)
        {
            const EvolutionGeneMetadata *gene = &engine->metadata[gene_index];
            double value;

            if (engine->config.initialization == EVOLUTION_INITIALIZATION_UNIFORM)
            {
                value = gene->minimum +
                        evolution_prng_unit(&engine->prng) *
                            (gene->maximum - gene->minimum);
            }
            else if (individual_index == 0)
            {
                value = gene->baseline_value;
            }
            else
            {
                double unit = evolution_prng_unit(&engine->prng) * 2.0 - 1.0;
                value = clamp_value(
                    gene->baseline_value + unit *
                        engine->config.initialization_scale *
                        (gene->maximum - gene->minimum),
                    gene->minimum,
                    gene->maximum);
            }

            individual->genes[gene_index] = value;
        }
    }

    return 1;
}

int evolution_engine_set_evaluation(
    EvolutionEngine *engine,
    size_t population_index,
    double fitness_mean,
    double fitness_std,
    double fitness_min,
    double fitness_max,
    int valid_replicates,
    int invalid_replicates)
{
    double selection;
    if (engine == NULL)
        return 0;
    selection = clamp_value(
        fitness_mean - engine->config.replicate_std_penalty * fitness_std,
        0.0,
        1.0);
    return evolution_engine_set_evaluation_with_selection(
        engine, population_index, fitness_mean, fitness_std,
        fitness_min, fitness_max, selection,
        valid_replicates, invalid_replicates);
}

int evolution_engine_set_evaluation_with_selection(
    EvolutionEngine *engine,
    size_t population_index,
    double fitness_mean,
    double fitness_std,
    double fitness_min,
    double fitness_max,
    double fitness_selection,
    int valid_replicates,
    int invalid_replicates)
{
    EvolutionIndividual *individual;

    if (engine == NULL || engine->population == NULL ||
        population_index >= engine->config.population_size ||
        !isfinite(fitness_mean) || fitness_mean < 0.0 || fitness_mean > 1.0 ||
        !isfinite(fitness_std) || fitness_std < 0.0 ||
        !isfinite(fitness_min) || fitness_min < 0.0 || fitness_min > 1.0 ||
        !isfinite(fitness_max) || fitness_max < 0.0 || fitness_max > 1.0 ||
        !isfinite(fitness_selection) || fitness_selection < 0.0 ||
        fitness_selection > 1.0 ||
        fitness_min > fitness_max || valid_replicates < 0 || invalid_replicates < 0 ||
        valid_replicates + invalid_replicates <= 0)
    {
        return 0;
    }

    individual = &engine->population[population_index];
    individual->fitness_mean = fitness_mean;
    individual->fitness_std = fitness_std;
    individual->fitness_min = fitness_min;
    individual->fitness_max = fitness_max;
    individual->fitness_selection = fitness_selection;
    individual->valid_replicates = valid_replicates;
    individual->invalid_replicates = invalid_replicates;
    individual->evaluated = 1;
    return update_global_best(engine, individual);
}

int evolution_engine_rank_population(
    const EvolutionEngine *engine,
    size_t *out_indices,
    size_t count)
{
    if (engine == NULL || engine->population == NULL ||
        out_indices == NULL || count < engine->config.population_size)
    {
        return 0;
    }

    for (size_t i = 0; i < engine->config.population_size; i++)
    {
        if (!engine->population[i].evaluated)
            return 0;
        out_indices[i] = i;
    }

    for (size_t i = 1; i < engine->config.population_size; i++)
    {
        size_t value = out_indices[i];
        size_t position = i;

        while (position > 0 &&
               individual_is_better(
                   &engine->population[value],
                   &engine->population[out_indices[position - 1]]))
        {
            out_indices[position] = out_indices[position - 1];
            position--;
        }
        out_indices[position] = value;
    }

    return 1;
}

int evolution_engine_tournament_select(
    EvolutionEngine *engine,
    size_t *out_population_index)
{
    size_t *candidates;
    size_t candidate_count = 0;
    size_t winner = 0;

    if (engine == NULL || engine->population == NULL ||
        out_population_index == NULL ||
        !evolution_engine_config_is_valid(&engine->config))
        return 0;

    candidates = calloc(
        engine->config.tournament_size,
        sizeof(*candidates));
    if (candidates == NULL)
        return 0;

    while (candidate_count < engine->config.tournament_size)
    {
        size_t candidate = (size_t)(
            evolution_prng_unit(&engine->prng) *
            (double)engine->config.population_size);
        int duplicate = 0;

        if (candidate >= engine->config.population_size)
            candidate = engine->config.population_size - 1;

        for (size_t i = 0; i < candidate_count; i++)
        {
            if (candidates[i] == candidate)
            {
                duplicate = 1;
                break;
            }
        }

        if (!duplicate)
            candidates[candidate_count++] = candidate;
    }

    winner = candidates[0];
    for (size_t i = 1; i < candidate_count; i++)
    {
        if (individual_is_better(
                &engine->population[candidates[i]],
                &engine->population[winner]))
        {
            winner = candidates[i];
        }
    }

    free(candidates);
    *out_population_index = winner;
    return 1;
}

static void mutate_individual(
    EvolutionEngine *engine,
    EvolutionIndividual *individual)
{
    for (size_t gene_index = 0; gene_index < engine->gene_count; gene_index++)
    {
        const EvolutionGeneMetadata *gene = &engine->metadata[gene_index];
        double scale = gene->mutation_scale > 0.0 ?
            gene->mutation_scale : engine->config.mutation_scale;

        if (evolution_prng_unit(&engine->prng) < engine->config.mutation_rate)
        {
            double before = individual->genes[gene_index];
            double unit = evolution_prng_unit(&engine->prng) * 2.0 - 1.0;
            double proposed = before + unit * scale *
                (gene->maximum - gene->minimum);
            double after = proposed;
            double absolute_change;

            if (after < gene->minimum)
            {
                after = gene->minimum;
                individual->mutation.clamp_min_count++;
            }
            else if (after > gene->maximum)
            {
                after = gene->maximum;
                individual->mutation.clamp_max_count++;
            }

            absolute_change = fabs(after - before);
            individual->genes[gene_index] = after;
            individual->mutation.mutation_count++;
            individual->mutation.mutation_absolute_sum += absolute_change;
            if (absolute_change > individual->mutation.mutation_max_absolute)
                individual->mutation.mutation_max_absolute = absolute_change;
        }
    }
}

static int breed_next_generation_internal(
    EvolutionEngine *engine,
    int apply_mutation)
{
    EvolutionIndividual *next_population;
    size_t *ranking;
    int next_generation;

    if (engine == NULL || engine->population == NULL)
        return 0;

    ranking = malloc(engine->config.population_size * sizeof(*ranking));
    next_population = calloc(
        engine->config.population_size,
        sizeof(*next_population));

    if (ranking == NULL || next_population == NULL ||
        !evolution_engine_rank_population(
            engine,
            ranking,
            engine->config.population_size))
    {
        free(ranking);
        free(next_population);
        return 0;
    }

    for (size_t i = 0; i < engine->config.population_size; i++)
    {
        if (!individual_allocate(&next_population[i], engine->gene_count))
        {
            population_destroy(next_population, engine->config.population_size);
            free(ranking);
            return 0;
        }
    }

    next_generation = engine->current_generation + 1;

    for (size_t i = 0; i < engine->config.elite_count; i++)
    {
        const EvolutionIndividual *parent = &engine->population[ranking[i]];
        EvolutionIndividual *child = &next_population[i];

        child->individual_id = engine->next_individual_id++;
        child->generation = next_generation;
        child->parent_a_id = parent->individual_id;
        child->has_parent_a = 1;
        snprintf(child->operation, sizeof(child->operation), "elite_copy");
        if (!individual_copy_genes(child, parent))
        {
            population_destroy(next_population, engine->config.population_size);
            free(ranking);
            return 0;
        }
    }

    for (size_t i = engine->config.elite_count;
         i < engine->config.population_size;
         i++)
    {
        EvolutionIndividual *child = &next_population[i];
        size_t parent_a_index;
        size_t parent_b_index;
        const EvolutionIndividual *parent_a;
        const EvolutionIndividual *parent_b;

        if (!evolution_engine_tournament_select(engine, &parent_a_index) ||
            !evolution_engine_tournament_select(engine, &parent_b_index))
        {
            population_destroy(next_population, engine->config.population_size);
            free(ranking);
            return 0;
        }

        parent_a = &engine->population[parent_a_index];
        parent_b = &engine->population[parent_b_index];
        child->individual_id = engine->next_individual_id++;
        child->generation = next_generation;
        child->parent_a_id = parent_a->individual_id;
        child->parent_b_id = parent_b->individual_id;
        child->has_parent_a = 1;
        child->has_parent_b = 1;

        if (evolution_prng_unit(&engine->prng) < engine->config.crossover_rate)
        {
            child->crossover_applied = 1;
            snprintf(
                child->operation,
                sizeof(child->operation),
                "crossover_mutation");

            for (size_t gene_index = 0;
                 gene_index < engine->gene_count;
                 gene_index++)
            {
                if (evolution_prng_unit(&engine->prng) < 0.5)
                {
                    child->genes[gene_index] = parent_a->genes[gene_index];
                    child->genes_from_parent_a++;
                }
                else
                {
                    child->genes[gene_index] = parent_b->genes[gene_index];
                    child->genes_from_parent_b++;
                }
            }
        }
        else
        {
            snprintf(child->operation, sizeof(child->operation), "clone_mutation");
            if (!individual_copy_genes(child, parent_a))
            {
                population_destroy(next_population, engine->config.population_size);
                free(ranking);
                return 0;
            }
            child->genes_from_parent_a = engine->gene_count;
        }

        if (apply_mutation)
            mutate_individual(engine, child);
    }

    free(ranking);
    population_destroy(engine->population, engine->config.population_size);
    engine->population = next_population;
    engine->current_generation = next_generation;
    return 1;
}

int evolution_engine_breed_next_generation(EvolutionEngine *engine)
{
    return breed_next_generation_internal(engine, 1);
}

int evolution_engine_breed_next_generation_deferred_mutation(
    EvolutionEngine *engine)
{
    return breed_next_generation_internal(engine, 0);
}

int evolution_engine_mutate_population_individual(
    EvolutionEngine *engine,
    size_t population_index)
{
    if (engine == NULL || engine->population == NULL ||
        population_index >= engine->config.population_size)
    {
        return 0;
    }
    mutate_individual(engine, &engine->population[population_index]);
    return 1;
}

int evolution_engine_write_checkpoint(
    const EvolutionEngine *engine,
    FILE *file,
    const char *signature,
    int next_generation,
    int completed)
{
    if (engine == NULL || file == NULL || signature == NULL ||
        engine->population == NULL || next_generation < 0)
    {
        return 0;
    }

    if (fprintf(
            file,
            "%s\nsignature=%s\nnext_generation=%d\ncompleted=%d\n"
            "population_size=%zu\ngene_count=%zu\ncurrent_generation=%d\n"
            "next_individual_id=%llu\nprng_state=%llu\nprng_increment=%llu\n"
            "has_global_best=%d\nglobal_best_individual_id=%llu\n"
            "global_best_generation=%d\nglobal_best_fitness_selection=%.17g\n"
            "global_best_fitness_mean=%.17g\nglobal_best_fitness_std=%.17g\n",
            EVOLUTION_CHECKPOINT_MAGIC,
            signature,
            next_generation,
            completed ? 1 : 0,
            engine->config.population_size,
            engine->gene_count,
            engine->current_generation,
            (unsigned long long)engine->next_individual_id,
            (unsigned long long)engine->prng.state,
            (unsigned long long)engine->prng.increment,
            engine->has_global_best,
            (unsigned long long)engine->global_best_individual_id,
            engine->global_best_generation,
            engine->global_best_fitness_selection,
            engine->global_best_fitness_mean,
            engine->global_best_fitness_std) < 0)
    {
        return 0;
    }

    for (size_t gene_index = 0; gene_index < engine->gene_count; gene_index++)
    {
        if (fprintf(
                file,
                "best_gene,%zu,%.17g\n",
                gene_index,
                engine->global_best_genes[gene_index]) < 0)
        {
            return 0;
        }
    }

    for (size_t i = 0; i < engine->config.population_size; i++)
    {
        const EvolutionIndividual *individual = &engine->population[i];

        if (fprintf(
                file,
                "individual,%zu,%llu,%d,%llu,%llu,%d,%d,%s,"
                "%.17g,%.17g,%.17g,%.17g,%.17g,%d,%d,%d,"
                "%zu,%.17g,%.17g,%zu,%zu,%d,%zu,%zu\n",
                i,
                (unsigned long long)individual->individual_id,
                individual->generation,
                (unsigned long long)individual->parent_a_id,
                (unsigned long long)individual->parent_b_id,
                individual->has_parent_a,
                individual->has_parent_b,
                individual->operation,
                individual->fitness_selection,
                individual->fitness_mean,
                individual->fitness_std,
                individual->fitness_min,
                individual->fitness_max,
                individual->valid_replicates,
                individual->invalid_replicates,
                individual->evaluated,
                individual->mutation.mutation_count,
                individual->mutation.mutation_absolute_sum,
                individual->mutation.mutation_max_absolute,
                individual->mutation.clamp_min_count,
                individual->mutation.clamp_max_count,
                individual->crossover_applied,
                individual->genes_from_parent_a,
                individual->genes_from_parent_b) < 0)
        {
            return 0;
        }

        for (size_t gene_index = 0;
             gene_index < engine->gene_count;
             gene_index++)
        {
            if (fprintf(
                    file,
                    "gene,%zu,%zu,%.17g\n",
                    i,
                    gene_index,
                    individual->genes[gene_index]) < 0)
            {
                return 0;
            }
        }
    }

    return fprintf(file, "END\n") >= 0;
}

static int read_key_string(
    FILE *file,
    const char *expected_key,
    char *out_value,
    size_t out_size)
{
    char line[512];
    char key[128];
    char value[384];

    if (fgets(line, sizeof(line), file) == NULL ||
        sscanf(line, "%127[^=]=%383[^\n]", key, value) != 2 ||
        strcmp(key, expected_key) != 0 ||
        snprintf(out_value, out_size, "%s", value) >= (int)out_size)
    {
        return 0;
    }
    return 1;
}

static int read_key_ull(
    FILE *file,
    const char *expected_key,
    unsigned long long *out_value)
{
    char value[128];
    char *end = NULL;

    if (!read_key_string(file, expected_key, value, sizeof(value)))
        return 0;

    *out_value = strtoull(value, &end, 10);
    return end != value && *end == '\0';
}

static int read_key_int(FILE *file, const char *key, int *out_value)
{
    unsigned long long value;

    if (!read_key_ull(file, key, &value) || value > 2147483647ULL)
        return 0;
    *out_value = (int)value;
    return 1;
}

static int read_key_size(FILE *file, const char *key, size_t *out_value)
{
    unsigned long long value;

    if (!read_key_ull(file, key, &value) || value > (unsigned long long)SIZE_MAX)
        return 0;
    *out_value = (size_t)value;
    return 1;
}

static int read_key_double(FILE *file, const char *key, double *out_value)
{
    char value[128];
    char *end = NULL;

    if (!read_key_string(file, key, value, sizeof(value)))
        return 0;
    *out_value = strtod(value, &end);
    return end != value && *end == '\0' && isfinite(*out_value);
}

int evolution_engine_read_checkpoint(
    EvolutionEngine *engine,
    FILE *file,
    const EvolutionEngineConfig *config,
    const EvolutionGeneMetadata *metadata,
    size_t gene_count,
    const char *expected_signature,
    int *out_next_generation,
    int *out_completed)
{
    char line[1024];
    char signature[384];
    size_t population_size;
    size_t stored_gene_count;
    unsigned long long next_id;
    unsigned long long prng_state;
    unsigned long long prng_increment;
    unsigned long long global_best_id;

    if (engine == NULL || file == NULL || config == NULL ||
        (gene_count > 0 && metadata == NULL) ||
        expected_signature == NULL || out_next_generation == NULL ||
        out_completed == NULL)
    {
        return 0;
    }

    if (fgets(line, sizeof(line), file) == NULL)
        return 0;
    line[strcspn(line, "\r\n")] = '\0';
    if (strcmp(line, EVOLUTION_CHECKPOINT_MAGIC) != 0 ||
        !read_key_string(file, "signature", signature, sizeof(signature)) ||
        strcmp(signature, expected_signature) != 0 ||
        !read_key_int(file, "next_generation", out_next_generation) ||
        !read_key_int(file, "completed", out_completed) ||
        !read_key_size(file, "population_size", &population_size) ||
        !read_key_size(file, "gene_count", &stored_gene_count) ||
        population_size != config->population_size || stored_gene_count != gene_count)
    {
        return 0;
    }

    if (!evolution_engine_init(engine, config, metadata, gene_count) ||
        !read_key_int(file, "current_generation", &engine->current_generation) ||
        !read_key_ull(file, "next_individual_id", &next_id) ||
        !read_key_ull(file, "prng_state", &prng_state) ||
        !read_key_ull(file, "prng_increment", &prng_increment) ||
        !read_key_int(file, "has_global_best", &engine->has_global_best) ||
        !read_key_ull(file, "global_best_individual_id", &global_best_id) ||
        !read_key_int(file, "global_best_generation", &engine->global_best_generation) ||
        !read_key_double(file, "global_best_fitness_selection", &engine->global_best_fitness_selection) ||
        !read_key_double(file, "global_best_fitness_mean", &engine->global_best_fitness_mean) ||
        !read_key_double(file, "global_best_fitness_std", &engine->global_best_fitness_std) ||
        !allocate_population(engine))
    {
        evolution_engine_destroy(engine);
        return 0;
    }

    engine->next_individual_id = (uint64_t)next_id;
    engine->prng.state = (uint64_t)prng_state;
    engine->prng.increment = (uint64_t)prng_increment;
    engine->global_best_individual_id = (uint64_t)global_best_id;

    for (size_t gene_index = 0; gene_index < gene_count; gene_index++)
    {
        size_t stored_index;
        double value;
        if (fgets(line, sizeof(line), file) == NULL ||
            sscanf(line, "best_gene,%zu,%lf", &stored_index, &value) != 2 ||
            stored_index != gene_index || !isfinite(value))
        {
            evolution_engine_destroy(engine);
            return 0;
        }
        engine->global_best_genes[gene_index] = value;
    }

    for (size_t i = 0; i < population_size; i++)
    {
        EvolutionIndividual *individual = &engine->population[i];
        size_t stored_index;
        unsigned long long id;
        unsigned long long parent_a;
        unsigned long long parent_b;
        char operation[EVOLUTION_OPERATION_MAX + 1];

        if (fgets(line, sizeof(line), file) == NULL ||
            sscanf(
                line,
                "individual,%zu,%llu,%d,%llu,%llu,%d,%d,%31[^,],"
                "%lf,%lf,%lf,%lf,%lf,%d,%d,%d,%zu,%lf,%lf,%zu,%zu,%d,%zu,%zu",
                &stored_index,
                &id,
                &individual->generation,
                &parent_a,
                &parent_b,
                &individual->has_parent_a,
                &individual->has_parent_b,
                operation,
                &individual->fitness_selection,
                &individual->fitness_mean,
                &individual->fitness_std,
                &individual->fitness_min,
                &individual->fitness_max,
                &individual->valid_replicates,
                &individual->invalid_replicates,
                &individual->evaluated,
                &individual->mutation.mutation_count,
                &individual->mutation.mutation_absolute_sum,
                &individual->mutation.mutation_max_absolute,
                &individual->mutation.clamp_min_count,
                &individual->mutation.clamp_max_count,
                &individual->crossover_applied,
                &individual->genes_from_parent_a,
                &individual->genes_from_parent_b) != 24 ||
            stored_index != i)
        {
            evolution_engine_destroy(engine);
            return 0;
        }

        individual->individual_id = (uint64_t)id;
        individual->parent_a_id = (uint64_t)parent_a;
        individual->parent_b_id = (uint64_t)parent_b;
        snprintf(individual->operation, sizeof(individual->operation), "%s", operation);

        for (size_t gene_index = 0; gene_index < gene_count; gene_index++)
        {
            size_t stored_population_index;
            size_t stored_gene_index;
            double value;

            if (fgets(line, sizeof(line), file) == NULL ||
                sscanf(
                    line,
                    "gene,%zu,%zu,%lf",
                    &stored_population_index,
                    &stored_gene_index,
                    &value) != 3 ||
                stored_population_index != i || stored_gene_index != gene_index ||
                !isfinite(value) ||
                value < metadata[gene_index].minimum ||
                value > metadata[gene_index].maximum)
            {
                evolution_engine_destroy(engine);
                return 0;
            }
            individual->genes[gene_index] = value;
        }
    }

    if (fgets(line, sizeof(line), file) == NULL)
    {
        evolution_engine_destroy(engine);
        return 0;
    }
    line[strcspn(line, "\r\n")] = '\0';
    if (strcmp(line, "END") != 0)
    {
        evolution_engine_destroy(engine);
        return 0;
    }

    return 1;
}
