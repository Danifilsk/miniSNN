#ifndef WF1_A5_TAU_PROBE_H
#define WF1_A5_TAU_PROBE_H

#include <stdint.h>

typedef enum
{
    WF1_A5_DIAGNOSIS_INCONCLUSIVE = 0,
    WF1_A5_DIAGNOSIS_TEMPORAL_CREDIT_SCOPE_SUPPORTED,
    WF1_A5_DIAGNOSIS_EFFECTIVE_TEMPORAL_CREDIT_REDUCED,
    WF1_A5_DIAGNOSIS_TAU_EFFECT_WEAK_AT_OBSERVED_REWARD
} WF1A5TauProbeDiagnosis;

typedef struct
{
    int pre_reward_equivalent;
    int clamps_zero;
    uint32_t variants_completed;
    WF1A5TauProbeDiagnosis diagnosis;
} WF1A5TauProbeSummary;

/* Runs three independent WF1-A.2 trajectories through the same first +1 EAT
 * reward while holding R-STDP learning_rate at 0.01 and changing only
 * eligibility_tau. This is a diagnostic, not a behavioral training pilot. */
int wf1_a5_tau_probe_run(
    const char *output_directory,
    WF1A5TauProbeSummary *out_summary);

const char *wf1_a5_tau_probe_diagnosis_name(WF1A5TauProbeDiagnosis diagnosis);

#endif