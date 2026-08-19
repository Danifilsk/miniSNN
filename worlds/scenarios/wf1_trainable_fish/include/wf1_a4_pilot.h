#ifndef WF1_A4_PILOT_H
#define WF1_A4_PILOT_H

#include <stdint.h>

typedef struct
{
    uint64_t positive_rewards;
    uint64_t clamp_events;
    uint32_t improved_seed_count;
    double untrained_mean_food;
    double trained_mean_food;
    double control_mean_food;
    double untrained_success;
    double trained_success;
    double control_success;
    int improvement;
} WF1A4PilotSummary;

int wf1_a4_pilot_run(const char *output_directory, WF1A4PilotSummary *out_summary);

#endif