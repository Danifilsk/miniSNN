#include "c7_integrated_audit_config.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "neuron_model.h"

static void write_error(char *buffer, size_t size, const char *message)
{
    if (buffer != NULL && size > 0U)
        snprintf(buffer, size, "%s", message != NULL ? message : "erro");
}

static char *trim(char *text)
{
    char *end;
    while (*text != '\0' && isspace((unsigned char)*text))
        text++;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]))
        *--end = '\0';
    return text;
}

static int parse_u32(const char *text, uint32_t *out_value)
{
    char *end;
    unsigned long value;
    errno = 0;
    if (text == NULL || out_value == NULL || text[0] == '\0')
        return 0;
    value = strtoul(text, &end, 10);
    return errno == 0 && *trim(end) == '\0' && value <= UINT32_MAX &&
        (*out_value = (uint32_t)value, 1);
}

static int parse_u64(const char *text, uint64_t *out_value)
{
    char *end;
    unsigned long long value;
    errno = 0;
    if (text == NULL || out_value == NULL || text[0] == '\0')
        return 0;
    value = strtoull(text, &end, 10);
    return errno == 0 && *trim(end) == '\0' && (*out_value = (uint64_t)value, 1);
}

static int parse_double(const char *text, double *out_value)
{
    char *end;
    double value;
    errno = 0;
    if (text == NULL || out_value == NULL || text[0] == '\0')
        return 0;
    value = strtod(text, &end);
    if (errno != 0 || *trim(end) != '\0' || !isfinite(value))
        return 0;
    *out_value = value;
    return 1;
}

static int parse_bool(const char *text, int *out_value)
{
    if (text == NULL || out_value == NULL)
        return 0;
    if (strcmp(text, "true") == 0 || strcmp(text, "1") == 0)
    {
        *out_value = 1;
        return 1;
    }
    if (strcmp(text, "false") == 0 || strcmp(text, "0") == 0)
    {
        *out_value = 0;
        return 1;
    }
    return 0;
}

static int valid_run_name(const char *name)
{
    size_t length;
    if (name == NULL || (length = strlen(name)) == 0U || length > C7_AUDIT_RUN_NAME_MAX)
        return 0;
    for (size_t index = 0U; index < length; index++)
        if (!(isalnum((unsigned char)name[index]) || name[index] == '_' || name[index] == '-'))
            return 0;
    return 1;
}

static int parse_models(const char *text, int enabled[3])
{
    char copy[128];
    char *cursor;
    int count = 0;
    if (text == NULL || strlen(text) >= sizeof(copy))
        return 0;
    memset(enabled, 0, 3U * sizeof(*enabled));
    snprintf(copy, sizeof(copy), "%s", text);
    cursor = copy;
    while (cursor != NULL)
    {
        char *next = strchr(cursor, ',');
        MiniSNNNeuronModel model;
        if (next != NULL)
            *next++ = '\0';
        cursor = trim(cursor);
        if (!neuron_model_from_name(cursor, &model) || model > MINISNN_NEURON_MODEL_HODGKIN_HUXLEY ||
            enabled[model])
            return 0;
        enabled[model] = 1;
        count++;
        cursor = next;
    }
    return count > 0;
}

void c7_integrated_audit_config_default(C7IntegratedAuditConfig *config)
{
    if (config == NULL)
        return;
    memset(config, 0, sizeof(*config));
    snprintf(config->run_name, sizeof(config->run_name), "c7_integrated_audit_demo");
    config->model_enabled[MINISNN_NEURON_MODEL_LIF] = 1;
    config->model_enabled[MINISNN_NEURON_MODEL_ADEX] = 1;
    config->model_enabled[MINISNN_NEURON_MODEL_HODGKIN_HUXLEY] = 1;
    config->seed = 701U;
    config->episodes = 3U;
    config->ticks_per_episode = 12U;
    config->brain_steps_per_tick = 64U;
    config->input_drive[MINISNN_NEURON_MODEL_LIF] = 1000.0;
    config->input_drive[MINISNN_NEURON_MODEL_ADEX] = 500.0;
    config->input_drive[MINISNN_NEURON_MODEL_HODGKIN_HUXLEY] = 12.0;
    config->stdp_enabled = 1;
    config->reward_enabled = 1;
    config->homeostasis_enabled = 1;
    config->structural_enabled = 1;
    config->checkpoint_ready_enabled = 1;
    config->checkpoint_pending_enabled = 1;
    config->replay_enabled = 1;
}

static int config_valid(const C7IntegratedAuditConfig *config)
{
    uint32_t required_ticks;
    if (config == NULL)
        return 0;
    required_ticks = config->checkpoint_pending_enabled ? 3U :
        (config->checkpoint_ready_enabled ? 2U : 1U);
    return config != NULL && valid_run_name(config->run_name) &&
        (config->model_enabled[0] || config->model_enabled[1] || config->model_enabled[2]) &&
        config->episodes > 0U && config->episodes <= 1000U &&
        config->ticks_per_episode > 0U && config->ticks_per_episode <= 100000U &&
        config->brain_steps_per_tick > 0U && config->brain_steps_per_tick <= 1024U &&
        isfinite(config->input_drive[MINISNN_NEURON_MODEL_LIF]) &&
        config->input_drive[MINISNN_NEURON_MODEL_LIF] > 0.0 &&
        isfinite(config->input_drive[MINISNN_NEURON_MODEL_ADEX]) &&
        config->input_drive[MINISNN_NEURON_MODEL_ADEX] > 0.0 &&
        isfinite(config->input_drive[MINISNN_NEURON_MODEL_HODGKIN_HUXLEY]) &&
        config->input_drive[MINISNN_NEURON_MODEL_HODGKIN_HUXLEY] > 0.0 &&
        (config->stdp_enabled == 0 || config->stdp_enabled == 1) &&
        (config->reward_enabled == 0 || config->reward_enabled == 1) &&
        (config->homeostasis_enabled == 0 || config->homeostasis_enabled == 1) &&
        (config->structural_enabled == 0 || config->structural_enabled == 1) &&
        (config->checkpoint_ready_enabled == 0 || config->checkpoint_ready_enabled == 1) &&
        (config->checkpoint_pending_enabled == 0 || config->checkpoint_pending_enabled == 1) &&
        (config->replay_enabled == 0 || config->replay_enabled == 1) &&
        config->ticks_per_episode >= required_ticks &&
        (!config->replay_enabled || config->checkpoint_ready_enabled ||
         config->checkpoint_pending_enabled);
}

int c7_integrated_audit_config_load_file(
    const char *filename,
    C7IntegratedAuditConfig *out_config,
    char *error_message,
    size_t error_message_size)
{
    FILE *file;
    C7IntegratedAuditConfig parsed;
    char line[256];
    unsigned int line_number = 0U;
    unsigned int seen = 0U;

    if (filename == NULL || out_config == NULL || error_message == NULL || error_message_size == 0U)
        return 0;
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        write_error(error_message, error_message_size, "arquivo nao pode ser aberto");
        return 0;
    }
    c7_integrated_audit_config_default(&parsed);
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *text;
        char *equals;
        char *key;
        char *value;
        uint32_t parsed_u32;
        uint64_t parsed_u64;
        double parsed_double;
        int parsed_bool;
        unsigned int bit = 0U;
        line_number++;
        text = trim(line);
        if (text[0] == '\0' || text[0] == '#' || text[0] == ';' || text[0] == '[')
            continue;
        equals = strchr(text, '=');
        if (equals == NULL)
            goto format_error;
        *equals = '\0';
        key = trim(text);
        value = trim(equals + 1U);
        if (strcmp(key, "run_name") == 0)
        {
            bit = 1U << 0U;
            if (!valid_run_name(value)) goto format_error;
            snprintf(parsed.run_name, sizeof(parsed.run_name), "%s", value);
        }
        else if (strcmp(key, "models") == 0)
        {
            bit = 1U << 1U;
            if (!parse_models(value, parsed.model_enabled)) goto format_error;
        }
        else if (strcmp(key, "seed") == 0)
        {
            bit = 1U << 2U;
            if (!parse_u64(value, &parsed_u64)) goto format_error;
            parsed.seed = parsed_u64;
        }
        else if (strcmp(key, "episodes") == 0)
        {
            bit = 1U << 3U;
            if (!parse_u32(value, &parsed_u32)) goto format_error;
            parsed.episodes = parsed_u32;
        }
        else if (strcmp(key, "ticks_per_episode") == 0)
        {
            bit = 1U << 4U;
            if (!parse_u32(value, &parsed_u32)) goto format_error;
            parsed.ticks_per_episode = parsed_u32;
        }
        else if (strcmp(key, "brain_steps_per_tick") == 0)
        {
            bit = 1U << 5U;
            if (!parse_u32(value, &parsed_u32)) goto format_error;
            parsed.brain_steps_per_tick = parsed_u32;
        }
        else if (strcmp(key, "lif_input_drive") == 0)
        {
            bit = 1U << 13U;
            if (!parse_double(value, &parsed_double)) goto format_error;
            parsed.input_drive[MINISNN_NEURON_MODEL_LIF] = parsed_double;
        }
        else if (strcmp(key, "adex_input_drive") == 0)
        {
            bit = 1U << 14U;
            if (!parse_double(value, &parsed_double)) goto format_error;
            parsed.input_drive[MINISNN_NEURON_MODEL_ADEX] = parsed_double;
        }
        else if (strcmp(key, "hodgkin_huxley_input_drive") == 0)
        {
            bit = 1U << 15U;
            if (!parse_double(value, &parsed_double)) goto format_error;
            parsed.input_drive[MINISNN_NEURON_MODEL_HODGKIN_HUXLEY] = parsed_double;
        }
        else if (strcmp(key, "stdp_enabled") == 0)
        {
            bit = 1U << 6U;
            if (!parse_bool(value, &parsed_bool)) goto format_error;
            parsed.stdp_enabled = parsed_bool;
        }
        else if (strcmp(key, "reward_enabled") == 0)
        {
            bit = 1U << 7U;
            if (!parse_bool(value, &parsed_bool)) goto format_error;
            parsed.reward_enabled = parsed_bool;
        }
        else if (strcmp(key, "homeostasis_enabled") == 0)
        {
            bit = 1U << 8U;
            if (!parse_bool(value, &parsed_bool)) goto format_error;
            parsed.homeostasis_enabled = parsed_bool;
        }
        else if (strcmp(key, "structural_enabled") == 0)
        {
            bit = 1U << 9U;
            if (!parse_bool(value, &parsed_bool)) goto format_error;
            parsed.structural_enabled = parsed_bool;
        }
        else if (strcmp(key, "checkpoint_ready") == 0)
        {
            bit = 1U << 10U;
            if (!parse_bool(value, &parsed_bool)) goto format_error;
            parsed.checkpoint_ready_enabled = parsed_bool;
        }
        else if (strcmp(key, "checkpoint_pending") == 0)
        {
            bit = 1U << 11U;
            if (!parse_bool(value, &parsed_bool)) goto format_error;
            parsed.checkpoint_pending_enabled = parsed_bool;
        }
        else if (strcmp(key, "replay_enabled") == 0)
        {
            bit = 1U << 12U;
            if (!parse_bool(value, &parsed_bool)) goto format_error;
            parsed.replay_enabled = parsed_bool;
        }
        else
            goto format_error;
        if ((seen & bit) != 0U)
            goto format_error;
        seen |= bit;
        continue;
format_error:
        fclose(file);
        snprintf(error_message, error_message_size, "linha %u invalida", line_number);
        return 0;
    }
    if (ferror(file))
    {
        fclose(file);
        write_error(error_message, error_message_size, "configuracao invalida");
        return 0;
    }
    if (fclose(file) != 0)
    {
        write_error(error_message, error_message_size, "configuracao invalida");
        return 0;
    }
    if (parsed.checkpoint_pending_enabled && parsed.ticks_per_episode < 3U)
    {
        write_error(error_message, error_message_size,
                    "ticks_per_episode insuficiente para checkpoint ACTION_PENDING");
        return 0;
    }
    if (parsed.checkpoint_ready_enabled && parsed.ticks_per_episode < 2U)
    {
        write_error(error_message, error_message_size,
                    "ticks_per_episode insuficiente para checkpoint READY");
        return 0;
    }
    if (!config_valid(&parsed))
    {
        write_error(error_message, error_message_size, "configuracao invalida");
        return 0;
    }
    *out_config = parsed;
    write_error(error_message, error_message_size, "ok");
    return 1;
}

int c7_integrated_audit_config_write_file(
    const char *filename,
    const C7IntegratedAuditConfig *config,
    char *error_message,
    size_t error_message_size)
{
    FILE *file;
    char models[64] = "";
    size_t used = 0U;
    if (filename == NULL || !config_valid(config))
    {
        write_error(error_message, error_message_size, "configuracao invalida");
        return 0;
    }
    for (int model = MINISNN_NEURON_MODEL_LIF;
         model <= MINISNN_NEURON_MODEL_HODGKIN_HUXLEY; model++)
    {
        if (config->model_enabled[model])
        {
            int written = snprintf(models + used, sizeof(models) - used, "%s%s",
                                   used == 0U ? "" : ",",
                                   neuron_model_name((MiniSNNNeuronModel)model));
            if (written < 0 || (size_t)written >= sizeof(models) - used)
                return 0;
            used += (size_t)written;
        }
    }
    file = fopen(filename, "wb");
    if (file == NULL)
    {
        write_error(error_message, error_message_size, "arquivo nao pode ser gravado");
        return 0;
    }
    if (fprintf(file,
                "[run]\nrun_name = %s\n\n[audit]\nmodels = %s\nseed = %llu\nepisodes = %u\n"
                "ticks_per_episode = %u\nbrain_steps_per_tick = %u\n"
                "lif_input_drive = %.17g\nadex_input_drive = %.17g\n"
                "hodgkin_huxley_input_drive = %.17g\nstdp_enabled = %s\n"
                "reward_enabled = %s\nhomeostasis_enabled = %s\nstructural_enabled = %s\n"
                "checkpoint_ready = %s\ncheckpoint_pending = %s\nreplay_enabled = %s\n",
                config->run_name, models, (unsigned long long)config->seed,
                config->episodes, config->ticks_per_episode, config->brain_steps_per_tick,
                config->input_drive[MINISNN_NEURON_MODEL_LIF],
                config->input_drive[MINISNN_NEURON_MODEL_ADEX],
                config->input_drive[MINISNN_NEURON_MODEL_HODGKIN_HUXLEY],
                config->stdp_enabled ? "true" : "false",
                config->reward_enabled ? "true" : "false",
                config->homeostasis_enabled ? "true" : "false",
                config->structural_enabled ? "true" : "false",
                config->checkpoint_ready_enabled ? "true" : "false",
                config->checkpoint_pending_enabled ? "true" : "false",
                config->replay_enabled ? "true" : "false") < 0 || fclose(file) != 0)
    {
        write_error(error_message, error_message_size, "erro ao gravar configuracao");
        return 0;
    }
    write_error(error_message, error_message_size, "ok");
    return 1;
}
