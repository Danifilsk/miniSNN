#ifndef SCENARIO_CONFIG_H
#define SCENARIO_CONFIG_H

#include <stddef.h>

#define SCENARIO_RUN_NAME_MAX 48
#define SCENARIO_TOPOLOGY_MAX 32
#define SCENARIO_DIAGNOSTICS_LEVEL_MAX 8
#define SCENARIO_PLASTICITY_RULE_MAX 32

typedef struct
{
    char run_name[SCENARIO_RUN_NAME_MAX + 1];
    char topology[SCENARIO_TOPOLOGY_MAX];

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
