#include "reward.h"

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

MiniSNNRewardConfig reward_default_config(void)
{
    MiniSNNRewardConfig config;

    config.enabled = 0;
    config.mode = MINISNN_REWARD_MODE_RSTDP;
    config.learning_rate = 1.0;
    config.eligibility_tau = 100.0;
    config.eligibility_min = -200.0;
    config.eligibility_max = 200.0;
    config.reward_min = -1.0;
    config.reward_max = 1.0;
    config.clip_reward = 1;
    return config;
}

int reward_config_is_valid(const MiniSNNRewardConfig *config)
{
    if (config == NULL || (config->enabled != 0 && config->enabled != 1) ||
        config->mode != MINISNN_REWARD_MODE_RSTDP ||
        !isfinite(config->learning_rate) || config->learning_rate < 0.0 ||
        !isfinite(config->eligibility_tau) || config->eligibility_tau <= 0.0 ||
        !isfinite(config->eligibility_min) || config->eligibility_min >= 0.0 ||
        !isfinite(config->eligibility_max) || config->eligibility_max <= 0.0 ||
        config->eligibility_max <= config->eligibility_min ||
        !isfinite(config->reward_min) || config->reward_min > 0.0 ||
        !isfinite(config->reward_max) || config->reward_max < 0.0 ||
        config->reward_max <= config->reward_min ||
        (config->clip_reward != 0 && config->clip_reward != 1))
    {
        return 0;
    }

    return 1;
}

int reward_state_init(RewardState *state)
{
    if (state == NULL)
        return 0;

    memset(state, 0, sizeof(*state));
    state->config = reward_default_config();
    return 1;
}

void reward_state_destroy(RewardState *state)
{
    if (state == NULL)
        return;

    free(state->connections);
    memset(state, 0, sizeof(*state));
}

void reward_state_invalidate(RewardState *state)
{
    if (state != NULL)
        state->state_valid = 0;
}

static int reward_state_rebuild(
    RewardState *state,
    const LIFNeuron *neurons,
    const ConnectionList *connections,
    PlasticityState *plasticity)
{
    RewardConnectionState *new_connections = NULL;
    size_t connection_count;
    size_t eligible_count = 0;

    if (state == NULL || neurons == NULL || connections == NULL ||
        plasticity == NULL)
    {
        return 0;
    }

    if (!plasticity->index_valid &&
        !plasticity_state_rebuild_index(plasticity, neurons, connections))
    {
        return 0;
    }

    connection_count = plasticity->connection_count;
    if (connection_count > 0)
    {
        new_connections = calloc(connection_count, sizeof(*new_connections));
        if (new_connections == NULL)
            return 0;
    }

    for (int source = 0; source < plasticity->neuron_count; source++)
    {
        const ConnectionList *list = &connections[source];

        for (int index = 0; index < list->count; index++)
        {
            size_t connection_id = plasticity->source_offsets[source] +
                (size_t)index;

            if (plasticity_connection_is_eligible(
                    plasticity,
                    neurons,
                    source,
                    &list->list[index]))
            {
                new_connections[connection_id].eligible = 1U;
                eligible_count++;
            }
        }
    }

    free(state->connections);
    state->connections = new_connections;
    state->connection_count = connection_count;
    state->stats.eligible_connection_count = eligible_count;
    state->state_valid = 1;
    return 1;
}

int reward_state_ensure(
    RewardState *state,
    const LIFNeuron *neurons,
    const ConnectionList *connections,
    PlasticityState *plasticity)
{
    if (state == NULL || !state->config.enabled)
        return 0;

    return state->state_valid ||
        reward_state_rebuild(state, neurons, connections, plasticity);
}

int reward_state_configure(
    RewardState *state,
    const MiniSNNRewardConfig *config,
    const LIFNeuron *neurons,
    const ConnectionList *connections,
    PlasticityState *plasticity)
{
    if (state == NULL || !reward_config_is_valid(config))
        return 0;

    free(state->connections);
    state->connections = NULL;
    state->connection_count = 0;
    state->config = *config;
    memset(&state->stats, 0, sizeof(state->stats));
    state->pending_raw_reward = 0.0;
    state->pending_component_count = 0;
    state->pending_queued = 0;
    state->state_valid = 0;

    if (!config->enabled)
        return 1;

    return reward_state_rebuild(state, neurons, connections, plasticity);
}

int reward_state_reset(
    RewardState *state,
    const LIFNeuron *neurons,
    const ConnectionList *connections,
    PlasticityState *plasticity)
{
    MiniSNNRewardConfig config;

    if (state == NULL)
        return 0;

    config = state->config;
    return reward_state_configure(
        state,
        &config,
        neurons,
        connections,
        plasticity);
}

int reward_state_queue(RewardState *state, double value)
{
    double updated;

    if (state == NULL || !state->config.enabled || !isfinite(value))
        return 0;

    updated = state->pending_raw_reward + value;
    if (!isfinite(updated))
        return 0;

    if (!state->config.clip_reward &&
        (updated < state->config.reward_min ||
         updated > state->config.reward_max))
    {
        return 0;
    }

    state->pending_raw_reward = updated;
    state->pending_component_count++;
    state->pending_queued = 1;
    return 1;
}

int reward_state_clear_pending(RewardState *state)
{
    if (state == NULL)
        return 0;

    state->pending_raw_reward = 0.0;
    state->pending_component_count = 0;
    state->pending_queued = 0;
    return 1;
}

static int reward_decay_connection(
    RewardState *state,
    RewardConnectionState *connection,
    unsigned long long current_step,
    double dt)
{
    unsigned long long elapsed_steps;
    double decay;

    if (state == NULL || connection == NULL || !isfinite(dt) || dt <= 0.0 ||
        current_step < connection->last_update_step)
    {
        return 0;
    }

    elapsed_steps = current_step - connection->last_update_step;
    if (elapsed_steps == 0)
        return 1;

    decay = exp(
        -((double)elapsed_steps * dt) / state->config.eligibility_tau);
    if (!isfinite(decay))
        return 0;

    connection->eligibility *= decay;
    connection->last_update_step = current_step;
    return isfinite(connection->eligibility);
}

static void reward_track_eligibility(
    RewardState *state,
    RewardConnectionState *connection)
{
    double absolute = fabs(connection->eligibility);

    if (absolute > connection->max_absolute_eligibility)
        connection->max_absolute_eligibility = absolute;
    if (absolute > state->stats.eligibility_max_absolute_observed)
        state->stats.eligibility_max_absolute_observed = absolute;
}

int reward_state_accumulate_candidates(
    RewardState *state,
    const PlasticityState *plasticity,
    unsigned long long current_step,
    double dt)
{
    if (state == NULL || plasticity == NULL || !state->config.enabled ||
        !state->state_valid ||
        plasticity->config.learning_mode !=
            MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP)
    {
        return 0;
    }

    for (size_t i = 0; i < plasticity->candidate_count; i++)
    {
        size_t connection_id = plasticity->candidate_ids[i];
        RewardConnectionState *connection;
        double candidate;
        double updated;

        if (connection_id >= state->connection_count)
            return 0;

        connection = &state->connections[connection_id];
        if (!connection->eligible)
            continue;

        if (!reward_decay_connection(
                state, connection, current_step, dt))
        {
            return 0;
        }

        candidate = plasticity->deltas[connection_id];
        updated = connection->eligibility + candidate;
        if (!isfinite(updated))
            return 0;

        if (candidate > 0.0)
            state->stats.eligibility_potentiation_events++;
        else if (candidate < 0.0)
            state->stats.eligibility_depression_events++;

        if (updated < state->config.eligibility_min)
        {
            updated = state->config.eligibility_min;
            state->stats.eligibility_clamp_min_events++;
        }
        else if (updated > state->config.eligibility_max)
        {
            updated = state->config.eligibility_max;
            state->stats.eligibility_clamp_max_events++;
        }

        connection->eligibility = updated;
        reward_track_eligibility(state, connection);
    }

    return 1;
}

static void reward_record_signal(
    RewardState *state,
    double raw_reward,
    double applied_reward)
{
    state->stats.reward_event_count++;
    if (applied_reward > 0.0)
    {
        state->stats.positive_reward_event_count++;
        state->stats.cumulative_positive_reward += applied_reward;
    }
    else if (applied_reward < 0.0)
    {
        state->stats.negative_reward_event_count++;
        state->stats.cumulative_negative_reward += applied_reward;
    }
    else
    {
        state->stats.zero_reward_event_count++;
    }

    state->stats.cumulative_raw_reward += raw_reward;
    state->stats.cumulative_applied_reward += applied_reward;
    state->stats.cumulative_absolute_reward += fabs(applied_reward);
    state->stats.last_raw_reward = raw_reward;
    state->stats.last_applied_reward = applied_reward;
    state->stats.last_reward_component_count =
        state->pending_component_count;
}

int reward_state_apply_pending(
    RewardState *state,
    const LIFNeuron *neurons,
    ConnectionList *connections,
    PlasticityState *plasticity,
    unsigned long long current_step,
    double dt)
{
    double raw_reward;
    double applied_reward;
    unsigned long long clamp_min_before;
    unsigned long long clamp_max_before;

    if (state == NULL || neurons == NULL || connections == NULL ||
        plasticity == NULL || !state->config.enabled || !state->state_valid ||
        !isfinite(dt) || dt <= 0.0)
    {
        return 0;
    }

    if (!state->pending_queued)
        return 1;

    raw_reward = state->pending_raw_reward;
    applied_reward = state->config.clip_reward ?
        clamp_value(
            raw_reward,
            state->config.reward_min,
            state->config.reward_max) :
        raw_reward;

    state->stats.last_active_eligibility_count = 0;
    state->stats.last_modified_connection_count = 0;
    state->stats.last_weight_signed_change = 0.0;
    state->stats.last_weight_absolute_change = 0.0;
    clamp_min_before = state->stats.weight_clamp_min_events;
    clamp_max_before = state->stats.weight_clamp_max_events;

    for (int source = 0; source < plasticity->neuron_count; source++)
    {
        ConnectionList *list = &connections[source];

        for (int index = 0; index < list->count; index++)
        {
            size_t connection_id = plasticity->source_offsets[source] +
                (size_t)index;
            RewardConnectionState *reward_connection;
            Connection *connection = &list->list[index];
            double requested_change;
            double old_weight;
            double new_weight;
            double actual_change;

            if (connection_id >= state->connection_count)
                return 0;

            reward_connection = &state->connections[connection_id];
            if (!reward_connection->eligible)
                continue;

            if (!reward_decay_connection(
                    state,
                    reward_connection,
                    current_step,
                    dt))
            {
                return 0;
            }

            if (reward_connection->eligibility != 0.0)
                state->stats.last_active_eligibility_count++;

            requested_change = state->config.learning_rate *
                applied_reward * reward_connection->eligibility;
            if (!isfinite(requested_change))
                return 0;

            old_weight = connection->weight;
            new_weight = old_weight + requested_change;
            if (!isfinite(new_weight))
                return 0;

            if (new_weight < plasticity->config.weight_min)
            {
                new_weight = plasticity->config.weight_min;
                state->stats.weight_clamp_min_events++;
            }
            else if (new_weight > plasticity->config.weight_max)
            {
                new_weight = plasticity->config.weight_max;
                state->stats.weight_clamp_max_events++;
            }

            actual_change = new_weight - old_weight;
            connection->weight = new_weight;

            if (actual_change == 0.0)
                continue;

            if (actual_change > 0.0)
                state->stats.reward_potentiation_events++;
            else
                state->stats.reward_depression_events++;

            if (!reward_connection->modified)
            {
                reward_connection->modified = 1U;
                state->stats.modified_connection_count++;
            }

            reward_connection->reward_update_count++;
            reward_connection->reward_signed_change += actual_change;
            reward_connection->reward_absolute_change += fabs(actual_change);
            state->stats.last_modified_connection_count++;
            state->stats.last_weight_signed_change += actual_change;
            state->stats.last_weight_absolute_change += fabs(actual_change);
            state->stats.total_signed_weight_change += actual_change;
            state->stats.total_absolute_weight_change += fabs(actual_change);

            if (fabs(actual_change) > state->stats.max_absolute_weight_change)
                state->stats.max_absolute_weight_change = fabs(actual_change);
        }
    }

    state->stats.last_weight_clamp_min_count =
        state->stats.weight_clamp_min_events - clamp_min_before;
    state->stats.last_weight_clamp_max_count =
        state->stats.weight_clamp_max_events - clamp_max_before;
    reward_record_signal(state, raw_reward, applied_reward);
    return reward_state_clear_pending(state);
}

static int reward_refresh_eligibility_summary(
    RewardState *state,
    unsigned long long current_step,
    double dt)
{
    double sum = 0.0;
    double absolute_sum = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
    size_t count = 0;

    if (state == NULL || !state->config.enabled || !state->state_valid)
        return 0;

    for (size_t i = 0; i < state->connection_count; i++)
    {
        RewardConnectionState *connection = &state->connections[i];

        if (!connection->eligible)
            continue;

        if (!reward_decay_connection(state, connection, current_step, dt))
            return 0;

        if (count == 0 || connection->eligibility < minimum)
            minimum = connection->eligibility;
        if (count == 0 || connection->eligibility > maximum)
            maximum = connection->eligibility;

        sum += connection->eligibility;
        absolute_sum += fabs(connection->eligibility);
        count++;
    }

    state->stats.eligibility_final_mean = count > 0 ? sum / (double)count : 0.0;
    state->stats.eligibility_final_min = minimum;
    state->stats.eligibility_final_max = maximum;
    state->stats.eligibility_final_mean_absolute =
        count > 0 ? absolute_sum / (double)count : 0.0;
    return isfinite(state->stats.eligibility_final_mean) &&
        isfinite(state->stats.eligibility_final_mean_absolute);
}

int reward_state_get_stats(
    RewardState *state,
    unsigned long long current_step,
    double dt,
    MiniSNNRewardStats *out_stats)
{
    if (state == NULL || out_stats == NULL)
        return 0;

    if (state->config.enabled &&
        !reward_refresh_eligibility_summary(state, current_step, dt))
    {
        return 0;
    }

    *out_stats = state->stats;
    return 1;
}

int reward_state_get_connection_stats(
    RewardState *state,
    size_t connection_id,
    unsigned long long current_step,
    double dt,
    MiniSNNRewardConnectionStats *out_stats)
{
    RewardConnectionState *connection;

    if (state == NULL || out_stats == NULL || !state->config.enabled ||
        !state->state_valid || connection_id >= state->connection_count)
    {
        return 0;
    }

    connection = &state->connections[connection_id];
    if (connection->eligible &&
        !reward_decay_connection(state, connection, current_step, dt))
    {
        return 0;
    }

    out_stats->eligible = connection->eligible != 0;
    out_stats->eligibility = connection->eligibility;
    out_stats->max_absolute_eligibility =
        connection->max_absolute_eligibility;
    out_stats->reward_update_count = connection->reward_update_count;
    out_stats->reward_signed_change = connection->reward_signed_change;
    out_stats->reward_absolute_change = connection->reward_absolute_change;
    return 1;
}
