#ifndef SCENARIO_RUNNER_H
#define SCENARIO_RUNNER_H

#include <stddef.h>

#include "scenario_config.h"

#define SCENARIO_OUTPUT_PATH_MAX 512
#define SCENARIO_ACTUAL_RUN_NAME_MAX 128

typedef struct
{
    int inhibitory_count;
    int connection_count;

    int spikes_total;
    int spikes_exc;
    int spikes_inh;

    int first_active_step;
    int last_active_step;
    int active_timesteps;
    int peak_activity_step;
    int peak_activity_value;
    int min_activity_value;

    char output_directory[SCENARIO_OUTPUT_PATH_MAX];
    char actual_run_name[SCENARIO_ACTUAL_RUN_NAME_MAX];
} ScenarioRunResult;

int scenario_runner_execute(
    const ScenarioConfig *config,
    const char *source_config_path,
    ScenarioRunResult *out_result,
    char *error_message,
    size_t error_message_size);

#endif
