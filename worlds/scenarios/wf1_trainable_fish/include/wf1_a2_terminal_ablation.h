#ifndef WF1_A2_TERMINAL_ABLATION_H
#define WF1_A2_TERMINAL_ABLATION_H

#include <stdint.h>

#include "wf1_trainable_fish.h"

typedef struct
{
    uint64_t positive_rewards;
    uint64_t training_starvations;
    uint32_t changed_weight_seed_count;
    uint32_t improved_seed_count;
    double untrained_eval_mean_food;
    double trained_eval_mean_food;
    double control_eval_mean_food;
    double untrained_success_rate;
    double trained_success_rate;
    double control_success_rate;
    double first_32_wait_rate;
    double last_32_wait_rate;
    int policy_collapse;
    WF1TrainableFishResult result;
} WF1A2TerminalAblationSummary;

/* The fixed WF1-A.1 bootstrap used unchanged by A.2 and diagnostic A.3. */
void wf1_a2_terminal_ablation_config(WF1TrainableFishConfig *out_config);

int wf1_a2_terminal_ablation_run_experiment(
    const char *output_directory,
    WF1A2TerminalAblationSummary *out_summary);

#endif