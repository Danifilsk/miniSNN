#include <stdio.h>
#include <string.h>

#include "minisnn.h"

int main(void)
{
    MiniSNNConfig config = minisnn_default_config();
    MiniSNN *network = NULL;
    MiniSNNSensorSchema *sensor_schema = NULL;
    MiniSNNActionSchema *action_schema = NULL;
    MiniSNNAgentIOContext *agent_io = NULL;
    MiniSNNSensorFrame sensor_frame = {0};
    MiniSNNSensorFrame consumed_sensor = {0};
    MiniSNNActionFrame action_frame = {0};
    MiniSNNActionFrame consumed_action = {0};
    MiniSNNAgentIOError error = MINISNN_AGENT_IO_ERROR_NONE;
    const MiniSNNSensorChannelSpec sensor_channel = {1U, "input", 0.0, 1.0, 0.0};
    const MiniSNNActionChannelSpec action_channel = {1U, "action", -1.0, 1.0, 0.0};
    const double sensor_value = 0.5;
    const double action_value = 0.0;
    int status = 1;

    if (strcmp(minisnn_version_string(), MINISNN_VERSION_STRING) != 0)
        goto cleanup;

    config.neuron_count = 2;
    config.neuron_model = MINISNN_NEURON_MODEL_LIF;
    network = minisnn_create_with_config(&config);
    if (network == NULL || !minisnn_set_input(network, 0, 1000.0) ||
        minisnn_step(network) < 0)
    {
        goto cleanup;
    }

    sensor_schema = minisnn_sensor_schema_create(&sensor_channel, 1U, &error);
    action_schema = minisnn_action_schema_create(&action_channel, 1U, &error);
    agent_io = minisnn_agent_io_create(sensor_schema, action_schema, &error);
    if (sensor_schema == NULL || action_schema == NULL || agent_io == NULL ||
        !minisnn_sensor_frame_init(&sensor_frame, 1U) ||
        !minisnn_sensor_frame_init(&consumed_sensor, 1U) ||
        !minisnn_action_frame_init(&action_frame, 1U) ||
        !minisnn_action_frame_init(&consumed_action, 1U) ||
        !minisnn_sensor_frame_set_values(&sensor_frame, 0U, &sensor_value, 1U, &error) ||
        !minisnn_agent_io_submit_sensor_frame(agent_io, &sensor_frame) ||
        !minisnn_agent_io_consume_sensor_frame(agent_io, &consumed_sensor) ||
        !minisnn_action_frame_set_values(&action_frame, 0U, &action_value, 1U, &error) ||
        !minisnn_agent_io_submit_action_and_finish_tick(agent_io, &action_frame) ||
        !minisnn_agent_io_consume_action_frame(agent_io, &consumed_action))
    {
        goto cleanup;
    }

    printf("External miniSNN Core consumer OK: %s action=%.2f\n",
           minisnn_version_string(), consumed_action.values[0]);
    status = 0;

cleanup:
    minisnn_action_frame_destroy(&consumed_action);
    minisnn_action_frame_destroy(&action_frame);
    minisnn_sensor_frame_destroy(&consumed_sensor);
    minisnn_sensor_frame_destroy(&sensor_frame);
    minisnn_agent_io_destroy(&agent_io);
    minisnn_action_schema_destroy(&action_schema);
    minisnn_sensor_schema_destroy(&sensor_schema);
    minisnn_destroy(&network);
    return status;
}
