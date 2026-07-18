#ifndef SCENARIO_RUNTIME_H
#define SCENARIO_RUNTIME_H

#include <stddef.h>

#include "minisnn.h"
#include "scenario_config.h"

#define SCENARIO_RUNTIME_MAX_NEURONS 1000

typedef struct
{
    int neuron_count;
    int inhibitory_count;
    MiniSNNNeuronType *neuron_types;
    size_t connection_count;
    MiniSNNConnectionInfo *connections;
    unsigned long long topology_signature;
    MiniSNNNeuronModel neuron_model;
    unsigned long long neuron_model_config_signature;
} ScenarioBlueprint;

typedef struct
{
    int step;
    int spikes_total;
    int spikes_exc;
    int spikes_inh;
    double voltage_sum;
    double synaptic_current_sum;
    double scheduled_reward;
    int reward_component_count;
    int spikes[SCENARIO_RUNTIME_MAX_NEURONS];
    double voltages[SCENARIO_RUNTIME_MAX_NEURONS];
    double synaptic_currents[SCENARIO_RUNTIME_MAX_NEURONS];
    double adex_adaptation[SCENARIO_RUNTIME_MAX_NEURONS];
    double hh_m[SCENARIO_RUNTIME_MAX_NEURONS];
    double hh_h[SCENARIO_RUNTIME_MAX_NEURONS];
    double hh_n[SCENARIO_RUNTIME_MAX_NEURONS];
} ScenarioRuntimeStep;

int scenario_runtime_inhibitory_count(const ScenarioConfig *config);

int scenario_runtime_neuron_is_inhibitory(
    int neuron_id,
    int neuron_count,
    int inhibitory_count);

const char *scenario_runtime_type_name(
    int neuron_id,
    int neuron_count,
    int inhibitory_count);

void scenario_blueprint_destroy(ScenarioBlueprint *blueprint);

int scenario_blueprint_write_checkpoint(
    const ScenarioBlueprint *blueprint,
    const char *filename,
    char *error_message,
    size_t error_message_size);

int scenario_blueprint_load_checkpoint(
    const char *filename,
    ScenarioBlueprint *out_blueprint,
    char *error_message,
    size_t error_message_size);

int scenario_runtime_make_minisnn_config(
    const ScenarioConfig *config,
    MiniSNNConfig *out_config);

int scenario_runtime_capture_network(
    const MiniSNN *snn,
    int inhibitory_count,
    unsigned long long topology_signature,
    ScenarioBlueprint *out_blueprint,
    char *error_message,
    size_t error_message_size);

int scenario_runtime_create_from_blueprint(
    const ScenarioConfig *config,
    const ScenarioBlueprint *blueprint,
    MiniSNN **out_snn,
    char *error_message,
    size_t error_message_size);

int scenario_runtime_configure_modules(
    MiniSNN *snn,
    const ScenarioConfig *config,
    char *error_message,
    size_t error_message_size);

int scenario_runtime_step(
    MiniSNN *snn,
    const ScenarioConfig *config,
    int inhibitory_count,
    int step,
    ScenarioRuntimeStep *out_step,
    char *error_message,
    size_t error_message_size);

int scenario_runtime_step_with_inputs(
    MiniSNN *snn,
    const ScenarioConfig *config,
    int inhibitory_count,
    int step,
    const double *inputs,
    int input_count,
    ScenarioRuntimeStep *out_step,
    char *error_message,
    size_t error_message_size);

#endif
