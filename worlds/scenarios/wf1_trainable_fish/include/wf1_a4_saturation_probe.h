#ifndef WF1_A4_SATURATION_PROBE_H
#define WF1_A4_SATURATION_PROBE_H

#include <stdint.h>

typedef struct
{
    int pre_reward_equivalent;
    int saturation_supported;
    int global_credit_remains_suspect;
    uint32_t variants_completed;
} WF1A4SaturationProbeSummary;

/* Runs three independent WF1-A.2 trajectories through the same first +1 EAT
 * reward while changing only the R-STDP learning_rate. */
int wf1_a4_saturation_probe_run(
    const char *output_directory,
    WF1A4SaturationProbeSummary *out_summary);

#endif