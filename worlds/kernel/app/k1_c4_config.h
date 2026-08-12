#ifndef K1_C4_CONFIG_H
#define K1_C4_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#include "minisnn_worlds_kernel.h"

#define K1_C4_SCENARIO_ID_MAX_LENGTH 48U
#define K1_C4_CONFIG_LINE_MAX_LENGTH 512U
#define K1_C4_MAX_ENTITIES 16U
#define K1_C4_MAX_LINKS 16U
#define K1_C4_MAX_COMMANDS 64U

typedef struct
{
    MiniSNNWorldsKernelEntityId id;
    MiniSNNWorldsKernelTransform transform;
    int has_occupancy;
    MiniSNNWorldsKernelOccupancy occupancy;
} K1C4EntitySpec;

typedef struct
{
    MiniSNNWorldsKernelEntityId parent;
    MiniSNNWorldsKernelEntityId child;
} K1C4LinkSpec;

typedef struct
{
    MiniSNNWorldsTick tick;
    uint32_t priority;
    MiniSNNWorldsKernelEntityId issuer;
    MiniSNNWorldsKernelCommandType type;
    MiniSNNWorldsKernelEntityId target;
    MiniSNNWorldsKernelEntityId parent;
    MiniSNNWorldsKernelEntityId child;
    MiniSNNWorldsKernelScalar x;
    MiniSNNWorldsKernelScalar y;
    MiniSNNWorldsKernelOccupancy occupancy;
} K1C4CommandSpec;

typedef struct
{
    uint32_t scenario_version;
    char scenario_id[K1_C4_SCENARIO_ID_MAX_LENGTH + 1U];
    uint64_t master_seed;
    uint64_t ticks;
    MiniSNNWorldsKernelSpaceBounds bounds;
    K1C4EntitySpec entities[K1_C4_MAX_ENTITIES];
    size_t entity_count;
    K1C4LinkSpec links[K1_C4_MAX_LINKS];
    size_t link_count;
    K1C4CommandSpec commands[K1_C4_MAX_COMMANDS];
    size_t command_count;
} K1C4Config;

int k1_c4_config_load_file(
    const char *filename,
    K1C4Config *out_config,
    char *error_message,
    size_t error_message_size);

int k1_c4_config_validate(
    const K1C4Config *config,
    char *error_message,
    size_t error_message_size);

uint64_t k1_c4_config_signature(const K1C4Config *config);

int k1_c4_config_write_canonical(
    const K1C4Config *config,
    const char *filename,
    char *error_message,
    size_t error_message_size);

#endif