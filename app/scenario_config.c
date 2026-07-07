#include "scenario_config.h"

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
    FIELD_NEURONS,
    FIELD_INHIBITORY_FRACTION,
    FIELD_CONNECTION_PROBABILITY,
    FIELD_SEED,
    FIELD_DELAY,
    FIELD_MAX_SYNAPTIC_DELAY,
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
    FIELD_RECORD_NEURON,
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
    {"neurons", FIELD_NEURONS},
    {"inhibitory_fraction", FIELD_INHIBITORY_FRACTION},
    {"connection_probability", FIELD_CONNECTION_PROBABILITY},
    {"seed", FIELD_SEED},
    {"delay", FIELD_DELAY},
    {"max_synaptic_delay", FIELD_MAX_SYNAPTIC_DELAY},
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
    {"record_neuron", FIELD_RECORD_NEURON}
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

static int find_field(const char *key, ScenarioField *out_field)
{
    size_t count = sizeof(KEY_MAP) / sizeof(KEY_MAP[0]);

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
    unsigned int uint_value;
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

    case FIELD_NEURONS:
    case FIELD_DELAY:
    case FIELD_MAX_SYNAPTIC_DELAY:
    case FIELD_SOURCE_COUNT:
    case FIELD_STEPS:
    case FIELD_RECORD_NEURON:
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
        else
            config->record_neuron = int_value;

        return 1;

    case FIELD_INHIBITORY_FRACTION:
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
        if (!parse_double_value(value, &double_value))
        {
            set_line_error(
                error_message,
                error_message_size,
                line_number,
                "valor numerico invalido");
            return 0;
        }

        if (field == FIELD_INHIBITORY_FRACTION)
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
        else
            config->synaptic_decay = double_value;

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
           strcmp(topology, "random_balanced") == 0;
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

    config->neurons = 20;
    config->inhibitory_fraction = 0.20;
    config->connection_probability = 0.25;
    config->seed = 1U;
    config->delay = 1;
    config->max_synaptic_delay = 8;

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

    config->record_neuron = 0;
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

    if (config->neurons < 1 || config->neurons > 1000)
    {
        set_error(error_message, error_message_size, "neurons deve estar entre 1 e 1000");
        return 0;
    }

    if (config->steps <= 0)
    {
        set_error(error_message, error_message_size, "steps deve ser maior que zero");
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
        !isfinite(config->synaptic_decay))
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
    unsigned long long seen_fields = 0ULL;
    char line[512];
    int line_number = 0;

    if (filename == NULL || out_config == NULL)
    {
        set_error(error_message, error_message_size, "argumento nulo");
        return 0;
    }

    scenario_config_default(&config);

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
        unsigned long long mask;

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

        if (!find_field(key, &field))
        {
            char message[160];
            snprintf(message, sizeof(message), "chave desconhecida '%s'", key);
            fclose(file);
            set_line_error(error_message, error_message_size, line_number, message);
            return 0;
        }

        mask = 1ULL << (unsigned int)field;

        if ((seen_fields & mask) != 0ULL)
        {
            char message[160];
            snprintf(message, sizeof(message), "chave duplicada '%s'", key);
            fclose(file);
            set_line_error(error_message, error_message_size, line_number, message);
            return 0;
        }

        seen_fields |= mask;

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
            "[recording]\n"
            "record_neuron = %d\n",
            config->run_name,
            config->topology,
            config->neurons,
            config->inhibitory_fraction,
            config->connection_probability,
            config->seed,
            config->delay,
            config->max_synaptic_delay,
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
            config->record_neuron) < 0)
    {
        fclose(file);
        set_error(error_message, error_message_size, "erro ao escrever arquivo de cenario");
        return 0;
    }

    if (fclose(file) != 0)
    {
        set_error(error_message, error_message_size, "erro ao fechar arquivo de cenario");
        return 0;
    }

    return 1;
}
