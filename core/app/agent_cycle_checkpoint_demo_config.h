#ifndef AGENT_CYCLE_CHECKPOINT_DEMO_CONFIG_H
#define AGENT_CYCLE_CHECKPOINT_DEMO_CONFIG_H

#include "agent_cycle_demo_config.h"

/* The checkpoint demo intentionally shares the small, audited C7.4 INI
 * vocabulary. Its runner, not its defaults, is driven by this configuration. */
typedef AgentCycleDemoConfig AgentCycleCheckpointDemoConfig;

int agent_cycle_checkpoint_demo_config_load_file(
    const char *filename,
    AgentCycleCheckpointDemoConfig *out_config,
    char *error_message,
    size_t error_message_size);

int agent_cycle_checkpoint_demo_config_write_file(
    const char *filename,
    const AgentCycleCheckpointDemoConfig *config,
    char *error_message,
    size_t error_message_size);

#endif
