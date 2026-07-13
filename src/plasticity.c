#include "plasticity.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static void plasticity_free_index(PlasticityState *state)
{
    if (state == NULL)
        return;

    if (state->incoming != NULL)
    {
        for (int i = 0; i < state->neuron_count; i++)
            free(state->incoming[i].refs);
    }

    free(state->incoming);
    free(state->source_offsets);
    free(state->deltas);
    free(state->modified);

    state->incoming = NULL;
    state->source_offsets = NULL;
    state->deltas = NULL;
    state->modified = NULL;
    state->connection_count = 0;
    state->index_valid = 0;
}

static void plasticity_reset_runtime(PlasticityState *state)
{
    if (state == NULL)
        return;

    memset(&state->stats, 0, sizeof(state->stats));

    if (state->pre_trace != NULL)
    {
        memset(
            state->pre_trace,
            0,
            (size_t)state->neuron_count * sizeof(*state->pre_trace));
    }

    if (state->post_trace != NULL)
    {
        memset(
            state->post_trace,
            0,
            (size_t)state->neuron_count * sizeof(*state->post_trace));
    }
}

MiniSNNPlasticityConfig plasticity_default_config(void)
{
    MiniSNNPlasticityConfig config;

    config.enabled = 0;
    config.rule = MINISNN_PLASTICITY_STDP_PAIR_TRACE;
    config.a_plus = 1.0;
    config.a_minus = 1.05;
    config.tau_plus = 20.0;
    config.tau_minus = 20.0;
    config.trace_increment = 1.0;
    config.weight_min = 0.0;
    config.weight_max = 200.0;

    return config;
}

int plasticity_config_is_valid(
    const MiniSNNPlasticityConfig *config)
{
    if (config == NULL || (config->enabled != 0 && config->enabled != 1))
        return 0;

    if (config->rule != MINISNN_PLASTICITY_NONE &&
        config->rule != MINISNN_PLASTICITY_STDP_PAIR_TRACE)
    {
        return 0;
    }

    if (config->enabled &&
        config->rule != MINISNN_PLASTICITY_STDP_PAIR_TRACE)
    {
        return 0;
    }

    if (!isfinite(config->a_plus) || config->a_plus < 0.0 ||
        !isfinite(config->a_minus) || config->a_minus < 0.0 ||
        !isfinite(config->tau_plus) || config->tau_plus <= 0.0 ||
        !isfinite(config->tau_minus) || config->tau_minus <= 0.0 ||
        !isfinite(config->trace_increment) || config->trace_increment <= 0.0 ||
        !isfinite(config->weight_min) || config->weight_min < 0.0 ||
        !isfinite(config->weight_max) ||
        config->weight_max <= config->weight_min)
    {
        return 0;
    }

    return 1;
}

int plasticity_state_init(
    PlasticityState *state,
    int neuron_count)
{
    if (state == NULL || neuron_count <= 0)
        return 0;

    memset(state, 0, sizeof(*state));
    state->config = plasticity_default_config();
    state->neuron_count = neuron_count;
    state->pre_trace = calloc((size_t)neuron_count, sizeof(*state->pre_trace));
    state->post_trace = calloc((size_t)neuron_count, sizeof(*state->post_trace));

    if (state->pre_trace == NULL || state->post_trace == NULL)
    {
        plasticity_state_destroy(state);
        return 0;
    }

    return 1;
}

void plasticity_state_destroy(PlasticityState *state)
{
    if (state == NULL)
        return;

    plasticity_free_index(state);
    free(state->pre_trace);
    free(state->post_trace);
    memset(state, 0, sizeof(*state));
}

void plasticity_state_invalidate_index(PlasticityState *state)
{
    if (state != NULL)
        state->index_valid = 0;
}

int plasticity_connection_is_eligible(
    const PlasticityState *state,
    const LIFNeuron *neurons,
    int source,
    const Connection *connection)
{
    if (state == NULL || neurons == NULL || connection == NULL ||
        source < 0 || source >= state->neuron_count)
    {
        return 0;
    }

    return state->config.enabled &&
           state->config.rule == MINISNN_PLASTICITY_STDP_PAIR_TRACE &&
           neurons[source].type == NEURON_EXCITATORY &&
           isfinite(connection->weight) &&
           connection->weight >= 0.0;
}

int plasticity_state_rebuild_index(
    PlasticityState *state,
    const LIFNeuron *neurons,
    const ConnectionList *connections)
{
    PlasticityIncomingList *incoming = NULL;
    size_t *source_offsets = NULL;
    double *deltas = NULL;
    unsigned char *modified = NULL;
    size_t *fill_counts = NULL;
    size_t total_connections = 0;
    size_t eligible_connections = 0;

    if (state == NULL || neurons == NULL || connections == NULL ||
        state->neuron_count <= 0)
    {
        return 0;
    }

    incoming = calloc((size_t)state->neuron_count, sizeof(*incoming));
    source_offsets = calloc(
        (size_t)state->neuron_count + 1U,
        sizeof(*source_offsets));
    fill_counts = calloc((size_t)state->neuron_count, sizeof(*fill_counts));

    if (incoming == NULL || source_offsets == NULL || fill_counts == NULL)
        goto fail;

    for (int source = 0; source < state->neuron_count; source++)
    {
        const ConnectionList *list = &connections[source];

        if (list->count < 0 || (list->count > 0 && list->list == NULL))
            goto fail;

        source_offsets[source] = total_connections;
        total_connections += (size_t)list->count;

        for (int index = 0; index < list->count; index++)
        {
            const Connection *connection = &list->list[index];

            if (connection->target < 0 ||
                connection->target >= state->neuron_count)
            {
                goto fail;
            }

            incoming[connection->target].count++;

            if (plasticity_connection_is_eligible(
                    state,
                    neurons,
                    source,
                    connection))
            {
                eligible_connections++;
            }
        }
    }

    source_offsets[state->neuron_count] = total_connections;

    for (int target = 0; target < state->neuron_count; target++)
    {
        size_t count = incoming[target].count;

        if (count == 0)
            continue;

        incoming[target].refs = malloc(count * sizeof(*incoming[target].refs));
        if (incoming[target].refs == NULL)
            goto fail;
    }

    if (total_connections > 0)
    {
        deltas = calloc(total_connections, sizeof(*deltas));
        modified = calloc(total_connections, sizeof(*modified));

        if (deltas == NULL || modified == NULL)
            goto fail;
    }

    for (int source = 0; source < state->neuron_count; source++)
    {
        const ConnectionList *list = &connections[source];

        for (int index = 0; index < list->count; index++)
        {
            int target = list->list[index].target;
            size_t position = fill_counts[target]++;

            incoming[target].refs[position].source = source;
            incoming[target].refs[position].outgoing_index = index;
        }
    }

    free(fill_counts);
    plasticity_free_index(state);
    state->incoming = incoming;
    state->source_offsets = source_offsets;
    state->deltas = deltas;
    state->modified = modified;
    state->connection_count = total_connections;
    state->stats.eligible_connections = eligible_connections;
    state->index_valid = 1;
    return 1;

fail:
    if (incoming != NULL)
    {
        for (int i = 0; i < state->neuron_count; i++)
            free(incoming[i].refs);
    }
    free(incoming);
    free(source_offsets);
    free(deltas);
    free(modified);
    free(fill_counts);
    return 0;
}

int plasticity_state_configure(
    PlasticityState *state,
    const MiniSNNPlasticityConfig *config,
    const LIFNeuron *neurons,
    const ConnectionList *connections)
{
    if (state == NULL || !plasticity_config_is_valid(config))
        return 0;

    state->config = *config;
    plasticity_reset_runtime(state);
    plasticity_free_index(state);

    if (!config->enabled)
        return 1;

    return plasticity_state_rebuild_index(state, neurons, connections);
}

static int plasticity_decay_traces(PlasticityState *state, double dt)
{
    double pre_decay = exp(-dt / state->config.tau_plus);
    double post_decay = exp(-dt / state->config.tau_minus);

    if (!isfinite(pre_decay) || !isfinite(post_decay))
        return 0;

    for (int i = 0; i < state->neuron_count; i++)
    {
        state->pre_trace[i] *= pre_decay;
        state->post_trace[i] *= post_decay;

        if (!isfinite(state->pre_trace[i]) ||
            !isfinite(state->post_trace[i]))
        {
            return 0;
        }
    }

    return 1;
}

static int plasticity_accumulate_deltas(
    PlasticityState *state,
    const LIFNeuron *neurons,
    const ConnectionList *connections,
    const int *spikes)
{
    for (int source = 0; source < state->neuron_count; source++)
    {
        const ConnectionList *list;

        if (!spikes[source])
            continue;

        list = &connections[source];

        for (int index = 0; index < list->count; index++)
        {
            const Connection *connection = &list->list[index];
            double delta;
            size_t connection_id;

            if (!plasticity_connection_is_eligible(
                    state,
                    neurons,
                    source,
                    connection))
            {
                continue;
            }

            delta = -state->config.a_minus *
                state->post_trace[connection->target];
            connection_id = state->source_offsets[source] + (size_t)index;

            if (!isfinite(delta))
                return 0;

            state->deltas[connection_id] += delta;
            if (delta != 0.0)
                state->stats.depression_events++;
        }
    }

    for (int target = 0; target < state->neuron_count; target++)
    {
        const PlasticityIncomingList *incoming;

        if (!spikes[target])
            continue;

        incoming = &state->incoming[target];

        for (size_t i = 0; i < incoming->count; i++)
        {
            int source = incoming->refs[i].source;
            int outgoing_index = incoming->refs[i].outgoing_index;
            const Connection *connection =
                &connections[source].list[outgoing_index];
            double delta;
            size_t connection_id;

            if (!plasticity_connection_is_eligible(
                    state,
                    neurons,
                    source,
                    connection))
            {
                continue;
            }

            delta = state->config.a_plus * state->pre_trace[source];
            connection_id = state->source_offsets[source] +
                (size_t)outgoing_index;

            if (!isfinite(delta))
                return 0;

            state->deltas[connection_id] += delta;
            if (delta != 0.0)
                state->stats.potentiation_events++;
        }
    }

    return 1;
}

static int plasticity_apply_deltas(
    PlasticityState *state,
    const LIFNeuron *neurons,
    ConnectionList *connections)
{
    for (int source = 0; source < state->neuron_count; source++)
    {
        ConnectionList *list = &connections[source];

        for (int index = 0; index < list->count; index++)
        {
            Connection *connection = &list->list[index];
            size_t connection_id = state->source_offsets[source] +
                (size_t)index;
            double delta = state->deltas[connection_id];
            double old_weight;
            double new_weight;
            double actual_change;

            if (delta == 0.0 ||
                !plasticity_connection_is_eligible(
                    state,
                    neurons,
                    source,
                    connection))
            {
                continue;
            }

            old_weight = connection->weight;
            new_weight = old_weight + delta;

            if (!isfinite(new_weight))
                return 0;

            if (new_weight < state->config.weight_min)
            {
                new_weight = state->config.weight_min;
                state->stats.clamp_min_events++;
            }
            else if (new_weight > state->config.weight_max)
            {
                new_weight = state->config.weight_max;
                state->stats.clamp_max_events++;
            }

            actual_change = new_weight - old_weight;
            connection->weight = new_weight;

            if (actual_change != 0.0)
            {
                double absolute_change = fabs(actual_change);

                if (!state->modified[connection_id])
                {
                    state->modified[connection_id] = 1U;
                    state->stats.modified_connections++;
                }

                state->stats.total_signed_change += actual_change;
                state->stats.total_absolute_change += absolute_change;

                if (absolute_change > state->stats.max_absolute_change)
                    state->stats.max_absolute_change = absolute_change;
            }
        }
    }

    return isfinite(state->stats.total_signed_change) &&
           isfinite(state->stats.total_absolute_change) &&
           isfinite(state->stats.max_absolute_change);
}

static int plasticity_increment_traces(
    PlasticityState *state,
    const int *spikes)
{
    for (int i = 0; i < state->neuron_count; i++)
    {
        if (!spikes[i])
            continue;

        state->pre_trace[i] += state->config.trace_increment;
        state->post_trace[i] += state->config.trace_increment;

        if (!isfinite(state->pre_trace[i]) ||
            !isfinite(state->post_trace[i]))
        {
            return 0;
        }
    }

    return 1;
}

int plasticity_apply_step(
    PlasticityState *state,
    const LIFNeuron *neurons,
    ConnectionList *connections,
    const int *spikes,
    double dt)
{
    if (state == NULL)
        return 0;

    if (!state->config.enabled)
        return 1;

    if (neurons == NULL || connections == NULL || spikes == NULL ||
        !isfinite(dt) || dt <= 0.0 ||
        !plasticity_config_is_valid(&state->config))
    {
        return 0;
    }

    if (!state->index_valid &&
        !plasticity_state_rebuild_index(state, neurons, connections))
    {
        return 0;
    }

    if (state->connection_count > 0)
    {
        memset(
            state->deltas,
            0,
            state->connection_count * sizeof(*state->deltas));
    }

    if (!plasticity_decay_traces(state, dt) ||
        !plasticity_accumulate_deltas(
            state,
            neurons,
            connections,
            spikes) ||
        !plasticity_apply_deltas(state, neurons, connections) ||
        !plasticity_increment_traces(state, spikes))
    {
        return 0;
    }

    return 1;
}
