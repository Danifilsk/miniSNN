#ifndef REWARD_H
#define REWARD_H

#include <stddef.h>

#include "connection.h"
#include "minisnn_types.h"
#include "neuron.h"
#include "plasticity.h"

typedef struct
{
    double eligibility;
    unsigned long long last_update_step;
    double max_absolute_eligibility;
    double reward_signed_change;
    double reward_absolute_change;
    unsigned long long reward_update_count;
    unsigned char eligible;
    unsigned char modified;
} RewardConnectionState;

typedef struct RewardState
{
    MiniSNNRewardConfig config;
    MiniSNNRewardStats stats;
    RewardConnectionState *connections;
    size_t connection_count;
    double pending_raw_reward;
    unsigned int pending_component_count;
    int pending_queued;
    int state_valid;
} RewardState;

MiniSNNRewardConfig reward_default_config(void);

int reward_config_is_valid(const MiniSNNRewardConfig *config);

int reward_state_init(RewardState *state);
void reward_state_destroy(RewardState *state);

int reward_state_configure(
    RewardState *state,
    const MiniSNNRewardConfig *config,
    const Neuron *neurons,
    const ConnectionList *connections,
    PlasticityState *plasticity);

int reward_state_reset(
    RewardState *state,
    const Neuron *neurons,
    const ConnectionList *connections,
    PlasticityState *plasticity);

void reward_state_invalidate(RewardState *state);

int reward_state_ensure(
    RewardState *state,
    const Neuron *neurons,
    const ConnectionList *connections,
    PlasticityState *plasticity);

int reward_state_queue(RewardState *state, double value);
int reward_state_clear_pending(RewardState *state);

int reward_state_accumulate_candidates(
    RewardState *state,
    const PlasticityState *plasticity,
    unsigned long long current_step,
    double dt);

int reward_state_apply_pending(
    RewardState *state,
    const Neuron *neurons,
    ConnectionList *connections,
    PlasticityState *plasticity,
    unsigned long long current_step,
    double dt);

int reward_state_get_stats(
    RewardState *state,
    unsigned long long current_step,
    double dt,
    MiniSNNRewardStats *out_stats);

int reward_state_get_connection_stats(
    RewardState *state,
    size_t connection_id,
    unsigned long long current_step,
    double dt,
    MiniSNNRewardConnectionStats *out_stats);

#endif
