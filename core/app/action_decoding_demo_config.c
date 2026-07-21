#include "action_decoding_demo_config.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "neuron_model.h"

static void set_error(char *message, size_t size, const char *format, ...)
{
    va_list args;
    if (message == NULL || size == 0U)
        return;
    va_start(args, format);
    vsnprintf(message, size, format, args);
    va_end(args);
}

static char *trim(char *text)
{
    char *end;
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
        text++;
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' ||
                          end[-1] == '\r' || end[-1] == '\n'))
        *--end = '\0';
    return text;
}

static int safe_name(const char *text)
{
    size_t length;
    if (text == NULL || (length = strlen(text)) == 0U ||
        length > MINISNN_AGENT_IO_MAX_CHANNEL_NAME_LENGTH)
        return 0;
    for (size_t index = 0U; index < length; index++)
    {
        unsigned char value = (unsigned char)text[index];
        if (!((value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
              (value >= '0' && value <= '9') || value == '_' || value == '-'))
            return 0;
    }
    return 1;
}

static int parse_u32(const char *text, uint32_t *out_value)
{
    char *end = NULL;
    unsigned long value;
    errno = 0;
    if (text == NULL || text[0] == '\0')
        return 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value > UINT32_MAX)
        return 0;
    *out_value = (uint32_t)value;
    return 1;
}

static int parse_double(const char *text, double *out_value)
{
    char *end = NULL;
    errno = 0;
    if (text == NULL || text[0] == '\0')
        return 0;
    *out_value = strtod(text, &end);
    return errno == 0 && end != text && *end == '\0' && isfinite(*out_value);
}

static int split_values(char *text, char **values, uint32_t expected)
{
    uint32_t count = 0U;
    char *cursor = text;
    while (cursor != NULL && count < expected)
    {
        char *comma = strchr(cursor, ',');
        if (comma != NULL)
            *comma = '\0';
        values[count++] = trim(cursor);
        cursor = comma == NULL ? NULL : comma + 1;
    }
    return count == expected && cursor == NULL;
}

static int find_action(const ActionDecodingDemoConfig *config, const char *name,
                       uint32_t *out_index)
{
    for (uint32_t index = 0U; index < config->action_count; index++)
    {
        if (strcmp(config->actions[index].name, name) == 0)
        {
            *out_index = index;
            return 1;
        }
    }
    return 0;
}

static int parse_action(ActionDecodingDemoConfig *config, const char *key,
                        char *value)
{
    char *parts[3];
    ActionDecodingDemoAction *action;
    if (!safe_name(key) || config->action_count >= ACTION_DECODING_DEMO_MAX_CHANNELS ||
        !split_values(value, parts, 3U) || find_action(config, key, &(uint32_t){0U}))
        return 0;
    action = &config->actions[config->action_count];
    if (!parse_double(parts[0], &action->minimum) ||
        !parse_double(parts[1], &action->maximum) ||
        !parse_double(parts[2], &action->default_value) ||
        action->minimum > action->maximum ||
        action->default_value < action->minimum || action->default_value > action->maximum)
        return 0;
    strcpy(action->name, key);
    action->id = config->action_count + 1U;
    config->action_count++;
    return 1;
}

static int parse_mapping(ActionDecodingDemoConfig *config, const char *key,
                         char *value)
{
    char *parts[8];
    char *mode;
    uint32_t action_index;
    MiniSNNActionDecodingSpec *spec;
    uint32_t count;

    if (!find_action(config, key, &action_index) ||
        config->mapping_count >= ACTION_DECODING_DEMO_MAX_MAPPINGS)
        return 0;
    mode = value;
    while (*mode != '\0' && *mode != ',')
        mode++;
    if (*mode != ',')
        return 0;
    *mode++ = '\0';
    mode = trim(value);
    memset(parts, 0, sizeof(parts));
    spec = &config->mappings[config->mapping_count].spec;
    memset(spec, 0, sizeof(*spec));
    spec->action_channel_id = config->actions[action_index].id;
    if (strcmp(mode, "population_rate") == 0)
    {
        count = 4U;
        if (!split_values(mode + strlen(mode) + 1U, parts, count))
            return 0;
        spec->mode = MINISNN_ACTION_DECODING_POPULATION_RATE;
        if (!parse_u32(parts[0], &spec->primary_neuron_start) ||
            !parse_u32(parts[1], &spec->primary_neuron_count) ||
            !parse_double(parts[2], &spec->minimum_rate) ||
            !parse_double(parts[3], &spec->maximum_rate))
            return 0;
    }
    else if (strcmp(mode, "bipolar_difference") == 0)
    {
        count = 4U;
        if (!split_values(mode + strlen(mode) + 1U, parts, count))
            return 0;
        spec->mode = MINISNN_ACTION_DECODING_BIPOLAR_DIFFERENCE;
        if (!parse_u32(parts[0], &spec->primary_neuron_start) ||
            !parse_u32(parts[1], &spec->primary_neuron_count) ||
            !parse_u32(parts[2], &spec->secondary_neuron_start) ||
            !parse_u32(parts[3], &spec->secondary_neuron_count))
            return 0;
    }
    else if (strcmp(mode, "threshold") == 0)
    {
        count = 5U;
        if (!split_values(mode + strlen(mode) + 1U, parts, count))
            return 0;
        spec->mode = MINISNN_ACTION_DECODING_THRESHOLD;
        if (!parse_u32(parts[0], &spec->primary_neuron_start) ||
            !parse_u32(parts[1], &spec->primary_neuron_count) ||
            !parse_double(parts[2], &spec->threshold) ||
            !parse_double(parts[3], &spec->active_value) ||
            !parse_double(parts[4], &spec->inactive_value))
            return 0;
    }
    else if (strcmp(mode, "wta_member") == 0)
    {
        count = 7U;
        if (!split_values(mode + strlen(mode) + 1U, parts, count))
            return 0;
        spec->mode = MINISNN_ACTION_DECODING_WTA_MEMBER;
        if (!parse_u32(parts[0], &spec->primary_neuron_start) ||
            !parse_u32(parts[1], &spec->primary_neuron_count) ||
            !parse_u32(parts[2], &spec->competition_group_id) ||
            !parse_double(parts[3], &spec->minimum_activation) ||
            !parse_double(parts[4], &spec->minimum_confidence) ||
            !parse_double(parts[5], &spec->winner_value) ||
            !parse_double(parts[6], &spec->loser_value))
            return 0;
    }
    else
        return 0;
    config->mappings[config->mapping_count].action_index = action_index;
    config->mapping_count++;
    return 1;
}

int action_decoding_demo_config_load_file(
    const char *filename, ActionDecodingDemoConfig *out_config,
    char *error_message, size_t error_message_size)
{
    FILE *file;
    char line[512];
    char section[32] = "";
    unsigned int line_number = 0U;
    int have_run = 0;
    int have_neurons = 0;
    int have_steps = 0;

    if (filename == NULL || out_config == NULL)
    {
        set_error(error_message, error_message_size, "argumento invalido");
        return 0;
    }
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        set_error(error_message, error_message_size, "nao foi possivel abrir o arquivo");
        return 0;
    }
    memset(out_config, 0, sizeof(*out_config));
    out_config->neuron_model = MINISNN_NEURON_MODEL_LIF;
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *text;
        char *equal;
        char *key;
        char *value;
        line_number++;
        text = trim(line);
        if (text[0] == '\0' || text[0] == '#' || text[0] == ';')
            continue;
        if (text[0] == '[')
        {
            size_t length = strlen(text);
            if (length < 3U || text[length - 1U] != ']')
                goto invalid_line;
            text[length - 1U] = '\0';
            if (strcmp(text + 1U, "run") != 0 && strcmp(text + 1U, "network") != 0 &&
                strcmp(text + 1U, "action_decoder") != 0 &&
                strcmp(text + 1U, "actions") != 0 && strcmp(text + 1U, "mappings") != 0)
                goto invalid_line;
            strcpy(section, text + 1U);
            continue;
        }
        equal = strchr(text, '=');
        if (equal == NULL || section[0] == '\0')
            goto invalid_line;
        *equal = '\0';
        key = trim(text);
        value = trim(equal + 1U);
        if (strcmp(section, "run") == 0)
        {
            if (strcmp(key, "run_name") != 0 || have_run || !safe_name(value) ||
                strlen(value) > ACTION_DECODING_DEMO_RUN_NAME_MAX)
                goto invalid_line;
            strcpy(out_config->run_name, value);
            have_run = 1;
        }
        else if (strcmp(section, "network") == 0)
        {
            if (strcmp(key, "neurons") == 0)
            {
                if (have_neurons || !parse_u32(value, &out_config->neuron_count) ||
                    out_config->neuron_count == 0U || out_config->neuron_count > INT_MAX)
                    goto invalid_line;
                have_neurons = 1;
            }
            else if (strcmp(key, "model") == 0)
            {
                MiniSNNNeuronModel model;
                if (!neuron_model_from_name(value, &model))
                    goto invalid_line;
                out_config->neuron_model = model;
            }
            else
                goto invalid_line;
        }
        else if (strcmp(section, "action_decoder") == 0)
        {
            if (strcmp(key, "brain_steps_per_tick") != 0 || have_steps ||
                !parse_u32(value, &out_config->brain_steps_per_tick) ||
                out_config->brain_steps_per_tick == 0U)
                goto invalid_line;
            have_steps = 1;
        }
        else if (strcmp(section, "actions") == 0)
        {
            if (!parse_action(out_config, key, value))
                goto invalid_line;
        }
        else if (!parse_mapping(out_config, key, value))
            goto invalid_line;
        continue;

invalid_line:
        fclose(file);
        set_error(error_message, error_message_size, "chave ou valor invalido na linha %u",
                  line_number);
        return 0;
    }
    if (ferror(file) || fclose(file) != 0 || !have_run || !have_neurons || !have_steps ||
        out_config->action_count == 0U || out_config->mapping_count == 0U)
    {
        set_error(error_message, error_message_size, "configuracao incompleta ou erro de leitura");
        return 0;
    }
    set_error(error_message, error_message_size, "");
    return 1;
}

static const char *mode_name(MiniSNNActionDecodingMode mode)
{
    if (mode == MINISNN_ACTION_DECODING_POPULATION_RATE)
        return "population_rate";
    if (mode == MINISNN_ACTION_DECODING_BIPOLAR_DIFFERENCE)
        return "bipolar_difference";
    if (mode == MINISNN_ACTION_DECODING_THRESHOLD)
        return "threshold";
    return "wta_member";
}

int action_decoding_demo_config_write_file(
    const char *filename, const ActionDecodingDemoConfig *config,
    char *error_message, size_t error_message_size)
{
    FILE *file;
    int failed = 0;
    if (filename == NULL || config == NULL)
    {
        set_error(error_message, error_message_size, "argumento invalido");
        return 0;
    }
    file = fopen(filename, "wb");
    if (file == NULL)
    {
        set_error(error_message, error_message_size, "nao foi possivel abrir a saida");
        return 0;
    }
    if (fprintf(file, "[run]\nrun_name = %s\n\n[network]\nneurons = %u\nmodel = %s\n"
                      "\n[action_decoder]\nbrain_steps_per_tick = %u\n\n[actions]\n",
                config->run_name, config->neuron_count,
                minisnn_neuron_model_name(config->neuron_model),
                config->brain_steps_per_tick) < 0)
        failed = 1;
    for (uint32_t index = 0U; !failed && index < config->action_count; index++)
    {
        const ActionDecodingDemoAction *action = &config->actions[index];
        failed = fprintf(file, "%s = %.17g,%.17g,%.17g\n", action->name,
                         action->minimum, action->maximum, action->default_value) < 0;
    }
    if (!failed && fprintf(file, "\n[mappings]\n") < 0)
        failed = 1;
    for (uint32_t index = 0U; !failed && index < config->mapping_count; index++)
    {
        const ActionDecodingDemoMapping *mapping = &config->mappings[index];
        const MiniSNNActionDecodingSpec *spec = &mapping->spec;
        const char *name = config->actions[mapping->action_index].name;
        if (spec->mode == MINISNN_ACTION_DECODING_POPULATION_RATE)
            failed = fprintf(file, "%s = %s,%u,%u,%.17g,%.17g\n", name,
                             mode_name(spec->mode), spec->primary_neuron_start,
                             spec->primary_neuron_count, spec->minimum_rate,
                             spec->maximum_rate) < 0;
        else if (spec->mode == MINISNN_ACTION_DECODING_BIPOLAR_DIFFERENCE)
            failed = fprintf(file, "%s = %s,%u,%u,%u,%u\n", name,
                             mode_name(spec->mode), spec->primary_neuron_start,
                             spec->primary_neuron_count, spec->secondary_neuron_start,
                             spec->secondary_neuron_count) < 0;
        else if (spec->mode == MINISNN_ACTION_DECODING_THRESHOLD)
            failed = fprintf(file, "%s = %s,%u,%u,%.17g,%.17g,%.17g\n", name,
                             mode_name(spec->mode), spec->primary_neuron_start,
                             spec->primary_neuron_count, spec->threshold,
                             spec->active_value, spec->inactive_value) < 0;
        else
            failed = fprintf(file, "%s = %s,%u,%u,%u,%.17g,%.17g,%.17g,%.17g\n", name,
                             mode_name(spec->mode), spec->primary_neuron_start,
                             spec->primary_neuron_count, spec->competition_group_id,
                             spec->minimum_activation, spec->minimum_confidence,
                             spec->winner_value, spec->loser_value) < 0;
    }
    if (fclose(file) != 0)
        failed = 1;
    if (failed)
    {
        set_error(error_message, error_message_size, "erro ao gravar configuracao");
        return 0;
    }
    set_error(error_message, error_message_size, "");
    return 1;
}
