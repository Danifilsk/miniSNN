#ifndef STRUCTURE_H
#define STRUCTURE_H

#include <stddef.h>
#include <stdint.h>

#include "evolution.h"
#include "minisnn_types.h"
#include "neuron.h"

typedef struct
{
    size_t neuron_count;
    const LIFNeuron *neurons;
    size_t min_connections;
    size_t max_connections;
    int allow_self_connections;
    int allow_inh_to_inh;
    unsigned int delay_min;
    unsigned int delay_max;
    int preserve_required_reachability;
    const size_t *required_inputs;
    size_t required_input_count;
    const size_t *required_outputs;
    size_t required_output_count;
    double new_exc_weight_min;
    double new_exc_weight_max;
    double new_inh_magnitude_min;
    double new_inh_magnitude_max;
    unsigned int delay_mutation_max_delta;
} StructureConstraints;

typedef struct
{
    MiniSNNConnectionGene *connections;
    size_t connection_count;
    size_t capacity;
} StructureGenome;

typedef struct
{
    size_t add_count;
    size_t remove_count;
    size_t rewire_count;
    size_t delay_mutation_count;
    size_t skipped_count;
    size_t common_inherited;
    size_t disjoint_inherited;
    int crossover_fallback;
} StructureMutationStats;

int structure_connection_key(
    size_t neuron_count,
    size_t source,
    size_t target,
    uint64_t *out_key);

void structure_genome_destroy(StructureGenome *genome);

int structure_genome_copy(
    StructureGenome *destination,
    const StructureGenome *source);

int structure_genome_set(
    StructureGenome *genome,
    const MiniSNNConnectionGene *connections,
    size_t connection_count);

int structure_genome_canonicalize(StructureGenome *genome);

int structure_genome_find(
    const StructureGenome *genome,
    uint64_t connection_key,
    size_t *out_index);

int structure_is_legal_pair(
    const StructureConstraints *constraints,
    const StructureGenome *genome,
    size_t source,
    size_t target,
    uint64_t ignored_key);

int structure_genome_has_required_reachability(
    const StructureGenome *genome,
    const StructureConstraints *constraints);

int structure_genome_validate(
    const StructureGenome *genome,
    const StructureConstraints *constraints);

uint64_t structure_neuron_blueprint_signature(
    const LIFNeuron *neurons,
    size_t neuron_count);

uint64_t structure_topology_signature(
    const StructureGenome *genome,
    uint64_t neuron_blueprint_signature);

uint64_t structure_genome_signature(
    const StructureGenome *genome,
    const double *scalar_genes,
    size_t scalar_gene_count,
    uint64_t neuron_blueprint_signature);

double structure_jaccard_distance(
    const StructureGenome *left,
    const StructureGenome *right);

double structure_complexity_normalized(
    size_t connection_count,
    size_t minimum,
    size_t maximum);

double structure_apply_complexity_penalty(
    double robust_fitness,
    double penalty,
    double complexity_normalized);

int structure_genome_crossover(
    const StructureGenome *parent_a,
    double fitness_a,
    uint64_t id_a,
    const StructureGenome *parent_b,
    double fitness_b,
    uint64_t id_b,
    const StructureConstraints *constraints,
    EvolutionPrng *prng,
    StructureGenome *out_child,
    StructureMutationStats *out_stats);

int structure_genome_mutate(
    StructureGenome *genome,
    const StructureConstraints *constraints,
    int allow_add,
    int allow_remove,
    int allow_rewire,
    int evolve_delays,
    double add_rate,
    double remove_rate,
    double rewire_rate,
    double delay_mutation_rate,
    size_t max_mutations,
    EvolutionPrng *prng,
    StructureMutationStats *out_stats);

const char *structure_operation_name(MiniSNNTopologyOperationType type);

struct Network;

int structure_capture_network_genome(
    const struct Network *net,
    StructureGenome *out_genome);

int structure_validate_network_patch(
    const struct Network *net,
    const MiniSNNTopologyOperation *operations,
    size_t operation_count,
    MiniSNNTopologyPatchResult *result);

int structure_apply_network_patch(
    struct Network *net,
    const MiniSNNTopologyOperation *operations,
    size_t operation_count,
    MiniSNNTopologyPatchResult *result);

int structure_apply_network_patch_at_step(
    struct Network *net,
    const MiniSNNTopologyOperation *operations,
    size_t operation_count,
    unsigned long long birth_step,
    MiniSNNTopologyPatchResult *result);

int structure_replace_network_genome(
    struct Network *net,
    const StructureGenome *genome,
    unsigned long long birth_step);

#endif
