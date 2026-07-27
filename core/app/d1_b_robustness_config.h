#ifndef D1_B_ROBUSTNESS_CONFIG_H
#define D1_B_ROBUSTNESS_CONFIG_H

#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint32_t neuron_count;
    uint64_t seed;
    uint64_t lif_steps;
    uint64_t adex_steps;
    uint64_t hodgkin_huxley_steps;
    uint64_t structural_steps;
    uint64_t c7_ticks;
    uint32_t brain_steps_per_tick;
} D1BRobustnessConfig;

void d1_b_robustness_config_default(D1BRobustnessConfig *config);

int d1_b_robustness_config_load_file(
    const char *filename,
    D1BRobustnessConfig *out_config,
    char *error_message,
    size_t error_message_size);

int d1_b_robustness_config_write_file(
    const char *filename,
    const D1BRobustnessConfig *config,
    char *error_message,
    size_t error_message_size);

int d1_b_robustness_config_is_valid(const D1BRobustnessConfig *config);

uint64_t d1_b_robustness_config_signature(const D1BRobustnessConfig *config);

#endif
