#include "k0_scenario_config.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minisnn_worlds_kernel.h"

enum
{
    K0_SECTION_KERNEL = 1,
    K0_SECTION_SCENARIO = 2
};

enum
{
    K0_SEEN_KERNEL_CONFIG_VERSION = 1ULL << 0,
    K0_SEEN_MASTER_SEED = 1ULL << 1,
    K0_SEEN_SCENARIO_VERSION = 1ULL << 2,
    K0_SEEN_SCENARIO_ID = 1ULL << 3,
    K0_SEEN_TICKS = 1ULL << 4,
    K0_SEEN_INITIAL_ENTITIES = 1ULL << 5,
    K0_SEEN_MINIMUM_ALIVE = 1ULL << 6,
    K0_SEEN_MAXIMUM_ALIVE = 1ULL << 7,
    K0_SEEN_CREATE_INTERVAL = 1ULL << 8,
    K0_SEEN_CREATE_BATCH = 1ULL << 9,
    K0_SEEN_DESTROY_INTERVAL = 1ULL << 10,
    K0_SEEN_DESTROY_BATCH = 1ULL << 11,
    K0_SEEN_COMMAND_DELAY = 1ULL << 12,
    K0_SEEN_PRIORITY_BASE = 1ULL << 13,
    K0_SEEN_RANDOM_NAMESPACE = 1ULL << 14,
    K0_SEEN_RANDOM_STREAM = 1ULL << 15,
    K0_SEEN_RANDOM_DRAWS = 1ULL << 16,
    K0_SEEN_TRACE_INTERVAL = 1ULL << 17
};

#define K0_ALL_REQUIRED_FIELDS \
    (K0_SEEN_KERNEL_CONFIG_VERSION | K0_SEEN_MASTER_SEED | \
     K0_SEEN_SCENARIO_VERSION | K0_SEEN_SCENARIO_ID | K0_SEEN_TICKS | \
     K0_SEEN_INITIAL_ENTITIES | K0_SEEN_MINIMUM_ALIVE | \
     K0_SEEN_MAXIMUM_ALIVE | K0_SEEN_CREATE_INTERVAL | K0_SEEN_CREATE_BATCH | \
     K0_SEEN_DESTROY_INTERVAL | K0_SEEN_DESTROY_BATCH | K0_SEEN_COMMAND_DELAY | \
     K0_SEEN_PRIORITY_BASE | K0_SEEN_RANDOM_NAMESPACE | K0_SEEN_RANDOM_STREAM | \
     K0_SEEN_RANDOM_DRAWS | K0_SEEN_TRACE_INTERVAL)

static void set_error(char *message, size_t message_size, const char *format, ...)
{
    va_list arguments;

    if (message == NULL || message_size == 0U)
    {
        return;
    }
    va_start(arguments, format);
    (void)vsnprintf(message, message_size, format, arguments);
    va_end(arguments);
}

static char *trim_ascii(char *text)
{
    char *end;

    while (*text == ' ' || *text == '\t')
    {
        ++text;
    }
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t'))
    {
        --end;
    }
    *end = '\0';
    return text;
}

static int parse_u64(const char *text, uint64_t *out_value)
{
    uint64_t value = 0U;
    const unsigned char *cursor = (const unsigned char *)text;

    if (text == NULL || *text == '\0')
    {
        return 0;
    }
    while (*cursor != '\0')
    {
        uint64_t digit;

        if (*cursor < (unsigned char)'0' || *cursor > (unsigned char)'9')
        {
            return 0;
        }
        digit = (uint64_t)(*cursor - (unsigned char)'0');
        if (value > (UINT64_MAX - digit) / UINT64_C(10))
        {
            return 0;
        }
        value = value * UINT64_C(10) + digit;
        ++cursor;
    }
    *out_value = value;
    return 1;
}

static int scenario_id_is_valid(const char *text)
{
    size_t length = 0U;

    if (text == NULL || *text == '\0')
    {
        return 0;
    }
    while (*text != '\0')
    {
        unsigned char value = (unsigned char)*text;

        if (!((value >= (unsigned char)'A' && value <= (unsigned char)'Z') ||
              (value >= (unsigned char)'a' && value <= (unsigned char)'z') ||
              (value >= (unsigned char)'0' && value <= (unsigned char)'9') ||
              value == (unsigned char)'_' || value == (unsigned char)'-'))
        {
            return 0;
        }
        ++length;
        if (length > K0_SCENARIO_ID_MAX_LENGTH)
        {
            return 0;
        }
        ++text;
    }
    return 1;
}

static uint64_t fnv_append_byte(uint64_t hash, uint8_t value)
{
    return (hash ^ (uint64_t)value) * UINT64_C(1099511628211);
}

static uint64_t fnv_append_u32(uint64_t hash, uint32_t value)
{
    size_t index;

    for (index = 0U; index < 4U; ++index)
    {
        hash = fnv_append_byte(hash, (uint8_t)(value >> (index * 8U)));
    }
    return hash;
}

static uint64_t fnv_append_u64(uint64_t hash, uint64_t value)
{
    size_t index;

    for (index = 0U; index < 8U; ++index)
    {
        hash = fnv_append_byte(hash, (uint8_t)(value >> (index * 8U)));
    }
    return hash;
}

static uint64_t fnv_append_text(uint64_t hash, const char *text)
{
    while (*text != '\0')
    {
        hash = fnv_append_byte(hash, (uint8_t)*text);
        ++text;
    }
    return hash;
}

void k0_scenario_config_default(K0ScenarioConfig *out_config)
{
    if (out_config == NULL)
    {
        return;
    }
    memset(out_config, 0, sizeof(*out_config));
    out_config->kernel_config_version = MINISNN_WORLDS_KERNEL_CONFIG_VERSION;
    out_config->master_seed = MINISNN_WORLDS_KERNEL_DEFAULT_MASTER_SEED;
    out_config->scenario_version = 1U;
    (void)strcpy(out_config->scenario_id, "k0_integrated_demo");
    out_config->ticks = 1000U;
    out_config->initial_entities = 16U;
    out_config->minimum_alive_entities = 8U;
    out_config->maximum_alive_entities = 64U;
    out_config->create_interval = 7U;
    out_config->create_batch = 2U;
    out_config->destroy_interval = 11U;
    out_config->destroy_batch = 1U;
    out_config->command_delay = 1U;
    out_config->priority_base = 100U;
    out_config->random_namespace = 1U;
    out_config->random_stream = 1U;
    out_config->random_draws_per_tick = 1U;
    out_config->trace_interval = 1U;
}

int k0_scenario_config_validate(
    const K0ScenarioConfig *config,
    char *error_message,
    size_t error_message_size)
{
    if (config == NULL)
    {
        set_error(error_message, error_message_size, "config is null");
        return 0;
    }
    if (config->kernel_config_version != MINISNN_WORLDS_KERNEL_CONFIG_VERSION ||
        config->scenario_version != 1U)
    {
        set_error(error_message, error_message_size, "unsupported config or scenario version");
        return 0;
    }
    if (!scenario_id_is_valid(config->scenario_id))
    {
        set_error(error_message, error_message_size, "invalid scenario_id");
        return 0;
    }
    if (config->ticks == 0U || config->ticks > K0_SCENARIO_TICKS_MAX ||
        config->initial_entities > K0_SCENARIO_ENTITY_MAX ||
        config->maximum_alive_entities == 0U ||
        config->maximum_alive_entities > K0_SCENARIO_ENTITY_MAX ||
        config->minimum_alive_entities > config->maximum_alive_entities ||
        config->initial_entities > config->maximum_alive_entities ||
        config->create_interval == 0U || config->destroy_interval == 0U ||
        config->command_delay == 0U || config->trace_interval == 0U ||
        config->create_batch == 0U || config->create_batch > K0_SCENARIO_BATCH_MAX ||
        config->destroy_batch == 0U || config->destroy_batch > K0_SCENARIO_BATCH_MAX ||
        config->random_draws_per_tick > K0_SCENARIO_RANDOM_DRAWS_MAX)
    {
        set_error(error_message, error_message_size, "scenario numeric bounds are invalid");
        return 0;
    }
    if (config->random_stream == UINT64_MAX ||
        (config->random_namespace == 0U && config->random_stream == 0U) ||
        config->ticks > UINT64_MAX - config->command_delay)
    {
        set_error(error_message, error_message_size, "random key or tick range is invalid");
        return 0;
    }
    set_error(error_message, error_message_size, "");
    return 1;
}

static int set_numeric_field(
    const char *key,
    const char *value,
    uint64_t *in_out_seen,
    uint64_t mask,
    uint64_t *destination)
{
    uint64_t parsed;

    if ((*in_out_seen & mask) != 0U || !parse_u64(value, &parsed))
    {
        return 0;
    }
    *destination = parsed;
    *in_out_seen |= mask;
    (void)key;
    return 1;
}

static int set_value(
    K0ScenarioConfig *config,
    int section,
    const char *key,
    const char *value,
    uint64_t *in_out_seen)
{
    uint64_t parsed;

    if (section == K0_SECTION_KERNEL)
    {
        if (strcmp(key, "config_version") == 0)
        {
            if (!set_numeric_field(key, value, in_out_seen,
                                   K0_SEEN_KERNEL_CONFIG_VERSION, &parsed) ||
                parsed > UINT32_MAX)
            {
                return 0;
            }
            config->kernel_config_version = (uint32_t)parsed;
            return 1;
        }
        if (strcmp(key, "master_seed") == 0)
        {
            return set_numeric_field(key, value, in_out_seen,
                                     K0_SEEN_MASTER_SEED, &config->master_seed);
        }
        return 0;
    }
    if (section != K0_SECTION_SCENARIO)
    {
        return 0;
    }
    if (strcmp(key, "scenario_version") == 0)
    {
        if (!set_numeric_field(key, value, in_out_seen,
                               K0_SEEN_SCENARIO_VERSION, &parsed) || parsed > UINT32_MAX)
        {
            return 0;
        }
        config->scenario_version = (uint32_t)parsed;
        return 1;
    }
    if (strcmp(key, "scenario_id") == 0)
    {
        if ((*in_out_seen & K0_SEEN_SCENARIO_ID) != 0U || !scenario_id_is_valid(value))
        {
            return 0;
        }
        (void)strcpy(config->scenario_id, value);
        *in_out_seen |= K0_SEEN_SCENARIO_ID;
        return 1;
    }
    if (strcmp(key, "ticks") == 0) return set_numeric_field(key, value, in_out_seen, K0_SEEN_TICKS, &config->ticks);
    if (strcmp(key, "initial_entities") == 0) return set_numeric_field(key, value, in_out_seen, K0_SEEN_INITIAL_ENTITIES, &config->initial_entities);
    if (strcmp(key, "minimum_alive_entities") == 0) return set_numeric_field(key, value, in_out_seen, K0_SEEN_MINIMUM_ALIVE, &config->minimum_alive_entities);
    if (strcmp(key, "maximum_alive_entities") == 0) return set_numeric_field(key, value, in_out_seen, K0_SEEN_MAXIMUM_ALIVE, &config->maximum_alive_entities);
    if (strcmp(key, "create_interval") == 0) return set_numeric_field(key, value, in_out_seen, K0_SEEN_CREATE_INTERVAL, &config->create_interval);
    if (strcmp(key, "create_batch") == 0) return set_numeric_field(key, value, in_out_seen, K0_SEEN_CREATE_BATCH, &config->create_batch);
    if (strcmp(key, "destroy_interval") == 0) return set_numeric_field(key, value, in_out_seen, K0_SEEN_DESTROY_INTERVAL, &config->destroy_interval);
    if (strcmp(key, "destroy_batch") == 0) return set_numeric_field(key, value, in_out_seen, K0_SEEN_DESTROY_BATCH, &config->destroy_batch);
    if (strcmp(key, "command_delay") == 0) return set_numeric_field(key, value, in_out_seen, K0_SEEN_COMMAND_DELAY, &config->command_delay);
    if (strcmp(key, "priority_base") == 0)
    {
        if (!set_numeric_field(key, value, in_out_seen, K0_SEEN_PRIORITY_BASE, &parsed) || parsed > UINT32_MAX) return 0;
        config->priority_base = (uint32_t)parsed;
        return 1;
    }
    if (strcmp(key, "random_namespace") == 0) return set_numeric_field(key, value, in_out_seen, K0_SEEN_RANDOM_NAMESPACE, &config->random_namespace);
    if (strcmp(key, "random_stream") == 0) return set_numeric_field(key, value, in_out_seen, K0_SEEN_RANDOM_STREAM, &config->random_stream);
    if (strcmp(key, "random_draws_per_tick") == 0) return set_numeric_field(key, value, in_out_seen, K0_SEEN_RANDOM_DRAWS, &config->random_draws_per_tick);
    if (strcmp(key, "trace_interval") == 0) return set_numeric_field(key, value, in_out_seen, K0_SEEN_TRACE_INTERVAL, &config->trace_interval);
    return 0;
}

int k0_scenario_config_load_file(
    const char *filename,
    K0ScenarioConfig *out_config,
    char *error_message,
    size_t error_message_size)
{
    FILE *file;
    char line[K0_SCENARIO_CONFIG_LINE_MAX_LENGTH + 2U];
    K0ScenarioConfig candidate;
    uint64_t seen = 0U;
    int section = 0;
    int sections_seen = 0;
    size_t line_number = 0U;
    long file_size;

    if (filename == NULL || out_config == NULL)
    {
        set_error(error_message, error_message_size, "filename or output config is null");
        return 0;
    }
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        set_error(error_message, error_message_size, "cannot open config");
        return 0;
    }
    if (fseek(file, 0L, SEEK_END) != 0 || (file_size = ftell(file)) < 0 ||
        (uint64_t)file_size > K0_SCENARIO_CONFIG_FILE_MAX_SIZE ||
        fseek(file, 0L, SEEK_SET) != 0)
    {
        (void)fclose(file);
        set_error(error_message, error_message_size, "config file is too large or unreadable");
        return 0;
    }
    k0_scenario_config_default(&candidate);
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *text;
        char *equals;
        size_t length;

        ++line_number;
        length = strlen(line);
        if (length == K0_SCENARIO_CONFIG_LINE_MAX_LENGTH + 1U && line[length - 1U] != '\n')
        {
            (void)fclose(file);
            set_error(error_message, error_message_size, "line %llu is too long", (unsigned long long)line_number);
            return 0;
        }
        if (length != 0U && line[length - 1U] == '\n')
        {
            line[--length] = '\0';
            if (length != 0U && line[length - 1U] == '\r')
            {
                line[--length] = '\0';
            }
        }
        if (strchr(line, '\r') != NULL)
        {
            (void)fclose(file);
            set_error(error_message, error_message_size, "line %llu has isolated CR", (unsigned long long)line_number);
            return 0;
        }
        text = trim_ascii(line);
        if (*text == '\0' || *text == '#' || *text == ';')
        {
            continue;
        }
        if (text[0] == '[')
        {
            if (strcmp(text, "[kernel]") == 0 && (sections_seen & K0_SECTION_KERNEL) == 0)
            {
                section = K0_SECTION_KERNEL;
                sections_seen |= K0_SECTION_KERNEL;
                continue;
            }
            if (strcmp(text, "[scenario]") == 0 && (sections_seen & K0_SECTION_SCENARIO) == 0)
            {
                section = K0_SECTION_SCENARIO;
                sections_seen |= K0_SECTION_SCENARIO;
                continue;
            }
            (void)fclose(file);
            set_error(error_message, error_message_size, "line %llu has unknown or duplicate section", (unsigned long long)line_number);
            return 0;
        }
        equals = strchr(text, '=');
        if (section == 0 || equals == NULL || strchr(equals + 1, '=') != NULL)
        {
            (void)fclose(file);
            set_error(error_message, error_message_size, "line %llu is not a valid assignment", (unsigned long long)line_number);
            return 0;
        }
        *equals = '\0';
        text = trim_ascii(text);
        equals = trim_ascii(equals + 1);
        if (*text == '\0' || *equals == '\0' || !set_value(&candidate, section, text, equals, &seen))
        {
            (void)fclose(file);
            set_error(error_message, error_message_size, "line %llu has an unknown, duplicate or invalid key", (unsigned long long)line_number);
            return 0;
        }
    }
    if (ferror(file) != 0 || fclose(file) != 0)
    {
        set_error(error_message, error_message_size, "cannot read config");
        return 0;
    }
    if (sections_seen != (K0_SECTION_KERNEL | K0_SECTION_SCENARIO) || seen != K0_ALL_REQUIRED_FIELDS)
    {
        set_error(error_message, error_message_size, "required section or field is missing");
        return 0;
    }
    if (!k0_scenario_config_validate(&candidate, error_message, error_message_size))
    {
        return 0;
    }
    *out_config = candidate;
    return 1;
}

uint64_t k0_scenario_config_signature(const K0ScenarioConfig *config)
{
    uint64_t hash = UINT64_C(14695981039346656037);

    if (config == NULL)
    {
        return 0U;
    }
    hash = fnv_append_text(hash, "MSWK_K0_SCENARIO_CONFIG_V1");
    hash = fnv_append_u32(hash, config->kernel_config_version);
    hash = fnv_append_u64(hash, config->master_seed);
    hash = fnv_append_u32(hash, config->scenario_version);
    hash = fnv_append_text(hash, config->scenario_id);
    hash = fnv_append_byte(hash, 0U);
    hash = fnv_append_u64(hash, config->ticks);
    hash = fnv_append_u64(hash, config->initial_entities);
    hash = fnv_append_u64(hash, config->minimum_alive_entities);
    hash = fnv_append_u64(hash, config->maximum_alive_entities);
    hash = fnv_append_u64(hash, config->create_interval);
    hash = fnv_append_u64(hash, config->create_batch);
    hash = fnv_append_u64(hash, config->destroy_interval);
    hash = fnv_append_u64(hash, config->destroy_batch);
    hash = fnv_append_u64(hash, config->command_delay);
    hash = fnv_append_u32(hash, config->priority_base);
    hash = fnv_append_u64(hash, config->random_namespace);
    hash = fnv_append_u64(hash, config->random_stream);
    hash = fnv_append_u64(hash, config->random_draws_per_tick);
    return fnv_append_u64(hash, config->trace_interval);
}

int k0_scenario_config_write_canonical(
    const K0ScenarioConfig *config,
    const char *filename,
    char *error_message,
    size_t error_message_size)
{
    FILE *file;

    if (!k0_scenario_config_validate(config, error_message, error_message_size) || filename == NULL)
    {
        return 0;
    }
    file = fopen(filename, "wb");
    if (file == NULL)
    {
        set_error(error_message, error_message_size, "cannot write canonical config");
        return 0;
    }
    if (fprintf(file,
                "[kernel]\nconfig_version=%u\nmaster_seed=%llu\n\n"
                "[scenario]\nscenario_version=%u\nscenario_id=%s\nticks=%llu\n"
                "initial_entities=%llu\nminimum_alive_entities=%llu\nmaximum_alive_entities=%llu\n"
                "create_interval=%llu\ncreate_batch=%llu\ndestroy_interval=%llu\ndestroy_batch=%llu\n"
                "command_delay=%llu\npriority_base=%u\nrandom_namespace=%llu\nrandom_stream=%llu\n"
                "random_draws_per_tick=%llu\ntrace_interval=%llu\n",
                config->kernel_config_version, (unsigned long long)config->master_seed,
                config->scenario_version, config->scenario_id, (unsigned long long)config->ticks,
                (unsigned long long)config->initial_entities,
                (unsigned long long)config->minimum_alive_entities,
                (unsigned long long)config->maximum_alive_entities,
                (unsigned long long)config->create_interval,
                (unsigned long long)config->create_batch,
                (unsigned long long)config->destroy_interval,
                (unsigned long long)config->destroy_batch,
                (unsigned long long)config->command_delay, config->priority_base,
                (unsigned long long)config->random_namespace,
                (unsigned long long)config->random_stream,
                (unsigned long long)config->random_draws_per_tick,
                (unsigned long long)config->trace_interval) < 0 || fclose(file) != 0)
    {
        set_error(error_message, error_message_size, "cannot finish canonical config");
        return 0;
    }
    set_error(error_message, error_message_size, "");
    return 1;
}
