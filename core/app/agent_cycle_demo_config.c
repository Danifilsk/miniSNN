#include "agent_cycle_demo_config.h"

#include <errno.h>
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

static int valid_run_name(const char *text)
{
    size_t length;
    if (text == NULL || (length = strlen(text)) == 0U ||
        length > AGENT_CYCLE_DEMO_RUN_NAME_MAX)
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

static int validate(const AgentCycleDemoConfig *config, char *error_message,
                    size_t error_message_size)
{
    if (config == NULL || !valid_run_name(config->run_name) ||
        !neuron_model_is_valid(config->neuron_model) || config->neuron_count < 2U ||
        config->neuron_count > 256U || config->brain_steps_per_tick == 0U ||
        config->brain_steps_per_tick > 1000U || config->ticks < 2U ||
        config->ticks > 100000U || config->reset_interval == 0U ||
        config->reset_interval > config->ticks || !isfinite(config->input_current) ||
        config->input_current < 0.0)
    {
        set_error(error_message, error_message_size, "configuracao invalida");
        return 0;
    }
    return 1;
}

int agent_cycle_demo_config_load_file(
    const char *filename, AgentCycleDemoConfig *out_config,
    char *error_message, size_t error_message_size)
{
    FILE *file;
    char line[512];
    char section[32] = "";
    unsigned int line_number = 0U;
    unsigned int seen = 0U;

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
        char *equals;
        char *key;
        char *value;
        unsigned int flag = 0U;
        line_number++;
        text = trim(line);
        if (text[0] == '\0' || text[0] == '#' || text[0] == ';')
            continue;
        if (text[0] == '[')
        {
            size_t length = strlen(text);
            if (length < 3U || text[length - 1U] != ']')
                goto invalid;
            text[length - 1U] = '\0';
            if (strcmp(text + 1U, "run") != 0 &&
                strcmp(text + 1U, "network") != 0 &&
                strcmp(text + 1U, "demo") != 0)
                goto invalid;
            strcpy(section, text + 1U);
            continue;
        }
        equals = strchr(text, '=');
        if (equals == NULL || section[0] == '\0')
            goto invalid;
        *equals = '\0';
        key = trim(text);
        value = trim(equals + 1U);
        if (strcmp(section, "run") == 0 && strcmp(key, "run_name") == 0)
        {
            flag = 1U;
            if (!valid_run_name(value))
                goto invalid;
            strcpy(out_config->run_name, value);
        }
        else if (strcmp(section, "network") == 0 && strcmp(key, "neurons") == 0)
        {
            flag = 2U;
            if (!parse_u32(value, &out_config->neuron_count))
                goto invalid;
        }
        else if (strcmp(section, "network") == 0 && strcmp(key, "model") == 0)
        {
            flag = 4U;
            if (!neuron_model_from_name(value, &out_config->neuron_model))
                goto invalid;
        }
        else if (strcmp(section, "network") == 0 &&
                 strcmp(key, "brain_steps_per_tick") == 0)
        {
            flag = 8U;
            if (!parse_u32(value, &out_config->brain_steps_per_tick))
                goto invalid;
        }
        else if (strcmp(section, "demo") == 0 && strcmp(key, "ticks") == 0)
        {
            flag = 16U;
            if (!parse_u32(value, &out_config->ticks))
                goto invalid;
        }
        else if (strcmp(section, "demo") == 0 && strcmp(key, "reset_interval") == 0)
        {
            flag = 32U;
            if (!parse_u32(value, &out_config->reset_interval))
                goto invalid;
        }
        else if (strcmp(section, "demo") == 0 && strcmp(key, "input_current") == 0)
        {
            flag = 64U;
            if (!parse_double(value, &out_config->input_current))
                goto invalid;
        }
        else
            goto invalid;
        if ((seen & flag) != 0U)
            goto invalid;
        seen |= flag;
        continue;
invalid:
        fclose(file);
        set_error(error_message, error_message_size, "linha %u invalida", line_number);
        return 0;
    }
    if (ferror(file))
    {
        fclose(file);
        set_error(error_message, error_message_size, "erro ao ler o arquivo");
        return 0;
    }
    if (fclose(file) != 0)
    {
        set_error(error_message, error_message_size, "erro ao ler o arquivo");
        return 0;
    }
    if (seen != 127U)
    {
        set_error(error_message, error_message_size, "chaves obrigatorias ausentes");
        return 0;
    }
    return validate(out_config, error_message, error_message_size);
}

int agent_cycle_demo_config_write_file(
    const char *filename, const AgentCycleDemoConfig *config,
    char *error_message, size_t error_message_size)
{
    FILE *file;
    int failed;
    if (filename == NULL || !validate(config, error_message, error_message_size))
        return 0;
    file = fopen(filename, "wb");
    if (file == NULL)
    {
        set_error(error_message, error_message_size, "nao foi possivel abrir a saida");
        return 0;
    }
    failed = fprintf(file,
                     "[run]\nrun_name = %s\n\n[network]\nneurons = %u\nmodel = %s\n"
                     "brain_steps_per_tick = %u\n\n[demo]\nticks = %u\n"
                     "reset_interval = %u\ninput_current = %.17g\n",
                     config->run_name, config->neuron_count,
                     neuron_model_name(config->neuron_model),
                     config->brain_steps_per_tick, config->ticks,
                     config->reset_interval, config->input_current) < 0;
    if (fclose(file) != 0)
        failed = 1;
    if (failed)
    {
        set_error(error_message, error_message_size, "erro ao gravar a saida");
        return 0;
    }
    return 1;
}
