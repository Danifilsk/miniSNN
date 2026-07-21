#include "agent_cycle_checkpoint_demo_config.h"

int agent_cycle_checkpoint_demo_config_load_file(
    const char *filename, AgentCycleCheckpointDemoConfig *out_config,
    char *error_message, size_t error_message_size)
{
    return agent_cycle_demo_config_load_file(filename, out_config, error_message,
                                             error_message_size);
}

int agent_cycle_checkpoint_demo_config_write_file(
    const char *filename, const AgentCycleCheckpointDemoConfig *config,
    char *error_message, size_t error_message_size)
{
    return agent_cycle_demo_config_write_file(filename, config, error_message,
                                              error_message_size);
}
