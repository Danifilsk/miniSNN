#ifndef SCENARIO_CONFIG_H
#define SCENARIO_CONFIG_H

#include <stddef.h>
#include <stdint.h>
#include "minisnn_types.h"

#define SCENARIO_RUN_NAME_MAX 48
#define SCENARIO_TOPOLOGY_MAX 32
#define SCENARIO_NEURON_MODEL_MAX 16
#define SCENARIO_DIAGNOSTICS_LEVEL_MAX 8
#define SCENARIO_PLASTICITY_RULE_MAX 32
#define SCENARIO_LEARNING_MODE_MAX 32
#define SCENARIO_SCALING_TARGET_MODE_MAX 32
#define SCENARIO_REWARD_MODE_MAX 16
#define SCENARIO_MAX_REWARD_EVENTS 256
#define SCENARIO_WORKING_MEMORY_PATTERN_MAX 24

typedef struct
{
    int index;
    int step;
    double value;
} ScenarioRewardEvent;

typedef struct
{
    char run_name[SCENARIO_RUN_NAME_MAX + 1];
    char topology[SCENARIO_TOPOLOGY_MAX];
    MiniSNNNeuronModel neuron_model;

    int neurons;
    double inhibitory_fraction;
    double connection_probability;
    unsigned int seed;
    int delay;
    int max_synaptic_delay;

    int allow_self_connections;
    int allow_inh_to_inh;

    double excitatory_weight;
    double inhibitory_weight;

    int source_count;
    double input_current;

    int steps;
    double dt;
    double tau;
    double v_rest;
    double v_reset;
    double v_threshold;
    double resistance;
    double synaptic_decay;

    MiniSNNAdExConfig adex;
    MiniSNNHodgkinHuxleyConfig hodgkin_huxley;

    int small_world_neighbors;
    double small_world_rewire_probability;
    int feedforward_layers;

    int record_neuron;

    int auto_unique_run;
    int history_enabled;

    char diagnostics_level[SCENARIO_DIAGNOSTICS_LEVEL_MAX];
    int time_bin_steps;
    double burst_z_threshold;
    int min_burst_steps;
    int isi_min_spikes;
    int correlation_sample_size;
    int neuron_sample_limit;
    int sample_stride;

    int plasticity_enabled;
    char plasticity_rule[SCENARIO_PLASTICITY_RULE_MAX];
    char plasticity_learning_mode[SCENARIO_LEARNING_MODE_MAX];
    double plasticity_a_plus;
    double plasticity_a_minus;
    double plasticity_tau_plus;
    double plasticity_tau_minus;
    double plasticity_trace_increment;
    double plasticity_weight_min;
    double plasticity_weight_max;
    int plasticity_record_weights;
    int plasticity_record_history;
    int plasticity_record_interval_steps;
    int plasticity_record_connection_limit;

    int homeostasis_enabled;
    int homeostasis_intrinsic_enabled;
    double homeostasis_target_rate;
    double homeostasis_rate_tau;
    int homeostasis_update_interval_steps;
    double homeostasis_threshold_eta;
    double homeostasis_threshold_min;
    double homeostasis_threshold_max;
    int homeostasis_synaptic_scaling_enabled;
    char homeostasis_scaling_target_mode[SCENARIO_SCALING_TARGET_MODE_MAX];
    double homeostasis_scaling_eta;
    double homeostasis_scaling_min_factor;
    double homeostasis_scaling_max_factor;
    double homeostasis_scaling_weight_min;
    double homeostasis_scaling_weight_max;
    int homeostasis_inhibitory_gain_enabled;
    double homeostasis_inhibitory_gain_initial;
    double homeostasis_inhibitory_gain_eta;
    double homeostasis_inhibitory_gain_min;
    double homeostasis_inhibitory_gain_max;
    int homeostasis_record_history;
    int homeostasis_record_interval_steps;
    int homeostasis_record_neuron_limit;

    int structural_plasticity_enabled;
    int structural_maintenance_interval_steps;
    int structural_grace_period_steps;
    int structural_pruning_enabled;
    double structural_prune_weight_threshold;
    double structural_prune_activity_threshold;
    int structural_max_prunes_per_interval;
    int structural_growth_enabled;
    int structural_growth_candidate_count;
    double structural_growth_score_threshold;
    int structural_max_growth_per_interval;
    uint64_t structural_growth_seed;
    double structural_new_exc_weight;
    double structural_new_inh_magnitude;
    int structural_new_delay;
    int structural_min_connections;
    int structural_max_connections;
    int structural_record_history;
    int structural_record_interval_steps;

    int reward_enabled;
    char reward_mode[SCENARIO_REWARD_MODE_MAX];
    double reward_learning_rate;
    double reward_eligibility_tau;
    double reward_eligibility_min;
    double reward_eligibility_max;
    double reward_min;
    double reward_max;
    int reward_clip;
    int reward_record_history;
    int reward_record_interval_steps;
    int reward_record_connection_limit;
    int reward_event_count;
    ScenarioRewardEvent reward_events[SCENARIO_MAX_REWARD_EVENTS];

    int working_memory_enabled;
    int working_memory_trials;
    int working_memory_cue_steps;
    int working_memory_delay_steps;
    int working_memory_probe_steps;
    char working_memory_cue_pattern[SCENARIO_WORKING_MEMORY_PATTERN_MAX];
    int working_memory_cue_start;
    int working_memory_cue_group_size;
    int working_memory_readout_start;
    int working_memory_readout_count;
    int working_memory_readout_group_size;
    unsigned int working_memory_seed;
    int working_memory_reset_between_trials;
    double working_memory_recall_tolerance;
    double working_memory_recall_threshold;
} ScenarioConfig;

void scenario_config_default(ScenarioConfig *config);

int scenario_config_load_file(
    const char *filename,
    ScenarioConfig *out_config,
    char *error_message,
    size_t error_message_size);

int scenario_config_validate(
    const ScenarioConfig *config,
    char *error_message,
    size_t error_message_size);

int scenario_config_save_file(
    const char *filename,
    const ScenarioConfig *config,
    char *error_message,
    size_t error_message_size);

#endif
