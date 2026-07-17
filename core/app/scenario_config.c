#include "scenario_config.h"
#include "neuron_model.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
    FIELD_RUN_NAME = 0,
    FIELD_TOPOLOGY,
    FIELD_NEURON_MODEL,
    FIELD_ADEX_CAPACITANCE,
    FIELD_ADEX_G_LEAK,
    FIELD_ADEX_E_LEAK,
    FIELD_ADEX_DELTA_T,
    FIELD_ADEX_V_THRESHOLD,
    FIELD_ADEX_TAU_W,
    FIELD_ADEX_A,
    FIELD_ADEX_B,
    FIELD_ADEX_V_RESET,
    FIELD_ADEX_V_PEAK,
    FIELD_HH_CAPACITANCE,
    FIELD_HH_G_NA,
    FIELD_HH_G_K,
    FIELD_HH_G_LEAK,
    FIELD_HH_E_NA,
    FIELD_HH_E_K,
    FIELD_HH_E_LEAK,
    FIELD_HH_V_INIT,
    FIELD_HH_SPIKE_THRESHOLD,
    FIELD_NEURONS,
    FIELD_INHIBITORY_FRACTION,
    FIELD_CONNECTION_PROBABILITY,
    FIELD_SEED,
    FIELD_DELAY,
    FIELD_MAX_SYNAPTIC_DELAY,
    FIELD_ALLOW_SELF_CONNECTIONS,
    FIELD_ALLOW_INH_TO_INH,
    FIELD_EXCITATORY_WEIGHT,
    FIELD_INHIBITORY_WEIGHT,
    FIELD_SOURCE_COUNT,
    FIELD_INPUT_CURRENT,
    FIELD_STEPS,
    FIELD_DT,
    FIELD_TAU,
    FIELD_V_REST,
    FIELD_V_RESET,
    FIELD_V_THRESHOLD,
    FIELD_RESISTANCE,
    FIELD_SYNAPTIC_DECAY,
    FIELD_SMALL_WORLD_NEIGHBORS,
    FIELD_SMALL_WORLD_REWIRE_PROBABILITY,
    FIELD_FEEDFORWARD_LAYERS,
    FIELD_RECORD_NEURON,
    FIELD_AUTO_UNIQUE_RUN,
    FIELD_HISTORY_ENABLED,
    FIELD_DIAGNOSTICS_LEVEL,
    FIELD_TIME_BIN_STEPS,
    FIELD_BURST_Z_THRESHOLD,
    FIELD_MIN_BURST_STEPS,
    FIELD_ISI_MIN_SPIKES,
    FIELD_CORRELATION_SAMPLE_SIZE,
    FIELD_NEURON_SAMPLE_LIMIT,
    FIELD_SAMPLE_STRIDE,
    FIELD_PLASTICITY_ENABLED,
    FIELD_PLASTICITY_RULE,
    FIELD_PLASTICITY_LEARNING_MODE,
    FIELD_PLASTICITY_A_PLUS,
    FIELD_PLASTICITY_A_MINUS,
    FIELD_PLASTICITY_TAU_PLUS,
    FIELD_PLASTICITY_TAU_MINUS,
    FIELD_PLASTICITY_TRACE_INCREMENT,
    FIELD_PLASTICITY_WEIGHT_MIN,
    FIELD_PLASTICITY_WEIGHT_MAX,
    FIELD_PLASTICITY_RECORD_WEIGHTS,
    FIELD_PLASTICITY_RECORD_HISTORY,
    FIELD_PLASTICITY_RECORD_INTERVAL_STEPS,
    FIELD_PLASTICITY_RECORD_CONNECTION_LIMIT,
    FIELD_HOMEOSTASIS_ENABLED,
    FIELD_HOMEOSTASIS_INTRINSIC_ENABLED,
    FIELD_HOMEOSTASIS_TARGET_RATE,
    FIELD_HOMEOSTASIS_RATE_TAU,
    FIELD_HOMEOSTASIS_UPDATE_INTERVAL_STEPS,
    FIELD_HOMEOSTASIS_THRESHOLD_ETA,
    FIELD_HOMEOSTASIS_THRESHOLD_MIN,
    FIELD_HOMEOSTASIS_THRESHOLD_MAX,
    FIELD_HOMEOSTASIS_SCALING_ENABLED,
    FIELD_HOMEOSTASIS_SCALING_TARGET_MODE,
    FIELD_HOMEOSTASIS_SCALING_ETA,
    FIELD_HOMEOSTASIS_SCALING_MIN_FACTOR,
    FIELD_HOMEOSTASIS_SCALING_MAX_FACTOR,
    FIELD_HOMEOSTASIS_SCALING_WEIGHT_MIN,
    FIELD_HOMEOSTASIS_SCALING_WEIGHT_MAX,
    FIELD_HOMEOSTASIS_INHIBITORY_GAIN_ENABLED,
    FIELD_HOMEOSTASIS_INHIBITORY_GAIN_INITIAL,
    FIELD_HOMEOSTASIS_INHIBITORY_GAIN_ETA,
    FIELD_HOMEOSTASIS_INHIBITORY_GAIN_MIN,
    FIELD_HOMEOSTASIS_INHIBITORY_GAIN_MAX,
    FIELD_HOMEOSTASIS_RECORD_HISTORY,
    FIELD_HOMEOSTASIS_RECORD_INTERVAL_STEPS,
    FIELD_HOMEOSTASIS_RECORD_NEURON_LIMIT,
    FIELD_STRUCTURAL_ENABLED,
    FIELD_STRUCTURAL_MAINTENANCE_INTERVAL_STEPS,
    FIELD_STRUCTURAL_GRACE_PERIOD_STEPS,
    FIELD_STRUCTURAL_PRUNING_ENABLED,
    FIELD_STRUCTURAL_PRUNE_WEIGHT_THRESHOLD,
    FIELD_STRUCTURAL_PRUNE_ACTIVITY_THRESHOLD,
    FIELD_STRUCTURAL_MAX_PRUNES_PER_INTERVAL,
    FIELD_STRUCTURAL_GROWTH_ENABLED,
    FIELD_STRUCTURAL_GROWTH_CANDIDATE_COUNT,
    FIELD_STRUCTURAL_GROWTH_SCORE_THRESHOLD,
    FIELD_STRUCTURAL_MAX_GROWTH_PER_INTERVAL,
    FIELD_STRUCTURAL_GROWTH_SEED,
    FIELD_STRUCTURAL_NEW_EXC_WEIGHT,
    FIELD_STRUCTURAL_NEW_INH_MAGNITUDE,
    FIELD_STRUCTURAL_NEW_DELAY,
    FIELD_STRUCTURAL_MIN_CONNECTIONS,
    FIELD_STRUCTURAL_MAX_CONNECTIONS,
    FIELD_STRUCTURAL_RECORD_HISTORY,
    FIELD_STRUCTURAL_RECORD_INTERVAL_STEPS,
    FIELD_REWARD_ENABLED,
    FIELD_REWARD_MODE,
    FIELD_REWARD_LEARNING_RATE,
    FIELD_REWARD_ELIGIBILITY_TAU,
    FIELD_REWARD_ELIGIBILITY_MIN,
    FIELD_REWARD_ELIGIBILITY_MAX,
    FIELD_REWARD_MIN,
    FIELD_REWARD_MAX,
    FIELD_REWARD_CLIP,
    FIELD_REWARD_RECORD_HISTORY,
    FIELD_REWARD_RECORD_INTERVAL_STEPS,
    FIELD_REWARD_RECORD_CONNECTION_LIMIT,
    FIELD_COUNT
} ScenarioField;

typedef struct
{
    const char *key;
    ScenarioField field;
} KeyMap;

static const KeyMap KEY_MAP[] =
{
    {"run_name", FIELD_RUN_NAME},
    {"topology", FIELD_TOPOLOGY},
    {"model", FIELD_NEURON_MODEL},
    {"neurons", FIELD_NEURONS},
    {"inhibitory_fraction", FIELD_INHIBITORY_FRACTION},
    {"connection_probability", FIELD_CONNECTION_PROBABILITY},
    {"seed", FIELD_SEED},
    {"delay", FIELD_DELAY},
    {"max_synaptic_delay", FIELD_MAX_SYNAPTIC_DELAY},
    {"allow_self_connections", FIELD_ALLOW_SELF_CONNECTIONS},
    {"allow_inh_to_inh", FIELD_ALLOW_INH_TO_INH},
    {"excitatory_weight", FIELD_EXCITATORY_WEIGHT},
    {"inhibitory_weight", FIELD_INHIBITORY_WEIGHT},
    {"source_count", FIELD_SOURCE_COUNT},
    {"input_current", FIELD_INPUT_CURRENT},
    {"steps", FIELD_STEPS},
    {"dt", FIELD_DT},
    {"tau", FIELD_TAU},
    {"v_rest", FIELD_V_REST},
    {"v_reset", FIELD_V_RESET},
    {"v_threshold", FIELD_V_THRESHOLD},
    {"resistance", FIELD_RESISTANCE},
    {"synaptic_decay", FIELD_SYNAPTIC_DECAY},
    {"small_world_neighbors", FIELD_SMALL_WORLD_NEIGHBORS},
    {"small_world_rewire_probability", FIELD_SMALL_WORLD_REWIRE_PROBABILITY},
    {"feedforward_layers", FIELD_FEEDFORWARD_LAYERS},
    {"record_neuron", FIELD_RECORD_NEURON},
    {"auto_unique_run", FIELD_AUTO_UNIQUE_RUN},
    {"history_enabled", FIELD_HISTORY_ENABLED},
    {"level", FIELD_DIAGNOSTICS_LEVEL},
    {"time_bin_steps", FIELD_TIME_BIN_STEPS},
    {"burst_z_threshold", FIELD_BURST_Z_THRESHOLD},
    {"min_burst_steps", FIELD_MIN_BURST_STEPS},
    {"isi_min_spikes", FIELD_ISI_MIN_SPIKES},
    {"correlation_sample_size", FIELD_CORRELATION_SAMPLE_SIZE},
    {"neuron_sample_limit", FIELD_NEURON_SAMPLE_LIMIT},
    {"sample_stride", FIELD_SAMPLE_STRIDE},
    {"enabled", FIELD_PLASTICITY_ENABLED},
    {"rule", FIELD_PLASTICITY_RULE},
    {"learning_mode", FIELD_PLASTICITY_LEARNING_MODE},
    {"a_plus", FIELD_PLASTICITY_A_PLUS},
    {"a_minus", FIELD_PLASTICITY_A_MINUS},
    {"tau_plus", FIELD_PLASTICITY_TAU_PLUS},
    {"tau_minus", FIELD_PLASTICITY_TAU_MINUS},
    {"trace_increment", FIELD_PLASTICITY_TRACE_INCREMENT},
    {"weight_min", FIELD_PLASTICITY_WEIGHT_MIN},
    {"weight_max", FIELD_PLASTICITY_WEIGHT_MAX},
    {"record_weights", FIELD_PLASTICITY_RECORD_WEIGHTS},
    {"record_history", FIELD_PLASTICITY_RECORD_HISTORY},
    {"record_interval_steps", FIELD_PLASTICITY_RECORD_INTERVAL_STEPS},
    {"record_connection_limit", FIELD_PLASTICITY_RECORD_CONNECTION_LIMIT}
};

static void set_error(
    char *error_message,
    size_t error_message_size,
    const char *message)
{
    if (error_message == NULL || error_message_size == 0)
        return;

    snprintf(error_message, error_message_size, "%s", message);
}

static void set_line_error(
    char *error_message,
    size_t error_message_size,
    int line_number,
    const char *message)
{
    if (error_message == NULL || error_message_size == 0)
        return;

    snprintf(
        error_message,
        error_message_size,
        "linha %d: %s",
        line_number,
        message);
}

static char *trim(char *text)
{
    char *end;

    while (*text != '\0' && isspace((unsigned char)*text))
        text++;

    if (*text == '\0')
        return text;

    end = text + strlen(text) - 1;

    while (end >= text && isspace((unsigned char)*end))
    {
        *end = '\0';
        end--;
    }

    return text;
}

static void remove_comment(char *line)
{
    char *hash = strchr(line, '#');
    char *semicolon = strchr(line, ';');
    char *comment = NULL;

    if (hash != NULL && semicolon != NULL)
        comment = hash < semicolon ? hash : semicolon;
    else if (hash != NULL)
        comment = hash;
    else
        comment = semicolon;

    if (comment != NULL)
        *comment = '\0';
}

static int find_field(
    const char *section,
    const char *key,
    ScenarioField *out_field)
{
    size_t count = sizeof(KEY_MAP) / sizeof(KEY_MAP[0]);

    if (strcmp(section, "adex") == 0)
    {
        if (strcmp(key, "capacitance") == 0) *out_field = FIELD_ADEX_CAPACITANCE;
        else if (strcmp(key, "g_leak") == 0) *out_field = FIELD_ADEX_G_LEAK;
        else if (strcmp(key, "e_leak") == 0) *out_field = FIELD_ADEX_E_LEAK;
        else if (strcmp(key, "delta_t") == 0) *out_field = FIELD_ADEX_DELTA_T;
        else if (strcmp(key, "v_threshold") == 0) *out_field = FIELD_ADEX_V_THRESHOLD;
        else if (strcmp(key, "tau_w") == 0) *out_field = FIELD_ADEX_TAU_W;
        else if (strcmp(key, "a") == 0) *out_field = FIELD_ADEX_A;
        else if (strcmp(key, "b") == 0) *out_field = FIELD_ADEX_B;
        else if (strcmp(key, "v_reset") == 0) *out_field = FIELD_ADEX_V_RESET;
        else if (strcmp(key, "v_peak") == 0) *out_field = FIELD_ADEX_V_PEAK;
        else return 0;
        return 1;
    }

    if (strcmp(section, "hodgkin_huxley") == 0)
    {
        if (strcmp(key, "capacitance") == 0) *out_field = FIELD_HH_CAPACITANCE;
        else if (strcmp(key, "g_na") == 0) *out_field = FIELD_HH_G_NA;
        else if (strcmp(key, "g_k") == 0) *out_field = FIELD_HH_G_K;
        else if (strcmp(key, "g_leak") == 0) *out_field = FIELD_HH_G_LEAK;
        else if (strcmp(key, "e_na") == 0) *out_field = FIELD_HH_E_NA;
        else if (strcmp(key, "e_k") == 0) *out_field = FIELD_HH_E_K;
        else if (strcmp(key, "e_leak") == 0) *out_field = FIELD_HH_E_LEAK;
        else if (strcmp(key, "v_init") == 0) *out_field = FIELD_HH_V_INIT;
        else if (strcmp(key, "spike_threshold") == 0) *out_field = FIELD_HH_SPIKE_THRESHOLD;
        else return 0;
        return 1;
    }

    if (strcmp(section, "structural_plasticity") == 0)
    {
        if (strcmp(key, "enabled") == 0)
            *out_field = FIELD_STRUCTURAL_ENABLED;
        else if (strcmp(key, "maintenance_interval_steps") == 0)
            *out_field = FIELD_STRUCTURAL_MAINTENANCE_INTERVAL_STEPS;
        else if (strcmp(key, "grace_period_steps") == 0)
            *out_field = FIELD_STRUCTURAL_GRACE_PERIOD_STEPS;
        else if (strcmp(key, "pruning_enabled") == 0)
            *out_field = FIELD_STRUCTURAL_PRUNING_ENABLED;
        else if (strcmp(key, "prune_weight_threshold") == 0)
            *out_field = FIELD_STRUCTURAL_PRUNE_WEIGHT_THRESHOLD;
        else if (strcmp(key, "prune_activity_threshold") == 0)
            *out_field = FIELD_STRUCTURAL_PRUNE_ACTIVITY_THRESHOLD;
        else if (strcmp(key, "max_prunes_per_interval") == 0)
            *out_field = FIELD_STRUCTURAL_MAX_PRUNES_PER_INTERVAL;
        else if (strcmp(key, "growth_enabled") == 0)
            *out_field = FIELD_STRUCTURAL_GROWTH_ENABLED;
        else if (strcmp(key, "growth_candidate_count") == 0)
            *out_field = FIELD_STRUCTURAL_GROWTH_CANDIDATE_COUNT;
        else if (strcmp(key, "growth_score_threshold") == 0)
            *out_field = FIELD_STRUCTURAL_GROWTH_SCORE_THRESHOLD;
        else if (strcmp(key, "max_growth_per_interval") == 0)
            *out_field = FIELD_STRUCTURAL_MAX_GROWTH_PER_INTERVAL;
        else if (strcmp(key, "growth_seed") == 0)
            *out_field = FIELD_STRUCTURAL_GROWTH_SEED;
        else if (strcmp(key, "new_exc_weight") == 0)
            *out_field = FIELD_STRUCTURAL_NEW_EXC_WEIGHT;
        else if (strcmp(key, "new_inh_magnitude") == 0)
            *out_field = FIELD_STRUCTURAL_NEW_INH_MAGNITUDE;
        else if (strcmp(key, "new_delay") == 0)
            *out_field = FIELD_STRUCTURAL_NEW_DELAY;
        else if (strcmp(key, "min_connections") == 0)
            *out_field = FIELD_STRUCTURAL_MIN_CONNECTIONS;
        else if (strcmp(key, "max_connections") == 0)
            *out_field = FIELD_STRUCTURAL_MAX_CONNECTIONS;
        else if (strcmp(key, "record_history") == 0)
            *out_field = FIELD_STRUCTURAL_RECORD_HISTORY;
        else if (strcmp(key, "record_interval_steps") == 0)
            *out_field = FIELD_STRUCTURAL_RECORD_INTERVAL_STEPS;
        else
            return 0;
        return 1;
    }

    if (strcmp(section, "homeostasis") == 0)
    {
        if (strcmp(key, "enabled") == 0)
            *out_field = FIELD_HOMEOSTASIS_ENABLED;
        else if (strcmp(key, "intrinsic_enabled") == 0)
            *out_field = FIELD_HOMEOSTASIS_INTRINSIC_ENABLED;
        else if (strcmp(key, "target_rate") == 0)
            *out_field = FIELD_HOMEOSTASIS_TARGET_RATE;
        else if (strcmp(key, "rate_tau") == 0)
            *out_field = FIELD_HOMEOSTASIS_RATE_TAU;
        else if (strcmp(key, "update_interval_steps") == 0)
            *out_field = FIELD_HOMEOSTASIS_UPDATE_INTERVAL_STEPS;
        else if (strcmp(key, "threshold_eta") == 0)
            *out_field = FIELD_HOMEOSTASIS_THRESHOLD_ETA;
        else if (strcmp(key, "threshold_min") == 0)
            *out_field = FIELD_HOMEOSTASIS_THRESHOLD_MIN;
        else if (strcmp(key, "threshold_max") == 0)
            *out_field = FIELD_HOMEOSTASIS_THRESHOLD_MAX;
        else if (strcmp(key, "synaptic_scaling_enabled") == 0)
            *out_field = FIELD_HOMEOSTASIS_SCALING_ENABLED;
        else if (strcmp(key, "scaling_target_mode") == 0)
            *out_field = FIELD_HOMEOSTASIS_SCALING_TARGET_MODE;
        else if (strcmp(key, "scaling_eta") == 0)
            *out_field = FIELD_HOMEOSTASIS_SCALING_ETA;
        else if (strcmp(key, "scaling_min_factor") == 0)
            *out_field = FIELD_HOMEOSTASIS_SCALING_MIN_FACTOR;
        else if (strcmp(key, "scaling_max_factor") == 0)
            *out_field = FIELD_HOMEOSTASIS_SCALING_MAX_FACTOR;
        else if (strcmp(key, "scaling_weight_min") == 0)
            *out_field = FIELD_HOMEOSTASIS_SCALING_WEIGHT_MIN;
        else if (strcmp(key, "scaling_weight_max") == 0)
            *out_field = FIELD_HOMEOSTASIS_SCALING_WEIGHT_MAX;
        else if (strcmp(key, "inhibitory_gain_enabled") == 0)
            *out_field = FIELD_HOMEOSTASIS_INHIBITORY_GAIN_ENABLED;
        else if (strcmp(key, "inhibitory_gain_initial") == 0)
            *out_field = FIELD_HOMEOSTASIS_INHIBITORY_GAIN_INITIAL;
        else if (strcmp(key, "inhibitory_gain_eta") == 0)
            *out_field = FIELD_HOMEOSTASIS_INHIBITORY_GAIN_ETA;
        else if (strcmp(key, "inhibitory_gain_min") == 0)
            *out_field = FIELD_HOMEOSTASIS_INHIBITORY_GAIN_MIN;
        else if (strcmp(key, "inhibitory_gain_max") == 0)
            *out_field = FIELD_HOMEOSTASIS_INHIBITORY_GAIN_MAX;
        else if (strcmp(key, "record_history") == 0)
            *out_field = FIELD_HOMEOSTASIS_RECORD_HISTORY;
        else if (strcmp(key, "record_interval_steps") == 0)
            *out_field = FIELD_HOMEOSTASIS_RECORD_INTERVAL_STEPS;
        else if (strcmp(key, "record_neuron_limit") == 0)
            *out_field = FIELD_HOMEOSTASIS_RECORD_NEURON_LIMIT;
        else
            return 0;
        return 1;
    }

    if (strcmp(section, "reward") == 0)
    {
        if (strcmp(key, "enabled") == 0)
            *out_field = FIELD_REWARD_ENABLED;
        else if (strcmp(key, "mode") == 0)
            *out_field = FIELD_REWARD_MODE;
        else if (strcmp(key, "learning_rate") == 0)
            *out_field = FIELD_REWARD_LEARNING_RATE;
        else if (strcmp(key, "eligibility_tau") == 0)
            *out_field = FIELD_REWARD_ELIGIBILITY_TAU;
        else if (strcmp(key, "eligibility_min") == 0)
            *out_field = FIELD_REWARD_ELIGIBILITY_MIN;
        else if (strcmp(key, "eligibility_max") == 0)
            *out_field = FIELD_REWARD_ELIGIBILITY_MAX;
        else if (strcmp(key, "reward_min") == 0)
            *out_field = FIELD_REWARD_MIN;
        else if (strcmp(key, "reward_max") == 0)
            *out_field = FIELD_REWARD_MAX;
        else if (strcmp(key, "clip_reward") == 0)
            *out_field = FIELD_REWARD_CLIP;
        else if (strcmp(key, "record_history") == 0)
            *out_field = FIELD_REWARD_RECORD_HISTORY;
        else if (strcmp(key, "record_interval_steps") == 0)
            *out_field = FIELD_REWARD_RECORD_INTERVAL_STEPS;
        else if (strcmp(key, "record_connection_limit") == 0)
            *out_field = FIELD_REWARD_RECORD_CONNECTION_LIMIT;
        else
            return 0;
        return 1;
    }

    for (size_t i = 0; i < count; i++)
    {
        if (strcmp(key, KEY_MAP[i].key) == 0)
        {
            *out_field = KEY_MAP[i].field;
            return 1;
        }
    }

    return 0;
}

static int parse_int_value(const char *text, int *out_value)
{
    char *end;
    long value;

    errno = 0;
    value = strtol(text, &end, 10);

    if (text == end || errno != 0)
        return 0;

    end = trim(end);

    if (*end != '\0')
        return 0;

    if (value < -2147483647L - 1L || value > 2147483647L)
        return 0;

    *out_value = (int)value;
    return 1;
}

static int parse_uint_value(const char *text, unsigned int *out_value)
{
    char *end;
    unsigned long value;
    const char *start = text;

    while (*start != '\0' && isspace((unsigned char)*start))
        start++;

    if (*start == '-' || *start == '+')
        return 0;

    errno = 0;
    value = strtoul(text, &end, 10);

    if (text == end || errno != 0)
        return 0;

    end = trim(end);

    if (*end != '\0')
        return 0;

    if (value > 4294967295UL)
        return 0;

    *out_value = (unsigned int)value;
    return 1;
}

static int parse_uint64_value(const char *text, uint64_t *out_value)
{
    char *end;
    unsigned long long value;
    const char *start = text;

    while (*start != '\0' && isspace((unsigned char)*start))
        start++;
    if (*start == '-' || *start == '+')
        return 0;
    errno = 0;
    value = strtoull(text, &end, 10);
    if (text == end || errno != 0)
        return 0;
    end = trim(end);
    if (*end != '\0')
        return 0;
    *out_value = (uint64_t)value;
    return 1;
}

static int parse_double_value(const char *text, double *out_value)
{
    char *end;
    double value;

    errno = 0;
    value = strtod(text, &end);

    if (text == end || errno != 0)
        return 0;

    end = trim(end);

    if (*end != '\0' || !isfinite(value))
        return 0;

    *out_value = value;
    return 1;
}

static int parse_reward_event_key(const char *key, int *out_index)
{
    if (key == NULL || out_index == NULL || strncmp(key, "event_", 6) != 0 ||
        key[6] == '\0')
    {
        return 0;
    }

    return parse_int_value(key + 6, out_index) && *out_index >= 0;
}

static int parse_reward_event_value(
    const char *value,
    int *out_step,
    double *out_reward)
{
    char step_text[64];
    char reward_text[128];
    const char *comma;
    size_t step_length;

    if (value == NULL || out_step == NULL || out_reward == NULL)
        return 0;

    comma = strchr(value, ',');
    if (comma == NULL || strchr(comma + 1, ',') != NULL)
        return 0;

    step_length = (size_t)(comma - value);
    if (step_length == 0 || step_length >= sizeof(step_text) ||
        strlen(comma + 1) >= sizeof(reward_text))
    {
        return 0;
    }

    memcpy(step_text, value, step_length);
    step_text[step_length] = '\0';
    snprintf(reward_text, sizeof(reward_text), "%s", comma + 1);
    return parse_int_value(trim(step_text), out_step) && *out_step >= 0 &&
        parse_double_value(trim(reward_text), out_reward);
}

static int reward_event_compare(const void *left, const void *right)
{
    const ScenarioRewardEvent *a = left;
    const ScenarioRewardEvent *b = right;

    if (a->step != b->step)
        return a->step < b->step ? -1 : 1;
    if (a->index != b->index)
        return a->index < b->index ? -1 : 1;
    return 0;
}

static int text_equals_ignore_case(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0')
    {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;

        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

static int parse_bool_value(const char *text, int *out_value)
{
    if (text_equals_ignore_case(text, "true") ||
        text_equals_ignore_case(text, "yes") ||
        text_equals_ignore_case(text, "sim") ||
        strcmp(text, "1") == 0)
    {
        *out_value = 1;
        return 1;
    }

    if (text_equals_ignore_case(text, "false") ||
        text_equals_ignore_case(text, "no") ||
        text_equals_ignore_case(text, "nao") ||
        strcmp(text, "0") == 0)
    {
        *out_value = 0;
        return 1;
    }

    return 0;
}

static int copy_string_value(
    char *destination,
    size_t destination_size,
    const char *value)
{
    size_t length = strlen(value);

    if (length >= destination_size)
        return 0;

    memcpy(destination, value, length + 1);
    return 1;
}

static int assign_value(
    ScenarioConfig *config,
    ScenarioField field,
    const char *value,
    int line_number,
    char *error_message,
    size_t error_message_size)
{
    int int_value;
    int bool_value;
    unsigned int uint_value;
    uint64_t uint64_value;
    double double_value;

    switch (field)
    {
    case FIELD_RUN_NAME:
        if (!copy_string_value(
                config->run_name,
                sizeof(config->run_name),
                value))
        {
            set_line_error(
                error_message,
                error_message_size,
                line_number,
                "run_name muito longo");
            return 0;
        }
        return 1;

    case FIELD_TOPOLOGY:
        if (!copy_string_value(
                config->topology,
                sizeof(config->topology),
                value))
        {
            set_line_error(
                error_message,
                error_message_size,
                line_number,
                "topology muito longo");
            return 0;
        }
        return 1;

    case FIELD_NEURON_MODEL:
        if (!neuron_model_from_name(value, &config->neuron_model))
        {
            set_line_error(error_message, error_message_size, line_number,
                           "model desconhecido");
            return 0;
        }
        return 1;

    case FIELD_DIAGNOSTICS_LEVEL:
        if (!copy_string_value(
                config->diagnostics_level,
                sizeof(config->diagnostics_level),
                value))
        {
            set_line_error(
                error_message,
                error_message_size,
                line_number,
                "level de diagnostico muito longo");
            return 0;
        }
        return 1;

    case FIELD_PLASTICITY_RULE:
        if (!copy_string_value(
                config->plasticity_rule,
                sizeof(config->plasticity_rule),
                value))
        {
            set_line_error(
                error_message,
                error_message_size,
                line_number,
                "regra de plasticidade muito longa");
            return 0;
        }
        return 1;

    case FIELD_PLASTICITY_LEARNING_MODE:
        if (!copy_string_value(
                config->plasticity_learning_mode,
                sizeof(config->plasticity_learning_mode),
                value))
        {
            set_line_error(
                error_message,
                error_message_size,
                line_number,
                "learning_mode muito longo");
            return 0;
        }
        return 1;

    case FIELD_REWARD_MODE:
        if (!copy_string_value(
                config->reward_mode,
                sizeof(config->reward_mode),
                value))
        {
            set_line_error(
                error_message,
                error_message_size,
                line_number,
                "modo de reward muito longo");
            return 0;
        }
        return 1;

    case FIELD_HOMEOSTASIS_SCALING_TARGET_MODE:
        if (!copy_string_value(
                config->homeostasis_scaling_target_mode,
                sizeof(config->homeostasis_scaling_target_mode),
                value))
        {
            set_line_error(
                error_message,
                error_message_size,
                line_number,
                "scaling_target_mode muito longo");
            return 0;
        }
        return 1;

    case FIELD_SEED:
        if (!parse_uint_value(value, &uint_value))
        {
            set_line_error(
                error_message,
                error_message_size,
                line_number,
                "seed invalido");
            return 0;
        }
        config->seed = uint_value;
        return 1;

    case FIELD_STRUCTURAL_GROWTH_SEED:
        if (!parse_uint64_value(value, &uint64_value))
        {
            set_line_error(error_message, error_message_size, line_number,
                           "growth_seed invalido");
            return 0;
        }
        config->structural_growth_seed = uint64_value;
        return 1;

    case FIELD_ALLOW_SELF_CONNECTIONS:
    case FIELD_ALLOW_INH_TO_INH:
    case FIELD_AUTO_UNIQUE_RUN:
    case FIELD_HISTORY_ENABLED:
    case FIELD_PLASTICITY_ENABLED:
    case FIELD_PLASTICITY_RECORD_WEIGHTS:
    case FIELD_PLASTICITY_RECORD_HISTORY:
    case FIELD_HOMEOSTASIS_ENABLED:
    case FIELD_HOMEOSTASIS_INTRINSIC_ENABLED:
    case FIELD_HOMEOSTASIS_SCALING_ENABLED:
    case FIELD_HOMEOSTASIS_INHIBITORY_GAIN_ENABLED:
    case FIELD_HOMEOSTASIS_RECORD_HISTORY:
    case FIELD_REWARD_ENABLED:
    case FIELD_REWARD_CLIP:
    case FIELD_REWARD_RECORD_HISTORY:
    case FIELD_STRUCTURAL_ENABLED:
    case FIELD_STRUCTURAL_PRUNING_ENABLED:
    case FIELD_STRUCTURAL_GROWTH_ENABLED:
    case FIELD_STRUCTURAL_RECORD_HISTORY:
        if (!parse_bool_value(value, &bool_value))
        {
            set_line_error(
                error_message,
                error_message_size,
                line_number,
                "valor booleano invalido");
            return 0;
        }

        if (field == FIELD_ALLOW_SELF_CONNECTIONS)
            config->allow_self_connections = bool_value;
        else if (field == FIELD_ALLOW_INH_TO_INH)
            config->allow_inh_to_inh = bool_value;
        else if (field == FIELD_AUTO_UNIQUE_RUN)
            config->auto_unique_run = bool_value;
        else if (field == FIELD_HISTORY_ENABLED)
            config->history_enabled = bool_value;
        else if (field == FIELD_PLASTICITY_ENABLED)
            config->plasticity_enabled = bool_value;
        else if (field == FIELD_PLASTICITY_RECORD_WEIGHTS)
            config->plasticity_record_weights = bool_value;
        else if (field == FIELD_PLASTICITY_RECORD_HISTORY)
            config->plasticity_record_history = bool_value;
        else if (field == FIELD_HOMEOSTASIS_ENABLED)
            config->homeostasis_enabled = bool_value;
        else if (field == FIELD_HOMEOSTASIS_INTRINSIC_ENABLED)
            config->homeostasis_intrinsic_enabled = bool_value;
        else if (field == FIELD_HOMEOSTASIS_SCALING_ENABLED)
            config->homeostasis_synaptic_scaling_enabled = bool_value;
        else if (field == FIELD_HOMEOSTASIS_INHIBITORY_GAIN_ENABLED)
            config->homeostasis_inhibitory_gain_enabled = bool_value;
        else if (field == FIELD_HOMEOSTASIS_RECORD_HISTORY)
            config->homeostasis_record_history = bool_value;
        else if (field == FIELD_REWARD_ENABLED)
            config->reward_enabled = bool_value;
        else if (field == FIELD_REWARD_CLIP)
            config->reward_clip = bool_value;
        else if (field == FIELD_STRUCTURAL_ENABLED)
            config->structural_plasticity_enabled = bool_value;
        else if (field == FIELD_STRUCTURAL_PRUNING_ENABLED)
            config->structural_pruning_enabled = bool_value;
        else if (field == FIELD_STRUCTURAL_GROWTH_ENABLED)
            config->structural_growth_enabled = bool_value;
        else if (field == FIELD_STRUCTURAL_RECORD_HISTORY)
            config->structural_record_history = bool_value;
        else
            config->reward_record_history = bool_value;

        return 1;

    case FIELD_NEURONS:
    case FIELD_DELAY:
    case FIELD_MAX_SYNAPTIC_DELAY:
    case FIELD_SOURCE_COUNT:
    case FIELD_STEPS:
    case FIELD_SMALL_WORLD_NEIGHBORS:
    case FIELD_FEEDFORWARD_LAYERS:
    case FIELD_RECORD_NEURON:
    case FIELD_TIME_BIN_STEPS:
    case FIELD_MIN_BURST_STEPS:
    case FIELD_ISI_MIN_SPIKES:
    case FIELD_CORRELATION_SAMPLE_SIZE:
    case FIELD_NEURON_SAMPLE_LIMIT:
    case FIELD_SAMPLE_STRIDE:
    case FIELD_PLASTICITY_RECORD_INTERVAL_STEPS:
    case FIELD_PLASTICITY_RECORD_CONNECTION_LIMIT:
    case FIELD_HOMEOSTASIS_UPDATE_INTERVAL_STEPS:
    case FIELD_HOMEOSTASIS_RECORD_INTERVAL_STEPS:
    case FIELD_HOMEOSTASIS_RECORD_NEURON_LIMIT:
    case FIELD_REWARD_RECORD_INTERVAL_STEPS:
    case FIELD_REWARD_RECORD_CONNECTION_LIMIT:
    case FIELD_STRUCTURAL_MAINTENANCE_INTERVAL_STEPS:
    case FIELD_STRUCTURAL_GRACE_PERIOD_STEPS:
    case FIELD_STRUCTURAL_MAX_PRUNES_PER_INTERVAL:
    case FIELD_STRUCTURAL_GROWTH_CANDIDATE_COUNT:
    case FIELD_STRUCTURAL_MAX_GROWTH_PER_INTERVAL:
    case FIELD_STRUCTURAL_NEW_DELAY:
    case FIELD_STRUCTURAL_MIN_CONNECTIONS:
    case FIELD_STRUCTURAL_MAX_CONNECTIONS:
    case FIELD_STRUCTURAL_RECORD_INTERVAL_STEPS:
        if (!parse_int_value(value, &int_value))
        {
            set_line_error(
                error_message,
                error_message_size,
                line_number,
                "valor inteiro invalido");
            return 0;
        }

        if (field == FIELD_NEURONS)
            config->neurons = int_value;
        else if (field == FIELD_DELAY)
            config->delay = int_value;
        else if (field == FIELD_MAX_SYNAPTIC_DELAY)
            config->max_synaptic_delay = int_value;
        else if (field == FIELD_SOURCE_COUNT)
            config->source_count = int_value;
        else if (field == FIELD_STEPS)
            config->steps = int_value;
        else if (field == FIELD_SMALL_WORLD_NEIGHBORS)
            config->small_world_neighbors = int_value;
        else if (field == FIELD_FEEDFORWARD_LAYERS)
            config->feedforward_layers = int_value;
        else if (field == FIELD_TIME_BIN_STEPS)
            config->time_bin_steps = int_value;
        else if (field == FIELD_MIN_BURST_STEPS)
            config->min_burst_steps = int_value;
        else if (field == FIELD_ISI_MIN_SPIKES)
            config->isi_min_spikes = int_value;
        else if (field == FIELD_CORRELATION_SAMPLE_SIZE)
            config->correlation_sample_size = int_value;
        else if (field == FIELD_NEURON_SAMPLE_LIMIT)
            config->neuron_sample_limit = int_value;
        else if (field == FIELD_SAMPLE_STRIDE)
            config->sample_stride = int_value;
        else if (field == FIELD_PLASTICITY_RECORD_INTERVAL_STEPS)
            config->plasticity_record_interval_steps = int_value;
        else if (field == FIELD_PLASTICITY_RECORD_CONNECTION_LIMIT)
            config->plasticity_record_connection_limit = int_value;
        else if (field == FIELD_HOMEOSTASIS_UPDATE_INTERVAL_STEPS)
            config->homeostasis_update_interval_steps = int_value;
        else if (field == FIELD_HOMEOSTASIS_RECORD_INTERVAL_STEPS)
            config->homeostasis_record_interval_steps = int_value;
        else if (field == FIELD_HOMEOSTASIS_RECORD_NEURON_LIMIT)
            config->homeostasis_record_neuron_limit = int_value;
        else if (field == FIELD_REWARD_RECORD_INTERVAL_STEPS)
            config->reward_record_interval_steps = int_value;
        else if (field == FIELD_REWARD_RECORD_CONNECTION_LIMIT)
            config->reward_record_connection_limit = int_value;
        else if (field == FIELD_STRUCTURAL_MAINTENANCE_INTERVAL_STEPS)
            config->structural_maintenance_interval_steps = int_value;
        else if (field == FIELD_STRUCTURAL_GRACE_PERIOD_STEPS)
            config->structural_grace_period_steps = int_value;
        else if (field == FIELD_STRUCTURAL_MAX_PRUNES_PER_INTERVAL)
            config->structural_max_prunes_per_interval = int_value;
        else if (field == FIELD_STRUCTURAL_GROWTH_CANDIDATE_COUNT)
            config->structural_growth_candidate_count = int_value;
        else if (field == FIELD_STRUCTURAL_MAX_GROWTH_PER_INTERVAL)
            config->structural_max_growth_per_interval = int_value;
        else if (field == FIELD_STRUCTURAL_NEW_DELAY)
            config->structural_new_delay = int_value;
        else if (field == FIELD_STRUCTURAL_MIN_CONNECTIONS)
            config->structural_min_connections = int_value;
        else if (field == FIELD_STRUCTURAL_MAX_CONNECTIONS)
            config->structural_max_connections = int_value;
        else if (field == FIELD_STRUCTURAL_RECORD_INTERVAL_STEPS)
            config->structural_record_interval_steps = int_value;
        else
            config->record_neuron = int_value;

        return 1;

    case FIELD_INHIBITORY_FRACTION:
    case FIELD_ADEX_CAPACITANCE:
    case FIELD_ADEX_G_LEAK:
    case FIELD_ADEX_E_LEAK:
    case FIELD_ADEX_DELTA_T:
    case FIELD_ADEX_V_THRESHOLD:
    case FIELD_ADEX_TAU_W:
    case FIELD_ADEX_A:
    case FIELD_ADEX_B:
    case FIELD_ADEX_V_RESET:
    case FIELD_ADEX_V_PEAK:
    case FIELD_HH_CAPACITANCE:
    case FIELD_HH_G_NA:
    case FIELD_HH_G_K:
    case FIELD_HH_G_LEAK:
    case FIELD_HH_E_NA:
    case FIELD_HH_E_K:
    case FIELD_HH_E_LEAK:
    case FIELD_HH_V_INIT:
    case FIELD_HH_SPIKE_THRESHOLD:
    case FIELD_CONNECTION_PROBABILITY:
    case FIELD_EXCITATORY_WEIGHT:
    case FIELD_INHIBITORY_WEIGHT:
    case FIELD_INPUT_CURRENT:
    case FIELD_DT:
    case FIELD_TAU:
    case FIELD_V_REST:
    case FIELD_V_RESET:
    case FIELD_V_THRESHOLD:
    case FIELD_RESISTANCE:
    case FIELD_SYNAPTIC_DECAY:
    case FIELD_SMALL_WORLD_REWIRE_PROBABILITY:
    case FIELD_BURST_Z_THRESHOLD:
    case FIELD_PLASTICITY_A_PLUS:
    case FIELD_PLASTICITY_A_MINUS:
    case FIELD_PLASTICITY_TAU_PLUS:
    case FIELD_PLASTICITY_TAU_MINUS:
    case FIELD_PLASTICITY_TRACE_INCREMENT:
    case FIELD_PLASTICITY_WEIGHT_MIN:
    case FIELD_PLASTICITY_WEIGHT_MAX:
    case FIELD_HOMEOSTASIS_TARGET_RATE:
    case FIELD_HOMEOSTASIS_RATE_TAU:
    case FIELD_HOMEOSTASIS_THRESHOLD_ETA:
    case FIELD_HOMEOSTASIS_THRESHOLD_MIN:
    case FIELD_HOMEOSTASIS_THRESHOLD_MAX:
    case FIELD_HOMEOSTASIS_SCALING_ETA:
    case FIELD_HOMEOSTASIS_SCALING_MIN_FACTOR:
    case FIELD_HOMEOSTASIS_SCALING_MAX_FACTOR:
    case FIELD_HOMEOSTASIS_SCALING_WEIGHT_MIN:
    case FIELD_HOMEOSTASIS_SCALING_WEIGHT_MAX:
    case FIELD_HOMEOSTASIS_INHIBITORY_GAIN_INITIAL:
    case FIELD_HOMEOSTASIS_INHIBITORY_GAIN_ETA:
    case FIELD_HOMEOSTASIS_INHIBITORY_GAIN_MIN:
    case FIELD_HOMEOSTASIS_INHIBITORY_GAIN_MAX:
    case FIELD_REWARD_LEARNING_RATE:
    case FIELD_REWARD_ELIGIBILITY_TAU:
    case FIELD_REWARD_ELIGIBILITY_MIN:
    case FIELD_REWARD_ELIGIBILITY_MAX:
    case FIELD_REWARD_MIN:
    case FIELD_REWARD_MAX:
    case FIELD_STRUCTURAL_PRUNE_WEIGHT_THRESHOLD:
    case FIELD_STRUCTURAL_PRUNE_ACTIVITY_THRESHOLD:
    case FIELD_STRUCTURAL_GROWTH_SCORE_THRESHOLD:
    case FIELD_STRUCTURAL_NEW_EXC_WEIGHT:
    case FIELD_STRUCTURAL_NEW_INH_MAGNITUDE:
        if (!parse_double_value(value, &double_value))
        {
            set_line_error(
                error_message,
                error_message_size,
                line_number,
                "valor numerico invalido");
            return 0;
        }

        if (field == FIELD_ADEX_CAPACITANCE)
            config->adex.capacitance = double_value;
        else if (field == FIELD_ADEX_G_LEAK)
            config->adex.g_leak = double_value;
        else if (field == FIELD_ADEX_E_LEAK)
            config->adex.e_leak = double_value;
        else if (field == FIELD_ADEX_DELTA_T)
            config->adex.delta_t = double_value;
        else if (field == FIELD_ADEX_V_THRESHOLD)
            config->adex.v_threshold = double_value;
        else if (field == FIELD_ADEX_TAU_W)
            config->adex.tau_w = double_value;
        else if (field == FIELD_ADEX_A)
            config->adex.a = double_value;
        else if (field == FIELD_ADEX_B)
            config->adex.b = double_value;
        else if (field == FIELD_ADEX_V_RESET)
            config->adex.v_reset = double_value;
        else if (field == FIELD_ADEX_V_PEAK)
            config->adex.v_peak = double_value;
        else if (field == FIELD_HH_CAPACITANCE)
            config->hodgkin_huxley.capacitance = double_value;
        else if (field == FIELD_HH_G_NA)
            config->hodgkin_huxley.g_na = double_value;
        else if (field == FIELD_HH_G_K)
            config->hodgkin_huxley.g_k = double_value;
        else if (field == FIELD_HH_G_LEAK)
            config->hodgkin_huxley.g_leak = double_value;
        else if (field == FIELD_HH_E_NA)
            config->hodgkin_huxley.e_na = double_value;
        else if (field == FIELD_HH_E_K)
            config->hodgkin_huxley.e_k = double_value;
        else if (field == FIELD_HH_E_LEAK)
            config->hodgkin_huxley.e_leak = double_value;
        else if (field == FIELD_HH_V_INIT)
            config->hodgkin_huxley.v_init = double_value;
        else if (field == FIELD_HH_SPIKE_THRESHOLD)
            config->hodgkin_huxley.spike_threshold = double_value;
        else if (field == FIELD_INHIBITORY_FRACTION)
            config->inhibitory_fraction = double_value;
        else if (field == FIELD_CONNECTION_PROBABILITY)
            config->connection_probability = double_value;
        else if (field == FIELD_EXCITATORY_WEIGHT)
            config->excitatory_weight = double_value;
        else if (field == FIELD_INHIBITORY_WEIGHT)
            config->inhibitory_weight = double_value;
        else if (field == FIELD_INPUT_CURRENT)
            config->input_current = double_value;
        else if (field == FIELD_DT)
            config->dt = double_value;
        else if (field == FIELD_TAU)
            config->tau = double_value;
        else if (field == FIELD_V_REST)
            config->v_rest = double_value;
        else if (field == FIELD_V_RESET)
            config->v_reset = double_value;
        else if (field == FIELD_V_THRESHOLD)
            config->v_threshold = double_value;
        else if (field == FIELD_RESISTANCE)
            config->resistance = double_value;
        else if (field == FIELD_SYNAPTIC_DECAY)
            config->synaptic_decay = double_value;
        else if (field == FIELD_BURST_Z_THRESHOLD)
            config->burst_z_threshold = double_value;
        else if (field == FIELD_SMALL_WORLD_REWIRE_PROBABILITY)
            config->small_world_rewire_probability = double_value;
        else if (field == FIELD_PLASTICITY_A_PLUS)
            config->plasticity_a_plus = double_value;
        else if (field == FIELD_PLASTICITY_A_MINUS)
            config->plasticity_a_minus = double_value;
        else if (field == FIELD_PLASTICITY_TAU_PLUS)
            config->plasticity_tau_plus = double_value;
        else if (field == FIELD_PLASTICITY_TAU_MINUS)
            config->plasticity_tau_minus = double_value;
        else if (field == FIELD_PLASTICITY_TRACE_INCREMENT)
            config->plasticity_trace_increment = double_value;
        else if (field == FIELD_PLASTICITY_WEIGHT_MIN)
            config->plasticity_weight_min = double_value;
        else if (field == FIELD_PLASTICITY_WEIGHT_MAX)
            config->plasticity_weight_max = double_value;
        else if (field == FIELD_HOMEOSTASIS_TARGET_RATE)
            config->homeostasis_target_rate = double_value;
        else if (field == FIELD_HOMEOSTASIS_RATE_TAU)
            config->homeostasis_rate_tau = double_value;
        else if (field == FIELD_HOMEOSTASIS_THRESHOLD_ETA)
            config->homeostasis_threshold_eta = double_value;
        else if (field == FIELD_HOMEOSTASIS_THRESHOLD_MIN)
            config->homeostasis_threshold_min = double_value;
        else if (field == FIELD_HOMEOSTASIS_THRESHOLD_MAX)
            config->homeostasis_threshold_max = double_value;
        else if (field == FIELD_HOMEOSTASIS_SCALING_ETA)
            config->homeostasis_scaling_eta = double_value;
        else if (field == FIELD_HOMEOSTASIS_SCALING_MIN_FACTOR)
            config->homeostasis_scaling_min_factor = double_value;
        else if (field == FIELD_HOMEOSTASIS_SCALING_MAX_FACTOR)
            config->homeostasis_scaling_max_factor = double_value;
        else if (field == FIELD_HOMEOSTASIS_SCALING_WEIGHT_MIN)
            config->homeostasis_scaling_weight_min = double_value;
        else if (field == FIELD_HOMEOSTASIS_SCALING_WEIGHT_MAX)
            config->homeostasis_scaling_weight_max = double_value;
        else if (field == FIELD_HOMEOSTASIS_INHIBITORY_GAIN_INITIAL)
            config->homeostasis_inhibitory_gain_initial = double_value;
        else if (field == FIELD_HOMEOSTASIS_INHIBITORY_GAIN_ETA)
            config->homeostasis_inhibitory_gain_eta = double_value;
        else if (field == FIELD_HOMEOSTASIS_INHIBITORY_GAIN_MIN)
            config->homeostasis_inhibitory_gain_min = double_value;
        else if (field == FIELD_HOMEOSTASIS_INHIBITORY_GAIN_MAX)
            config->homeostasis_inhibitory_gain_max = double_value;
        else if (field == FIELD_REWARD_LEARNING_RATE)
            config->reward_learning_rate = double_value;
        else if (field == FIELD_REWARD_ELIGIBILITY_TAU)
            config->reward_eligibility_tau = double_value;
        else if (field == FIELD_REWARD_ELIGIBILITY_MIN)
            config->reward_eligibility_min = double_value;
        else if (field == FIELD_REWARD_ELIGIBILITY_MAX)
            config->reward_eligibility_max = double_value;
        else if (field == FIELD_REWARD_MIN)
            config->reward_min = double_value;
        else if (field == FIELD_STRUCTURAL_PRUNE_WEIGHT_THRESHOLD)
            config->structural_prune_weight_threshold = double_value;
        else if (field == FIELD_STRUCTURAL_PRUNE_ACTIVITY_THRESHOLD)
            config->structural_prune_activity_threshold = double_value;
        else if (field == FIELD_STRUCTURAL_GROWTH_SCORE_THRESHOLD)
            config->structural_growth_score_threshold = double_value;
        else if (field == FIELD_STRUCTURAL_NEW_EXC_WEIGHT)
            config->structural_new_exc_weight = double_value;
        else if (field == FIELD_STRUCTURAL_NEW_INH_MAGNITUDE)
            config->structural_new_inh_magnitude = double_value;
        else
            config->reward_max = double_value;

        return 1;

    default:
        break;
    }

    set_line_error(
        error_message,
        error_message_size,
        line_number,
        "campo invalido");
    return 0;
}

static int topology_is_supported(const char *topology)
{
    return strcmp(topology, "chain") == 0 ||
           strcmp(topology, "ring") == 0 ||
           strcmp(topology, "all_to_all") == 0 ||
           strcmp(topology, "random") == 0 ||
           strcmp(topology, "random_balanced") == 0 ||
           strcmp(topology, "small_world") == 0 ||
           strcmp(topology, "feedforward") == 0;
}

static int run_name_is_valid(const char *run_name)
{
    size_t length = strlen(run_name);

    if (length == 0 || length > SCENARIO_RUN_NAME_MAX)
        return 0;

    for (size_t i = 0; i < length; i++)
    {
        unsigned char c = (unsigned char)run_name[i];

        if (!(isalnum(c) || c == '_' || c == '-'))
            return 0;
    }

    return 1;
}

void scenario_config_default(ScenarioConfig *config)
{
    if (config == NULL)
        return;

    memset(config, 0, sizeof(*config));

    snprintf(config->run_name, sizeof(config->run_name), "random_balanced_demo");
    snprintf(config->topology, sizeof(config->topology), "random_balanced");
    config->neuron_model = MINISNN_NEURON_MODEL_LIF;
    adex_parameters_default(&config->adex);
    hodgkin_huxley_parameters_default(&config->hodgkin_huxley);

    config->neurons = 20;
    config->inhibitory_fraction = 0.20;
    config->connection_probability = 0.25;
    config->seed = 1U;
    config->delay = 1;
    config->max_synaptic_delay = 8;
    config->allow_self_connections = 0;
    config->allow_inh_to_inh = 1;

    config->excitatory_weight = 200.0;
    config->inhibitory_weight = -400.0;

    config->source_count = 3;
    config->input_current = 20.0;

    config->steps = 1000;
    config->dt = 0.1;
    config->tau = 20.0;
    config->v_rest = -65.0;
    config->v_reset = -65.0;
    config->v_threshold = -50.0;
    config->resistance = 1.0;
    config->synaptic_decay = 0.95;
    config->small_world_neighbors = 4;
    config->small_world_rewire_probability = 0.10;
    config->feedforward_layers = 3;

    config->record_neuron = 0;
    config->auto_unique_run = 0;
    config->history_enabled = 1;

    snprintf(config->diagnostics_level, sizeof(config->diagnostics_level), "basic");
    config->time_bin_steps = 10;
    config->burst_z_threshold = 2.0;
    config->min_burst_steps = 1;
    config->isi_min_spikes = 4;
    config->correlation_sample_size = 128;
    config->neuron_sample_limit = 1000;
    config->sample_stride = 1;

    config->plasticity_enabled = 0;
    snprintf(
        config->plasticity_rule,
        sizeof(config->plasticity_rule),
        "stdp_pair_trace");
    snprintf(
        config->plasticity_learning_mode,
        sizeof(config->plasticity_learning_mode),
        "direct_stdp");
    config->plasticity_a_plus = 1.0;
    config->plasticity_a_minus = 1.05;
    config->plasticity_tau_plus = 20.0;
    config->plasticity_tau_minus = 20.0;
    config->plasticity_trace_increment = 1.0;
    config->plasticity_weight_min = 0.0;
    config->plasticity_weight_max = 200.0;
    config->plasticity_record_weights = 1;
    config->plasticity_record_history = 1;
    config->plasticity_record_interval_steps = 10;
    config->plasticity_record_connection_limit = 256;

    config->homeostasis_enabled = 0;
    config->homeostasis_intrinsic_enabled = 1;
    config->homeostasis_target_rate = 0.05;
    config->homeostasis_rate_tau = 100.0;
    config->homeostasis_update_interval_steps = 10;
    config->homeostasis_threshold_eta = 0.05;
    config->homeostasis_threshold_min = -60.0;
    config->homeostasis_threshold_max = -40.0;
    config->homeostasis_synaptic_scaling_enabled = 0;
    snprintf(
        config->homeostasis_scaling_target_mode,
        sizeof(config->homeostasis_scaling_target_mode),
        "initial_incoming_sum");
    config->homeostasis_scaling_eta = 0.10;
    config->homeostasis_scaling_min_factor = 0.50;
    config->homeostasis_scaling_max_factor = 2.0;
    config->homeostasis_scaling_weight_min = 0.0;
    config->homeostasis_scaling_weight_max = 1000.0;
    config->homeostasis_inhibitory_gain_enabled = 0;
    config->homeostasis_inhibitory_gain_initial = 1.0;
    config->homeostasis_inhibitory_gain_eta = 0.05;
    config->homeostasis_inhibitory_gain_min = 0.25;
    config->homeostasis_inhibitory_gain_max = 4.0;
    config->homeostasis_record_history = 1;
    config->homeostasis_record_interval_steps = 10;
    config->homeostasis_record_neuron_limit = 256;

    config->structural_plasticity_enabled = 0;
    config->structural_maintenance_interval_steps = 100;
    config->structural_grace_period_steps = 500;
    config->structural_pruning_enabled = 1;
    config->structural_prune_weight_threshold = 0.50;
    config->structural_prune_activity_threshold = 0.001;
    config->structural_max_prunes_per_interval = 1;
    config->structural_growth_enabled = 1;
    config->structural_growth_candidate_count = 16;
    config->structural_growth_score_threshold = 0.010;
    config->structural_max_growth_per_interval = 1;
    config->structural_growth_seed = 9001U;
    config->structural_new_exc_weight = 5.0;
    config->structural_new_inh_magnitude = 5.0;
    config->structural_new_delay = 1;
    config->structural_min_connections = 4;
    config->structural_max_connections = 64;
    config->structural_record_history = 1;
    config->structural_record_interval_steps = 100;

    config->reward_enabled = 0;
    snprintf(config->reward_mode, sizeof(config->reward_mode), "rstdp");
    config->reward_learning_rate = 1.0;
    config->reward_eligibility_tau = 100.0;
    config->reward_eligibility_min = -200.0;
    config->reward_eligibility_max = 200.0;
    config->reward_min = -1.0;
    config->reward_max = 1.0;
    config->reward_clip = 1;
    config->reward_record_history = 1;
    config->reward_record_interval_steps = 10;
    config->reward_record_connection_limit = 256;
    config->reward_event_count = 0;
}

int scenario_config_validate(
    const ScenarioConfig *config,
    char *error_message,
    size_t error_message_size)
{
    if (config == NULL)
    {
        set_error(error_message, error_message_size, "configuracao nula");
        return 0;
    }

    if (!run_name_is_valid(config->run_name))
    {
        set_error(error_message, error_message_size, "run_name invalido");
        return 0;
    }

    if (!topology_is_supported(config->topology))
    {
        set_error(error_message, error_message_size, "topologia invalida");
        return 0;
    }

    if (strcmp(config->diagnostics_level, "off") != 0 &&
        strcmp(config->diagnostics_level, "basic") != 0 &&
        strcmp(config->diagnostics_level, "full") != 0)
    {
        set_error(error_message, error_message_size, "diagnostics level invalido");
        return 0;
    }

    if (config->time_bin_steps <= 0 ||
        config->min_burst_steps <= 0 ||
        config->isi_min_spikes <= 0 ||
        config->correlation_sample_size <= 0 ||
        config->neuron_sample_limit <= 0 ||
        config->sample_stride <= 0 ||
        !isfinite(config->burst_z_threshold) ||
        config->burst_z_threshold < 0.0)
    {
        set_error(error_message, error_message_size, "parametros de diagnostico invalidos");
        return 0;
    }

    if (config->plasticity_enabled != 0 &&
        config->plasticity_enabled != 1)
    {
        set_error(error_message, error_message_size, "plasticity enabled invalido");
        return 0;
    }

    if (strcmp(config->plasticity_rule, "stdp_pair_trace") != 0)
    {
        set_error(error_message, error_message_size, "plasticity rule invalida");
        return 0;
    }

    if (strcmp(config->plasticity_learning_mode, "direct_stdp") != 0 &&
        strcmp(
            config->plasticity_learning_mode,
            "reward_modulated_stdp") != 0)
    {
        set_error(error_message, error_message_size, "learning_mode invalido");
        return 0;
    }

    if (!isfinite(config->plasticity_a_plus) ||
        config->plasticity_a_plus < 0.0 ||
        !isfinite(config->plasticity_a_minus) ||
        config->plasticity_a_minus < 0.0 ||
        !isfinite(config->plasticity_tau_plus) ||
        config->plasticity_tau_plus <= 0.0 ||
        !isfinite(config->plasticity_tau_minus) ||
        config->plasticity_tau_minus <= 0.0 ||
        !isfinite(config->plasticity_trace_increment) ||
        config->plasticity_trace_increment <= 0.0 ||
        !isfinite(config->plasticity_weight_min) ||
        config->plasticity_weight_min < 0.0 ||
        !isfinite(config->plasticity_weight_max) ||
        config->plasticity_weight_max <= config->plasticity_weight_min)
    {
        set_error(error_message, error_message_size, "parametros de plasticidade invalidos");
        return 0;
    }

    if ((config->plasticity_record_weights != 0 &&
         config->plasticity_record_weights != 1) ||
        (config->plasticity_record_history != 0 &&
         config->plasticity_record_history != 1) ||
        config->plasticity_record_interval_steps <= 0 ||
        config->plasticity_record_connection_limit <= 0)
    {
        set_error(error_message, error_message_size, "registro de plasticidade invalido");
        return 0;
    }

    if ((config->homeostasis_enabled != 0 &&
         config->homeostasis_enabled != 1) ||
        (config->homeostasis_intrinsic_enabled != 0 &&
         config->homeostasis_intrinsic_enabled != 1) ||
        (config->homeostasis_synaptic_scaling_enabled != 0 &&
         config->homeostasis_synaptic_scaling_enabled != 1) ||
        (config->homeostasis_inhibitory_gain_enabled != 0 &&
         config->homeostasis_inhibitory_gain_enabled != 1) ||
        (config->homeostasis_record_history != 0 &&
         config->homeostasis_record_history != 1))
    {
        set_error(error_message, error_message_size, "opcoes de homeostase invalidas");
        return 0;
    }

    if (!isfinite(config->homeostasis_target_rate) ||
        config->homeostasis_target_rate < 0.0 ||
        !isfinite(config->homeostasis_rate_tau) ||
        config->homeostasis_rate_tau <= 0.0 ||
        config->homeostasis_update_interval_steps <= 0 ||
        !isfinite(config->homeostasis_threshold_eta) ||
        config->homeostasis_threshold_eta < 0.0 ||
        !isfinite(config->homeostasis_threshold_min) ||
        !isfinite(config->homeostasis_threshold_max) ||
        config->homeostasis_threshold_max <= config->homeostasis_threshold_min ||
        (config->homeostasis_enabled && config->homeostasis_intrinsic_enabled &&
         (config->v_threshold < config->homeostasis_threshold_min ||
          config->v_threshold > config->homeostasis_threshold_max)))
    {
        set_error(error_message, error_message_size, "parametros intrinsecos de homeostase invalidos");
        return 0;
    }

    if (strcmp(
            config->homeostasis_scaling_target_mode,
            "initial_incoming_sum") != 0 ||
        !isfinite(config->homeostasis_scaling_eta) ||
        config->homeostasis_scaling_eta < 0.0 ||
        config->homeostasis_scaling_eta > 1.0 ||
        !isfinite(config->homeostasis_scaling_min_factor) ||
        config->homeostasis_scaling_min_factor <= 0.0 ||
        !isfinite(config->homeostasis_scaling_max_factor) ||
        config->homeostasis_scaling_max_factor <
            config->homeostasis_scaling_min_factor ||
        !isfinite(config->homeostasis_scaling_weight_min) ||
        config->homeostasis_scaling_weight_min < 0.0 ||
        !isfinite(config->homeostasis_scaling_weight_max) ||
        config->homeostasis_scaling_weight_max <=
            config->homeostasis_scaling_weight_min)
    {
        set_error(error_message, error_message_size, "parametros de scaling homeostatico invalidos");
        return 0;
    }

    if (!isfinite(config->homeostasis_inhibitory_gain_initial) ||
        config->homeostasis_inhibitory_gain_initial <= 0.0 ||
        !isfinite(config->homeostasis_inhibitory_gain_eta) ||
        config->homeostasis_inhibitory_gain_eta < 0.0 ||
        !isfinite(config->homeostasis_inhibitory_gain_min) ||
        config->homeostasis_inhibitory_gain_min <= 0.0 ||
        !isfinite(config->homeostasis_inhibitory_gain_max) ||
        config->homeostasis_inhibitory_gain_max <
            config->homeostasis_inhibitory_gain_min ||
        config->homeostasis_inhibitory_gain_initial <
            config->homeostasis_inhibitory_gain_min ||
        config->homeostasis_inhibitory_gain_initial >
            config->homeostasis_inhibitory_gain_max ||
        config->homeostasis_record_interval_steps <= 0 ||
        config->homeostasis_record_neuron_limit <= 0)
    {
        set_error(error_message, error_message_size, "parametros de ganho/registro homeostatico invalidos");
        return 0;
    }

    if (config->homeostasis_enabled &&
        !config->homeostasis_intrinsic_enabled &&
        !config->homeostasis_synaptic_scaling_enabled &&
        !config->homeostasis_inhibitory_gain_enabled)
    {
        set_error(error_message, error_message_size, "homeostase ativa sem mecanismo habilitado");
        return 0;
    }

    if ((config->reward_enabled != 0 && config->reward_enabled != 1) ||
        (config->reward_clip != 0 && config->reward_clip != 1) ||
        (config->reward_record_history != 0 &&
         config->reward_record_history != 1) ||
        strcmp(config->reward_mode, "rstdp") != 0 ||
        !isfinite(config->reward_learning_rate) ||
        config->reward_learning_rate < 0.0 ||
        !isfinite(config->reward_eligibility_tau) ||
        config->reward_eligibility_tau <= 0.0 ||
        !isfinite(config->reward_eligibility_min) ||
        config->reward_eligibility_min >= 0.0 ||
        !isfinite(config->reward_eligibility_max) ||
        config->reward_eligibility_max <= 0.0 ||
        config->reward_eligibility_max <= config->reward_eligibility_min ||
        !isfinite(config->reward_min) || config->reward_min > 0.0 ||
        !isfinite(config->reward_max) || config->reward_max < 0.0 ||
        config->reward_max <= config->reward_min ||
        config->reward_record_interval_steps <= 0 ||
        config->reward_record_connection_limit <= 0)
    {
        set_error(error_message, error_message_size, "parametros de reward invalidos");
        return 0;
    }

    if ((config->reward_enabled &&
         (!config->plasticity_enabled ||
          strcmp(
              config->plasticity_learning_mode,
              "reward_modulated_stdp") != 0)) ||
        (!config->reward_enabled &&
         strcmp(
             config->plasticity_learning_mode,
             "reward_modulated_stdp") == 0))
    {
        set_error(
            error_message,
            error_message_size,
            "combinacao incompativel entre plasticity e reward");
        return 0;
    }

    if (config->neurons < 1 || config->neurons > 1000)
    {
        set_error(error_message, error_message_size, "neurons deve estar entre 1 e 1000");
        return 0;
    }

    if ((config->structural_plasticity_enabled != 0 &&
         config->structural_plasticity_enabled != 1) ||
        (config->structural_pruning_enabled != 0 &&
         config->structural_pruning_enabled != 1) ||
        (config->structural_growth_enabled != 0 &&
         config->structural_growth_enabled != 1) ||
        (config->structural_record_history != 0 &&
         config->structural_record_history != 1))
    {
        set_error(error_message, error_message_size,
                  "booleano de plasticidade estrutural invalido");
        return 0;
    }

    if (config->structural_plasticity_enabled)
    {
        int inhibitory_count;
        size_t possible = (size_t)config->neurons * (size_t)config->neurons;
        if (!isfinite(config->inhibitory_fraction) ||
            config->inhibitory_fraction < 0.0 ||
            config->inhibitory_fraction > 1.0)
        {
            set_error(error_message, error_message_size,
                      "inhibitory_fraction invalido");
            return 0;
        }
        inhibitory_count = (int)(
            (double)config->neurons * config->inhibitory_fraction + 0.5);
        if (!config->allow_self_connections)
            possible -= (size_t)config->neurons;
        if (!config->allow_inh_to_inh)
        {
            size_t inh_pairs = config->allow_self_connections ?
                (size_t)inhibitory_count * (size_t)inhibitory_count :
                (size_t)inhibitory_count *
                    (size_t)(inhibitory_count > 0 ? inhibitory_count - 1 : 0);
            possible -= inh_pairs;
        }
        if ((!config->structural_pruning_enabled &&
             !config->structural_growth_enabled) ||
            config->structural_maintenance_interval_steps <= 0 ||
            config->structural_grace_period_steps <
                config->structural_maintenance_interval_steps ||
            !isfinite(config->structural_prune_weight_threshold) ||
            config->structural_prune_weight_threshold < 0.0 ||
            !isfinite(config->structural_prune_activity_threshold) ||
            config->structural_prune_activity_threshold < 0.0 ||
            config->structural_max_prunes_per_interval <= 0 ||
            config->structural_growth_candidate_count <= 0 ||
            !isfinite(config->structural_growth_score_threshold) ||
            config->structural_growth_score_threshold < 0.0 ||
            config->structural_max_growth_per_interval <= 0 ||
            !isfinite(config->structural_new_exc_weight) ||
            config->structural_new_exc_weight < 0.0 ||
            !isfinite(config->structural_new_inh_magnitude) ||
            config->structural_new_inh_magnitude < 0.0 ||
            config->structural_new_delay < 1 ||
            config->structural_new_delay > config->max_synaptic_delay ||
            config->structural_min_connections < 1 ||
            config->structural_max_connections <
                config->structural_min_connections ||
            (size_t)config->structural_max_connections > possible ||
            config->structural_record_interval_steps <= 0)
        {
            set_error(error_message, error_message_size,
                      "parametros de plasticidade estrutural invalidos");
            return 0;
        }
    }

    if (config->steps <= 0)
    {
        set_error(error_message, error_message_size, "steps deve ser maior que zero");
        return 0;
    }

    if (config->reward_event_count < 0 ||
        config->reward_event_count > SCENARIO_MAX_REWARD_EVENTS)
    {
        set_error(error_message, error_message_size, "quantidade de reward_events invalida");
        return 0;
    }

    {
        unsigned char seen_events[SCENARIO_MAX_REWARD_EVENTS] = {0};

        for (int i = 0; i < config->reward_event_count; i++)
        {
            const ScenarioRewardEvent *event = &config->reward_events[i];

            if (event->index < 0 || event->index >= config->reward_event_count ||
                seen_events[event->index] || event->step < 0 ||
                event->step >= config->steps || !isfinite(event->value))
            {
                set_error(error_message, error_message_size, "reward_event invalido");
                return 0;
            }
            seen_events[event->index] = 1U;
        }

        for (int i = 0; i < config->reward_event_count; i++)
        {
            if (!seen_events[i])
            {
                set_error(error_message, error_message_size, "indice de reward_event ausente");
                return 0;
            }
        }
    }

    if (config->reward_event_count > 0 && !config->reward_enabled)
    {
        set_error(error_message, error_message_size, "reward_events sem reward habilitado");
        return 0;
    }

    if (config->source_count < 1 || config->source_count > config->neurons)
    {
        set_error(error_message, error_message_size, "source_count invalido");
        return 0;
    }

    if (config->record_neuron < 0 || config->record_neuron >= config->neurons)
    {
        set_error(error_message, error_message_size, "record_neuron invalido");
        return 0;
    }

    if (config->inhibitory_fraction < 0.0 ||
        config->inhibitory_fraction > 1.0)
    {
        set_error(error_message, error_message_size, "inhibitory_fraction invalido");
        return 0;
    }

    if (config->connection_probability < 0.0 ||
        config->connection_probability > 1.0)
    {
        set_error(error_message, error_message_size, "connection_probability invalido");
        return 0;
    }

    if (config->max_synaptic_delay <= 0)
    {
        set_error(error_message, error_message_size, "max_synaptic_delay deve ser maior que zero");
        return 0;
    }

    if (config->delay < 1 || config->delay > config->max_synaptic_delay)
    {
        set_error(error_message, error_message_size, "delay invalido");
        return 0;
    }

    if ((config->allow_self_connections != 0 &&
         config->allow_self_connections != 1) ||
        (config->allow_inh_to_inh != 0 &&
         config->allow_inh_to_inh != 1) ||
        (config->auto_unique_run != 0 &&
         config->auto_unique_run != 1) ||
        (config->history_enabled != 0 &&
         config->history_enabled != 1))
    {
        set_error(error_message, error_message_size, "opcoes booleanas invalidas");
        return 0;
    }

    if (strcmp(config->topology, "small_world") == 0 &&
        (config->small_world_neighbors <= 0 ||
         config->small_world_neighbors >= config->neurons ||
         (config->small_world_neighbors % 2) != 0))
    {
        set_error(error_message, error_message_size, "small_world_neighbors invalido");
        return 0;
    }

    if (strcmp(config->topology, "small_world") == 0 &&
        (config->small_world_rewire_probability < 0.0 ||
         config->small_world_rewire_probability > 1.0))
    {
        set_error(error_message, error_message_size, "small_world_rewire_probability invalido");
        return 0;
    }

    if (strcmp(config->topology, "feedforward") == 0 &&
        (config->feedforward_layers < 2 ||
         config->feedforward_layers > config->neurons))
    {
        set_error(error_message, error_message_size, "feedforward_layers invalido");
        return 0;
    }

    if (config->excitatory_weight <= 0.0)
    {
        set_error(error_message, error_message_size, "excitatory_weight deve ser positivo");
        return 0;
    }

    if (config->inhibitory_weight >= 0.0)
    {
        set_error(error_message, error_message_size, "inhibitory_weight deve ser negativo");
        return 0;
    }

    {
        NeuronModelConfig model_config;
        if (config->neuron_model == MINISNN_NEURON_MODEL_LIF)
        {
            LIFParameters lif = {config->dt, config->tau, config->v_rest,
                config->v_reset, config->v_threshold, config->resistance};
            neuron_model_config_lif(&model_config, &lif);
        }
        else if (config->neuron_model == MINISNN_NEURON_MODEL_ADEX)
        {
            AdExParameters adex = config->adex;
            adex.dt = config->dt;
            neuron_model_config_adex(&model_config, &adex);
        }
        else if (config->neuron_model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY)
        {
            HodgkinHuxleyParameters hh = config->hodgkin_huxley;
            hh.dt = config->dt;
            neuron_model_config_hodgkin_huxley(&model_config, &hh);
        }
        else
        {
            set_error(error_message, error_message_size, "model desconhecido");
            return 0;
        }
        if (!neuron_model_validate_config(&model_config))
        {
            set_error(error_message, error_message_size,
                      "parametros do modelo neuronal invalidos");
            return 0;
        }
    }

    if (config->dt <= 0.0 || config->tau <= 0.0 ||
        config->resistance <= 0.0 || config->synaptic_decay <= 0.0)
    {
        set_error(error_message, error_message_size, "dt, tau, resistance e synaptic_decay devem ser positivos");
        return 0;
    }

    if (!isfinite(config->inhibitory_fraction) ||
        !isfinite(config->connection_probability) ||
        !isfinite(config->excitatory_weight) ||
        !isfinite(config->inhibitory_weight) ||
        !isfinite(config->input_current) ||
        !isfinite(config->dt) ||
        !isfinite(config->tau) ||
        !isfinite(config->v_rest) ||
        !isfinite(config->v_reset) ||
        !isfinite(config->v_threshold) ||
        !isfinite(config->resistance) ||
        !isfinite(config->synaptic_decay) ||
        !isfinite(config->small_world_rewire_probability))
    {
        set_error(error_message, error_message_size, "valores numericos devem ser finitos");
        return 0;
    }

    return 1;
}

int scenario_config_load_file(
    const char *filename,
    ScenarioConfig *out_config,
    char *error_message,
    size_t error_message_size)
{
    FILE *file;
    ScenarioConfig config;
    unsigned char seen_fields[FIELD_COUNT];
    char current_section[64] = "";
    char line[512];
    int line_number = 0;

    if (filename == NULL || out_config == NULL)
    {
        set_error(error_message, error_message_size, "argumento nulo");
        return 0;
    }

    scenario_config_default(&config);
    memset(seen_fields, 0, sizeof(seen_fields));
    snprintf(config.diagnostics_level, sizeof(config.diagnostics_level), "off");

    file = fopen(filename, "r");

    if (file == NULL)
    {
        set_error(error_message, error_message_size, "nao foi possivel abrir arquivo de cenario");
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *text;
        char *equal;
        char *key;
        char *value;
        ScenarioField field;

        line_number++;

        if (strchr(line, '\n') == NULL && !feof(file))
        {
            fclose(file);
            set_line_error(
                error_message,
                error_message_size,
                line_number,
                "linha muito longa");
            return 0;
        }

        remove_comment(line);
        text = trim(line);

        if (*text == '\0')
            continue;

        if (*text == '[')
        {
            size_t length = strlen(text);

            if (length < 2 || text[length - 1] != ']')
            {
                fclose(file);
                set_line_error(
                    error_message,
                    error_message_size,
                    line_number,
                    "secao invalida");
                return 0;
            }

            text[length - 1] = '\0';
            if (!copy_string_value(
                    current_section,
                    sizeof(current_section),
                    trim(text + 1)))
            {
                fclose(file);
                set_line_error(
                    error_message,
                    error_message_size,
                    line_number,
                    "nome de secao muito longo");
                return 0;
            }
            continue;
        }

        equal = strchr(text, '=');

        if (equal == NULL)
        {
            fclose(file);
            set_line_error(
                error_message,
                error_message_size,
                line_number,
                "linha sem '='");
            return 0;
        }

        *equal = '\0';
        key = trim(text);
        value = trim(equal + 1);

        if (*key == '\0')
        {
            fclose(file);
            set_line_error(
                error_message,
                error_message_size,
                line_number,
                "chave vazia");
            return 0;
        }

        if (strcmp(current_section, "reward_events") == 0)
        {
            ScenarioRewardEvent event;

            if (!parse_reward_event_key(key, &event.index) ||
                !parse_reward_event_value(value, &event.step, &event.value))
            {
                fclose(file);
                set_line_error(
                    error_message,
                    error_message_size,
                    line_number,
                    "reward_event malformado");
                return 0;
            }

            if (config.reward_event_count >= SCENARIO_MAX_REWARD_EVENTS)
            {
                fclose(file);
                set_line_error(
                    error_message,
                    error_message_size,
                    line_number,
                    "limite de reward_events excedido");
                return 0;
            }

            for (int i = 0; i < config.reward_event_count; i++)
            {
                if (config.reward_events[i].index == event.index)
                {
                    fclose(file);
                    set_line_error(
                        error_message,
                        error_message_size,
                        line_number,
                        "indice de reward_event duplicado");
                    return 0;
                }
            }

            config.reward_events[config.reward_event_count++] = event;
            continue;
        }

        if (!find_field(current_section, key, &field))
        {
            char message[160];
            snprintf(message, sizeof(message), "chave desconhecida '%s'", key);
            fclose(file);
            set_line_error(error_message, error_message_size, line_number, message);
            return 0;
        }

        if (seen_fields[field])
        {
            char message[160];
            snprintf(message, sizeof(message), "chave duplicada '%s'", key);
            fclose(file);
            set_line_error(error_message, error_message_size, line_number, message);
            return 0;
        }

        seen_fields[field] = 1U;

        if (!assign_value(
                &config,
                field,
                value,
                line_number,
                error_message,
                error_message_size))
        {
            fclose(file);
            return 0;
        }
    }

    fclose(file);

    if (config.reward_event_count > 1)
    {
        qsort(
            config.reward_events,
            (size_t)config.reward_event_count,
            sizeof(config.reward_events[0]),
            reward_event_compare);
    }

    if (!scenario_config_validate(&config, error_message, error_message_size))
        return 0;

    *out_config = config;
    return 1;
}

int scenario_config_save_file(
    const char *filename,
    const ScenarioConfig *config,
    char *error_message,
    size_t error_message_size)
{
    FILE *file;

    if (filename == NULL || config == NULL)
    {
        set_error(error_message, error_message_size, "argumento nulo");
        return 0;
    }

    if (!scenario_config_validate(config, error_message, error_message_size))
        return 0;

    file = fopen(filename, "w");

    if (file == NULL)
    {
        set_error(error_message, error_message_size, "nao foi possivel abrir arquivo para escrita");
        return 0;
    }

    if (fprintf(
            file,
            "[run]\n"
            "run_name = %s\n"
            "\n"
            "[network]\n"
            "topology = %s\n"
            "neurons = %d\n"
            "inhibitory_fraction = %.17g\n"
            "connection_probability = %.17g\n"
            "seed = %u\n"
            "delay = %d\n"
            "max_synaptic_delay = %d\n"
            "\n"
            "[neuron]\n"
            "model = %s\n"
            "\n"
            "[connectivity]\n"
            "allow_self_connections = %s\n"
            "allow_inh_to_inh = %s\n"
            "\n"
            "[weights]\n"
            "excitatory_weight = %.17g\n"
            "inhibitory_weight = %.17g\n"
            "\n"
            "[input]\n"
            "source_count = %d\n"
            "input_current = %.17g\n"
            "\n"
            "[simulation]\n"
            "steps = %d\n"
            "dt = %.17g\n"
            "tau = %.17g\n"
            "v_rest = %.17g\n"
            "v_reset = %.17g\n"
            "v_threshold = %.17g\n"
            "resistance = %.17g\n"
            "synaptic_decay = %.17g\n"
            "\n"
            "[topology_options]\n"
            "small_world_neighbors = %d\n"
            "small_world_rewire_probability = %.17g\n"
            "feedforward_layers = %d\n"
            "\n"
            "[recording]\n"
            "record_neuron = %d\n"
            "\n"
            "[output]\n"
            "auto_unique_run = %s\n"
            "history_enabled = %s\n"
            "\n"
            "[diagnostics]\n"
            "level = %s\n"
            "time_bin_steps = %d\n"
            "burst_z_threshold = %.17g\n"
            "min_burst_steps = %d\n"
            "isi_min_spikes = %d\n"
            "correlation_sample_size = %d\n"
            "neuron_sample_limit = %d\n"
            "sample_stride = %d\n"
            "\n"
            "[plasticity]\n"
            "enabled = %s\n"
            "rule = %s\n"
            "learning_mode = %s\n"
            "a_plus = %.17g\n"
            "a_minus = %.17g\n"
            "tau_plus = %.17g\n"
            "tau_minus = %.17g\n"
            "trace_increment = %.17g\n"
            "weight_min = %.17g\n"
            "weight_max = %.17g\n"
            "record_weights = %s\n"
            "record_history = %s\n"
            "record_interval_steps = %d\n"
            "record_connection_limit = %d\n",
            config->run_name,
            config->topology,
            config->neurons,
            config->inhibitory_fraction,
            config->connection_probability,
            config->seed,
            config->delay,
            config->max_synaptic_delay,
            neuron_model_name(config->neuron_model),
            config->allow_self_connections ? "true" : "false",
            config->allow_inh_to_inh ? "true" : "false",
            config->excitatory_weight,
            config->inhibitory_weight,
            config->source_count,
            config->input_current,
            config->steps,
            config->dt,
            config->tau,
            config->v_rest,
            config->v_reset,
            config->v_threshold,
            config->resistance,
            config->synaptic_decay,
            config->small_world_neighbors,
            config->small_world_rewire_probability,
            config->feedforward_layers,
            config->record_neuron,
            config->auto_unique_run ? "true" : "false",
            config->history_enabled ? "true" : "false",
            config->diagnostics_level,
            config->time_bin_steps,
            config->burst_z_threshold,
            config->min_burst_steps,
            config->isi_min_spikes,
            config->correlation_sample_size,
            config->neuron_sample_limit,
            config->sample_stride,
            config->plasticity_enabled ? "true" : "false",
            config->plasticity_rule,
            config->plasticity_learning_mode,
            config->plasticity_a_plus,
            config->plasticity_a_minus,
            config->plasticity_tau_plus,
            config->plasticity_tau_minus,
            config->plasticity_trace_increment,
            config->plasticity_weight_min,
            config->plasticity_weight_max,
            config->plasticity_record_weights ? "true" : "false",
            config->plasticity_record_history ? "true" : "false",
            config->plasticity_record_interval_steps,
            config->plasticity_record_connection_limit) < 0)
    {
        fclose(file);
        set_error(error_message, error_message_size, "erro ao escrever arquivo de cenario");
        return 0;
    }

    if (fprintf(
            file,
            "\n"
            "[adex]\n"
            "capacitance = %.17g\n"
            "g_leak = %.17g\n"
            "e_leak = %.17g\n"
            "delta_t = %.17g\n"
            "v_threshold = %.17g\n"
            "tau_w = %.17g\n"
            "a = %.17g\n"
            "b = %.17g\n"
            "v_reset = %.17g\n"
            "v_peak = %.17g\n"
            "\n"
            "[hodgkin_huxley]\n"
            "capacitance = %.17g\n"
            "g_na = %.17g\n"
            "g_k = %.17g\n"
            "g_leak = %.17g\n"
            "e_na = %.17g\n"
            "e_k = %.17g\n"
            "e_leak = %.17g\n"
            "v_init = %.17g\n"
            "spike_threshold = %.17g\n",
            config->adex.capacitance,
            config->adex.g_leak,
            config->adex.e_leak,
            config->adex.delta_t,
            config->adex.v_threshold,
            config->adex.tau_w,
            config->adex.a,
            config->adex.b,
            config->adex.v_reset,
            config->adex.v_peak,
            config->hodgkin_huxley.capacitance,
            config->hodgkin_huxley.g_na,
            config->hodgkin_huxley.g_k,
            config->hodgkin_huxley.g_leak,
            config->hodgkin_huxley.e_na,
            config->hodgkin_huxley.e_k,
            config->hodgkin_huxley.e_leak,
            config->hodgkin_huxley.v_init,
            config->hodgkin_huxley.spike_threshold) < 0)
    {
        fclose(file);
        set_error(error_message, error_message_size,
                  "erro ao escrever configuracao neuronal");
        return 0;
    }

    if (fprintf(
            file,
            "\n"
            "[structural_plasticity]\n"
            "enabled = %s\n"
            "maintenance_interval_steps = %d\n"
            "grace_period_steps = %d\n"
            "pruning_enabled = %s\n"
            "prune_weight_threshold = %.17g\n"
            "prune_activity_threshold = %.17g\n"
            "max_prunes_per_interval = %d\n"
            "growth_enabled = %s\n"
            "growth_candidate_count = %d\n"
            "growth_score_threshold = %.17g\n"
            "max_growth_per_interval = %d\n"
            "growth_seed = %llu\n"
            "new_exc_weight = %.17g\n"
            "new_inh_magnitude = %.17g\n"
            "new_delay = %d\n"
            "min_connections = %d\n"
            "max_connections = %d\n"
            "record_history = %s\n"
            "record_interval_steps = %d\n",
            config->structural_plasticity_enabled ? "true" : "false",
            config->structural_maintenance_interval_steps,
            config->structural_grace_period_steps,
            config->structural_pruning_enabled ? "true" : "false",
            config->structural_prune_weight_threshold,
            config->structural_prune_activity_threshold,
            config->structural_max_prunes_per_interval,
            config->structural_growth_enabled ? "true" : "false",
            config->structural_growth_candidate_count,
            config->structural_growth_score_threshold,
            config->structural_max_growth_per_interval,
            (unsigned long long)config->structural_growth_seed,
            config->structural_new_exc_weight,
            config->structural_new_inh_magnitude,
            config->structural_new_delay,
            config->structural_min_connections,
            config->structural_max_connections,
            config->structural_record_history ? "true" : "false",
            config->structural_record_interval_steps) < 0)
    {
        fclose(file);
        set_error(error_message, error_message_size,
                  "erro ao escrever plasticidade estrutural");
        return 0;
    }

    if (fprintf(
            file,
            "\n"
            "[homeostasis]\n"
            "enabled = %s\n"
            "intrinsic_enabled = %s\n"
            "target_rate = %.17g\n"
            "rate_tau = %.17g\n"
            "update_interval_steps = %d\n"
            "threshold_eta = %.17g\n"
            "threshold_min = %.17g\n"
            "threshold_max = %.17g\n"
            "synaptic_scaling_enabled = %s\n"
            "scaling_target_mode = %s\n"
            "scaling_eta = %.17g\n"
            "scaling_min_factor = %.17g\n"
            "scaling_max_factor = %.17g\n"
            "scaling_weight_min = %.17g\n"
            "scaling_weight_max = %.17g\n"
            "inhibitory_gain_enabled = %s\n"
            "inhibitory_gain_initial = %.17g\n"
            "inhibitory_gain_eta = %.17g\n"
            "inhibitory_gain_min = %.17g\n"
            "inhibitory_gain_max = %.17g\n"
            "record_history = %s\n"
            "record_interval_steps = %d\n"
            "record_neuron_limit = %d\n",
            config->homeostasis_enabled ? "true" : "false",
            config->homeostasis_intrinsic_enabled ? "true" : "false",
            config->homeostasis_target_rate,
            config->homeostasis_rate_tau,
            config->homeostasis_update_interval_steps,
            config->homeostasis_threshold_eta,
            config->homeostasis_threshold_min,
            config->homeostasis_threshold_max,
            config->homeostasis_synaptic_scaling_enabled ? "true" : "false",
            config->homeostasis_scaling_target_mode,
            config->homeostasis_scaling_eta,
            config->homeostasis_scaling_min_factor,
            config->homeostasis_scaling_max_factor,
            config->homeostasis_scaling_weight_min,
            config->homeostasis_scaling_weight_max,
            config->homeostasis_inhibitory_gain_enabled ? "true" : "false",
            config->homeostasis_inhibitory_gain_initial,
            config->homeostasis_inhibitory_gain_eta,
            config->homeostasis_inhibitory_gain_min,
            config->homeostasis_inhibitory_gain_max,
            config->homeostasis_record_history ? "true" : "false",
            config->homeostasis_record_interval_steps,
            config->homeostasis_record_neuron_limit) < 0)
    {
        fclose(file);
        set_error(error_message, error_message_size, "erro ao escrever homeostase");
        return 0;
    }

    if (fprintf(
            file,
            "\n"
            "[reward]\n"
            "enabled = %s\n"
            "mode = %s\n"
            "learning_rate = %.17g\n"
            "eligibility_tau = %.17g\n"
            "eligibility_min = %.17g\n"
            "eligibility_max = %.17g\n"
            "reward_min = %.17g\n"
            "reward_max = %.17g\n"
            "clip_reward = %s\n"
            "record_history = %s\n"
            "record_interval_steps = %d\n"
            "record_connection_limit = %d\n",
            config->reward_enabled ? "true" : "false",
            config->reward_mode,
            config->reward_learning_rate,
            config->reward_eligibility_tau,
            config->reward_eligibility_min,
            config->reward_eligibility_max,
            config->reward_min,
            config->reward_max,
            config->reward_clip ? "true" : "false",
            config->reward_record_history ? "true" : "false",
            config->reward_record_interval_steps,
            config->reward_record_connection_limit) < 0 ||
        fprintf(file, "\n[reward_events]\n") < 0)
    {
        fclose(file);
        set_error(error_message, error_message_size, "erro ao escrever reward");
        return 0;
    }

    for (int event_index = 0; event_index < config->reward_event_count;
         event_index++)
    {
        const ScenarioRewardEvent *event = NULL;

        for (int i = 0; i < config->reward_event_count; i++)
        {
            if (config->reward_events[i].index == event_index)
            {
                event = &config->reward_events[i];
                break;
            }
        }

        if (event == NULL ||
            fprintf(
                file,
                "event_%d = %d,%.17g\n",
                event->index,
                event->step,
                event->value) < 0)
        {
            fclose(file);
            set_error(error_message, error_message_size, "erro ao escrever reward_events");
            return 0;
        }
    }

    if (fclose(file) != 0)
    {
        set_error(error_message, error_message_size, "erro ao fechar arquivo de cenario");
        return 0;
    }

    return 1;
}
