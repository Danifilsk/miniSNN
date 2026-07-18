#ifndef WORKING_MEMORY_H
#define WORKING_MEMORY_H

#include <stddef.h>

#include "scenario_config.h"
#include "scenario_runtime.h"

typedef struct
{
    int trial;
    int cue_pattern;
    int expected_pattern;
    int recalled_pattern;
    double recall_score;
    int recall_correct;
    int control_correct;
    int delay_steps;
    int first_response_step;
    double mean_readout_activity;
    int delay_inputs_zero;
    int probe_inputs_zero;
} WorkingMemoryTrial;

typedef struct
{
    int trial_count;
    int correct_trials;
    double recall_accuracy;
    double mean_recall_score;
    double recall_score_stddev;
    double mean_response_latency;
    double chance_accuracy;
    double control_accuracy;
    double retention_margin;
    WorkingMemoryTrial *trials;
} WorkingMemoryResult;

void working_memory_result_destroy(WorkingMemoryResult *result);

int working_memory_decode_readout(
    const int *readout_spike_counts,
    int readout_count,
    int expected_pattern,
    double *out_score,
    int *out_recalled_pattern,
    double *out_mean_absolute_error);

int working_memory_execute(
    const ScenarioConfig *config,
    const ScenarioBlueprint *blueprint,
    WorkingMemoryResult *out_result,
    char *error_message,
    size_t error_message_size);

int working_memory_write_outputs(
    const ScenarioConfig *config,
    const WorkingMemoryResult *result,
    const char *output_directory,
    char *error_message,
    size_t error_message_size);

#endif
