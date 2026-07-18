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

    int working_memory_enabled;
    int working_memory_trial_count;
    int working_memory_correct_trials;
    double working_memory_recall_accuracy;
    double working_memory_mean_recall_score;
    double working_memory_recall_score_stddev;
    double working_memory_mean_response_latency;
    double working_memory_chance_accuracy;
    double working_memory_control_accuracy;
    double working_memory_retention_margin;

    int associative_memory_enabled;
    int associative_memory_trial_count;
    int associative_memory_correct_trials;
    double associative_memory_recall_accuracy;
    double associative_memory_mean_pattern_similarity;
    double associative_memory_mean_completion_score;
    double associative_memory_mean_response_latency;
    double associative_memory_chance_accuracy;
    double associative_memory_control_accuracy;
    double associative_memory_association_margin;

    int sequence_prediction_enabled;
    int sequence_prediction_trial_count;
    int sequence_prediction_correct_predictions;
    double sequence_prediction_next_pattern_accuracy;
    double sequence_prediction_mean_similarity;
    double sequence_prediction_mean_error;
    double sequence_prediction_mean_latency;
    double sequence_prediction_chance_accuracy;
    double sequence_prediction_untrained_control_accuracy;
    double sequence_prediction_shuffled_order_control_accuracy;
    double sequence_prediction_context_accuracy;
    double sequence_prediction_last_symbol_only_control_accuracy;
    double sequence_prediction_context_margin;
    double sequence_prediction_control_accuracy;
    double sequence_prediction_margin;

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
