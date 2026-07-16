#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "evolution.h"
#include "minisnn.h"
#include "neuron.h"
#include "structure.h"

#define TEST_NEURON_COUNT 4U

static int fail(const char *message)
{
    fprintf(stderr, "Structure test failed: %s\n", message);
    return 0;
}

static int same_double(double left, double right)
{
    return fabs(left - right) < 1e-12;
}

static MiniSNNConnectionGene make_gene(
    size_t source,
    size_t target,
    double magnitude,
    unsigned int delay)
{
    MiniSNNConnectionGene gene;
    memset(&gene, 0, sizeof(gene));
    gene.source = source;
    gene.target = target;
    gene.magnitude = magnitude;
    gene.delay = delay;
    (void)structure_connection_key(
        TEST_NEURON_COUNT, source, target, &gene.connection_key);
    return gene;
}

static void initialize_neurons(LIFNeuron *neurons)
{
    for (size_t i = 0; i < TEST_NEURON_COUNT; i++)
        lif_init(&neurons[i]);
    neurons[2].type = NEURON_INHIBITORY;
    neurons[3].type = NEURON_INHIBITORY;
}

static StructureConstraints make_constraints(LIFNeuron *neurons)
{
    StructureConstraints constraints;
    memset(&constraints, 0, sizeof(constraints));
    constraints.neuron_count = TEST_NEURON_COUNT;
    constraints.neurons = neurons;
    constraints.max_connections = 12U;
    constraints.delay_min = 1U;
    constraints.delay_max = 4U;
    constraints.new_exc_weight_min = 10.0;
    constraints.new_exc_weight_max = 20.0;
    constraints.new_inh_magnitude_min = 12.0;
    constraints.new_inh_magnitude_max = 24.0;
    constraints.delay_mutation_max_delta = 2U;
    return constraints;
}

static int check_keys_and_canonicalization(void)
{
    uint64_t key = 0U;
    MiniSNNConnectionGene unordered[] = {
        make_gene(2U, 0U, 12.0, 1U),
        make_gene(0U, 3U, 10.0, 2U),
        make_gene(0U, 1U, 11.0, 1U)};
    MiniSNNConnectionGene duplicate[] = {
        make_gene(0U, 1U, 10.0, 1U),
        make_gene(0U, 1U, 20.0, 2U)};
    StructureGenome genome = {0};

    if (!structure_connection_key(4U, 2U, 3U, &key) || key != 11U ||
        structure_connection_key(4U, 4U, 0U, &key) ||
        structure_connection_key(0U, 0U, 0U, &key))
        return fail("connection key calculation or bounds");
    if (SIZE_MAX == UINT64_MAX &&
        structure_connection_key(SIZE_MAX, SIZE_MAX - 1U,
                                 SIZE_MAX - 1U, &key))
        return fail("connection key overflow accepted");

    if (!structure_genome_set(&genome, unordered, 3U) ||
        !structure_genome_canonicalize(&genome) ||
        genome.connections[0].connection_key >=
            genome.connections[1].connection_key ||
        genome.connections[1].connection_key >=
            genome.connections[2].connection_key)
    {
        structure_genome_destroy(&genome);
        return fail("canonical ordering");
    }
    structure_genome_destroy(&genome);
    if (!structure_genome_set(&genome, duplicate, 2U) ||
        structure_genome_canonicalize(&genome))
    {
        structure_genome_destroy(&genome);
        return fail("duplicate key accepted");
    }
    structure_genome_destroy(&genome);
    return 1;
}

static int check_constraints_and_reachability(void)
{
    LIFNeuron neurons[TEST_NEURON_COUNT];
    StructureConstraints constraints;
    StructureGenome genome = {0};
    MiniSNNConnectionGene valid[] = {
        make_gene(0U, 1U, 10.0, 1U),
        make_gene(1U, 3U, 11.0, 2U)};
    MiniSNNConnectionGene self = make_gene(0U, 0U, 10.0, 1U);
    MiniSNNConnectionGene inh_inh = make_gene(2U, 3U, 12.0, 1U);
    size_t inputs[] = {0U};
    size_t outputs[] = {3U};

    initialize_neurons(neurons);
    constraints = make_constraints(neurons);
    constraints.preserve_required_reachability = 1;
    constraints.required_inputs = inputs;
    constraints.required_input_count = 1U;
    constraints.required_outputs = outputs;
    constraints.required_output_count = 1U;
    if (!structure_genome_set(&genome, valid, 2U) ||
        !structure_genome_canonicalize(&genome) ||
        !structure_genome_validate(&genome, &constraints))
        goto fail;
    genome.connection_count = 1U;
    if (structure_genome_has_required_reachability(&genome, &constraints))
        goto fail;
    structure_genome_destroy(&genome);

    constraints.preserve_required_reachability = 0;
    if (!structure_genome_set(&genome, &self, 1U) ||
        !structure_genome_canonicalize(&genome) ||
        structure_genome_validate(&genome, &constraints))
        goto fail;
    constraints.allow_self_connections = 1;
    if (!structure_genome_validate(&genome, &constraints))
        goto fail;
    structure_genome_destroy(&genome);

    constraints.allow_self_connections = 0;
    if (!structure_genome_set(&genome, &inh_inh, 1U) ||
        !structure_genome_canonicalize(&genome) ||
        structure_genome_validate(&genome, &constraints))
        goto fail;
    constraints.allow_inh_to_inh = 1;
    if (!structure_genome_validate(&genome, &constraints))
        goto fail;
    structure_genome_destroy(&genome);
    return 1;
fail:
    structure_genome_destroy(&genome);
    return fail("constraints or directed reachability");
}

static int check_distance_and_complexity(void)
{
    MiniSNNConnectionGene left_genes[] = {
        make_gene(0U, 1U, 10.0, 1U),
        make_gene(0U, 2U, 10.0, 1U)};
    MiniSNNConnectionGene right_genes[] = {
        make_gene(0U, 2U, 99.0, 4U),
        make_gene(1U, 2U, 10.0, 1U)};
    StructureGenome left = {0};
    StructureGenome right = {0};
    int ok = structure_genome_set(&left, left_genes, 2U) &&
             structure_genome_set(&right, right_genes, 2U) &&
             structure_genome_canonicalize(&left) &&
             structure_genome_canonicalize(&right) &&
             same_double(structure_jaccard_distance(&left, &right), 2.0 / 3.0) &&
             same_double(structure_complexity_normalized(5U, 2U, 8U), 0.5) &&
             same_double(structure_complexity_normalized(1U, 2U, 8U), 0.0) &&
             same_double(structure_complexity_normalized(9U, 2U, 8U), 1.0) &&
             same_double(structure_apply_complexity_penalty(0.8, 0.2, 0.5),
                         0.7) &&
             same_double(structure_apply_complexity_penalty(0.1, 1.0, 1.0),
                         0.0) &&
             structure_apply_complexity_penalty(1.1, 0.2, 0.5) < 0.0;
    structure_genome_destroy(&left);
    structure_genome_destroy(&right);
    return ok ? 1 : fail("Jaccard distance or complexity penalty");
}

static int genomes_equal(
    const StructureGenome *left,
    const StructureGenome *right)
{
    if (left->connection_count != right->connection_count)
        return 0;
    for (size_t i = 0; i < left->connection_count; i++)
    {
        if (left->connections[i].connection_key !=
                right->connections[i].connection_key ||
            left->connections[i].delay != right->connections[i].delay ||
            !same_double(left->connections[i].magnitude,
                         right->connections[i].magnitude))
            return 0;
    }
    return 1;
}

static int check_crossover_and_mutation(void)
{
    LIFNeuron neurons[TEST_NEURON_COUNT];
    StructureConstraints constraints;
    MiniSNNConnectionGene a_genes[] = {
        make_gene(0U, 1U, 10.0, 1U),
        make_gene(1U, 3U, 11.0, 2U),
        make_gene(2U, 0U, 12.0, 3U)};
    MiniSNNConnectionGene b_genes[] = {
        make_gene(0U, 1U, 19.0, 4U),
        make_gene(0U, 2U, 15.0, 2U)};
    StructureGenome parent_a = {0};
    StructureGenome parent_b = {0};
    StructureGenome child_a = {0};
    StructureGenome child_b = {0};
    StructureMutationStats stats;
    EvolutionPrng first;
    EvolutionPrng second;
    size_t common_index;
    size_t required_input = 0U;
    size_t required_output = 3U;
    int fallback_found = 0;
    int ok = 0;

    initialize_neurons(neurons);
    constraints = make_constraints(neurons);
    constraints.allow_inh_to_inh = 1;
    if (!structure_genome_set(&parent_a, a_genes, 3U) ||
        !structure_genome_set(&parent_b, b_genes, 2U) ||
        !structure_genome_canonicalize(&parent_a) ||
        !structure_genome_canonicalize(&parent_b))
        goto done;

    evolution_prng_seed(&first, 123U, 77U);
    evolution_prng_seed(&second, 123U, 77U);
    if (!structure_genome_crossover(&parent_a, 0.8, 1U,
                                    &parent_b, 0.4, 2U,
                                    &constraints, &first, &child_a, &stats) ||
        !structure_genome_crossover(&parent_a, 0.8, 1U,
                                    &parent_b, 0.4, 2U,
                                    &constraints, &second, &child_b, NULL) ||
        !genomes_equal(&child_a, &child_b) ||
        !structure_genome_find(&child_a, a_genes[0].connection_key,
                               &common_index) ||
        !((same_double(child_a.connections[common_index].magnitude, 10.0) &&
           child_a.connections[common_index].delay == 1U) ||
          (same_double(child_a.connections[common_index].magnitude, 19.0) &&
           child_a.connections[common_index].delay == 4U)) ||
        !structure_genome_find(&child_a, a_genes[2].connection_key,
                               &common_index) ||
        structure_genome_find(&child_a, b_genes[1].connection_key,
                              &common_index) ||
        stats.common_inherited != 1U || stats.disjoint_inherited != 2U ||
        stats.crossover_fallback != 0U)
        goto done;

    constraints.preserve_required_reachability = 1;
    constraints.required_inputs = &required_input;
    constraints.required_input_count = 1U;
    constraints.required_outputs = &required_output;
    constraints.required_output_count = 1U;
    for (uint64_t seed = 1U; seed <= 10000U && !fallback_found; seed++)
    {
        structure_genome_destroy(&child_b);
        memset(&stats, 0, sizeof(stats));
        evolution_prng_seed(&second, seed, 91U);
        if (!structure_genome_crossover(&parent_a, 0.8, 1U,
                                        &parent_b, 0.8, 2U,
                                        &constraints, &second, &child_b,
                                        &stats))
            goto done;
        if (stats.crossover_fallback)
        {
            if (!genomes_equal(&child_b, &parent_a))
                goto done;
            fallback_found = 1;
        }
    }
    if (!fallback_found)
        goto done;
    constraints.preserve_required_reachability = 0;
    constraints.required_inputs = NULL;
    constraints.required_input_count = 0U;
    constraints.required_outputs = NULL;
    constraints.required_output_count = 0U;

    evolution_prng_seed(&first, 444U, 9U);
    memset(&stats, 0, sizeof(stats));
    if (!structure_genome_mutate(&child_a, &constraints,
                                 1, 1, 1, 1,
                                 1.0, 1.0, 1.0, 1.0, 4U,
                                 &first, &stats) ||
        !structure_genome_validate(&child_a, &constraints) ||
        child_a.connection_count < constraints.min_connections ||
        child_a.connection_count > constraints.max_connections ||
        stats.add_count + stats.remove_count + stats.rewire_count +
                stats.delay_mutation_count > 4U)
        goto done;
    ok = 1;
done:
    structure_genome_destroy(&parent_a);
    structure_genome_destroy(&parent_b);
    structure_genome_destroy(&child_a);
    structure_genome_destroy(&child_b);
    return ok ? 1 : fail("crossover, determinism, or bounded mutation");
}

static int check_crossover_retry_stats_and_tied_subset(void)
{
    LIFNeuron neurons[TEST_NEURON_COUNT];
    StructureConstraints constraints;
    MiniSNNConnectionGene retry_a_genes[] = {
        make_gene(0U, 1U, 10.0, 1U),
        make_gene(1U, 3U, 10.0, 1U)};
    MiniSNNConnectionGene retry_b_genes[] = {
        make_gene(0U, 1U, 12.0, 2U),
        make_gene(0U, 2U, 11.0, 1U)};
    MiniSNNConnectionGene subset_a_genes[] = {
        make_gene(0U, 1U, 10.0, 1U),
        make_gene(0U, 2U, 10.0, 1U),
        make_gene(0U, 3U, 10.0, 1U),
        make_gene(1U, 0U, 10.0, 1U),
        make_gene(1U, 2U, 10.0, 1U)};
    MiniSNNConnectionGene subset_b_genes[] = {
        make_gene(0U, 1U, 11.0, 2U),
        make_gene(0U, 2U, 11.0, 2U),
        make_gene(0U, 3U, 11.0, 2U),
        make_gene(1U, 0U, 11.0, 2U),
        make_gene(2U, 0U, 12.0, 2U)};
    StructureGenome retry_a = {0};
    StructureGenome retry_b = {0};
    StructureGenome subset_a = {0};
    StructureGenome subset_b = {0};
    StructureGenome child = {0};
    StructureGenome same_seed = {0};
    StructureGenome other_seed = {0};
    StructureMutationStats stats;
    EvolutionPrng prng;
    EvolutionPrng preview;
    size_t required_input = 0U;
    size_t required_output = 3U;
    uint64_t selected_seed = 0U;
    int retried_success = 0;
    int different_seed = 0;
    int ok = 0;

    initialize_neurons(neurons);
    constraints = make_constraints(neurons);
    constraints.preserve_required_reachability = 1;
    constraints.required_inputs = &required_input;
    constraints.required_input_count = 1U;
    constraints.required_outputs = &required_output;
    constraints.required_output_count = 1U;
    if (!structure_genome_set(&retry_a, retry_a_genes, 2U) ||
        !structure_genome_set(&retry_b, retry_b_genes, 2U) ||
        !structure_genome_canonicalize(&retry_a) ||
        !structure_genome_canonicalize(&retry_b))
        goto done;

    for (uint64_t seed = 1U; seed <= 10000U && !retried_success; seed++)
    {
        (void)evolution_prng_seed(&preview, seed, 91U);
        (void)evolution_prng_unit(&preview);
        (void)evolution_prng_unit(&preview);
        if (evolution_prng_unit(&preview) < 0.5)
            continue;
        structure_genome_destroy(&child);
        memset(&stats, 0, sizeof(stats));
        evolution_prng_seed(&prng, seed, 91U);
        if (!structure_genome_crossover(
                &retry_a, 0.8, 1U, &retry_b, 0.8, 2U,
                &constraints, &prng, &child, &stats))
            goto done;
        if (!stats.crossover_fallback &&
            stats.common_inherited + stats.disjoint_inherited ==
                child.connection_count)
            retried_success = 1;
    }
    if (!retried_success)
        goto done;

    constraints.preserve_required_reachability = 0;
    constraints.required_inputs = NULL;
    constraints.required_input_count = 0U;
    constraints.required_outputs = NULL;
    constraints.required_output_count = 0U;
    constraints.min_connections = 2U;
    constraints.max_connections = 2U;
    if (!structure_genome_set(&subset_a, subset_a_genes, 5U) ||
        !structure_genome_set(&subset_b, subset_b_genes, 5U) ||
        !structure_genome_canonicalize(&subset_a) ||
        !structure_genome_canonicalize(&subset_b))
        goto done;

    for (uint64_t seed = 1U; seed <= 10000U && selected_seed == 0U; seed++)
    {
        structure_genome_destroy(&child);
        evolution_prng_seed(&prng, seed, 17U);
        if (!structure_genome_crossover(
                &subset_a, 0.5, 1U, &subset_b, 0.5, 2U,
                &constraints, &prng, &child, &stats) ||
            child.connection_count != 2U ||
            child.connections[0].connection_key >= child.connections[1].connection_key ||
            stats.common_inherited + stats.disjoint_inherited !=
                child.connection_count)
            goto done;
        if (child.connections[0].connection_key != subset_a_genes[0].connection_key ||
            child.connections[1].connection_key != subset_a_genes[1].connection_key)
        {
            if (!structure_genome_copy(&same_seed, &child))
                goto done;
            selected_seed = seed;
        }
    }
    if (selected_seed == 0U)
        goto done;
    evolution_prng_seed(&prng, selected_seed, 17U);
    if (!structure_genome_crossover(
            &subset_a, 0.5, 1U, &subset_b, 0.5, 2U,
            &constraints, &prng, &child, &stats) ||
        !genomes_equal(&same_seed, &child))
        goto done;
    for (uint64_t seed = 1U; seed <= 10000U && !different_seed; seed++)
    {
        if (seed == selected_seed)
            continue;
        structure_genome_destroy(&other_seed);
        evolution_prng_seed(&prng, seed, 17U);
        if (!structure_genome_crossover(
                &subset_a, 0.5, 1U, &subset_b, 0.5, 2U,
                &constraints, &prng, &other_seed, &stats))
            goto done;
        if (!genomes_equal(&same_seed, &other_seed))
            different_seed = 1;
    }
    if (!different_seed)
        goto done;
    ok = 1;
done:
    structure_genome_destroy(&retry_a);
    structure_genome_destroy(&retry_b);
    structure_genome_destroy(&subset_a);
    structure_genome_destroy(&subset_b);
    structure_genome_destroy(&child);
    structure_genome_destroy(&same_seed);
    structure_genome_destroy(&other_seed);
    return ok ? 1 : fail("crossover retry statistics or tied subset sampling");
}

static int check_public_atomic_patch(void)
{
    MiniSNN *snn = minisnn_create(4);
    MiniSNNTopologyPatchResult result;
    MiniSNNTopologyOperation operation;
    MiniSNNConnectionInfo connection;
    uint64_t signature_before;
    uint64_t signature_after;
    size_t count_before;
    int found_positive = 0;
    int found_negative = 0;

    if (snn == NULL ||
        !minisnn_set_neuron_type(snn, 2, MINISNN_NEURON_INHIBITORY) ||
        !minisnn_connect_delayed(snn, 0, 1, 10.0, 1) ||
        !minisnn_connect_delayed(snn, 2, 0, -12.0, 2) ||
        !minisnn_get_topology_signature(snn, &signature_before))
        goto fail;

    memset(&operation, 0, sizeof(operation));
    operation.type = MINISNN_TOPOLOGY_ADD;
    operation.source = 2U;
    operation.target = 1U;
    operation.magnitude = 15.0;
    operation.delay = 3U;
    if (!minisnn_validate_topology_patch(snn, &operation, 1U, &result) ||
        minisnn_connection_count(snn) != 2U ||
        !minisnn_apply_topology_patch(snn, &operation, 1U, &result) ||
        !result.success || result.connections_added != 1U ||
        minisnn_connection_count(snn) != 3U)
        goto fail;

    for (size_t i = 0; i < minisnn_connection_count(snn); i++)
    {
        if (!minisnn_get_connection(snn, i, &connection))
            goto fail;
        if (connection.source == 0U && connection.weight > 0.0)
            found_positive = 1;
        if (connection.source == 2U && connection.weight < 0.0)
            found_negative = 1;
    }
    if (!found_positive || !found_negative)
        goto fail;

    count_before = minisnn_connection_count(snn);
    if (!minisnn_get_topology_signature(snn, &signature_before))
        goto fail;
    {
        MiniSNNTopologyOperation invalid_patch[2];
        memset(invalid_patch, 0, sizeof(invalid_patch));
        invalid_patch[0].type = MINISNN_TOPOLOGY_ADD;
        invalid_patch[0].source = 1U;
        invalid_patch[0].target = 3U;
        invalid_patch[0].magnitude = 13.0;
        invalid_patch[0].delay = 1U;
        invalid_patch[1].type = MINISNN_TOPOLOGY_REMOVE;
        invalid_patch[1].source = 3U;
        invalid_patch[1].target = 0U;
        if (minisnn_apply_topology_patch(snn, invalid_patch, 2U, &result) ||
            minisnn_connection_count(snn) != count_before ||
            !minisnn_get_topology_signature(snn, &signature_after) ||
            signature_after != signature_before)
            goto fail;
    }

    memset(&operation, 0, sizeof(operation));
    operation.type = MINISNN_TOPOLOGY_REWIRE;
    operation.source = 0U;
    operation.target = 1U;
    operation.new_source = 0U;
    operation.new_target = 2U;
    operation.magnitude = 18.0;
    operation.delay = 2U;
    if (!minisnn_apply_topology_patch(snn, &operation, 1U, &result) ||
        result.connections_rewired != 1U)
        goto fail;

    memset(&operation, 0, sizeof(operation));
    operation.type = MINISNN_TOPOLOGY_SET_DELAY;
    operation.source = 0U;
    operation.target = 2U;
    operation.delay = 4U;
    if (!minisnn_apply_topology_patch(snn, &operation, 1U, &result) ||
        result.delays_changed != 1U)
        goto fail;

    memset(&operation, 0, sizeof(operation));
    operation.type = MINISNN_TOPOLOGY_REMOVE;
    operation.source = 2U;
    operation.target = 1U;
    if (!minisnn_apply_topology_patch(snn, &operation, 1U, NULL) ||
        minisnn_connection_count(snn) != 2U)
        goto fail;
    minisnn_destroy(&snn);
    return 1;
fail:
    minisnn_destroy(&snn);
    return fail("public topology patch, signs, or atomicity");
}

static int find_connection(
    const MiniSNN *snn,
    size_t source,
    size_t target,
    size_t *out_id)
{
    for (size_t i = 0; i < minisnn_connection_count(snn); i++)
    {
        MiniSNNConnectionInfo connection;
        if (!minisnn_get_connection(snn, i, &connection))
            return 0;
        if (connection.source == source && connection.target == target)
        {
            *out_id = i;
            return 1;
        }
    }
    return 0;
}

static int check_rewire_identity_and_atomicity(void)
{
    LIFNeuron neurons[TEST_NEURON_COUNT];
    StructureConstraints constraints;
    MiniSNNConnectionGene genes[] = {
        make_gene(0U, 1U, 10.0, 1U),
        make_gene(0U, 2U, 11.0, 1U),
        make_gene(1U, 0U, 12.0, 1U)};
    StructureGenome original = {0};
    StructureGenome mutated = {0};
    EvolutionPrng prng;
    MiniSNN *snn = NULL;
    MiniSNNTopologyOperation operation;
    MiniSNNTopologyPatchResult result;
    MiniSNNConnectionInfo before_connection;
    MiniSNNConnectionInfo after_connection;
    uint64_t signature_before;
    uint64_t signature_after;
    size_t old_only;
    size_t new_only;
    int ok = 0;

    initialize_neurons(neurons);
    constraints = make_constraints(neurons);
    constraints.min_connections = 3U;
    if (!structure_genome_set(&original, genes, 3U) ||
        !structure_genome_canonicalize(&original))
        goto done;
    for (uint64_t seed = 1U; seed <= 64U; seed++)
    {
        StructureMutationStats stats;
        size_t removed_count = 0U;
        size_t added_count = 0U;
        structure_genome_destroy(&mutated);
        if (!structure_genome_copy(&mutated, &original))
            goto done;
        evolution_prng_seed(&prng, seed, 33U);
        memset(&stats, 0, sizeof(stats));
        if (!structure_genome_mutate(
                &mutated, &constraints, 0, 0, 1, 0,
                0.0, 0.0, 1.0, 0.0, 1U, &prng, &stats) ||
            stats.rewire_count != 1U ||
            mutated.connection_count != original.connection_count)
            goto done;
        for (size_t i = 0; i < original.connection_count; i++)
        {
            size_t ignored;
            if (!structure_genome_find(
                    &mutated, original.connections[i].connection_key, &ignored))
            {
                old_only = original.connections[i].connection_key;
                removed_count++;
            }
        }
        for (size_t i = 0; i < mutated.connection_count; i++)
        {
            size_t ignored;
            if (!structure_genome_find(
                    &original, mutated.connections[i].connection_key, &ignored))
            {
                new_only = mutated.connections[i].connection_key;
                added_count++;
            }
        }
        if (removed_count != 1U || added_count != 1U || old_only == new_only)
            goto done;
    }

    snn = minisnn_create(3);
    if (snn == NULL || !minisnn_connect_delayed(snn, 0, 1, 42.0, 3) ||
        !minisnn_get_connection(snn, 0U, &before_connection) ||
        !minisnn_get_topology_signature(snn, &signature_before))
        goto done;
    memset(&operation, 0, sizeof(operation));
    operation.type = MINISNN_TOPOLOGY_REWIRE;
    operation.source = 0U;
    operation.target = 1U;
    operation.new_source = 0U;
    operation.new_target = 1U;
    operation.magnitude = 7.0;
    operation.delay = 1U;
    if (minisnn_apply_topology_patch(snn, &operation, 1U, &result) ||
        result.success || strstr(result.reason, "rewire deve alterar endpoints") == NULL ||
        minisnn_connection_count(snn) != 1U ||
        !minisnn_get_connection(snn, 0U, &after_connection) ||
        !minisnn_get_topology_signature(snn, &signature_after) ||
        signature_after != signature_before ||
        after_connection.source != before_connection.source ||
        after_connection.target != before_connection.target ||
        !same_double(after_connection.weight, before_connection.weight) ||
        after_connection.delay != before_connection.delay)
        goto done;
    ok = 1;
done:
    structure_genome_destroy(&original);
    structure_genome_destroy(&mutated);
    minisnn_destroy(&snn);
    return ok ? 1 : fail("rewire identity or public atomicity");
}

static int check_learning_state_rebuild(void)
{
    MiniSNN *snn = minisnn_create(3);
    MiniSNNPlasticityConfig plasticity = minisnn_default_plasticity_config();
    MiniSNNRewardConfig reward = minisnn_default_reward_config();
    MiniSNNHomeostasisConfig homeostasis = minisnn_default_homeostasis_config();
    MiniSNNTopologyOperation operation;
    size_t old_id;
    size_t new_id;
    double eligibility_before;
    double eligibility_after;
    double new_eligibility;
    double threshold_before;
    double threshold_after;
    double rate_before;
    double rate_after;
    const char *stage = "setup";

    plasticity.enabled = 1;
    plasticity.learning_mode = MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP;
    reward.enabled = 1;
    homeostasis.enabled = 1;
    homeostasis.intrinsic_enabled = 1;
    homeostasis.update_interval_steps = 5U;
    if (snn == NULL ||
        !minisnn_connect_delayed(snn, 0, 1, 100.0, 1) ||
        !minisnn_set_plasticity_config(snn, &plasticity) ||
        !minisnn_set_reward_config(snn, &reward) ||
        !minisnn_set_homeostasis_config(snn, &homeostasis))
        goto fail;
    for (int step = 0; step < 350; step++)
    {
        minisnn_clear_inputs(snn);
        if (!minisnn_set_input(snn, 0, 20.0) ||
            !minisnn_set_input(snn, 1, 20.0) ||
            minisnn_step(snn) < 0)
            goto fail;
    }
    if (!find_connection(snn, 0U, 1U, &old_id) ||
        !minisnn_get_connection_eligibility(snn, old_id, &eligibility_before) ||
        !minisnn_get_neuron_effective_threshold(snn, 1, &threshold_before) ||
        !minisnn_get_neuron_rate_trace(snn, 1, &rate_before))
        goto fail;

    memset(&operation, 0, sizeof(operation));
    operation.type = MINISNN_TOPOLOGY_ADD;
    operation.source = 1U;
    operation.target = 2U;
    operation.magnitude = 75.0;
    operation.delay = 2U;
    stage = "apply topology patch";
    if (!minisnn_apply_topology_patch(snn, &operation, 1U, NULL))
        goto fail;
    stage = "find surviving connection";
    if (!find_connection(snn, 0U, 1U, &old_id))
        goto fail;
    stage = "find new connection";
    if (!find_connection(snn, 1U, 2U, &new_id))
        goto fail;
    stage = "read surviving eligibility";
    if (!minisnn_get_connection_eligibility(
            snn, old_id, &eligibility_after))
        goto fail;
    stage = "read new eligibility";
    if (!minisnn_get_connection_eligibility(snn, new_id, &new_eligibility))
        goto fail;
    stage = "read effective threshold";
    if (!minisnn_get_neuron_effective_threshold(snn, 1, &threshold_after))
        goto fail;
    stage = "read rate trace";
    if (!minisnn_get_neuron_rate_trace(snn, 1, &rate_after))
        goto fail;
    stage = "compare preserved state";
    if (!same_double(eligibility_before, eligibility_after) ||
        !same_double(new_eligibility, 0.0) ||
        !same_double(threshold_before, threshold_after) ||
        !same_double(rate_before, rate_after))
        goto fail;

    memset(&operation, 0, sizeof(operation));
    operation.type = MINISNN_TOPOLOGY_REMOVE;
    operation.source = 0U;
    operation.target = 1U;
    if (!minisnn_apply_topology_patch(snn, &operation, 1U, NULL) ||
        minisnn_connection_count(snn) != 1U ||
        !minisnn_queue_reward(snn, 0.5) || minisnn_step(snn) < 0)
        goto fail;
    minisnn_destroy(&snn);
    return 1;
fail:
    fprintf(stderr, "rebuild stage failed: %s\n", stage);
    minisnn_destroy(&snn);
    return fail("STDP, R-STDP, or homeostasis state rebuild");
}

static int check_future_only_transmission(void)
{
    MiniSNN *snn = minisnn_create(2);
    MiniSNNTopologyOperation operation;
    double current_after_add;
    double current_from_last_transmission;
    double current_without_new_transmission;

    if (snn == NULL || !minisnn_set_input(snn, 0, 10000.0) ||
        minisnn_step(snn) < 0)
        goto fail;
    memset(&operation, 0, sizeof(operation));
    operation.type = MINISNN_TOPOLOGY_ADD;
    operation.source = 0U;
    operation.target = 1U;
    operation.magnitude = 100.0;
    operation.delay = 1U;
    if (!minisnn_apply_topology_patch(snn, &operation, 1U, NULL) ||
        minisnn_step(snn) < 0 ||
        !minisnn_get_synaptic_current(snn, 1, &current_after_add) ||
        !same_double(current_after_add, 0.0))
        goto fail;

    memset(&operation, 0, sizeof(operation));
    operation.type = MINISNN_TOPOLOGY_REMOVE;
    operation.source = 0U;
    operation.target = 1U;
    if (!minisnn_apply_topology_patch(snn, &operation, 1U, NULL) ||
        minisnn_step(snn) < 0 ||
        !minisnn_get_synaptic_current(
            snn, 1, &current_from_last_transmission) ||
        !same_double(current_from_last_transmission, 100.0) ||
        minisnn_step(snn) < 0 ||
        !minisnn_get_synaptic_current(
            snn, 1, &current_without_new_transmission) ||
        !same_double(current_without_new_transmission, 95.0))
        goto fail;
    minisnn_destroy(&snn);
    return 1;
fail:
    minisnn_destroy(&snn);
    return fail("future-only transmission after add/remove");
}

static int add_initial_eight_connections(MiniSNN *snn)
{
    static const int endpoints[8][2] = {
        {0, 1}, {0, 2}, {0, 3}, {1, 0},
        {1, 2}, {1, 3}, {2, 0}, {2, 1}};
    for (size_t i = 0; i < 8U; i++)
    {
        if (!minisnn_connect_delayed(
                snn, endpoints[i][0], endpoints[i][1],
                20.0 + (double)i, 1))
            return 0;
    }
    return 1;
}

static int check_darwinian_structure_inheritance(void)
{
    MiniSNN *parent = minisnn_create(4);
    MiniSNN *child = minisnn_create(4);
    MiniSNNTopologyOperation lifetime_patch[3];
    MiniSNNTopologyOperation reproductive_mutation;

    if (parent == NULL || child == NULL ||
        !add_initial_eight_connections(parent) ||
        !add_initial_eight_connections(child) ||
        minisnn_connection_count(parent) != 8U ||
        minisnn_connection_count(child) != 8U)
        goto fail;

    memset(lifetime_patch, 0, sizeof(lifetime_patch));
    lifetime_patch[0].type = MINISNN_TOPOLOGY_REMOVE;
    lifetime_patch[0].source = 0U;
    lifetime_patch[0].target = 1U;
    lifetime_patch[1].type = MINISNN_TOPOLOGY_REMOVE;
    lifetime_patch[1].source = 0U;
    lifetime_patch[1].target = 2U;
    lifetime_patch[2].type = MINISNN_TOPOLOGY_ADD;
    lifetime_patch[2].source = 3U;
    lifetime_patch[2].target = 0U;
    lifetime_patch[2].magnitude = 12.0;
    lifetime_patch[2].delay = 1U;
    if (!minisnn_apply_topology_patch(
            parent, lifetime_patch, 3U, NULL) ||
        minisnn_connection_count(parent) != 7U ||
        minisnn_connection_count(child) != 8U)
        goto fail;

    memset(&reproductive_mutation, 0, sizeof(reproductive_mutation));
    reproductive_mutation.type = MINISNN_TOPOLOGY_ADD;
    reproductive_mutation.source = 3U;
    reproductive_mutation.target = 0U;
    reproductive_mutation.magnitude = 12.0;
    reproductive_mutation.delay = 1U;
    if (!minisnn_apply_topology_patch(
            child, &reproductive_mutation, 1U, NULL) ||
        minisnn_connection_count(child) != 9U)
        goto fail;
    minisnn_destroy(&parent);
    minisnn_destroy(&child);
    return 1;
fail:
    minisnn_destroy(&parent);
    minisnn_destroy(&child);
    return fail("Darwinian structural inheritance");
}

int main(void)
{
    if (!check_keys_and_canonicalization() ||
        !check_constraints_and_reachability() ||
        !check_distance_and_complexity() ||
        !check_crossover_and_mutation() ||
        !check_crossover_retry_stats_and_tied_subset() ||
        !check_public_atomic_patch() ||
        !check_rewire_identity_and_atomicity() ||
        !check_learning_state_rebuild() ||
        !check_future_only_transmission() ||
        !check_darwinian_structure_inheritance())
        return 1;
    printf("Structural evolution validation OK\n");
    return 0;
}
