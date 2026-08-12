#ifndef K0_SCENARIO_CONFIG_H
#define K0_SCENARIO_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#define K0_SCENARIO_ID_MAX_LENGTH 48U
#define K0_SCENARIO_CONFIG_FILE_MAX_SIZE 65536U
#define K0_SCENARIO_CONFIG_LINE_MAX_LENGTH 512U
#define K0_SCENARIO_TICKS_MAX UINT64_C(1000000)
#define K0_SCENARIO_ENTITY_MAX UINT64_C(4096)
#define K0_SCENARIO_BATCH_MAX UINT64_C(1024)
#define K0_SCENARIO_RANDOM_DRAWS_MAX UINT64_C(1024)

typedef struct
{
    uint32_t kernel_config_version;
    uint64_t master_seed;
    uint32_t scenario_version;
    char scenario_id[K0_SCENARIO_ID_MAX_LENGTH + 1U];
    uint64_t ticks;
    uint64_t initial_entities;
    uint64_t minimum_alive_entities;
    uint64_t maximum_alive_entities;
    uint64_t create_interval;
    uint64_t create_batch;
    uint64_t destroy_interval;
    uint64_t destroy_batch;
    uint64_t command_delay;
    uint32_t priority_base;
    uint64_t random_namespace;
    uint64_t random_stream;
    uint64_t random_draws_per_tick;
    uint64_t trace_interval;
} K0ScenarioConfig;

void k0_scenario_config_default(K0ScenarioConfig *out_config);

int k0_scenario_config_load_file(
    const char *filename,
    K0ScenarioConfig *out_config,
    char *error_message,
    size_t error_message_size);

int k0_scenario_config_validate(
    const K0ScenarioConfig *config,
    char *error_message,
    size_t error_message_size);

uint64_t k0_scenario_config_signature(const K0ScenarioConfig *config);

int k0_scenario_config_write_canonical(
    const K0ScenarioConfig *config,
    const char *filename,
    char *error_message,
    size_t error_message_size);

#endif
