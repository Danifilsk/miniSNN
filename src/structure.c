#include "structure.h"

#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network.h"
#include "structural_plasticity.h"

#define STRUCTURE_FNV_OFFSET 1469598103934665603ULL
#define STRUCTURE_FNV_PRIME 1099511628211ULL
#define STRUCTURE_RANDOM_ATTEMPTS 128U
#define STRUCTURE_CROSSOVER_ATTEMPTS 8U

static void hash_byte(uint64_t *hash, unsigned char value)
{
    *hash ^= (uint64_t)value;
    *hash *= STRUCTURE_FNV_PRIME;
}

static void hash_u64(uint64_t *hash, uint64_t value)
{
    for (unsigned int shift = 0; shift < 64U; shift += 8U)
        hash_byte(hash, (unsigned char)((value >> shift) & 0xffU));
}

static void hash_double(uint64_t *hash, double value)
{
    uint64_t bits = 0U;
    memcpy(&bits, &value, sizeof(bits));
    hash_u64(hash, bits);
}

static int gene_compare(const void *left, const void *right)
{
    const MiniSNNConnectionGene *a = left;
    const MiniSNNConnectionGene *b = right;
    if (a->connection_key < b->connection_key)
        return -1;
    if (a->connection_key > b->connection_key)
        return 1;
    return 0;
}

static int genome_reserve(StructureGenome *genome, size_t capacity)
{
    MiniSNNConnectionGene *updated;
    size_t next;

    if (genome == NULL)
        return 0;
    if (capacity <= genome->capacity)
        return 1;
    next = genome->capacity == 0 ? 4U : genome->capacity;
    while (next < capacity)
    {
        if (next > SIZE_MAX / 2U)
            return 0;
        next *= 2U;
    }
    if (next > SIZE_MAX / sizeof(*updated))
        return 0;
    updated = realloc(genome->connections, next * sizeof(*updated));
    if (updated == NULL)
        return 0;
    genome->connections = updated;
    genome->capacity = next;
    return 1;
}

int structure_connection_key(
    size_t neuron_count,
    size_t source,
    size_t target,
    uint64_t *out_key)
{
    uint64_t count;
    uint64_t source64;

    if (out_key == NULL || neuron_count == 0 || source >= neuron_count ||
        target >= neuron_count)
        return 0;
    count = (uint64_t)neuron_count;
    source64 = (uint64_t)source;
    if (source64 > (UINT64_MAX - (uint64_t)target) / count)
        return 0;
    *out_key = source64 * count + (uint64_t)target;
    return 1;
}

void structure_genome_destroy(StructureGenome *genome)
{
    if (genome == NULL)
        return;
    free(genome->connections);
    memset(genome, 0, sizeof(*genome));
}

int structure_genome_set(
    StructureGenome *genome,
    const MiniSNNConnectionGene *connections,
    size_t connection_count)
{
    StructureGenome temporary = {0};

    if (genome == NULL || (connection_count > 0 && connections == NULL) ||
        connection_count > SIZE_MAX / sizeof(*connections))
        return 0;
    if (connection_count > 0)
    {
        temporary.connections = malloc(connection_count * sizeof(*connections));
        if (temporary.connections == NULL)
            return 0;
        memcpy(temporary.connections, connections,
               connection_count * sizeof(*connections));
        temporary.connection_count = connection_count;
        temporary.capacity = connection_count;
    }
    structure_genome_destroy(genome);
    *genome = temporary;
    return 1;
}

int structure_genome_copy(
    StructureGenome *destination,
    const StructureGenome *source)
{
    if (destination == NULL || source == NULL)
        return 0;
    return structure_genome_set(
        destination, source->connections, source->connection_count);
}

int structure_genome_canonicalize(StructureGenome *genome)
{
    if (genome == NULL ||
        (genome->connection_count > 0 && genome->connections == NULL))
        return 0;
    if (genome->connection_count > 1)
        qsort(genome->connections, genome->connection_count,
              sizeof(*genome->connections), gene_compare);
    for (size_t i = 1; i < genome->connection_count; i++)
    {
        if (genome->connections[i - 1].connection_key ==
            genome->connections[i].connection_key)
            return 0;
    }
    return 1;
}

int structure_genome_find(
    const StructureGenome *genome,
    uint64_t connection_key,
    size_t *out_index)
{
    size_t low = 0;
    size_t high;

    if (genome == NULL || out_index == NULL ||
        (genome->connection_count > 0 && genome->connections == NULL))
        return 0;
    high = genome->connection_count;
    while (low < high)
    {
        size_t middle = low + (high - low) / 2U;
        uint64_t key = genome->connections[middle].connection_key;
        if (key == connection_key)
        {
            *out_index = middle;
            return 1;
        }
        if (key < connection_key)
            low = middle + 1U;
        else
            high = middle;
    }
    *out_index = low;
    return 0;
}

int structure_is_legal_pair(
    const StructureConstraints *constraints,
    const StructureGenome *genome,
    size_t source,
    size_t target,
    uint64_t ignored_key)
{
    uint64_t key;
    size_t index;

    if (constraints == NULL || constraints->neurons == NULL || genome == NULL ||
        source >= constraints->neuron_count || target >= constraints->neuron_count ||
        (!constraints->allow_self_connections && source == target) ||
        (!constraints->allow_inh_to_inh &&
         constraints->neurons[source].type == NEURON_INHIBITORY &&
         constraints->neurons[target].type == NEURON_INHIBITORY) ||
        !structure_connection_key(constraints->neuron_count, source, target, &key))
        return 0;
    if (key == ignored_key)
        return 1;
    return !structure_genome_find(genome, key, &index);
}

int structure_genome_has_required_reachability(
    const StructureGenome *genome,
    const StructureConstraints *constraints)
{
    unsigned char *visited;
    size_t *queue;
    size_t head = 0;
    size_t tail = 0;
    int valid = 1;

    if (genome == NULL || constraints == NULL)
        return 0;
    if (!constraints->preserve_required_reachability)
        return 1;
    if (constraints->required_input_count == 0 ||
        constraints->required_output_count == 0 ||
        constraints->required_inputs == NULL ||
        constraints->required_outputs == NULL)
        return 0;
    visited = calloc(constraints->neuron_count, sizeof(*visited));
    queue = malloc(constraints->neuron_count * sizeof(*queue));
    if (visited == NULL || queue == NULL)
    {
        free(visited);
        free(queue);
        return 0;
    }
    for (size_t i = 0; i < constraints->required_input_count; i++)
    {
        size_t source = constraints->required_inputs[i];
        if (source >= constraints->neuron_count)
        {
            valid = 0;
            goto done;
        }
        if (!visited[source])
        {
            visited[source] = 1U;
            queue[tail++] = source;
        }
    }
    while (head < tail)
    {
        size_t source = queue[head++];
        for (size_t i = 0; i < genome->connection_count; i++)
        {
            size_t target;
            if (genome->connections[i].source != source)
                continue;
            target = genome->connections[i].target;
            if (!visited[target])
            {
                visited[target] = 1U;
                queue[tail++] = target;
            }
        }
    }
    for (size_t i = 0; i < constraints->required_output_count; i++)
    {
        size_t output = constraints->required_outputs[i];
        if (output >= constraints->neuron_count || !visited[output])
        {
            valid = 0;
            break;
        }
    }
done:
    free(visited);
    free(queue);
    return valid;
}

int structure_genome_validate(
    const StructureGenome *genome,
    const StructureConstraints *constraints)
{
    uint64_t previous = 0U;

    if (genome == NULL || constraints == NULL || constraints->neurons == NULL ||
        constraints->neuron_count == 0 ||
        genome->connection_count < constraints->min_connections ||
        genome->connection_count > constraints->max_connections ||
        (genome->connection_count > 0 && genome->connections == NULL) ||
        constraints->delay_min > constraints->delay_max)
        return 0;
    for (size_t i = 0; i < genome->connection_count; i++)
    {
        const MiniSNNConnectionGene *gene = &genome->connections[i];
        uint64_t expected;
        if (!structure_connection_key(constraints->neuron_count,
                                      gene->source, gene->target, &expected) ||
            gene->connection_key != expected ||
            (i > 0 && gene->connection_key <= previous) ||
            !isfinite(gene->magnitude) || gene->magnitude < 0.0 ||
            gene->delay < constraints->delay_min ||
            gene->delay > constraints->delay_max ||
            (!constraints->allow_self_connections &&
             gene->source == gene->target) ||
            (!constraints->allow_inh_to_inh &&
             constraints->neurons[gene->source].type == NEURON_INHIBITORY &&
             constraints->neurons[gene->target].type == NEURON_INHIBITORY))
            return 0;
        previous = gene->connection_key;
    }
    return structure_genome_has_required_reachability(genome, constraints);
}

uint64_t structure_neuron_blueprint_signature(
    const LIFNeuron *neurons,
    size_t neuron_count)
{
    uint64_t hash = STRUCTURE_FNV_OFFSET;
    if (neurons == NULL || neuron_count == 0)
        return 0U;
    hash_u64(&hash, (uint64_t)neuron_count);
    for (size_t i = 0; i < neuron_count; i++)
        hash_u64(&hash, (uint64_t)neurons[i].type);
    return hash;
}

uint64_t structure_topology_signature(
    const StructureGenome *genome,
    uint64_t neuron_blueprint_signature)
{
    uint64_t hash = STRUCTURE_FNV_OFFSET;
    if (genome == NULL || neuron_blueprint_signature == 0U)
        return 0U;
    hash_u64(&hash, neuron_blueprint_signature);
    hash_u64(&hash, (uint64_t)genome->connection_count);
    for (size_t i = 0; i < genome->connection_count; i++)
    {
        hash_u64(&hash, genome->connections[i].connection_key);
        hash_u64(&hash, (uint64_t)genome->connections[i].delay);
    }
    return hash;
}

uint64_t structure_genome_signature(
    const StructureGenome *genome,
    const double *scalar_genes,
    size_t scalar_gene_count,
    uint64_t neuron_blueprint_signature)
{
    uint64_t hash = STRUCTURE_FNV_OFFSET;
    if (genome == NULL || neuron_blueprint_signature == 0U ||
        (scalar_gene_count > 0 && scalar_genes == NULL))
        return 0U;
    hash_u64(&hash, neuron_blueprint_signature);
    hash_u64(&hash, (uint64_t)scalar_gene_count);
    for (size_t i = 0; i < scalar_gene_count; i++)
        hash_double(&hash, scalar_genes[i]);
    hash_u64(&hash, (uint64_t)genome->connection_count);
    for (size_t i = 0; i < genome->connection_count; i++)
    {
        hash_u64(&hash, genome->connections[i].connection_key);
        hash_double(&hash, genome->connections[i].magnitude);
        hash_u64(&hash, (uint64_t)genome->connections[i].delay);
    }
    return hash;
}

double structure_jaccard_distance(
    const StructureGenome *left,
    const StructureGenome *right)
{
    size_t i = 0;
    size_t j = 0;
    size_t intersection = 0;
    size_t union_count = 0;
    if (left == NULL || right == NULL)
        return -1.0;
    while (i < left->connection_count || j < right->connection_count)
    {
        if (i < left->connection_count && j < right->connection_count &&
            left->connections[i].connection_key ==
                right->connections[j].connection_key)
        {
            intersection++;
            union_count++;
            i++;
            j++;
        }
        else if (j >= right->connection_count ||
                 (i < left->connection_count &&
                  left->connections[i].connection_key <
                      right->connections[j].connection_key))
        {
            union_count++;
            i++;
        }
        else
        {
            union_count++;
            j++;
        }
    }
    return union_count == 0 ? 0.0 :
        1.0 - (double)intersection / (double)union_count;
}

double structure_complexity_normalized(
    size_t connection_count,
    size_t minimum,
    size_t maximum)
{
    if (maximum <= minimum)
        return 0.0;
    if (connection_count <= minimum)
        return 0.0;
    if (connection_count >= maximum)
        return 1.0;
    return (double)(connection_count - minimum) /
           (double)(maximum - minimum);
}

double structure_apply_complexity_penalty(
    double robust_fitness,
    double penalty,
    double complexity_normalized)
{
    double value;
    if (!isfinite(robust_fitness) || !isfinite(penalty) ||
        !isfinite(complexity_normalized) || robust_fitness < 0.0 ||
        robust_fitness > 1.0 || penalty < 0.0 || penalty > 1.0 ||
        complexity_normalized < 0.0 || complexity_normalized > 1.0)
        return -1.0;
    value = robust_fitness - penalty * complexity_normalized;
    if (value < 0.0)
        value = 0.0;
    if (value > 1.0)
        value = 1.0;
    return value;
}

static const StructureGenome *fitter_parent(
    const StructureGenome *a,
    double fitness_a,
    uint64_t id_a,
    const StructureGenome *b,
    double fitness_b,
    uint64_t id_b)
{
    if (fitness_a > fitness_b || (fitness_a == fitness_b && id_a < id_b))
        return a;
    return b;
}

static size_t random_index(EvolutionPrng *prng, size_t count)
{
    size_t index = (size_t)(evolution_prng_unit(prng) * (double)count);
    return index < count ? index : count - 1U;
}

static int gene_is_common_to_parents(
    const StructureGenome *parent_a,
    const StructureGenome *parent_b,
    uint64_t key)
{
    size_t ignored_index;
    return structure_genome_find(parent_a, key, &ignored_index) &&
           structure_genome_find(parent_b, key, &ignored_index);
}

static void crossover_count_final_genes(
    const StructureGenome *child,
    const StructureGenome *parent_a,
    const StructureGenome *parent_b,
    size_t *out_common,
    size_t *out_disjoint)
{
    size_t common = 0U;
    size_t disjoint = 0U;
    for (size_t i = 0; i < child->connection_count; i++)
    {
        if (gene_is_common_to_parents(
                parent_a, parent_b, child->connections[i].connection_key))
            common++;
        else
            disjoint++;
    }
    *out_common = common;
    *out_disjoint = disjoint;
}

static void crossover_select_subset(
    StructureGenome *candidate,
    size_t maximum,
    EvolutionPrng *prng)
{
    if (candidate->connection_count <= maximum)
        return;
    for (size_t index = 0; index < maximum; index++)
    {
        size_t selected = index + random_index(
            prng, candidate->connection_count - index);
        MiniSNNConnectionGene temporary = candidate->connections[index];
        candidate->connections[index] = candidate->connections[selected];
        candidate->connections[selected] = temporary;
    }
    candidate->connection_count = maximum;
}

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
    StructureMutationStats *out_stats)
{
    const StructureGenome *fitter;
    int tied;

    if (parent_a == NULL || parent_b == NULL || constraints == NULL ||
        prng == NULL || out_child == NULL || !isfinite(fitness_a) ||
        !isfinite(fitness_b))
        return 0;
    fitter = fitter_parent(parent_a, fitness_a, id_a,
                           parent_b, fitness_b, id_b);
    tied = fitness_a == fitness_b;
    for (unsigned int attempt = 0; attempt < STRUCTURE_CROSSOVER_ATTEMPTS; attempt++)
    {
        StructureGenome candidate = {0};
        size_t attempt_common = 0U;
        size_t attempt_disjoint = 0U;
        StructureMutationStats result = {0};
        size_t i = 0;
        size_t j = 0;
        if (!genome_reserve(&candidate,
                            parent_a->connection_count + parent_b->connection_count))
            return 0;
        while (i < parent_a->connection_count || j < parent_b->connection_count)
        {
            const MiniSNNConnectionGene *selected = NULL;
            unsigned int inherited_from = 0U;
            if (i < parent_a->connection_count && j < parent_b->connection_count &&
                parent_a->connections[i].connection_key ==
                    parent_b->connections[j].connection_key)
            {
                if (evolution_prng_unit(prng) < 0.5)
                {
                    selected = &parent_a->connections[i];
                    inherited_from = 1U;
                }
                else
                {
                    selected = &parent_b->connections[j];
                    inherited_from = 2U;
                }
                attempt_common++;
                i++;
                j++;
            }
            else
            {
                const MiniSNNConnectionGene *disjoint;
                const StructureGenome *owner;
                if (j >= parent_b->connection_count ||
                    (i < parent_a->connection_count &&
                     parent_a->connections[i].connection_key <
                         parent_b->connections[j].connection_key))
                {
                    disjoint = &parent_a->connections[i++];
                    owner = parent_a;
                }
                else
                {
                    disjoint = &parent_b->connections[j++];
                    owner = parent_b;
                }
                if ((tied && evolution_prng_unit(prng) < 0.5) ||
                    (!tied && owner == fitter))
                {
                    selected = disjoint;
                    inherited_from = owner == parent_a ? 1U : 2U;
                    attempt_disjoint++;
                }
            }
            if (selected != NULL)
            {
                candidate.connections[candidate.connection_count] = *selected;
                candidate.connections[candidate.connection_count].inherited_from =
                    inherited_from;
                candidate.connection_count++;
            }
        }
        crossover_select_subset(
            &candidate, constraints->max_connections, prng);
        if (structure_genome_canonicalize(&candidate) &&
            structure_genome_validate(&candidate, constraints))
        {
            crossover_count_final_genes(
                &candidate, parent_a, parent_b,
                &attempt_common, &attempt_disjoint);
            result.common_inherited = attempt_common;
            result.disjoint_inherited = attempt_disjoint;
            structure_genome_destroy(out_child);
            *out_child = candidate;
            if (out_stats != NULL)
                *out_stats = result;
            return 1;
        }
        structure_genome_destroy(&candidate);
    }
    if (!structure_genome_copy(out_child, fitter))
        return 0;
    for (size_t i = 0; i < out_child->connection_count; i++)
        out_child->connections[i].inherited_from =
            fitter == parent_a ? 1U : 2U;
    {
        StructureMutationStats result = {0};
        crossover_count_final_genes(
            out_child, parent_a, parent_b,
            &result.common_inherited, &result.disjoint_inherited);
        result.crossover_fallback = 1;
        if (out_stats != NULL)
            *out_stats = result;
    }
    return structure_genome_validate(out_child, constraints);
}

static double random_magnitude(
    EvolutionPrng *prng,
    const StructureConstraints *constraints,
    size_t source)
{
    double minimum;
    double maximum;
    if (constraints->neurons[source].type == NEURON_INHIBITORY)
    {
        minimum = constraints->new_inh_magnitude_min;
        maximum = constraints->new_inh_magnitude_max;
    }
    else
    {
        minimum = constraints->new_exc_weight_min;
        maximum = constraints->new_exc_weight_max;
    }
    return minimum + evolution_prng_unit(prng) * (maximum - minimum);
}

static unsigned int random_delay(
    EvolutionPrng *prng,
    const StructureConstraints *constraints)
{
    uint64_t range = (uint64_t)constraints->delay_max -
                     (uint64_t)constraints->delay_min + 1U;
    uint64_t offset = (uint64_t)(evolution_prng_unit(prng) * (double)range);
    if (offset >= range)
        offset = range - 1U;
    return constraints->delay_min + (unsigned int)offset;
}

static int find_absent_pair(
    const StructureGenome *genome,
    const StructureConstraints *constraints,
    EvolutionPrng *prng,
    uint64_t forbidden_key,
    size_t *out_source,
    size_t *out_target)
{
    for (unsigned int attempt = 0; attempt < STRUCTURE_RANDOM_ATTEMPTS; attempt++)
    {
        size_t source = random_index(prng, constraints->neuron_count);
        size_t target = random_index(prng, constraints->neuron_count);
        uint64_t key;
        if (!structure_connection_key(
                constraints->neuron_count, source, target, &key))
            continue;
        if (key != forbidden_key && structure_is_legal_pair(
                constraints, genome, source, target, UINT64_MAX))
        {
            *out_source = source;
            *out_target = target;
            return 1;
        }
    }
    for (size_t source = 0; source < constraints->neuron_count; source++)
    {
        for (size_t target = 0; target < constraints->neuron_count; target++)
        {
            uint64_t key;
            if (!structure_connection_key(
                    constraints->neuron_count, source, target, &key))
                continue;
            if (key != forbidden_key && structure_is_legal_pair(
                    constraints, genome, source, target, UINT64_MAX))
            {
                *out_source = source;
                *out_target = target;
                return 1;
            }
        }
    }
    return 0;
}

static int genome_add_random(
    StructureGenome *genome,
    const StructureConstraints *constraints,
    EvolutionPrng *prng)
{
    MiniSNNConnectionGene gene;
    if (genome->connection_count >= constraints->max_connections ||
        !find_absent_pair(genome, constraints, prng, UINT64_MAX,
                          &gene.source, &gene.target) ||
        !structure_connection_key(constraints->neuron_count,
                                  gene.source, gene.target, &gene.connection_key) ||
        !genome_reserve(genome, genome->connection_count + 1U))
        return 0;
    gene.magnitude = random_magnitude(prng, constraints, gene.source);
    gene.delay = random_delay(prng, constraints);
    gene.inherited_from = 0U;
    genome->connections[genome->connection_count++] = gene;
    return structure_genome_canonicalize(genome);
}

static int genome_remove_random(
    StructureGenome *genome,
    const StructureConstraints *constraints,
    EvolutionPrng *prng)
{
    if (genome->connection_count <= constraints->min_connections)
        return 0;
    for (size_t attempt = 0; attempt < genome->connection_count; attempt++)
    {
        size_t index = random_index(prng, genome->connection_count);
        MiniSNNConnectionGene saved = genome->connections[index];
        memmove(&genome->connections[index], &genome->connections[index + 1U],
                (genome->connection_count - index - 1U) *
                    sizeof(*genome->connections));
        genome->connection_count--;
        if (structure_genome_validate(genome, constraints))
            return 1;
        memmove(&genome->connections[index + 1U], &genome->connections[index],
                (genome->connection_count - index) *
                    sizeof(*genome->connections));
        genome->connections[index] = saved;
        genome->connection_count++;
    }
    return 0;
}

static int genome_rewire_random(
    StructureGenome *genome,
    const StructureConstraints *constraints,
    EvolutionPrng *prng)
{
    StructureGenome original = {0};
    size_t index;
    size_t source;
    size_t target;
    uint64_t old_key;
    if (genome->connection_count == 0 ||
        !structure_genome_copy(&original, genome))
        return 0;
    index = random_index(prng, genome->connection_count);
    old_key = genome->connections[index].connection_key;
    memmove(&genome->connections[index], &genome->connections[index + 1U],
            (genome->connection_count - index - 1U) * sizeof(*genome->connections));
    genome->connection_count--;
    if (!find_absent_pair(genome, constraints, prng, old_key,
                          &source, &target))
        goto fail;
    {
        MiniSNNConnectionGene gene;
        gene.source = source;
        gene.target = target;
        gene.magnitude = random_magnitude(prng, constraints, source);
        gene.delay = random_delay(prng, constraints);
        gene.inherited_from = 0U;
        if (!structure_connection_key(constraints->neuron_count, source, target,
                                      &gene.connection_key) ||
            !genome_reserve(genome, genome->connection_count + 1U))
            goto fail;
        genome->connections[genome->connection_count++] = gene;
    }
    if (!structure_genome_canonicalize(genome) ||
        !structure_genome_validate(genome, constraints))
        goto fail;
    structure_genome_destroy(&original);
    return 1;
fail:
    structure_genome_destroy(genome);
    *genome = original;
    return 0;
}

static int genome_mutate_delay(
    StructureGenome *genome,
    const StructureConstraints *constraints,
    EvolutionPrng *prng)
{
    size_t index;
    int delta;
    int before;
    int after;
    unsigned int max_delta = constraints->delay_mutation_max_delta;
    if (genome->connection_count == 0 || max_delta == 0U)
        return 0;
    index = random_index(prng, genome->connection_count);
    do
    {
        delta = (int)random_index(prng, (size_t)max_delta * 2U + 1U) -
                (int)max_delta;
    } while (delta == 0);
    before = (int)genome->connections[index].delay;
    after = before + delta;
    if (after < (int)constraints->delay_min)
        after = (int)constraints->delay_min;
    if (after > (int)constraints->delay_max)
        after = (int)constraints->delay_max;
    if (after == before)
        return 0;
    genome->connections[index].delay = (unsigned int)after;
    return 1;
}

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
    StructureMutationStats *out_stats)
{
    StructureMutationStats stats = {0};
    size_t applied = 0;
    if (genome == NULL || constraints == NULL || prng == NULL ||
        max_mutations == 0 || !isfinite(add_rate) || !isfinite(remove_rate) ||
        !isfinite(rewire_rate) || !isfinite(delay_mutation_rate))
        return 0;
#define TRY_MUTATION(enabled, rate, operation, field) \
    do { \
        if ((enabled) && applied < max_mutations && \
            evolution_prng_unit(prng) < (rate)) { \
            if (operation) { stats.field++; applied++; } \
            else stats.skipped_count++; \
        } \
    } while (0)
    TRY_MUTATION(allow_add, add_rate,
                 genome_add_random(genome, constraints, prng), add_count);
    TRY_MUTATION(allow_remove, remove_rate,
                 genome_remove_random(genome, constraints, prng), remove_count);
    TRY_MUTATION(allow_rewire, rewire_rate,
                 genome_rewire_random(genome, constraints, prng), rewire_count);
    TRY_MUTATION(evolve_delays, delay_mutation_rate,
                 genome_mutate_delay(genome, constraints, prng),
                 delay_mutation_count);
#undef TRY_MUTATION
    if (!structure_genome_canonicalize(genome) ||
        !structure_genome_validate(genome, constraints))
        return 0;
    if (out_stats != NULL)
        *out_stats = stats;
    return 1;
}

const char *structure_operation_name(MiniSNNTopologyOperationType type)
{
    switch (type)
    {
    case MINISNN_TOPOLOGY_ADD:
        return "add";
    case MINISNN_TOPOLOGY_REMOVE:
        return "remove";
    case MINISNN_TOPOLOGY_REWIRE:
        return "rewire";
    case MINISNN_TOPOLOGY_SET_DELAY:
        return "delay_mutation";
    default:
        return "unknown";
    }
}

static void patch_result_reset(
    MiniSNNTopologyPatchResult *result,
    size_t requested)
{
    if (result == NULL)
        return;
    memset(result, 0, sizeof(*result));
    result->requested_operations = requested;
}

static int patch_fail(
    MiniSNNTopologyPatchResult *result,
    const char *reason)
{
    if (result != NULL)
    {
        result->success = 0;
        snprintf(result->reason, sizeof(result->reason), "%s", reason);
    }
    return 0;
}

int structure_capture_network_genome(
    const Network *net,
    StructureGenome *out_genome)
{
    StructureGenome captured = {0};
    size_t count;

    if (net == NULL || out_genome == NULL || net->size <= 0 ||
        net->neurons == NULL || net->connections == NULL)
        return 0;
    count = network_connection_count(net);
    if (!genome_reserve(&captured, count))
        return 0;
    for (int source = 0; source < net->size; source++)
    {
        const ConnectionList *list = &net->connections[source];
        if (list->count < 0 || (list->count > 0 && list->list == NULL))
            goto fail;
        for (int index = 0; index < list->count; index++)
        {
            const Connection *connection = &list->list[index];
            MiniSNNConnectionGene gene;
            if (captured.connections == NULL ||
                captured.connection_count >= count)
                goto fail;
            if (!structure_connection_key(
                    (size_t)net->size, (size_t)source,
                    (size_t)connection->target, &gene.connection_key) ||
                !isfinite(connection->weight) ||
                connection->delay < 1 ||
                connection->delay > net->max_synaptic_delay)
                goto fail;
            gene.source = (size_t)source;
            gene.target = (size_t)connection->target;
            gene.magnitude = fabs(connection->weight);
            gene.delay = (unsigned int)connection->delay;
            gene.inherited_from = 0U;
            captured.connections[captured.connection_count++] = gene;
        }
    }
    if (!structure_genome_canonicalize(&captured))
        goto fail;
    structure_genome_destroy(out_genome);
    *out_genome = captured;
    return 1;
fail:
    structure_genome_destroy(&captured);
    return 0;
}

static void constraints_from_network(
    const Network *net,
    StructureConstraints *constraints)
{
    size_t possible = (size_t)net->size * (size_t)net->size;
    memset(constraints, 0, sizeof(*constraints));
    constraints->neuron_count = (size_t)net->size;
    constraints->neurons = net->neurons;
    constraints->min_connections = 0U;
    constraints->max_connections = possible;
    constraints->allow_self_connections = 0;
    constraints->allow_inh_to_inh = 1;
    constraints->delay_min = 1U;
    constraints->delay_max = (unsigned int)net->max_synaptic_delay;
    constraints->new_exc_weight_min = 0.0;
    constraints->new_exc_weight_max = 1.0;
    constraints->new_inh_magnitude_min = 0.0;
    constraints->new_inh_magnitude_max = 1.0;
    constraints->delay_mutation_max_delta = 1U;
    for (int source = 0; source < net->size; source++)
    {
        const ConnectionList *list = &net->connections[source];
        for (int i = 0; i < list->count; i++)
        {
            if (list->list[i].target == source)
            {
                constraints->allow_self_connections = 1;
                break;
            }
        }
        if (constraints->allow_self_connections)
            break;
    }
    if (net->structural_plasticity != NULL)
    {
        const MiniSNNStructuralPlasticityConfig *config =
            &net->structural_plasticity->config;
        constraints->min_connections = config->min_connections;
        constraints->max_connections = config->max_connections;
        constraints->allow_self_connections = config->allow_self_connections;
        constraints->allow_inh_to_inh = config->allow_inh_to_inh;
    }
}

static int patch_enable_explicit_self_connections(
    const MiniSNNTopologyOperation *operations,
    size_t operation_count,
    StructureConstraints *constraints)
{
    for (size_t i = 0; i < operation_count; i++)
    {
        const MiniSNNTopologyOperation *operation = &operations[i];
        int creates_self =
            (operation->type == MINISNN_TOPOLOGY_ADD &&
             operation->source == operation->target) ||
            (operation->type == MINISNN_TOPOLOGY_REWIRE &&
             operation->new_source == operation->new_target);
        if (creates_self && !operation->allow_self_connection)
            return 0;
        if (creates_self)
            constraints->allow_self_connections = 1;
    }
    return 1;
}

static int genome_insert_gene(
    StructureGenome *genome,
    const MiniSNNConnectionGene *gene)
{
    if (!genome_reserve(genome, genome->connection_count + 1U))
        return 0;
    genome->connections[genome->connection_count++] = *gene;
    return structure_genome_canonicalize(genome);
}

static void genome_remove_at(StructureGenome *genome, size_t index)
{
    memmove(&genome->connections[index], &genome->connections[index + 1U],
            (genome->connection_count - index - 1U) *
                sizeof(*genome->connections));
    genome->connection_count--;
}

static int apply_patch_to_genome(
    StructureGenome *genome,
    const StructureConstraints *constraints,
    const MiniSNNTopologyOperation *operations,
    size_t operation_count,
    MiniSNNTopologyPatchResult *result)
{
    for (size_t operation_index = 0;
         operation_index < operation_count;
         operation_index++)
    {
        const MiniSNNTopologyOperation *operation =
            &operations[operation_index];
        MiniSNNConnectionGene gene;
        uint64_t key;
        size_t index;

        if (!structure_connection_key(
                constraints->neuron_count, operation->source,
                operation->target, &key))
            return patch_fail(result, "endpoints invalidos");

        if (operation->type == MINISNN_TOPOLOGY_ADD)
        {
            if (!isfinite(operation->magnitude) || operation->magnitude < 0.0 ||
                operation->delay < constraints->delay_min ||
                operation->delay > constraints->delay_max ||
                !structure_is_legal_pair(
                    constraints, genome, operation->source,
                    operation->target, UINT64_MAX))
                return patch_fail(result, "conexao add invalida");
            gene.connection_key = key;
            gene.source = operation->source;
            gene.target = operation->target;
            gene.magnitude = operation->magnitude;
            gene.delay = operation->delay;
            gene.inherited_from = 0U;
            if (!genome_insert_gene(genome, &gene))
                return patch_fail(result, "memoria insuficiente no add");
            if (result != NULL)
                result->connections_added++;
        }
        else if (operation->type == MINISNN_TOPOLOGY_REMOVE)
        {
            if (!structure_genome_find(genome, key, &index))
                return patch_fail(result, "conexao remove ausente");
            genome_remove_at(genome, index);
            if (result != NULL)
                result->connections_removed++;
        }
        else if (operation->type == MINISNN_TOPOLOGY_REWIRE)
        {
            uint64_t new_key;
            StructureGenome original = {0};
            if (!structure_genome_find(genome, key, &index) ||
                !isfinite(operation->magnitude) || operation->magnitude < 0.0 ||
                operation->delay < constraints->delay_min ||
                operation->delay > constraints->delay_max ||
                !structure_genome_copy(&original, genome))
                return patch_fail(result, "rewire invalido");
            if (!structure_connection_key(
                    constraints->neuron_count, operation->new_source,
                    operation->new_target, &new_key) ||
                new_key == key)
            {
                structure_genome_destroy(&original);
                return patch_fail(result, "rewire deve alterar endpoints");
            }
            genome_remove_at(genome, index);
            if (!structure_is_legal_pair(
                    constraints, genome, operation->new_source,
                    operation->new_target, UINT64_MAX))
            {
                structure_genome_destroy(genome);
                *genome = original;
                return patch_fail(result, "novo par do rewire invalido");
            }
            gene.connection_key = new_key;
            gene.source = operation->new_source;
            gene.target = operation->new_target;
            gene.magnitude = operation->magnitude;
            gene.delay = operation->delay;
            gene.inherited_from = 0U;
            if (!genome_insert_gene(genome, &gene))
            {
                structure_genome_destroy(genome);
                *genome = original;
                return patch_fail(result, "memoria insuficiente no rewire");
            }
            structure_genome_destroy(&original);
            if (result != NULL)
            {
                result->connections_rewired++;
                result->connections_removed++;
                result->connections_added++;
            }
        }
        else if (operation->type == MINISNN_TOPOLOGY_SET_DELAY)
        {
            if (!structure_genome_find(genome, key, &index) ||
                operation->delay < constraints->delay_min ||
                operation->delay > constraints->delay_max)
                return patch_fail(result, "delay mutation invalida");
            if (genome->connections[index].delay != operation->delay)
            {
                genome->connections[index].delay = operation->delay;
                if (result != NULL)
                    result->delays_changed++;
            }
        }
        else
            return patch_fail(result, "tipo de operacao invalido");
    }
    if (!structure_genome_canonicalize(genome) ||
        !structure_genome_validate(genome, constraints))
        return patch_fail(result, "topologia final invalida");
    if (result != NULL)
        result->applied_operations = operation_count;
    return 1;
}

static void connection_lists_destroy(ConnectionList *lists, int neuron_count)
{
    if (lists == NULL)
        return;
    for (int i = 0; i < neuron_count; i++)
        free(lists[i].list);
    free(lists);
}

static ConnectionList *connection_lists_from_genome(
    const Network *net,
    const StructureGenome *genome)
{
    ConnectionList *lists = calloc((size_t)net->size, sizeof(*lists));
    size_t *counts = calloc((size_t)net->size, sizeof(*counts));
    size_t *filled = calloc((size_t)net->size, sizeof(*filled));
    if (lists == NULL || counts == NULL || filled == NULL)
        goto fail;
    for (size_t i = 0; i < genome->connection_count; i++)
        counts[genome->connections[i].source]++;
    for (int source = 0; source < net->size; source++)
    {
        if (counts[source] > (size_t)INT_MAX)
            goto fail;
        lists[source].count = (int)counts[source];
        if (counts[source] > 0)
        {
            lists[source].list = calloc(counts[source], sizeof(*lists[source].list));
            if (lists[source].list == NULL)
                goto fail;
        }
    }
    for (size_t i = 0; i < genome->connection_count; i++)
    {
        const MiniSNNConnectionGene *gene = &genome->connections[i];
        Connection *connection =
            &lists[gene->source].list[filled[gene->source]++];
        connection->target = (int)gene->target;
        connection->weight = net->neurons[gene->source].type ==
            NEURON_INHIBITORY ? -gene->magnitude : gene->magnitude;
        connection->delay = (int)gene->delay;
    }
    free(counts);
    free(filled);
    return lists;
fail:
    free(counts);
    free(filled);
    connection_lists_destroy(lists, net->size);
    return NULL;
}

static int old_reward_keys(
    const Network *net,
    uint64_t **out_keys,
    size_t *out_count)
{
    size_t count = network_connection_count(net);
    uint64_t *keys = count > 0 ? malloc(count * sizeof(*keys)) : NULL;
    size_t cursor = 0;
    if (count > 0 && keys == NULL)
        return 0;
    for (int source = 0; source < net->size; source++)
    {
        for (int index = 0; index < net->connections[source].count; index++)
        {
            if (!structure_connection_key(
                    (size_t)net->size, (size_t)source,
                    (size_t)net->connections[source].list[index].target,
                    &keys[cursor++]))
            {
                free(keys);
                return 0;
            }
        }
    }
    *out_keys = keys;
    *out_count = count;
    return 1;
}

static int find_key_linear(
    const uint64_t *keys,
    size_t count,
    uint64_t key,
    size_t *out_index)
{
    for (size_t i = 0; i < count; i++)
    {
        if (keys[i] == key)
        {
            *out_index = i;
            return 1;
        }
    }
    return 0;
}

static int commit_network_genome(
    Network *net,
    const StructureGenome *before,
    const StructureGenome *after,
    unsigned long long birth_step)
{
    ConnectionList *new_lists = NULL;
    PlasticityState prepared_plasticity;
    RewardConnectionState *new_reward = NULL;
    uint64_t *reward_keys = NULL;
    size_t reward_key_count = 0;
    MiniSNNStructuralConnectionState *new_structural_states = NULL;
    size_t eligible_count = 0;
    unsigned long long reward_birth_step =
        birth_step > 0U ? birth_step - 1U : 0U;

    memset(&prepared_plasticity, 0, sizeof(prepared_plasticity));
    new_lists = connection_lists_from_genome(net, after);
    if (new_lists == NULL ||
        !plasticity_state_prepare_topology_rebuild(
            net->plasticity, net->neurons, new_lists, &prepared_plasticity) ||
        !old_reward_keys(net, &reward_keys, &reward_key_count))
        goto fail;

    if (after->connection_count > 0)
    {
        new_reward = calloc(after->connection_count, sizeof(*new_reward));
        if (new_reward == NULL)
            goto fail;
        if (net->structural_plasticity != NULL)
        {
            new_structural_states = calloc(
                after->connection_count, sizeof(*new_structural_states));
            if (new_structural_states == NULL)
                goto fail;
        }
    }

    for (size_t i = 0; i < after->connection_count; i++)
    {
        const MiniSNNConnectionGene *gene = &after->connections[i];
        size_t old_index;
        Connection *connection = &new_lists[gene->source].list[0];
        size_t local_index = 0;
        for (size_t j = 0; j < i; j++)
            if (after->connections[j].source == gene->source)
                local_index++;
        connection = &new_lists[gene->source].list[local_index];
        if (find_key_linear(reward_keys, reward_key_count,
                            gene->connection_key, &old_index) &&
            net->reward->connections != NULL &&
            old_index < net->reward->connection_count)
            new_reward[i] = net->reward->connections[old_index];
        else
            new_reward[i].last_update_step = reward_birth_step;
        new_reward[i].eligible = (unsigned char)
            plasticity_connection_is_eligible(
                &prepared_plasticity, net->neurons,
                (int)gene->source, connection);
        if (new_reward[i].eligible)
            eligible_count++;

        if (new_structural_states != NULL)
        {
            size_t old_state_index;
            new_structural_states[i].connection_key = gene->connection_key;
            if (structure_genome_find(before, gene->connection_key,
                                      &old_state_index) &&
                old_state_index <
                    net->structural_plasticity->connection_state_count)
            {
                new_structural_states[i] =
                    net->structural_plasticity->connection_states[old_state_index];
            }
            else
            {
                new_structural_states[i].birth_step = birth_step;
                new_structural_states[i].last_structural_update_step = birth_step;
                new_structural_states[i].growth_origin = 1U;
            }
            new_structural_states[i].connection_key = gene->connection_key;
            if (gene->magnitude > new_structural_states[i].max_absolute_weight)
                new_structural_states[i].max_absolute_weight = gene->magnitude;
        }
    }

    connection_lists_destroy(net->connections, net->size);
    net->connections = new_lists;
    new_lists = NULL;
    plasticity_state_commit_topology_rebuild(
        net->plasticity, &prepared_plasticity);
    free(net->reward->connections);
    net->reward->connections = new_reward;
    net->reward->connection_count = after->connection_count;
    net->reward->stats.eligible_connection_count = eligible_count;
    net->reward->state_valid = 1;
    new_reward = NULL;

    if (net->structural_plasticity != NULL)
    {
        free(net->structural_plasticity->connection_states);
        net->structural_plasticity->connection_states = new_structural_states;
        net->structural_plasticity->connection_state_count =
            after->connection_count;
        net->structural_plasticity->stats.current_connection_count =
            after->connection_count;
        net->structural_plasticity->stats.rebuild_count++;
        new_structural_states = NULL;
    }
    free(reward_keys);
    return 1;
fail:
    connection_lists_destroy(new_lists, net != NULL ? net->size : 0);
    plasticity_state_destroy(&prepared_plasticity);
    free(new_reward);
    free(new_structural_states);
    free(reward_keys);
    return 0;
}

int structure_validate_network_patch(
    const Network *net,
    const MiniSNNTopologyOperation *operations,
    size_t operation_count,
    MiniSNNTopologyPatchResult *result)
{
    StructureGenome current = {0};
    StructureGenome candidate = {0};
    StructureConstraints constraints;
    uint64_t neuron_signature;
    int ok;

    patch_result_reset(result, operation_count);
    if (net == NULL || operations == NULL || operation_count == 0)
        return patch_fail(result, "patch vazio ou rede invalida");
    if (!structure_capture_network_genome(net, &current) ||
        !structure_genome_copy(&candidate, &current))
    {
        structure_genome_destroy(&current);
        return patch_fail(result, "falha ao capturar topologia");
    }
    constraints_from_network(net, &constraints);
    if (!patch_enable_explicit_self_connections(
            operations, operation_count, &constraints))
    {
        structure_genome_destroy(&current);
        structure_genome_destroy(&candidate);
        return patch_fail(result, "autoconexao nao autorizada");
    }
    neuron_signature = structure_neuron_blueprint_signature(
        net->neurons, (size_t)net->size);
    if (result != NULL)
        result->signature_before =
            structure_topology_signature(&current, neuron_signature);
    ok = apply_patch_to_genome(
        &candidate, &constraints, operations, operation_count, result);
    if (ok && result != NULL)
    {
        result->signature_after =
            structure_topology_signature(&candidate, neuron_signature);
        result->success = 1;
        snprintf(result->reason, sizeof(result->reason), "ok");
    }
    structure_genome_destroy(&current);
    structure_genome_destroy(&candidate);
    return ok;
}

int structure_apply_network_patch(
    Network *net,
    const MiniSNNTopologyOperation *operations,
    size_t operation_count,
    MiniSNNTopologyPatchResult *result)
{
    return structure_apply_network_patch_at_step(
        net, operations, operation_count,
        net != NULL ? (unsigned long long)net->step : 0U,
        result);
}

int structure_apply_network_patch_at_step(
    Network *net,
    const MiniSNNTopologyOperation *operations,
    size_t operation_count,
    unsigned long long birth_step,
    MiniSNNTopologyPatchResult *result)
{
    StructureGenome before = {0};
    StructureGenome after = {0};
    StructureConstraints constraints;
    MiniSNNTopologyPatchResult local_result;
    MiniSNNTopologyPatchResult *effective_result =
        result != NULL ? result : &local_result;
    uint64_t neuron_signature;

    patch_result_reset(effective_result, operation_count);
    if (net == NULL || operations == NULL || operation_count == 0 ||
        !structure_capture_network_genome(net, &before) ||
        !structure_genome_copy(&after, &before))
        goto fail;
    constraints_from_network(net, &constraints);
    if (!patch_enable_explicit_self_connections(
            operations, operation_count, &constraints))
        goto fail;
    neuron_signature = structure_neuron_blueprint_signature(
        net->neurons, (size_t)net->size);
    effective_result->signature_before =
        structure_topology_signature(&before, neuron_signature);
    if (!apply_patch_to_genome(
            &after, &constraints, operations, operation_count,
            effective_result) ||
        !commit_network_genome(net, &before, &after, birth_step))
        goto fail;
    effective_result->signature_after =
        structure_topology_signature(&after, neuron_signature);
    effective_result->success = 1;
    snprintf(effective_result->reason, sizeof(effective_result->reason), "ok");
    if (net->structural_plasticity != NULL)
    {
        MiniSNNStructuralStats *stats = &net->structural_plasticity->stats;
        stats->add_success_count += effective_result->connections_added;
        stats->remove_success_count += effective_result->connections_removed;
        stats->rewire_count += effective_result->connections_rewired;
        stats->delay_change_count += effective_result->delays_changed;
        stats->current_topology_signature = effective_result->signature_after;
        if (stats->current_connection_count <
            stats->minimum_connection_count_observed)
            stats->minimum_connection_count_observed =
                stats->current_connection_count;
        if (stats->current_connection_count >
            stats->maximum_connection_count_observed)
            stats->maximum_connection_count_observed =
                stats->current_connection_count;
    }
    structure_genome_destroy(&before);
    structure_genome_destroy(&after);
    return 1;
fail:
    structure_genome_destroy(&before);
    structure_genome_destroy(&after);
    if (effective_result->reason[0] == '\0')
        return patch_fail(result, "patch nao aplicado");
    effective_result->success = 0;
    return 0;
}

int structure_replace_network_genome(
    Network *net,
    const StructureGenome *genome,
    unsigned long long birth_step)
{
    StructureGenome before = {0};
    StructureConstraints constraints;
    int ok;
    if (net == NULL || genome == NULL ||
        !structure_capture_network_genome(net, &before))
        return 0;
    constraints_from_network(net, &constraints);
    ok = structure_genome_validate(genome, &constraints) &&
         commit_network_genome(net, &before, genome, birth_step);
    structure_genome_destroy(&before);
    return ok;
}
