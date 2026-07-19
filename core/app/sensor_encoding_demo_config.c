#include "sensor_encoding_demo_config.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "neuron_model.h"

#define SENSOR_ENCODING_DEMO_LINE_MAX 512U

typedef enum
{
    DEMO_SECTION_NONE = 0,
    DEMO_SECTION_RUN,
    DEMO_SECTION_NETWORK,
    DEMO_SECTION_ENCODER,
    DEMO_SECTION_SENSORS,
    DEMO_SECTION_MAPPINGS
} SensorEncodingDemoSection;

static void set_error(char *buffer, size_t buffer_size, const char *format, ...)
{
    va_list arguments;

    if (buffer == NULL || buffer_size == 0U)
        return;
    va_start(arguments, format);
    vsnprintf(buffer, buffer_size, format, arguments);
    va_end(arguments);
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

static int copy_text(char *destination, size_t destination_size, const char *source)
{
    size_t length;

    if (destination == NULL || source == NULL)
        return 0;
    length = strlen(source);
    if (length >= destination_size)
        return 0;
    memcpy(destination, source, length + 1U);
    return 1;
}

static int valid_run_name(const char *name)
{
    size_t length;

    if (name == NULL)
        return 0;
    length = strlen(name);
    if (length == 0U || length > SENSOR_ENCODING_DEMO_RUN_NAME_MAX)
        return 0;
    for (size_t index = 0U; index < length; index++)
    {
        char character = name[index];
        if (!((character >= 'A' && character <= 'Z') ||
              (character >= 'a' && character <= 'z') ||
              (character >= '0' && character <= '9') ||
              character == '_' || character == '-'))
            return 0;
    }
    return 1;
}

static int parse_u32(const char *text, uint32_t *out_value)
{
    char *end = NULL;
    unsigned long value;

    if (text == NULL || text[0] == '\0' || out_value == NULL)
        return 0;
    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value > UINT32_MAX)
        return 0;
    *out_value = (uint32_t)value;
    return 1;
}

static int parse_double(const char *text, double *out_value)
{
    char *end = NULL;
    double value;

    if (text == NULL || text[0] == '\0' || out_value == NULL)
        return 0;
    errno = 0;
    value = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !isfinite(value))
        return 0;
    *out_value = value;
    return 1;
}

static int split_csv(char *text, char **fields, size_t expected_count)
{
    char *cursor = text;

    for (size_t index = 0U; index < expected_count; index++)
    {
        char *separator;

        fields[index] = trim(cursor);
        separator = strchr(cursor, ',');
        if (index + 1U == expected_count)
        {
            if (separator != NULL || fields[index][0] == '\0')
                return 0;
            return 1;
        }
        if (separator == NULL)
            return 0;
        *separator = '\0';
        if (fields[index][0] == '\0')
            return 0;
        cursor = separator + 1U;
    }
    return 0;
}

static int find_sensor_index(const SensorEncodingDemoConfig *config,
                             const char *name, uint32_t *out_index)
{
    for (uint32_t index = 0U; index < config->sensor_count; index++)
    {
        if (strcmp(config->sensors[index].name, name) == 0)
        {
            *out_index = index;
            return 1;
        }
    }
    return 0;
}

static SensorEncodingDemoSection parse_section(const char *text)
{
    if (strcmp(text, "[run]") == 0)
        return DEMO_SECTION_RUN;
    if (strcmp(text, "[network]") == 0)
        return DEMO_SECTION_NETWORK;
    if (strcmp(text, "[sensor_encoder]") == 0)
        return DEMO_SECTION_ENCODER;
    if (strcmp(text, "[sensors]") == 0)
        return DEMO_SECTION_SENSORS;
    if (strcmp(text, "[mappings]") == 0)
        return DEMO_SECTION_MAPPINGS;
    return DEMO_SECTION_NONE;
}

static int parse_sensor_value(SensorEncodingDemoConfig *config, const char *name,
                              char *value, unsigned int line, char *error,
                              size_t error_size)
{
    char *fields[3];
    SensorEncodingDemoSensor *sensor;

    if (config->sensor_count >= SENSOR_ENCODING_DEMO_MAX_CHANNELS)
    {
        set_error(error, error_size, "linha %u: muitos sensores", line);
        return 0;
    }
    if (!valid_run_name(name) || find_sensor_index(config, name, &(uint32_t){0U}))
    {
        set_error(error, error_size, "linha %u: nome de sensor invalido ou duplicado", line);
        return 0;
    }
    if (!split_csv(value, fields, 3U))
    {
        set_error(error, error_size, "linha %u: sensor deve usar minimo,maximo,default", line);
        return 0;
    }
    sensor = &config->sensors[config->sensor_count];
    if (!copy_text(sensor->name, sizeof(sensor->name), name) ||
        !parse_double(fields[0], &sensor->minimum) ||
        !parse_double(fields[1], &sensor->maximum) ||
        !parse_double(fields[2], &sensor->default_value) ||
        sensor->minimum > sensor->maximum ||
        sensor->default_value < sensor->minimum ||
        sensor->default_value > sensor->maximum)
    {
        set_error(error, error_size, "linha %u: limites de sensor invalidos", line);
        return 0;
    }
    sensor->id = config->sensor_count + 1U;
    config->sensor_count++;
    return 1;
}

static int parse_mapping_value(SensorEncodingDemoConfig *config, const char *name,
                               char *value, unsigned int line, char *error,
                               size_t error_size)
{
    SensorEncodingDemoMapping *mapping;
    uint32_t sensor_index;
    char *fields[6];
    MiniSNNSensorEncodingMode mode;
    size_t field_count;

    if (config->mapping_count >= SENSOR_ENCODING_DEMO_MAX_MAPPINGS)
    {
        set_error(error, error_size, "linha %u: muitos mappings", line);
        return 0;
    }
    if (!find_sensor_index(config, name, &sensor_index))
    {
        set_error(error, error_size, "linha %u: sensor de mapping desconhecido", line);
        return 0;
    }
    if (strncmp(value, "linear_current,", 15U) == 0)
    {
        mode = MINISNN_SENSOR_ENCODING_LINEAR_CURRENT;
        field_count = 5U;
    }
    else if (strncmp(value, "bipolar_current,", 16U) == 0)
    {
        mode = MINISNN_SENSOR_ENCODING_BIPOLAR_CURRENT;
        field_count = 5U;
    }
    else if (strncmp(value, "deterministic_rate,", 19U) == 0)
    {
        mode = MINISNN_SENSOR_ENCODING_DETERMINISTIC_RATE;
        field_count = 6U;
    }
    else
    {
        set_error(error, error_size, "linha %u: modo de encoding desconhecido", line);
        return 0;
    }
    if (!split_csv(value, fields, field_count))
    {
        set_error(error, error_size, "linha %u: campos de mapping invalidos", line);
        return 0;
    }
    mapping = &config->mappings[config->mapping_count];
    memset(mapping, 0, sizeof(*mapping));
    mapping->sensor_index = sensor_index;
    mapping->spec.sensor_channel_id = config->sensors[sensor_index].id;
    mapping->spec.mode = mode;
    if (!parse_u32(fields[1], &mapping->spec.target_neuron_start) ||
        !parse_u32(fields[2], &mapping->spec.target_neuron_count))
    {
        set_error(error, error_size, "linha %u: range de neuronios invalido", line);
        return 0;
    }
    if (mode == MINISNN_SENSOR_ENCODING_DETERMINISTIC_RATE)
    {
        if (!parse_double(fields[3], &mapping->spec.pulse_current) ||
            !parse_double(fields[4], &mapping->spec.maximum_rate) ||
            !parse_u32(fields[5], &mapping->spec.phase_offset) ||
            mapping->spec.phase_offset >= 1000U)
        {
            set_error(error, error_size, "linha %u: parametros de rate invalidos", line);
            return 0;
        }
    }
    else if (!parse_double(fields[3], &mapping->spec.gain) ||
             !parse_double(fields[4], &mapping->spec.bias))
    {
        set_error(error, error_size, "linha %u: gain ou bias invalido", line);
        return 0;
    }
    config->mapping_count++;
    return 1;
}

int sensor_encoding_demo_config_load_file(
    const char *filename, SensorEncodingDemoConfig *out_config,
    char *error_message, size_t error_message_size)
{
    FILE *file;
    char line_buffer[SENSOR_ENCODING_DEMO_LINE_MAX];
    SensorEncodingDemoSection section = DEMO_SECTION_NONE;
    unsigned int line = 0U;
    int has_run_name = 0;
    int has_neuron_count = 0;
    int has_model = 0;
    int has_brain_steps = 0;
    int has_rate_unit = 0;

    if (error_message != NULL && error_message_size > 0U)
        error_message[0] = '\0';
    if (filename == NULL || out_config == NULL)
    {
        set_error(error_message, error_message_size, "argumento invalido");
        return 0;
    }
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        set_error(error_message, error_message_size, "nao foi possivel abrir %s", filename);
        return 0;
    }
    memset(out_config, 0, sizeof(*out_config));
    out_config->neuron_model = MINISNN_NEURON_MODEL_LIF;
    while (fgets(line_buffer, sizeof(line_buffer), file) != NULL)
    {
        char *line_text;
        char *equals;
        char *key;
        char *value;

        line++;
        if (strchr(line_buffer, '\n') == NULL && !feof(file))
        {
            set_error(error_message, error_message_size, "linha %u: linha muito longa", line);
            fclose(file);
            return 0;
        }
        line_text = trim(line_buffer);
        if (line_text[0] == '\0' || line_text[0] == '#' || line_text[0] == ';')
            continue;
        if (line_text[0] == '[')
        {
            section = parse_section(line_text);
            if (section == DEMO_SECTION_NONE)
            {
                set_error(error_message, error_message_size, "linha %u: secao desconhecida", line);
                fclose(file);
                return 0;
            }
            continue;
        }
        equals = strchr(line_text, '=');
        if (equals == NULL || strchr(equals + 1U, '=') != NULL)
        {
            set_error(error_message, error_message_size, "linha %u: esperado chave = valor", line);
            fclose(file);
            return 0;
        }
        *equals = '\0';
        key = trim(line_text);
        value = trim(equals + 1U);
        if (section == DEMO_SECTION_NONE || key[0] == '\0' || value[0] == '\0')
        {
            set_error(error_message, error_message_size, "linha %u: chave ou secao invalida", line);
            fclose(file);
            return 0;
        }
        if (section == DEMO_SECTION_RUN)
        {
            if (strcmp(key, "run_name") != 0 || has_run_name || !valid_run_name(value) ||
                !copy_text(out_config->run_name, sizeof(out_config->run_name), value))
            {
                set_error(error_message, error_message_size, "linha %u: run_name invalido ou duplicado", line);
                fclose(file);
                return 0;
            }
            has_run_name = 1;
        }
        else if (section == DEMO_SECTION_NETWORK)
        {
            if (strcmp(key, "neurons") == 0 && !has_neuron_count &&
                parse_u32(value, &out_config->neuron_count) && out_config->neuron_count > 0U &&
                out_config->neuron_count <= (uint32_t)INT_MAX)
                has_neuron_count = 1;
            else if (strcmp(key, "model") == 0 && !has_model &&
                     neuron_model_from_name(value, &out_config->neuron_model))
                has_model = 1;
            else
            {
                set_error(error_message, error_message_size, "linha %u: chave ou valor de network invalido", line);
                fclose(file);
                return 0;
            }
        }
        else if (section == DEMO_SECTION_ENCODER)
        {
            if (strcmp(key, "brain_steps_per_tick") == 0 && !has_brain_steps &&
                parse_u32(value, &out_config->brain_steps_per_tick) &&
                out_config->brain_steps_per_tick > 0U)
                has_brain_steps = 1;
            else if (strcmp(key, "rate_unit") == 0 && !has_rate_unit &&
                     strcmp(value, "pulses_per_neural_step") == 0)
                has_rate_unit = 1;
            else
            {
                set_error(error_message, error_message_size, "linha %u: chave ou valor de sensor_encoder invalido", line);
                fclose(file);
                return 0;
            }
        }
        else if (section == DEMO_SECTION_SENSORS)
        {
            if (!parse_sensor_value(out_config, key, value, line, error_message,
                                    error_message_size))
            {
                fclose(file);
                return 0;
            }
        }
        else if (section == DEMO_SECTION_MAPPINGS)
        {
            if (!parse_mapping_value(out_config, key, value, line, error_message,
                                     error_message_size))
            {
                fclose(file);
                return 0;
            }
        }
    }
    if (ferror(file))
    {
        fclose(file);
        set_error(error_message, error_message_size, "erro ao ler %s", filename);
        return 0;
    }
    if (fclose(file) != 0)
    {
        set_error(error_message, error_message_size, "erro ao ler %s", filename);
        return 0;
    }
    if (!has_run_name || !has_neuron_count || !has_model || !has_brain_steps ||
        !has_rate_unit || out_config->sensor_count == 0U || out_config->mapping_count == 0U)
    {
        set_error(error_message, error_message_size, "configuracao incompleta");
        return 0;
    }
    return 1;
}

int sensor_encoding_demo_config_write_file(
    const char *filename, const SensorEncodingDemoConfig *config,
    char *error_message, size_t error_message_size)
{
    FILE *file;
    int write_failed = 0;

    if (error_message != NULL && error_message_size > 0U)
        error_message[0] = '\0';
    if (filename == NULL || config == NULL || config->sensor_count == 0U ||
        config->mapping_count == 0U)
    {
        set_error(error_message, error_message_size, "argumento invalido");
        return 0;
    }
    file = fopen(filename, "wb");
    if (file == NULL)
    {
        set_error(error_message, error_message_size, "nao foi possivel criar %s", filename);
        return 0;
    }
    if (fprintf(file, "[run]\nrun_name = %s\n\n[network]\nneurons = %u\nmodel = %s\n"
                     "\n[sensor_encoder]\nbrain_steps_per_tick = %u\n"
                     "rate_unit = pulses_per_neural_step\n\n[sensors]\n",
                config->run_name, config->neuron_count,
                minisnn_neuron_model_name(config->neuron_model),
                config->brain_steps_per_tick) < 0)
        write_failed = 1;
    for (uint32_t index = 0U; !write_failed && index < config->sensor_count; index++)
    {
        const SensorEncodingDemoSensor *sensor = &config->sensors[index];
        if (fprintf(file, "%s = %.17g,%.17g,%.17g\n", sensor->name, sensor->minimum,
                    sensor->maximum, sensor->default_value) < 0)
            write_failed = 1;
    }
    if (!write_failed && fprintf(file, "\n[mappings]\n") < 0)
        write_failed = 1;
    for (uint32_t index = 0U; !write_failed && index < config->mapping_count; index++)
    {
        const SensorEncodingDemoMapping *mapping = &config->mappings[index];
        const MiniSNNSensorEncodingSpec *spec = &mapping->spec;
        const char *name = config->sensors[mapping->sensor_index].name;

        if (spec->mode == MINISNN_SENSOR_ENCODING_DETERMINISTIC_RATE)
            write_failed = fprintf(file, "%s = deterministic_rate,%u,%u,%.17g,%.17g,%u\n",
                                   name, spec->target_neuron_start,
                                   spec->target_neuron_count, spec->pulse_current,
                                   spec->maximum_rate, spec->phase_offset) < 0;
        else
            write_failed = fprintf(file, "%s = %s,%u,%u,%.17g,%.17g\n", name,
                                   spec->mode == MINISNN_SENSOR_ENCODING_LINEAR_CURRENT ?
                                   "linear_current" : "bipolar_current",
                                   spec->target_neuron_start,
                                   spec->target_neuron_count, spec->gain, spec->bias) < 0;
    }
    if (fclose(file) != 0)
        write_failed = 1;
    if (write_failed)
    {
        set_error(error_message, error_message_size, "erro ao escrever %s", filename);
        return 0;
    }
    return 1;
}
