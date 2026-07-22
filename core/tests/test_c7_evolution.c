#include <math.h>
#include <stdio.h>
#include <string.h>

#include "c7_audit_common.h"
#include "evolution.h"

static int fail(const char *message)
{
    fprintf(stderr, "C7 evolution test failed: %s\n", message);
    return 0;
}

static void fill_metadata(EvolutionGeneMetadata *metadata)
{
    memset(metadata, 0, sizeof(*metadata));
    metadata->gene_index = 0U;
    snprintf(metadata->gene_name, sizeof(metadata->gene_name), "audit.connection_0");
    metadata->gene_kind = EVOLUTION_GENE_EXC_CONNECTION_WEIGHT;
    metadata->minimum = 0.05;
    metadata->maximum = 2.0;
    metadata->baseline_value = 0.5;
    metadata->mutation_scale = 0.15;
    metadata->connection_id = 0U;
    metadata->has_connection_id = 1;
}

static int evaluate_individual(
    const EvolutionIndividual *individual,
    MiniSNNNeuronModel model,
    double *out_fitness)
{
    C7AuditFixture fixture;
    const double inputs[3][C7_AUDIT_SENSOR_COUNT] =
    {
        {1.0, 0.0, 1.0}, {0.5, -0.5, 0.0}, {0.75, 0.25, 1.0}
    };
    const double target[C7_AUDIT_ACTION_COUNT] = {0.25, 0.0, 1.0, 0.0};
    double actions[C7_AUDIT_ACTION_COUNT] = {0};
    double error = 0.0;

    if (individual == NULL || out_fitness == NULL || individual->gene_count != 1U)
        return 0;
    if (!c7_audit_fixture_create(&fixture, model, C7_AUDIT_MIN_NEURONS, 4U,
                                 1,
                                 model == MINISNN_NEURON_MODEL_LIF,
                                 model == MINISNN_NEURON_MODEL_LIF,
                                 model == MINISNN_NEURON_MODEL_LIF))
    {
        c7_audit_fixture_destroy(&fixture);
        return 0;
    }
    if (!minisnn_set_connection_weight(fixture.network, 0U, individual->genes[0]))
    {
        fprintf(stderr, "C7 evolution connection mutation rejected %.17g\n", individual->genes[0]);
        c7_audit_fixture_destroy(&fixture);
        return 0;
    }
    for (uint64_t tick = 0U; tick < 3U; tick++)
    {
        if (!c7_audit_run_tick(&fixture, tick, inputs[tick], actions, NULL) ||
            !c7_audit_fixture_all_finite(&fixture))
        {
            fprintf(stderr, "C7 evolution cycle failed tick=%llu model=%d\n",
                    (unsigned long long)tick, (int)model);
            c7_audit_fixture_destroy(&fixture);
            return 0;
        }
        for (uint32_t index = 0U; index < C7_AUDIT_ACTION_COUNT; index++)
            error += fabs(actions[index] - target[index]);
    }
    *out_fitness = 1.0 / (1.0 + error);
    c7_audit_fixture_destroy(&fixture);
    return isfinite(*out_fitness);
}

static int run_engine(EvolutionEngine *engine, MiniSNNNeuronModel model)
{
    for (size_t index = 0U; index < engine->config.population_size; index++)
    {
        double fitness;
        if (!evaluate_individual(&engine->population[index], model, &fitness) ||
            !evolution_engine_set_evaluation(engine, index, fitness, 0.0,
                                             fitness, fitness, 1, 0))
        {
            fprintf(stderr, "C7 evolution evaluation failed index=%zu model=%d\n",
                    index, (int)model);
            return 0;
        }
    }
    if (!evolution_engine_breed_next_generation_deferred_mutation(engine))
    {
        fprintf(stderr, "C7 evolution breeding failed model=%d\n", (int)model);
        return 0;
    }
    return 1;
}

static int test_lif_evolution_determinism(void)
{
    EvolutionEngineConfig config;
    EvolutionGeneMetadata metadata;
    EvolutionEngine first = {0};
    EvolutionEngine second = {0};
    int ok;

    memset(&config, 0, sizeof(config));
    config.population_size = 4U;
    config.elite_count = 1U;
    config.tournament_size = 2U;
    config.crossover_rate = 0.75;
    config.mutation_rate = 1.0;
    config.mutation_scale = 1.0;
    config.initialization_scale = 1.0;
    config.replicate_std_penalty = 0.0;
    config.initialization = EVOLUTION_INITIALIZATION_UNIFORM;
    config.evolution_seed = UINT64_C(701);
    fill_metadata(&metadata);
    ok = evolution_engine_init(&first, &config, &metadata, 1U);
    ok = ok && evolution_engine_init(&second, &config, &metadata, 1U);
    ok = ok && evolution_engine_initialize_population(&first);
    ok = ok && evolution_engine_initialize_population(&second);
    ok = ok && run_engine(&first, MINISNN_NEURON_MODEL_LIF);
    ok = ok && run_engine(&second, MINISNN_NEURON_MODEL_LIF);
    if (!ok)
        fprintf(stderr, "C7 evolution setup/evaluation failed\n");
    ok = ok && first.current_generation == second.current_generation &&
        first.next_individual_id == second.next_individual_id;
    for (size_t index = 0U; ok && index < first.config.population_size; index++)
    {
        if (first.population[index].individual_id != second.population[index].individual_id ||
            first.population[index].genes[0] != second.population[index].genes[0])
        {
            fprintf(stderr, "C7 evolution mismatch index=%zu ids=%llu/%llu genes=%.17g/%.17g\n",
                    index, (unsigned long long)first.population[index].individual_id,
                    (unsigned long long)second.population[index].individual_id,
                    first.population[index].genes[0], second.population[index].genes[0]);
            ok = 0;
        }
    }
    if (!ok)
        fail("evolucao LIF externa nao foi deterministica");
    evolution_engine_destroy(&first);
    evolution_engine_destroy(&second);
    return ok;
}

static int test_model_smokes_and_isolation(void)
{
    EvolutionEngineConfig config;
    EvolutionGeneMetadata metadata;
    EvolutionEngine engine = {0};
    int ok;

    memset(&config, 0, sizeof(config));
    config.population_size = 2U;
    config.elite_count = 1U;
    config.tournament_size = 2U;
    config.crossover_rate = 0.0;
    config.mutation_rate = 0.0;
    config.mutation_scale = 1.0;
    config.initialization_scale = 1.0;
    config.initialization = EVOLUTION_INITIALIZATION_BASELINE_PLUS_MUTATION;
    config.evolution_seed = UINT64_C(702);
    fill_metadata(&metadata);
    ok = evolution_engine_init(&engine, &config, &metadata, 1U) &&
        evolution_engine_initialize_population(&engine) &&
        run_engine(&engine, MINISNN_NEURON_MODEL_ADEX) &&
        run_engine(&engine, MINISNN_NEURON_MODEL_HODGKIN_HUXLEY);
    if (!ok)
        fail("smoke evolutivo AdEx/HH ou isolamento de ciclos");
    evolution_engine_destroy(&engine);
    return ok;
}

static int test_lif_evolution_checkpoint_resume(void)
{
    EvolutionEngineConfig config;
    EvolutionGeneMetadata metadata;
    EvolutionEngine source = {0};
    EvolutionEngine restored = {0};
    FILE *file = NULL;
    int next_generation = -1;
    int completed = -1;
    int ok;

    memset(&config, 0, sizeof(config));
    config.population_size = 4U;
    config.elite_count = 1U;
    config.tournament_size = 2U;
    config.crossover_rate = 0.75;
    config.mutation_rate = 1.0;
    config.mutation_scale = 1.0;
    config.initialization_scale = 1.0;
    config.initialization = EVOLUTION_INITIALIZATION_UNIFORM;
    config.evolution_seed = UINT64_C(703);
    fill_metadata(&metadata);
    ok = evolution_engine_init(&source, &config, &metadata, 1U) &&
        evolution_engine_initialize_population(&source) &&
        run_engine(&source, MINISNN_NEURON_MODEL_LIF) && (file = tmpfile()) != NULL &&
        evolution_engine_write_checkpoint(&source, file, "c7-agentio-lif", 1, 0) &&
        fflush(file) == 0 && fseek(file, 0L, SEEK_SET) == 0 &&
        evolution_engine_read_checkpoint(&restored, file, &config, &metadata, 1U,
                                         "c7-agentio-lif", &next_generation, &completed) &&
        next_generation == 1 && !completed &&
        run_engine(&source, MINISNN_NEURON_MODEL_LIF) &&
        run_engine(&restored, MINISNN_NEURON_MODEL_LIF) &&
        source.current_generation == restored.current_generation &&
        source.next_individual_id == restored.next_individual_id;
    for (size_t index = 0U; ok && index < config.population_size; index++)
        ok = source.population[index].individual_id == restored.population[index].individual_id &&
            source.population[index].genes[0] == restored.population[index].genes[0];
    if (!ok)
        fail("checkpoint/resume evolutivo com avaliacao C7");
    if (file != NULL)
        fclose(file);
    evolution_engine_destroy(&source);
    evolution_engine_destroy(&restored);
    return ok;
}

int main(void)
{
    if (!test_lif_evolution_determinism() || !test_model_smokes_and_isolation() ||
        !test_lif_evolution_checkpoint_resume())
        return 1;
    printf("C7 evolution integration validation OK\n");
    return 0;
}
