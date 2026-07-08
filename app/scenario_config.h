#ifndef SCENARIO_CONFIG_H
#define SCENARIO_CONFIG_H

#include <stddef.h>

#define SCENARIO_RUN_NAME_MAX 48
#define SCENARIO_TOPOLOGY_MAX 32

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
