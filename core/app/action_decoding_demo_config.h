#ifndef ACTION_DECODING_DEMO_CONFIG_H
#define ACTION_DECODING_DEMO_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#include "minisnn.h"

#define ACTION_DECODING_DEMO_RUN_NAME_MAX 48U
#define ACTION_DECODING_DEMO_MAX_CHANNELS 16U
#define ACTION_DECODING_DEMO_MAX_MAPPINGS 64U

typedef struct
{
    char name[MINISNN_AGENT_IO_MAX_CHANNEL_NAME_LENGTH + 1U];
    uint32_t id;
    double minimum;
    double maximum;
    double default_value;
} ActionDecodingDemoAction;

typedef struct
{
    uint32_t action_index;
    MiniSNNActionDecodingSpec spec;
} ActionDecodingDemoMapping;

typedef struct
{
    char run_name[ACTION_DECODING_DEMO_RUN_NAME_MAX + 1U];
    MiniSNNNeuronModel neuron_model;
    uint32_t neuron_count;
    uint32_t brain_steps_per_tick;
    uint32_t action_count;
    uint32_t mapping_count;
    ActionDecodingDemoAction actions[ACTION_DECODING_DEMO_MAX_CHANNELS];
    ActionDecodingDemoMapping mappings[ACTION_DECODING_DEMO_MAX_MAPPINGS];
} ActionDecodingDemoConfig;

int action_decoding_demo_config_load_file(
    const char *filename,
    ActionDecodingDemoConfig *out_config,
    char *error_message,
    size_t error_message_size);

int action_decoding_demo_config_write_file(
    const char *filename,
    const ActionDecodingDemoConfig *config,
    char *error_message,
    size_t error_message_size);

#endif
