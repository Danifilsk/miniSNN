#ifndef K0_SCENARIO_ENGINE_H
#define K0_SCENARIO_ENGINE_H

#include <stddef.h>
#include <stdint.h>

#include "k0_scenario_config.h"

typedef struct
{
    uint64_t tick;
    uint64_t state_hash;
    uint64_t alive_entities;
    uint64_t pending_commands;
    uint64_t last_tick_events;
    uint64_t total_entities_created;
    uint64_t total_entities_destroyed;
    uint64_t total_commands_submitted;
    uint64_t total_commands_applied;
    uint64_t total_commands_rejected;
    uint64_t total_events_emitted;
    uint64_t random_streams;
    uint64_t total_random_u32_generated;
    uint32_t state_hash_version;
    uint32_t prng_version;
} K0ScenarioTraceRow;

typedef struct
{
    uint64_t event_id;
    uint64_t tick;
    uint32_t event_type;
    uint64_t command_id;
    uint64_t issuer_entity_id;
    uint64_t subject_entity_id;
    uint32_t rejection_reason;
} K0ScenarioEventRow;

typedef struct
{
    K0ScenarioConfig config;
    uint64_t scenario_config_signature;
    uint64_t initial_state_hash;
    uint64_t final_state_hash;
    K0ScenarioTraceRow *trace_rows;
    size_t trace_row_count;
    K0ScenarioEventRow *event_rows;
    size_t event_row_count;
    uint64_t ticks_completed;
    uint64_t final_alive_entities;
    uint64_t final_pending_commands;
    uint64_t total_entities_created;
    uint64_t total_entities_destroyed;
    uint64_t total_commands_submitted;
    uint64_t total_commands_applied;
    uint64_t total_commands_rejected;
    uint64_t total_events_emitted;
    uint64_t random_streams;
    uint64_t total_random_u32_generated;
    uint32_t state_hash_version;
    uint32_t prng_version;
} K0ScenarioRunResult;

void k0_scenario_run_result_destroy(K0ScenarioRunResult *result);

int k0_scenario_execute(
    const K0ScenarioConfig *config,
    K0ScenarioRunResult *out_result,
    char *error_message,
    size_t error_message_size);

#endif
