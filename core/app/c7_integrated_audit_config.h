#ifndef C7_INTEGRATED_AUDIT_CONFIG_H
#define C7_INTEGRATED_AUDIT_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#include "minisnn_types.h"

#define C7_AUDIT_RUN_NAME_MAX 48U

typedef struct
{
    char run_name[C7_AUDIT_RUN_NAME_MAX + 1U];
    int model_enabled[3];
    uint64_t seed;
    uint32_t episodes;
    uint32_t ticks_per_episode;
    uint32_t brain_steps_per_tick;
    int stdp_enabled;
    int reward_enabled;
    int homeostasis_enabled;
    int structural_enabled;
    int checkpoint_ready_enabled;
    int checkpoint_pending_enabled;
    int replay_enabled;
} C7IntegratedAuditConfig;

void c7_integrated_audit_config_default(C7IntegratedAuditConfig *config);

int c7_integrated_audit_config_load_file(
    const char *filename,
    C7IntegratedAuditConfig *out_config,
    char *error_message,
    size_t error_message_size);

int c7_integrated_audit_config_write_file(
    const char *filename,
    const C7IntegratedAuditConfig *config,
    char *error_message,
    size_t error_message_size);

#endif
