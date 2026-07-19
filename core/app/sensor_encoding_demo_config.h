#ifndef SENSOR_ENCODING_DEMO_CONFIG_H
#define SENSOR_ENCODING_DEMO_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#include "minisnn.h"

#define SENSOR_ENCODING_DEMO_RUN_NAME_MAX 48U
#define SENSOR_ENCODING_DEMO_MAX_CHANNELS 16U
#define SENSOR_ENCODING_DEMO_MAX_MAPPINGS 64U

typedef struct
{
    char name[MINISNN_AGENT_IO_MAX_CHANNEL_NAME_LENGTH + 1U];
    uint32_t id;
    double minimum;
    double maximum;
    double default_value;
} SensorEncodingDemoSensor;

typedef struct
{
    uint32_t sensor_index;
    MiniSNNSensorEncodingSpec spec;
} SensorEncodingDemoMapping;

typedef struct
{
    char run_name[SENSOR_ENCODING_DEMO_RUN_NAME_MAX + 1U];
    MiniSNNNeuronModel neuron_model;
    uint32_t neuron_count;
    uint32_t brain_steps_per_tick;
    uint32_t sensor_count;
    uint32_t mapping_count;
    SensorEncodingDemoSensor sensors[SENSOR_ENCODING_DEMO_MAX_CHANNELS];
    SensorEncodingDemoMapping mappings[SENSOR_ENCODING_DEMO_MAX_MAPPINGS];
} SensorEncodingDemoConfig;

int sensor_encoding_demo_config_load_file(
    const char *filename,
    SensorEncodingDemoConfig *out_config,
    char *error_message,
    size_t error_message_size);

int sensor_encoding_demo_config_write_file(
    const char *filename,
    const SensorEncodingDemoConfig *config,
    char *error_message,
    size_t error_message_size);

#endif
