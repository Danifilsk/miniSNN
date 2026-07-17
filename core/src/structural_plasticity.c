#include "structural_plasticity.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network.h"

#define STRUCTURAL_TRACE_TAU 100.0
#define STRUCTURAL_RANDOM_ATTEMPTS 256U

typedef struct
{
    size_t genome_index;
    double usage;
    double absolute_weight;
    unsigned long long age;
} PruneCandidate;

typedef struct
{
    size_t source;
    size_t target;
    uint64_t key;
    double score;
} GrowthCandidate;

static int prune_candidate_compare(const void *left, const void *right)
{
    const PruneCandidate *a = left;
    const PruneCandidate *b = right;
    if (a->usage < b->usage)
        return -1;
    if (a->usage > b->usage)
        return 1;
    if (a->absolute_weight < b->absolute_weight)
        return -1;
    if (a->absolute_weight > b->absolute_weight)
        return 1;
    return a->genome_index < b->genome_index ? -1 :
           a->genome_index > b->genome_index ? 1 : 0;
}

static int growth_candidate_compare(const void *left, const void *right)
{
    const GrowthCandidate *a = left;
    const GrowthCandidate *b = right;
    if (a->score > b->score)
        return -1;
    if (a->score < b->score)
        return 1;
    return a->key < b->key ? -1 : a->key > b->key ? 1 : 0;
}

void structural_prng_seed(StructuralPrng *prng, uint64_t seed)
{
    if (prng == NULL)
        return;
    prng->state = 0U;
    prng->increment = (1442695040888963407ULL << 1U) | 1U;
    (void)structural_prng_next(prng);
    prng->state += seed;
    (void)structural_prng_next(prng);
}

uint32_t structural_prng_next(StructuralPrng *prng)
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

double structural_prng_unit(StructuralPrng *prng)
{
    return (double)structural_prng_next(prng) / 4294967296.0;
}

MiniSNNStructuralPlasticityConfig structural_plasticity_default_config(void)
{
    MiniSNNStructuralPlasticityConfig config;
    memset(&config, 0, sizeof(config));
    config.enabled = 0;
    config.maintenance_interval_steps = 100U;
    config.grace_period_steps = 500U;
    config.pruning_enabled = 1;
    config.prune_weight_threshold = 0.5;
    config.prune_activity_threshold = 0.001;
    config.max_prunes_per_interval = 1U;
    config.growth_enabled = 1;
    config.growth_candidate_count = 16U;
    config.growth_score_threshold = 0.01;
    config.max_growth_per_interval = 1U;
    config.growth_seed = 9001U;
    config.new_exc_weight = 5.0;
    config.new_inh_magnitude = 5.0;
    config.new_delay = 1U;
    config.min_connections = 1U;
    config.max_connections = 64U;
    config.allow_self_connections = 0;
    config.allow_inh_to_inh = 1;
    return config;
}

int structural_plasticity_config_is_valid(
    const MiniSNNStructuralPlasticityConfig *config,
    int neuron_count,
    int max_synaptic_delay,
    size_t current_connection_count)
{
    size_t possible;
    if (config == NULL || neuron_count <= 0 || max_synaptic_delay <= 0 ||
        (config->enabled != 0 && config->enabled != 1))
        return 0;
    if (!config->enabled)
        return 1;
    possible = (size_t)neuron_count * (size_t)neuron_count;
    if (!config->allow_self_connections)
        possible -= (size_t)neuron_count;
    if ((config->pruning_enabled != 0 && config->pruning_enabled != 1) ||
        (config->growth_enabled != 0 && config->growth_enabled != 1) ||
        (config->allow_self_connections != 0 &&
         config->allow_self_connections != 1) ||
        (config->allow_inh_to_inh != 0 && config->allow_inh_to_inh != 1) ||
        (!config->pruning_enabled && !config->growth_enabled) ||
        config->maintenance_interval_steps == 0U ||
        config->grace_period_steps < config->maintenance_interval_steps ||
        !isfinite(config->prune_weight_threshold) ||
        config->prune_weight_threshold < 0.0 ||
        !isfinite(config->prune_activity_threshold) ||
        config->prune_activity_threshold < 0.0 ||
        config->max_prunes_per_interval == 0U ||
        config->growth_candidate_count == 0U ||
        !isfinite(config->growth_score_threshold) ||
        config->growth_score_threshold < 0.0 ||
        config->max_growth_per_interval == 0U ||
        !isfinite(config->new_exc_weight) || config->new_exc_weight < 0.0 ||
        !isfinite(config->new_inh_magnitude) ||
        config->new_inh_magnitude < 0.0 ||
        config->new_delay < 1U ||
        config->new_delay > (unsigned int)max_synaptic_delay ||
        config->min_connections < 1U ||
        config->max_connections < config->min_connections ||
        config->max_connections > possible ||
        current_connection_count < config->min_connections ||
        current_connection_count > config->max_connections)
        return 0;
    return 1;
}

static int initialize_connection_states(
    StructuralPlasticityState *state,
    const StructureGenome *genome,
    unsigned long long birth_step)
{
    MiniSNNStructuralConnectionState *states = NULL;
    if (genome->connection_count > 0)
    {
        states = calloc(genome->connection_count, sizeof(*states));
        if (states == NULL)
            return 0;
    }
    for (size_t i = 0; i < genome->connection_count; i++)
    {
        states[i].connection_key = genome->connections[i].connection_key;
        states[i].birth_step = birth_step;
        states[i].last_structural_update_step = birth_step;
        states[i].max_absolute_weight = genome->connections[i].magnitude;
    }
    free(state->connection_states);
    state->connection_states = states;
    state->connection_state_count = genome->connection_count;
    return 1;
}

int structural_plasticity_state_create(
    StructuralPlasticityState **out_state,
    Network *net,
    const MiniSNNStructuralPlasticityConfig *config)
{
    StructuralPlasticityState *state;
    uint64_t neuron_signature;
    if (out_state == NULL || net == NULL || config == NULL ||
        !structural_plasticity_config_is_valid(
            config, net->size, net->max_synaptic_delay,
            network_connection_count(net)))
        return 0;
    state = calloc(1, sizeof(*state));
    if (state == NULL)
        return 0;
    state->config = *config;
    state->neuron_count = net->size;
    state->rate_traces = calloc((size_t)net->size, sizeof(*state->rate_traces));
    if (state->rate_traces == NULL ||
        !structure_capture_network_genome(net, &state->initial_topology) ||
        !initialize_connection_states(state, &state->initial_topology,
                                      (unsigned long long)net->step))
    {
        structural_plasticity_state_destroy(state);
        return 0;
    }
    structural_prng_seed(&state->prng, config->growth_seed);
    neuron_signature = structure_neuron_blueprint_signature(
        net->neurons, (size_t)net->size, net->model_config.model,
        neuron_model_config_signature(&net->model_config));
    state->stats.initial_connection_count =
        state->initial_topology.connection_count;
    state->stats.current_connection_count =
        state->initial_topology.connection_count;
    state->stats.minimum_connection_count_observed =
        state->initial_topology.connection_count;
    state->stats.maximum_connection_count_observed =
        state->initial_topology.connection_count;
    state->stats.initial_topology_signature = structure_topology_signature(
        &state->initial_topology, neuron_signature);
    state->stats.current_topology_signature =
        state->stats.initial_topology_signature;
    *out_state = state;
    return 1;
}

void structural_plasticity_state_destroy(
    StructuralPlasticityState *state)
{
    if (state == NULL)
        return;
    structure_genome_destroy(&state->initial_topology);
    free(state->connection_states);
    free(state->rate_traces);
    free(state->events);
    free(state);
}

int structural_plasticity_append_event(
    StructuralPlasticityState *state,
    const MiniSNNStructuralEvent *event)
{
    MiniSNNStructuralEvent *updated;
    size_t capacity;
    if (state == NULL || event == NULL)
        return 0;
    if (state->event_count >= state->event_capacity)
    {
        capacity = state->event_capacity == 0U ? 16U :
                   state->event_capacity * 2U;
        if (capacity < state->event_capacity ||
            capacity > SIZE_MAX / sizeof(*updated))
            return 0;
        updated = realloc(state->events, capacity * sizeof(*updated));
        if (updated == NULL)
            return 0;
        state->events = updated;
        state->event_capacity = capacity;
    }
    state->events[state->event_count] = *event;
    state->events[state->event_count].event_index = state->event_count;
    state->event_count++;
    return 1;
}

static int reserve_structural_events(
    StructuralPlasticityState *state,
    size_t additional_count)
{
    MiniSNNStructuralEvent *updated;
    size_t required;
    size_t capacity;
    if (state == NULL || additional_count > SIZE_MAX - state->event_count)
        return 0;
    required = state->event_count + additional_count;
    if (required <= state->event_capacity)
        return 1;
    capacity = state->event_capacity == 0U ? 16U : state->event_capacity;
    while (capacity < required)
    {
        if (capacity > SIZE_MAX / 2U)
            return 0;
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*updated))
        return 0;
    updated = realloc(state->events, capacity * sizeof(*updated));
    if (updated == NULL)
        return 0;
    state->events = updated;
    state->event_capacity = capacity;
    return 1;
}

int structural_plasticity_rebuild_connection_states(
    StructuralPlasticityState *state,
    const StructureGenome *before,
    const StructureGenome *after,
    unsigned long long birth_step)
{
    MiniSNNStructuralConnectionState *states = NULL;
    if (state == NULL || before == NULL || after == NULL)
        return 0;
    if (after->connection_count > 0)
    {
        states = calloc(after->connection_count, sizeof(*states));
        if (states == NULL)
            return 0;
    }
    for (size_t i = 0; i < after->connection_count; i++)
    {
        size_t old_index;
        states[i].connection_key = after->connections[i].connection_key;
        if (structure_genome_find(before, after->connections[i].connection_key,
                                  &old_index) &&
            old_index < state->connection_state_count)
            states[i] = state->connection_states[old_index];
        else
        {
            states[i].connection_key = after->connections[i].connection_key;
            states[i].birth_step = birth_step;
            states[i].last_structural_update_step = birth_step;
            states[i].growth_origin = 1U;
        }
    }
    free(state->connection_states);
    state->connection_states = states;
    state->connection_state_count = after->connection_count;
    return 1;
}

static void update_rate_traces(
    StructuralPlasticityState *state,
    const Network *net)
{
    double dt = neuron_model_dt(&net->model_config);
    double alpha = exp(-dt / STRUCTURAL_TRACE_TAU);
    double scale = (1.0 - alpha) / dt;
    for (int i = 0; i < net->size; i++)
        state->rate_traces[i] = alpha * state->rate_traces[i] +
            scale * (double)net->spikes[i];
}

static int pair_already_candidate(
    const GrowthCandidate *candidates,
    size_t count,
    uint64_t key)
{
    for (size_t i = 0; i < count; i++)
        if (candidates[i].key == key)
            return 1;
    return 0;
}

static size_t collect_prune_candidates(
    StructuralPlasticityState *state,
    const StructureGenome *genome,
    unsigned long long completed_step,
    PruneCandidate *candidates)
{
    size_t count = 0;
    for (size_t i = 0; i < genome->connection_count; i++)
    {
        const MiniSNNConnectionGene *gene = &genome->connections[i];
        MiniSNNStructuralConnectionState *connection_state =
            &state->connection_states[i];
        double usage = gene->magnitude *
            state->rate_traces[gene->source] *
            state->rate_traces[gene->target];
        unsigned long long age = completed_step >= connection_state->birth_step ?
            completed_step - connection_state->birth_step : 0U;
        connection_state->activity_score = usage;
        if (gene->magnitude <= state->config.prune_weight_threshold &&
            usage <= state->config.prune_activity_threshold &&
            age >= state->config.grace_period_steps)
        {
            connection_state->prune_candidate_count++;
            candidates[count].genome_index = i;
            candidates[count].usage = usage;
            candidates[count].absolute_weight = gene->magnitude;
            candidates[count].age = age;
            count++;
        }
    }
    qsort(candidates, count, sizeof(*candidates), prune_candidate_compare);
    return count;
}

static size_t collect_growth_candidates(
    StructuralPlasticityState *state,
    const Network *net,
    const StructureGenome *genome,
    GrowthCandidate *candidates)
{
    StructureConstraints constraints;
    size_t count = 0;
    memset(&constraints, 0, sizeof(constraints));
    constraints.neuron_count = (size_t)net->size;
    constraints.neurons = net->neurons;
    constraints.allow_self_connections = state->config.allow_self_connections;
    constraints.allow_inh_to_inh = state->config.allow_inh_to_inh;
    for (unsigned int attempt = 0;
         attempt < STRUCTURAL_RANDOM_ATTEMPTS &&
         count < state->config.growth_candidate_count;
         attempt++)
    {
        size_t source = (size_t)(
            structural_prng_unit(&state->prng) * (double)net->size);
        size_t target = (size_t)(
            structural_prng_unit(&state->prng) * (double)net->size);
        uint64_t key;
        if (source >= (size_t)net->size)
            source = (size_t)net->size - 1U;
        if (target >= (size_t)net->size)
            target = (size_t)net->size - 1U;
        if (!structure_connection_key((size_t)net->size, source, target, &key) ||
            pair_already_candidate(candidates, count, key) ||
            !structure_is_legal_pair(
                &constraints, genome, source, target, UINT64_MAX))
            continue;
        candidates[count].source = source;
        candidates[count].target = target;
        candidates[count].key = key;
        candidates[count].score =
            state->rate_traces[source] * state->rate_traces[target];
        count++;
    }
    qsort(candidates, count, sizeof(*candidates), growth_candidate_compare);
    return count;
}

static int append_operation_event(
    StructuralPlasticityState *state,
    const MiniSNNTopologyOperation *operation,
    const MiniSNNTopologyPatchResult *result,
    unsigned long long step,
    double activity_score,
    double growth_score,
    unsigned long long age)
{
    MiniSNNStructuralEvent event;
    memset(&event, 0, sizeof(event));
    event.step = step;
    event.type = operation->type;
    event.source = operation->source;
    event.target = operation->target;
    event.new_source = operation->new_source;
    event.new_target = operation->new_target;
    (void)structure_connection_key(
        state->neuron_count, operation->source, operation->target,
        &event.connection_key);
    if (operation->type == MINISNN_TOPOLOGY_REWIRE)
        (void)structure_connection_key(
            state->neuron_count, operation->new_source,
            operation->new_target, &event.new_connection_key);
    event.magnitude = operation->magnitude;
    event.delay = operation->delay;
    event.activity_score = activity_score;
    event.growth_score = growth_score;
    event.age_steps = age;
    event.applied = result->success;
    snprintf(event.reason, sizeof(event.reason), "%s", result->reason);
    event.signature_before = result->signature_before;
    event.signature_after = result->signature_after;
    return structural_plasticity_append_event(state, &event);
}

int structural_plasticity_apply_step(
    StructuralPlasticityState *state,
    Network *net,
    unsigned long long completed_step)
{
    StructureGenome genome = {0};
    StructureGenome virtual_genome = {0};
    PruneCandidate *prune_candidates = NULL;
    GrowthCandidate *growth_candidates = NULL;
    MiniSNNTopologyOperation *operations = NULL;
    double *activity_scores = NULL;
    double *growth_scores = NULL;
    unsigned long long *ages = NULL;
    size_t prune_count = 0;
    size_t growth_count = 0;
    size_t operation_count = 0;
    size_t operation_capacity;
    size_t add_attempts_before;
    size_t remove_attempts_before;
    MiniSNNTopologyPatchResult result;
    int ok = 0;

    if (state == NULL || net == NULL || !state->config.enabled)
        return state != NULL && !state->config.enabled;
    add_attempts_before = state->stats.add_attempt_count;
    remove_attempts_before = state->stats.remove_attempt_count;
    update_rate_traces(state, net);
    if (completed_step < state->config.grace_period_steps ||
        completed_step % state->config.maintenance_interval_steps != 0U)
        return 1;
    state->stats.maintenance_count++;
    if (!structure_capture_network_genome(net, &genome) ||
        !structure_genome_copy(&virtual_genome, &genome))
        goto done;

    operation_capacity = state->config.max_prunes_per_interval +
                         state->config.max_growth_per_interval;
    operations = calloc(operation_capacity, sizeof(*operations));
    activity_scores = calloc(operation_capacity, sizeof(*activity_scores));
    growth_scores = calloc(operation_capacity, sizeof(*growth_scores));
    ages = calloc(operation_capacity, sizeof(*ages));
    prune_candidates = calloc(
        genome.connection_count > 0 ? genome.connection_count : 1U,
        sizeof(*prune_candidates));
    growth_candidates = calloc(
        state->config.growth_candidate_count,
        sizeof(*growth_candidates));
    if (operations == NULL || activity_scores == NULL ||
        growth_scores == NULL || ages == NULL || prune_candidates == NULL ||
        growth_candidates == NULL)
        goto done;

    if (state->config.pruning_enabled)
    {
        prune_count = collect_prune_candidates(
            state, &genome, completed_step, prune_candidates);
        for (size_t i = 0;
             i < prune_count &&
             operation_count < state->config.max_prunes_per_interval &&
             virtual_genome.connection_count > state->config.min_connections;
             i++)
        {
            MiniSNNConnectionGene gene =
                genome.connections[prune_candidates[i].genome_index];
            size_t virtual_index;
            if (!structure_genome_find(
                    &virtual_genome, gene.connection_key, &virtual_index))
                continue;
            operations[operation_count].type = MINISNN_TOPOLOGY_REMOVE;
            operations[operation_count].source = gene.source;
            operations[operation_count].target = gene.target;
            operations[operation_count].magnitude = gene.magnitude;
            operations[operation_count].delay = gene.delay;
            activity_scores[operation_count] = prune_candidates[i].usage;
            ages[operation_count] = prune_candidates[i].age;
            memmove(&virtual_genome.connections[virtual_index],
                    &virtual_genome.connections[virtual_index + 1U],
                    (virtual_genome.connection_count - virtual_index - 1U) *
                        sizeof(*virtual_genome.connections));
            virtual_genome.connection_count--;
            operation_count++;
            state->stats.remove_attempt_count++;
        }
    }

    if (state->config.growth_enabled &&
        virtual_genome.connection_count < state->config.max_connections)
    {
        size_t growth_start = operation_count;
        growth_count = collect_growth_candidates(
            state, net, &virtual_genome, growth_candidates);
        for (size_t i = 0;
             i < growth_count &&
             operation_count - growth_start <
                 state->config.max_growth_per_interval &&
             virtual_genome.connection_count < state->config.max_connections;
             i++)
        {
            MiniSNNConnectionGene gene;
            if (growth_candidates[i].score <
                state->config.growth_score_threshold)
                continue;
            operations[operation_count].type = MINISNN_TOPOLOGY_ADD;
            operations[operation_count].source = growth_candidates[i].source;
            operations[operation_count].target = growth_candidates[i].target;
            operations[operation_count].magnitude =
                net->neurons[growth_candidates[i].source].type ==
                    NEURON_INHIBITORY ?
                    state->config.new_inh_magnitude :
                    state->config.new_exc_weight;
            operations[operation_count].delay = state->config.new_delay;
            operations[operation_count].allow_self_connection =
                state->config.allow_self_connections;
            growth_scores[operation_count] = growth_candidates[i].score;
            gene.connection_key = growth_candidates[i].key;
            gene.source = growth_candidates[i].source;
            gene.target = growth_candidates[i].target;
            gene.magnitude = operations[operation_count].magnitude;
            gene.delay = state->config.new_delay;
            gene.inherited_from = 0U;
            {
                MiniSNNConnectionGene *updated = realloc(
                    virtual_genome.connections,
                    (virtual_genome.connection_count + 1U) * sizeof(*updated));
                if (updated == NULL)
                    goto done;
                virtual_genome.connections = updated;
                virtual_genome.capacity = virtual_genome.connection_count + 1U;
                virtual_genome.connections[virtual_genome.connection_count++] = gene;
                if (!structure_genome_canonicalize(&virtual_genome))
                    goto done;
            }
            operation_count++;
            state->stats.add_attempt_count++;
        }
    }

    if (operation_count == 0)
    {
        ok = 1;
        goto done;
    }
    if (!reserve_structural_events(state, operation_count))
        goto done;
    memset(&result, 0, sizeof(result));
    if (!structure_apply_network_patch_at_step(
            net, operations, operation_count, completed_step, &result))
    {
        state->stats.add_rejected_count +=
            state->stats.add_attempt_count - add_attempts_before;
        state->stats.remove_rejected_count +=
            state->stats.remove_attempt_count - remove_attempts_before;
        goto done;
    }
    for (size_t i = 0; i < operation_count; i++)
    {
        if (!append_operation_event(
                state, &operations[i], &result, completed_step,
                activity_scores[i], growth_scores[i], ages[i]))
            goto done;
        state->stats.cumulative_growth_score += growth_scores[i];
        state->stats.cumulative_pruned_usage += activity_scores[i];
    }
    ok = 1;
done:
    structure_genome_destroy(&genome);
    structure_genome_destroy(&virtual_genome);
    free(prune_candidates);
    free(growth_candidates);
    free(operations);
    free(activity_scores);
    free(growth_scores);
    free(ages);
    return ok;
}

int structural_plasticity_reset(
    StructuralPlasticityState *state,
    Network *net,
    MiniSNNStructuralResetMode mode)
{
    if (state == NULL || net == NULL ||
        (mode != MINISNN_STRUCTURAL_RESET_STATE &&
         mode != MINISNN_STRUCTURAL_RESTORE_INITIAL_TOPOLOGY))
        return 0;
    if (mode == MINISNN_STRUCTURAL_RESTORE_INITIAL_TOPOLOGY &&
        !structure_replace_network_genome(
            net, &state->initial_topology,
            (unsigned long long)net->step))
        return 0;
    memset(state->rate_traces, 0,
           (size_t)state->neuron_count * sizeof(*state->rate_traces));
    structural_prng_seed(&state->prng, state->config.growth_seed);
    free(state->events);
    state->events = NULL;
    state->event_count = 0;
    state->event_capacity = 0;
    {
        MiniSNNStructuralStats stats;
        StructureGenome current = {0};
        uint64_t neuron_signature;
        if (!structure_capture_network_genome(net, &current) ||
            !initialize_connection_states(
                state, &current, (unsigned long long)net->step))
        {
            structure_genome_destroy(&current);
            return 0;
        }
        memset(&stats, 0, sizeof(stats));
        stats.initial_connection_count = state->initial_topology.connection_count;
        stats.current_connection_count = current.connection_count;
        stats.minimum_connection_count_observed = current.connection_count;
        stats.maximum_connection_count_observed = current.connection_count;
        neuron_signature = structure_neuron_blueprint_signature(
            net->neurons, (size_t)net->size, net->model_config.model,
            neuron_model_config_signature(&net->model_config));
        stats.initial_topology_signature = structure_topology_signature(
            &state->initial_topology, neuron_signature);
        stats.current_topology_signature = structure_topology_signature(
            &current, neuron_signature);
        state->stats = stats;
        structure_genome_destroy(&current);
    }
    return 1;
}
