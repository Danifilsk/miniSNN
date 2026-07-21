#ifndef AGENT_CYCLE_DEMO_CONFIG_H
#define AGENT_CYCLE_DEMO_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#include "minisnn.h"

#define AGENT_CYCLE_DEMO_RUN_NAME_MAX 48U

typedef struct
{
    char run_name[AGENT_CYCLE_DEMO_RUN_NAME_MAX + 1U];
    MiniSNNNeuronModel neuron_model;
    uint32_t neuron_count;
    uint32_t brain_steps_per_tick;
    uint32_t ticks;
    uint32_t reset_interval;
    double input_current;
} AgentCycleDemoConfig;

int agent_cycle_demo_config_load_file(
    const char *filename,
    AgentCycleDemoConfig *out_config,
    char *error_message,
    size_t error_message_size);

int agent_cycle_demo_config_write_file(
    const char *filename,
    const AgentCycleDemoConfig *config,
    char *error_message,
    size_t error_message_size);

#endif
