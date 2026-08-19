#ifndef WF1_A3_REWARD_PROBE_H
#define WF1_A3_REWARD_PROBE_H

#include <stdint.h>

typedef struct
{
    uint32_t rewards_observed;
    uint32_t episodes_run;
    uint32_t weight_changed_reward_count;
    uint64_t clamp_min_events;
    uint64_t clamp_max_events;
    double mean_active_eligibility_count;
    double mean_modified_connection_count;
    double mean_weight_absolute_change;
    double max_weight_absolute_change_observed;
    double eligibility_mean_absolute;
    double eligibility_max_absolute_observed;
} WF1A3RewardProbeSummary;

/* Runs the fixed WF1-A.2 configuration until two real positive EAT rewards
 * have been delivered through the ordinary AgentCycle path. */
int wf1_a3_reward_probe_run(
    const char *output_directory,
    WF1A3RewardProbeSummary *out_summary);

#endif