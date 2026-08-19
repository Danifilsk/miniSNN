#ifndef WF1_A1_EXPLORATION_H
#define WF1_A1_EXPLORATION_H

#include <stdint.h>

#include "wf1_trainable_fish.h"

typedef struct
{
    char calibration_config[64];
    uint64_t original_decisions;
    uint64_t original_waits;
    uint64_t original_moves;
    uint64_t original_eats;
    uint64_t original_total_spikes;
    uint64_t bootstrap_decisions;
    uint64_t bootstrap_waits;
    uint64_t bootstrap_moves;
    uint64_t bootstrap_eats;
    uint64_t bootstrap_total_spikes;
    uint64_t training_successful_eats;
    uint64_t positive_rewards;
    uint32_t improved_seed_count;
    uint32_t changed_weight_seed_count;
    int exploration_bootstrapped;
    int positive_reward_reached;
    WF1TrainableFishResult result;
} WF1A1ExplorationSummary;

int wf1_a1_exploration_run_experiment(
    const char *output_directory,
    WF1A1ExplorationSummary *out_summary);

#endif