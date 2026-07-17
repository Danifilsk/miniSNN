#ifndef SCENARIO_RUNNER_H
#define SCENARIO_RUNNER_H

#include <stddef.h>

#include "scenario_config.h"
#include "scenario_runtime.h"

#define SCENARIO_OUTPUT_PATH_MAX 512
#define SCENARIO_ACTUAL_RUN_NAME_MAX 128

typedef struct
{
    int inhibitory_count;
    int connection_count;
    int self_connection_count;
    int excitatory_to_excitatory_count;
    int excitatory_to_inhibitory_count;
    int inhibitory_to_excitatory_count;
    int inhibitory_to_inhibitory_count;
    unsigned long long topology_signature;

    int spikes_total;
    int spikes_exc;
    int spikes_inh;

    int first_active_step;
    int last_active_step;
    int active_timesteps;
    int peak_activity_step;
    int peak_activity_value;
    int min_activity_value;

    int state_nonfinite_count;
    int step_error_count;
    double voltage_min;
    double voltage_max;
    double voltage_mean;
    double firing_rate;
    double mean_isi;
    int mean_isi_available;

    double adex_w_initial;
    double adex_w_final;
    double adex_w_mean;
    double adex_adaptation_index;

    double hh_m_mean;
    double hh_h_mean;
    double hh_n_mean;
    double hh_peak_voltage;
    int hh_action_potential_count;

    char output_directory[SCENARIO_OUTPUT_PATH_MAX];
    char actual_run_name[SCENARIO_ACTUAL_RUN_NAME_MAX];
} ScenarioRunResult;

int scenario_runner_execute(
    const ScenarioConfig *config,
    const char *source_config_path,
    ScenarioRunResult *out_result,
    char *error_message,
    size_t error_message_size);

int scenario_runner_capture_blueprint(
    const ScenarioConfig *config,
    ScenarioBlueprint *out_blueprint,
    char *error_message,
    size_t error_message_size);

#endif
