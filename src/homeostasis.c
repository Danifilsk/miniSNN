#include "homeostasis.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static double clamp_value(double value, double minimum, double maximum)
{
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

MiniSNNHomeostasisConfig homeostasis_default_config(void)
{
    MiniSNNHomeostasisConfig config;

    config.enabled = 0;
    config.intrinsic_enabled = 1;
    config.target_rate = 0.05;
    config.rate_tau = 100.0;
    config.update_interval_steps = 10U;
    config.threshold_eta = 0.05;
    config.threshold_min = -60.0;
    config.threshold_max = -40.0;
    config.synaptic_scaling_enabled = 0;
    config.scaling_eta = 0.10;
    config.scaling_min_factor = 0.50;
    config.scaling_max_factor = 2.0;
    config.scaling_weight_min = 0.0;
    config.scaling_weight_max = 1000.0;
    config.inhibitory_gain_enabled = 0;
    config.inhibitory_gain_initial = 1.0;
    config.inhibitory_gain_eta = 0.05;
    config.inhibitory_gain_min = 0.25;
    config.inhibitory_gain_max = 4.0;
    return config;
}

int homeostasis_config_is_valid(
    const MiniSNNHomeostasisConfig *config,
    double base_threshold)
{
    if (config == NULL || !isfinite(base_threshold) ||
        (config->enabled != 0 && config->enabled != 1) ||
        (config->intrinsic_enabled != 0 && config->intrinsic_enabled != 1) ||
        (config->synaptic_scaling_enabled != 0 &&
         config->synaptic_scaling_enabled != 1) ||
        (config->inhibitory_gain_enabled != 0 &&
         config->inhibitory_gain_enabled != 1))
    {
        return 0;
    }

    if (!isfinite(config->target_rate) || config->target_rate < 0.0 ||
        !isfinite(config->rate_tau) || config->rate_tau <= 0.0 ||
        config->update_interval_steps == 0U ||
        !isfinite(config->threshold_eta) || config->threshold_eta < 0.0 ||
        !isfinite(config->threshold_min) ||
        !isfinite(config->threshold_max) ||
        config->threshold_max <= config->threshold_min ||
        (config->enabled && config->intrinsic_enabled &&
         (base_threshold < config->threshold_min ||
          base_threshold > config->threshold_max)) ||
        !isfinite(config->scaling_eta) || config->scaling_eta < 0.0 ||
        config->scaling_eta > 1.0 ||
        !isfinite(config->scaling_min_factor) ||
        config->scaling_min_factor <= 0.0 ||
        !isfinite(config->scaling_max_factor) ||
        config->scaling_max_factor < config->scaling_min_factor ||
        !isfinite(config->scaling_weight_min) ||
        config->scaling_weight_min < 0.0 ||
        !isfinite(config->scaling_weight_max) ||
        config->scaling_weight_max <= config->scaling_weight_min ||
        !isfinite(config->inhibitory_gain_initial) ||
        config->inhibitory_gain_initial <= 0.0 ||
        !isfinite(config->inhibitory_gain_eta) ||
        config->inhibitory_gain_eta < 0.0 ||
        !isfinite(config->inhibitory_gain_min) ||
        config->inhibitory_gain_min <= 0.0 ||
        !isfinite(config->inhibitory_gain_max) ||
        config->inhibitory_gain_max < config->inhibitory_gain_min ||
        config->inhibitory_gain_initial < config->inhibitory_gain_min ||
        config->inhibitory_gain_initial > config->inhibitory_gain_max)
    {
        return 0;
    }

    if (config->enabled && !config->intrinsic_enabled &&
        !config->synaptic_scaling_enabled &&
        !config->inhibitory_gain_enabled)
    {
        return 0;
    }

    return 1;
}

static void homeostasis_free_arrays(HomeostasisState *state)
{
    free(state->rate_trace);
    free(state->effective_threshold);
    free(state->initial_threshold);
    free(state->initial_incoming_exc_sum);
    free(state->current_incoming_exc_sum);
    free(state->threshold_modified);
    state->rate_trace = NULL;
    state->effective_threshold = NULL;
    state->initial_threshold = NULL;
    state->initial_incoming_exc_sum = NULL;
    state->current_incoming_exc_sum = NULL;
    state->threshold_modified = NULL;
    state->targets_valid = 0;
}

int homeostasis_state_init(HomeostasisState *state, int neuron_count)
{
    if (state == NULL || neuron_count <= 0)
        return 0;

    memset(state, 0, sizeof(*state));
    state->config = homeostasis_default_config();
    state->inhibitory_gain = 1.0;
    state->neuron_count = neuron_count;
    state->stats.final_inhibitory_gain = 1.0;
    return 1;
}

void homeostasis_state_destroy(HomeostasisState *state)
{
    if (state == NULL)
        return;

    homeostasis_free_arrays(state);
    memset(state, 0, sizeof(*state));
}

static int ensure_incoming_index(
    PlasticityState *incoming_index,
    const LIFNeuron *neurons,
    const ConnectionList *connections)
{
    if (incoming_index == NULL)
        return 0;

    return incoming_index->index_valid ||
        plasticity_state_rebuild_index(incoming_index, neurons, connections);
}

static int calculate_incoming_sum(
    const HomeostasisState *state,
    int neuron_id,
    const LIFNeuron *neurons,
    const ConnectionList *connections,
    const PlasticityState *incoming_index,
    double *out_sum)
{
    const PlasticityIncomingList *incoming;
    double sum = 0.0;

    if (state == NULL || neurons == NULL || connections == NULL ||
        incoming_index == NULL || out_sum == NULL || neuron_id < 0 ||
        neuron_id >= state->neuron_count || !incoming_index->index_valid)
    {
        return 0;
    }

    incoming = &incoming_index->incoming[neuron_id];
    for (size_t i = 0; i < incoming->count; i++)
    {
        int source = incoming->refs[i].source;
        int index = incoming->refs[i].outgoing_index;
        const Connection *connection = &connections[source].list[index];

        if (neurons[source].type == NEURON_EXCITATORY &&
            connection->weight > 0.0)
        {
            sum += connection->weight;
        }
    }

    if (!isfinite(sum))
        return 0;

    *out_sum = sum;
    return 1;
}

static int capture_targets(
    HomeostasisState *state,
    const LIFNeuron *neurons,
    const ConnectionList *connections,
    PlasticityState *incoming_index)
{
    if (!ensure_incoming_index(incoming_index, neurons, connections))
        return 0;

    for (int i = 0; i < state->neuron_count; i++)
    {
        double sum;
        if (!calculate_incoming_sum(
                state, i, neurons, connections, incoming_index, &sum))
        {
            return 0;
        }
        state->initial_incoming_exc_sum[i] = sum;
        state->current_incoming_exc_sum[i] = sum;
    }

    state->targets_valid = 1;
    return 1;
}

static void reset_stats(HomeostasisState *state)
{
    memset(&state->stats, 0, sizeof(state->stats));
    state->stats.population_rate_min = HUGE_VAL;
    state->stats.scaling_factor_min = HUGE_VAL;
    state->stats.inhibitory_gain_min_observed = state->inhibitory_gain;
    state->stats.inhibitory_gain_max_observed = state->inhibitory_gain;
    state->stats.final_inhibitory_gain = state->inhibitory_gain;
}

static int allocate_arrays(HomeostasisState *state)
{
    size_t count = (size_t)state->neuron_count;

    state->rate_trace = calloc(count, sizeof(*state->rate_trace));
    state->effective_threshold = malloc(count * sizeof(*state->effective_threshold));
    state->initial_threshold = malloc(count * sizeof(*state->initial_threshold));
    state->initial_incoming_exc_sum = malloc(
        count * sizeof(*state->initial_incoming_exc_sum));
    state->current_incoming_exc_sum = malloc(
        count * sizeof(*state->current_incoming_exc_sum));
    state->threshold_modified = calloc(count, sizeof(*state->threshold_modified));

    return state->rate_trace != NULL &&
        state->effective_threshold != NULL &&
        state->initial_threshold != NULL &&
        state->initial_incoming_exc_sum != NULL &&
        state->current_incoming_exc_sum != NULL &&
        state->threshold_modified != NULL;
}

int homeostasis_state_configure(
    HomeostasisState *state,
    const MiniSNNHomeostasisConfig *config,
    double base_threshold,
    const LIFNeuron *neurons,
    const ConnectionList *connections,
    PlasticityState *incoming_index)
{
    if (state == NULL || !homeostasis_config_is_valid(config, base_threshold))
        return 0;

    homeostasis_free_arrays(state);
    state->config = *config;
    state->inhibitory_gain = config->enabled ?
        config->inhibitory_gain_initial : 1.0;
    reset_stats(state);

    if (!config->enabled)
        return 1;

    if (!allocate_arrays(state))
    {
        homeostasis_free_arrays(state);
        state->config = homeostasis_default_config();
        state->inhibitory_gain = 1.0;
        reset_stats(state);
        return 0;
    }

    for (int i = 0; i < state->neuron_count; i++)
    {
        state->effective_threshold[i] = base_threshold;
        state->initial_threshold[i] = base_threshold;
    }

    if (!capture_targets(state, neurons, connections, incoming_index))
    {
        homeostasis_free_arrays(state);
        state->config = homeostasis_default_config();
        state->inhibitory_gain = 1.0;
        reset_stats(state);
        return 0;
    }

    return 1;
}

int homeostasis_state_reset(
    HomeostasisState *state,
    double base_threshold,
    const LIFNeuron *neurons,
    const ConnectionList *connections,
    PlasticityState *incoming_index)
{
    MiniSNNHomeostasisConfig config;

    if (state == NULL)
        return 0;

    config = state->config;
    return homeostasis_state_configure(
        state, &config, base_threshold, neurons, connections, incoming_index);
}

void homeostasis_state_invalidate_targets(HomeostasisState *state)
{
    if (state != NULL && state->config.enabled)
        state->targets_valid = 0;
}

double homeostasis_rate_trace_next(
    double previous_rate,
    int spike,
    double dt,
    double rate_tau)
{
    double alpha;

    if (!isfinite(previous_rate) || (spike != 0 && spike != 1) ||
        !isfinite(dt) || dt <= 0.0 ||
        !isfinite(rate_tau) || rate_tau <= 0.0)
    {
        return NAN;
    }

    alpha = exp(-dt / rate_tau);
    return alpha * previous_rate +
        (1.0 - alpha) * ((double)spike / dt);
}

double homeostasis_threshold_next(
    double threshold,
    double rate,
    const MiniSNNHomeostasisConfig *config)
{
    double updated;

    if (config == NULL || !isfinite(threshold) || !isfinite(rate))
        return NAN;

    updated = threshold + config->threshold_eta *
        (rate - config->target_rate);
    return clamp_value(updated, config->threshold_min, config->threshold_max);
}

double homeostasis_scaling_factor(
    double target_sum,
    double current_sum,
    const MiniSNNHomeostasisConfig *config)
{
    double desired;
    double applied;

    if (config == NULL || !isfinite(target_sum) || !isfinite(current_sum) ||
        target_sum <= 0.0 || current_sum <= 0.0)
    {
        return NAN;
    }

    desired = target_sum / current_sum;
    applied = 1.0 + config->scaling_eta * (desired - 1.0);
    return clamp_value(
        applied, config->scaling_min_factor, config->scaling_max_factor);
}

double homeostasis_inhibitory_gain_next(
    double gain,
    double population_rate,
    const MiniSNNHomeostasisConfig *config)
{
    double updated;

    if (config == NULL || !isfinite(gain) || !isfinite(population_rate))
        return NAN;

    updated = gain + config->inhibitory_gain_eta *
        (population_rate - config->target_rate);
    return clamp_value(
        updated, config->inhibitory_gain_min, config->inhibitory_gain_max);
}

static int apply_thresholds(HomeostasisState *state)
{
    for (int i = 0; i < state->neuron_count; i++)
    {
        double old_value = state->effective_threshold[i];
        double raw_value = old_value + state->config.threshold_eta *
            (state->rate_trace[i] - state->config.target_rate);
        double new_value = homeostasis_threshold_next(
            old_value, state->rate_trace[i], &state->config);
        double change;

        if (!isfinite(new_value))
            return 0;

        if (raw_value < state->config.threshold_min)
            state->stats.threshold_clamp_min_events++;
        else if (raw_value > state->config.threshold_max)
            state->stats.threshold_clamp_max_events++;

        change = new_value - old_value;
        state->effective_threshold[i] = new_value;
        state->stats.total_threshold_absolute_change += fabs(change);

        if (change > 0.0)
            state->stats.threshold_increase_events++;
        else if (change < 0.0)
            state->stats.threshold_decrease_events++;

        if (change != 0.0 && !state->threshold_modified[i])
        {
            state->threshold_modified[i] = 1U;
            state->stats.threshold_modified_neuron_count++;
        }
    }
    return 1;
}

static int apply_scaling(
    HomeostasisState *state,
    const LIFNeuron *neurons,
    ConnectionList *connections,
    PlasticityState *incoming_index)
{
    for (int target = 0; target < state->neuron_count; target++)
    {
        const PlasticityIncomingList *incoming = &incoming_index->incoming[target];
        double target_sum = state->initial_incoming_exc_sum[target];
        double current_sum;
        double factor;

        if (!calculate_incoming_sum(
                state, target, neurons, connections, incoming_index, &current_sum))
        {
            return 0;
        }
        state->current_incoming_exc_sum[target] = current_sum;

        if (target_sum <= 0.0)
            continue;
        if (current_sum <= 0.0)
        {
            state->stats.scaling_zero_sum_skips++;
            continue;
        }

        factor = homeostasis_scaling_factor(
            target_sum, current_sum, &state->config);
        if (!isfinite(factor))
            return 0;

        state->stats.scaling_events++;
        state->stats.scaling_factor_sum += factor;
        if (state->stats.scaling_factor_count == 0 ||
            factor < state->stats.scaling_factor_min)
        {
            state->stats.scaling_factor_min = factor;
        }
        if (state->stats.scaling_factor_count == 0 ||
            factor > state->stats.scaling_factor_max)
        {
            state->stats.scaling_factor_max = factor;
        }
        state->stats.scaling_factor_count++;

        for (size_t i = 0; i < incoming->count; i++)
        {
            int source = incoming->refs[i].source;
            int index = incoming->refs[i].outgoing_index;
            Connection *connection = &connections[source].list[index];
            double old_weight;
            double raw_weight;
            double new_weight;
            double change;

            if (neurons[source].type != NEURON_EXCITATORY ||
                connection->weight <= 0.0)
            {
                continue;
            }

            old_weight = connection->weight;
            raw_weight = old_weight * factor;
            new_weight = clamp_value(
                raw_weight,
                state->config.scaling_weight_min,
                state->config.scaling_weight_max);

            if (raw_weight < state->config.scaling_weight_min)
                state->stats.scaling_clamp_min_events++;
            else if (raw_weight > state->config.scaling_weight_max)
                state->stats.scaling_clamp_max_events++;

            change = new_weight - old_weight;
            connection->weight = new_weight;
            if (change != 0.0)
            {
                state->stats.scaling_connections_modified++;
                state->stats.total_scaling_signed_change += change;
                state->stats.total_scaling_absolute_change += fabs(change);
            }
        }

        if (!calculate_incoming_sum(
                state, target, neurons, connections, incoming_index,
                &state->current_incoming_exc_sum[target]))
        {
            return 0;
        }
    }
    return 1;
}

static int apply_inhibitory_gain(HomeostasisState *state, double population_rate)
{
    double old_gain = state->inhibitory_gain;
    double raw_gain = old_gain + state->config.inhibitory_gain_eta *
        (population_rate - state->config.target_rate);
    double new_gain = homeostasis_inhibitory_gain_next(
        old_gain, population_rate, &state->config);

    if (!isfinite(new_gain))
        return 0;

    if (raw_gain < state->config.inhibitory_gain_min)
        state->stats.inhibitory_gain_clamp_min_events++;
    else if (raw_gain > state->config.inhibitory_gain_max)
        state->stats.inhibitory_gain_clamp_max_events++;

    if (new_gain > old_gain)
        state->stats.inhibitory_gain_increase_events++;
    else if (new_gain < old_gain)
        state->stats.inhibitory_gain_decrease_events++;

    state->inhibitory_gain = new_gain;
    if (new_gain < state->stats.inhibitory_gain_min_observed)
        state->stats.inhibitory_gain_min_observed = new_gain;
    if (new_gain > state->stats.inhibitory_gain_max_observed)
        state->stats.inhibitory_gain_max_observed = new_gain;
    state->stats.final_inhibitory_gain = new_gain;
    return 1;
}

int homeostasis_apply_step(
    HomeostasisState *state,
    double base_threshold,
    double dt,
    unsigned long long completed_steps,
    const int *spikes,
    const LIFNeuron *neurons,
    ConnectionList *connections,
    PlasticityState *incoming_index)
{
    double population_rate = 0.0;

    if (state == NULL)
        return 0;
    if (!state->config.enabled)
        return 1;
    if (!homeostasis_config_is_valid(&state->config, base_threshold) ||
        spikes == NULL || neurons == NULL || connections == NULL ||
        !isfinite(dt) || dt <= 0.0)
    {
        return 0;
    }

    if (!state->targets_valid &&
        !capture_targets(state, neurons, connections, incoming_index))
    {
        return 0;
    }

    for (int i = 0; i < state->neuron_count; i++)
    {
        state->rate_trace[i] = homeostasis_rate_trace_next(
            state->rate_trace[i], spikes[i], dt, state->config.rate_tau);
        if (!isfinite(state->rate_trace[i]))
            return 0;
        population_rate += state->rate_trace[i];
    }
    population_rate /= (double)state->neuron_count;

    state->stats.final_population_rate = population_rate;
    state->stats.population_rate_sum += population_rate;
    state->stats.rate_error_sum +=
        population_rate - state->config.target_rate;
    state->stats.rate_error_absolute_sum +=
        fabs(population_rate - state->config.target_rate);
    if (state->stats.population_rate_sample_count == 0 ||
        population_rate < state->stats.population_rate_min)
    {
        state->stats.population_rate_min = population_rate;
    }
    if (state->stats.population_rate_sample_count == 0 ||
        population_rate > state->stats.population_rate_max)
    {
        state->stats.population_rate_max = population_rate;
    }
    state->stats.population_rate_sample_count++;

    if (completed_steps % state->config.update_interval_steps != 0U)
        return 1;

    state->stats.update_count++;
    if (state->config.intrinsic_enabled && !apply_thresholds(state))
        return 0;
    if (state->config.synaptic_scaling_enabled &&
        (!ensure_incoming_index(incoming_index, neurons, connections) ||
         !apply_scaling(state, neurons, connections, incoming_index)))
    {
        return 0;
    }
    if (state->config.inhibitory_gain_enabled &&
        !apply_inhibitory_gain(state, population_rate))
    {
        return 0;
    }

    return isfinite(state->stats.total_threshold_absolute_change) &&
        isfinite(state->stats.total_scaling_absolute_change) &&
        isfinite(state->stats.total_scaling_signed_change);
}

double homeostasis_effective_threshold(
    const HomeostasisState *state,
    int neuron_id,
    double base_threshold)
{
    if (state == NULL || !state->config.enabled ||
        state->effective_threshold == NULL || neuron_id < 0 ||
        neuron_id >= state->neuron_count)
    {
        return base_threshold;
    }
    return state->effective_threshold[neuron_id];
}

double homeostasis_transmission_gain(
    const HomeostasisState *state,
    NeuronType source_type)
{
    if (state != NULL && state->config.enabled &&
        state->config.inhibitory_gain_enabled &&
        source_type == NEURON_INHIBITORY)
    {
        return state->inhibitory_gain;
    }
    return 1.0;
}

int homeostasis_current_incoming_sum(
    HomeostasisState *state,
    int neuron_id,
    const LIFNeuron *neurons,
    const ConnectionList *connections,
    PlasticityState *incoming_index,
    double *out_sum)
{
    if (state == NULL || !state->config.enabled || out_sum == NULL ||
        !ensure_incoming_index(incoming_index, neurons, connections) ||
        !calculate_incoming_sum(
            state, neuron_id, neurons, connections, incoming_index, out_sum))
    {
        return 0;
    }
    state->current_incoming_exc_sum[neuron_id] = *out_sum;
    return 1;
}
