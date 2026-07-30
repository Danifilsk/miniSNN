#include "k0_scenario_artifacts.h"

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "minisnn_worlds_kernel.h"

#ifdef _WIN32
#include <direct.h>
#define K0_MKDIR(path) _mkdir(path)
#else
#include <unistd.h>
#define K0_MKDIR(path) mkdir((path), 0777)
#endif

static const char *const artifact_names[] = {
    "trace.csv", "events.csv", "manifest.ini", "report.txt"
};
static const char *const temporary_names[] = {
    "trace.csv.tmp", "events.csv.tmp", "manifest.ini.tmp", "report.txt.tmp"
};
static const char *const backup_names[] = {
    "trace.csv.bak", "events.csv.bak", "manifest.ini.bak", "report.txt.bak"
};

typedef struct
{
    uint32_t artifact_format_version;
    uint32_t scenario_version;
    char scenario_id[K0_SCENARIO_ID_MAX_LENGTH + 1U];
    uint64_t scenario_config_signature;
    uint32_t kernel_config_version;
    uint64_t master_seed;
    uint32_t prng_version;
    uint32_t state_hash_version;
    uint64_t ticks_requested;
    uint64_t ticks_completed;
    uint64_t initial_state_hash;
    uint64_t final_state_hash;
    uint64_t trace_rows;
    uint64_t event_rows;
    uint64_t trace_signature;
    uint64_t events_signature;
    uint64_t final_alive_entities;
    uint64_t final_pending_commands;
    uint64_t total_entities_created;
    uint64_t total_entities_destroyed;
    uint64_t total_commands_submitted;
    uint64_t total_commands_applied;
    uint64_t total_commands_rejected;
    uint64_t total_events_emitted;
    uint64_t random_streams;
    uint64_t total_random_u32_generated;
} ArtifactManifest;

typedef struct
{
    char scenario_id[K0_SCENARIO_ID_MAX_LENGTH + 1U];
    uint64_t master_seed;
    uint64_t ticks;
    uint64_t scenario_config_signature;
    uint64_t initial_state_hash;
    uint64_t final_state_hash;
    uint64_t trace_rows;
    uint64_t event_rows;
    uint64_t alive_entities;
    uint64_t pending_commands;
    uint64_t entities_created;
    uint64_t entities_destroyed;
    uint64_t commands_submitted;
    uint64_t commands_applied;
    uint64_t commands_rejected;
    uint64_t events_emitted;
    uint64_t random_streams;
    uint64_t random_u32_generated;
    uint64_t trace_signature;
    uint64_t events_signature;
    uint32_t workload_version;
} ArtifactReport;

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

static int join_path(char *out_path, size_t out_size, const char *directory, const char *name)
{
    size_t length;

    if (out_path == NULL || directory == NULL || name == NULL)
    {
        return 0;
    }
    length = strlen(directory);
    return snprintf(out_path, out_size, "%s%s%s", directory,
                    length != 0U && (directory[length - 1U] == '/' || directory[length - 1U] == '\\') ? "" : "/",
                    name) >= 0 && strlen(out_path) + 1U <= out_size;
}

static int path_is_directory(const char *path)
{
    struct stat status;

    return stat(path, &status) == 0 && S_ISDIR(status.st_mode);
}

static int path_exists(const char *path)
{
    struct stat status;

    return stat(path, &status) == 0;
}

static int output_path_is_allowed(const char *path)
{
    char normalized[K0_SCENARIO_ARTIFACT_PATH_MAX];
    size_t index;

    if (path == NULL || strlen(path) + 1U > sizeof(normalized))
    {
        return 0;
    }
    (void)strcpy(normalized, path);
    for (index = 0U; normalized[index] != '\0'; ++index)
    {
        if (normalized[index] == '\\') normalized[index] = '/';
    }
    if (strstr(normalized, "build/worlds/kernel/") != NULL)
    {
        return 1;
    }
    return strstr(normalized, "worlds/kernel/src") == NULL &&
           strstr(normalized, "worlds/kernel/include") == NULL &&
           strstr(normalized, "worlds/kernel/tests") == NULL &&
           strstr(normalized, "worlds/kernel/configs") == NULL;
}

static int ensure_directory(const char *directory)
{
    char path[K0_SCENARIO_ARTIFACT_PATH_MAX];
    size_t index;

    if (directory == NULL || *directory == '\0' || strlen(directory) + 1U > sizeof(path))
    {
        return 0;
    }
    (void)strcpy(path, directory);
    for (index = 0U; path[index] != '\0'; ++index)
    {
        if (path[index] == '\\')
        {
            path[index] = '/';
        }
    }
    for (index = 1U; path[index] != '\0'; ++index)
    {
        if (path[index] == '/' && path[index - 1U] != ':')
        {
            path[index] = '\0';
            if (*path != '\0' && !path_is_directory(path) && K0_MKDIR(path) != 0 && errno != EEXIST)
            {
                return 0;
            }
            path[index] = '/';
        }
    }
    return path_is_directory(path) || (K0_MKDIR(path) == 0 || (errno == EEXIST && path_is_directory(path)));
}

static uint64_t fnv_append_byte(uint64_t hash, uint8_t value)
{
    return (hash ^ (uint64_t)value) * UINT64_C(1099511628211);
}

static int file_signature(const char *filename, uint64_t *out_signature)
{
    FILE *file = fopen(filename, "rb");
    uint64_t hash = UINT64_C(14695981039346656037);
    unsigned char buffer[4096];
    size_t count;
    size_t index;

    if (file == NULL)
    {
        return 0;
    }
    while ((count = fread(buffer, 1U, sizeof(buffer), file)) != 0U)
    {
        for (index = 0U; index < count; ++index)
        {
            hash = fnv_append_byte(hash, buffer[index]);
        }
    }
    if (ferror(file) != 0 || fclose(file) != 0)
    {
        return 0;
    }
    *out_signature = hash;
    return 1;
}

static int write_trace_file(const char *filename, const K0ScenarioRunResult *result)
{
    FILE *file = fopen(filename, "wb");
    size_t index;

    if (file == NULL ||
        fputs("tick,state_hash,alive_entities,pending_commands,last_tick_events,total_entities_created,total_entities_destroyed,total_commands_submitted,total_commands_applied,total_commands_rejected,total_events_emitted,random_streams,total_random_u32_generated,state_hash_version,prng_version\n", file) == EOF)
    {
        if (file != NULL) (void)fclose(file);
        return 0;
    }
    for (index = 0U; index < result->trace_row_count; ++index)
    {
        const K0ScenarioTraceRow *row = &result->trace_rows[index];

        if (fprintf(file,
                    "%llu,0x%016llX,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%u,%u\n",
                    (unsigned long long)row->tick, (unsigned long long)row->state_hash,
                    (unsigned long long)row->alive_entities,
                    (unsigned long long)row->pending_commands,
                    (unsigned long long)row->last_tick_events,
                    (unsigned long long)row->total_entities_created,
                    (unsigned long long)row->total_entities_destroyed,
                    (unsigned long long)row->total_commands_submitted,
                    (unsigned long long)row->total_commands_applied,
                    (unsigned long long)row->total_commands_rejected,
                    (unsigned long long)row->total_events_emitted,
                    (unsigned long long)row->random_streams,
                    (unsigned long long)row->total_random_u32_generated,
                    row->state_hash_version, row->prng_version) < 0)
        {
            (void)fclose(file);
            return 0;
        }
    }
    return fclose(file) == 0;
}

static int write_events_file(const char *filename, const K0ScenarioRunResult *result)
{
    FILE *file = fopen(filename, "wb");
    size_t index;

    if (file == NULL ||
        fputs("event_id,tick,event_type,command_id,issuer_entity_id,subject_entity_id,rejection_reason\n", file) == EOF)
    {
        if (file != NULL) (void)fclose(file);
        return 0;
    }
    for (index = 0U; index < result->event_row_count; ++index)
    {
        const K0ScenarioEventRow *row = &result->event_rows[index];

        if (fprintf(file, "%llu,%llu,%u,%llu,%llu,%llu,%u\n",
                    (unsigned long long)row->event_id, (unsigned long long)row->tick,
                    row->event_type, (unsigned long long)row->command_id,
                    (unsigned long long)row->issuer_entity_id,
                    (unsigned long long)row->subject_entity_id,
                    row->rejection_reason) < 0)
        {
            (void)fclose(file);
            return 0;
        }
    }
    return fclose(file) == 0;
}

static int write_manifest_file(
    const char *filename,
    const K0ScenarioRunResult *result,
    uint64_t trace_signature,
    uint64_t events_signature)
{
    const K0ScenarioConfig *config = &result->config;
    FILE *file = fopen(filename, "wb");

    if (file == NULL)
    {
        return 0;
    }
    if (fprintf(file,
                "artifact_format_version=1\nscenario_version=%u\nscenario_id=%s\nscenario_config_signature=%llu\n"
                "kernel_config_version=%u\nmaster_seed=%llu\nprng_version=%u\nstate_hash_version=%u\n"
                "ticks_requested=%llu\nticks_completed=%llu\ninitial_state_hash=0x%016llX\nfinal_state_hash=0x%016llX\n"
                "trace_rows=%llu\nevent_rows=%llu\nfinal_alive_entities=%llu\nfinal_pending_commands=%llu\n"
                "total_entities_created=%llu\ntotal_entities_destroyed=%llu\ntotal_commands_submitted=%llu\n"
                "total_commands_applied=%llu\ntotal_commands_rejected=%llu\ntotal_events_emitted=%llu\n"
                "random_streams=%llu\ntotal_random_u32_generated=%llu\ntrace_file_signature=%llu\n"
                "events_file_signature=%llu\nstatus=OK\n",
                config->scenario_version, config->scenario_id,
                (unsigned long long)result->scenario_config_signature,
                config->kernel_config_version, (unsigned long long)config->master_seed,
                result->prng_version, result->state_hash_version,
                (unsigned long long)config->ticks, (unsigned long long)result->ticks_completed,
                (unsigned long long)result->initial_state_hash,
                (unsigned long long)result->final_state_hash,
                (unsigned long long)result->trace_row_count,
                (unsigned long long)result->event_row_count,
                (unsigned long long)result->final_alive_entities,
                (unsigned long long)result->final_pending_commands,
                (unsigned long long)result->total_entities_created,
                (unsigned long long)result->total_entities_destroyed,
                (unsigned long long)result->total_commands_submitted,
                (unsigned long long)result->total_commands_applied,
                (unsigned long long)result->total_commands_rejected,
                (unsigned long long)result->total_events_emitted,
                (unsigned long long)result->random_streams,
                (unsigned long long)result->total_random_u32_generated,
                (unsigned long long)trace_signature,
                (unsigned long long)events_signature) < 0)
    {
        (void)fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static int write_report_file(
    const char *filename,
    const K0ScenarioRunResult *result,
    uint64_t trace_signature,
    uint64_t events_signature)
{
    const K0ScenarioConfig *config = &result->config;
    FILE *file = fopen(filename, "wb");

    if (file == NULL)
    {
        return 0;
    }
    if (fprintf(file,
                "miniSNN Worlds Kernel K0-D report\nstatus=OK\nscenario_id=%s\nmaster_seed=%llu\n"
                "ticks=%llu\nscenario_config_signature=%llu\ninitial_state_hash=0x%016llX\n"
                "final_state_hash=0x%016llX\ntrace_rows=%llu\nevent_rows=%llu\n"
                "alive_entities=%llu\npending_commands=%llu\nentities_created=%llu\n"
                "entities_destroyed=%llu\ncommands_submitted=%llu\ncommands_applied=%llu\n"
                "commands_rejected=%llu\nevents_emitted=%llu\nrandom_streams=%llu\n"
                "random_u32_generated=%llu\ntrace_file_signature=%llu\nevents_file_signature=%llu\n"
                "workload_version=1\nworkload_note=generic deterministic entity-command workload\n",
                config->scenario_id, (unsigned long long)config->master_seed,
                (unsigned long long)result->ticks_completed,
                (unsigned long long)result->scenario_config_signature,
                (unsigned long long)result->initial_state_hash,
                (unsigned long long)result->final_state_hash,
                (unsigned long long)result->trace_row_count,
                (unsigned long long)result->event_row_count,
                (unsigned long long)result->final_alive_entities,
                (unsigned long long)result->final_pending_commands,
                (unsigned long long)result->total_entities_created,
                (unsigned long long)result->total_entities_destroyed,
                (unsigned long long)result->total_commands_submitted,
                (unsigned long long)result->total_commands_applied,
                (unsigned long long)result->total_commands_rejected,
                (unsigned long long)result->total_events_emitted,
                (unsigned long long)result->random_streams,
                (unsigned long long)result->total_random_u32_generated,
                (unsigned long long)trace_signature,
                (unsigned long long)events_signature) < 0)
    {
        (void)fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static int parse_unsigned(const char *text, uint64_t *out_value)
{
    uint64_t value = 0U;

    if (text == NULL || *text == '\0') return 0;
    while (*text != '\0')
    {
        uint64_t digit;
        if (*text < '0' || *text > '9') return 0;
        digit = (uint64_t)(*text - '0');
        if (value > (UINT64_MAX - digit) / UINT64_C(10)) return 0;
        value = value * UINT64_C(10) + digit;
        ++text;
    }
    *out_value = value;
    return 1;
}

static int parse_hex_hash(const char *text, uint64_t *out_value)
{
    uint64_t value = 0U;
    size_t index;

    if (text == NULL || strlen(text) != 18U || text[0] != '0' || text[1] != 'x') return 0;
    for (index = 2U; index < 18U; ++index)
    {
        unsigned char value_char = (unsigned char)text[index];
        uint64_t digit;
        if (value_char >= '0' && value_char <= '9') digit = (uint64_t)(value_char - '0');
        else if (value_char >= 'A' && value_char <= 'F') digit = (uint64_t)(value_char - 'A' + 10U);
        else return 0;
        value = (value << 4U) | digit;
    }
    *out_value = value;
    return 1;
}

static int validate_trace_file(
    const char *filename,
    uint64_t *out_rows,
    uint64_t *out_initial_state_hash,
    K0ScenarioTraceRow *out_last_row)
{
    static const char header[] =
        "tick,state_hash,alive_entities,pending_commands,last_tick_events,total_entities_created,total_entities_destroyed,total_commands_submitted,total_commands_applied,total_commands_rejected,total_events_emitted,random_streams,total_random_u32_generated,state_hash_version,prng_version\n";
    FILE *file = fopen(filename, "rb");
    char line[1024];
    uint64_t rows = 0U;
    uint64_t previous_tick = 0U;
    uint32_t expected_state_hash_version = 0U;
    uint32_t expected_prng_version = 0U;
    int have_row = 0;

    if (file == NULL || fgets(line, sizeof(line), file) == NULL || strcmp(line, header) != 0)
    {
        if (file != NULL) (void)fclose(file);
        return 0;
    }
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *fields[15];
        size_t count = 0U;
        char *cursor = line;
        K0ScenarioTraceRow row;
        uint64_t version_value;

        while (count < 15U)
        {
            fields[count++] = cursor;
            cursor = strchr(cursor, count == 15U ? '\n' : ',');
            if (cursor == NULL) break;
            *cursor++ = '\0';
        }
        memset(&row, 0, sizeof(row));
        if (count != 15U || cursor == NULL || *cursor != '\0' ||
            !parse_unsigned(fields[0], &row.tick) || !parse_hex_hash(fields[1], &row.state_hash) ||
            !parse_unsigned(fields[2], &row.alive_entities) ||
            !parse_unsigned(fields[3], &row.pending_commands) ||
            !parse_unsigned(fields[4], &row.last_tick_events) ||
            !parse_unsigned(fields[5], &row.total_entities_created) ||
            !parse_unsigned(fields[6], &row.total_entities_destroyed) ||
            !parse_unsigned(fields[7], &row.total_commands_submitted) ||
            !parse_unsigned(fields[8], &row.total_commands_applied) ||
            !parse_unsigned(fields[9], &row.total_commands_rejected) ||
            !parse_unsigned(fields[10], &row.total_events_emitted) ||
            !parse_unsigned(fields[11], &row.random_streams) ||
            !parse_unsigned(fields[12], &row.total_random_u32_generated) ||
            !parse_unsigned(fields[13], &version_value) || version_value > UINT32_MAX)
        {
            (void)fclose(file);
            return 0;
        }
        row.state_hash_version = (uint32_t)version_value;
        if (!parse_unsigned(fields[14], &version_value) || version_value > UINT32_MAX)
        {
            (void)fclose(file);
            return 0;
        }
        row.prng_version = (uint32_t)version_value;
        if ((!have_row && row.tick != 0U) || (have_row && row.tick <= previous_tick) ||
            (have_row && (row.state_hash_version != expected_state_hash_version ||
                          row.prng_version != expected_prng_version)))
        {
            (void)fclose(file);
            return 0;
        }
        if (!have_row)
        {
            *out_initial_state_hash = row.state_hash;
            expected_state_hash_version = row.state_hash_version;
            expected_prng_version = row.prng_version;
        }
        previous_tick = row.tick;
        *out_last_row = row;
        have_row = 1;
        ++rows;
    }
    if (ferror(file) != 0 || fclose(file) != 0 || !have_row)
    {
        return 0;
    }
    *out_rows = rows;
    return 1;
}

static int validate_events_file(const char *filename, uint64_t *out_rows)
{
    static const char header[] =
        "event_id,tick,event_type,command_id,issuer_entity_id,subject_entity_id,rejection_reason\n";
    FILE *file = fopen(filename, "rb");
    char line[512];
    uint64_t rows = 0U;
    uint64_t previous_event = 0U;
    uint64_t previous_tick = 0U;
    int have_event = 0;

    if (file == NULL || fgets(line, sizeof(line), file) == NULL || strcmp(line, header) != 0)
    {
        if (file != NULL) (void)fclose(file);
        return 0;
    }
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *fields[7];
        size_t count = 0U;
        char *cursor = line;
        uint64_t event_id;
        uint64_t tick;

        while (count < 7U)
        {
            fields[count++] = cursor;
            cursor = strchr(cursor, count == 7U ? '\n' : ',');
            if (cursor == NULL) break;
            *cursor++ = '\0';
        }
        uint64_t ignored;

        if (count != 7U || cursor == NULL || *cursor != '\0' ||
            !parse_unsigned(fields[0], &event_id) || !parse_unsigned(fields[1], &tick) ||
            !parse_unsigned(fields[2], &ignored) || !parse_unsigned(fields[3], &ignored) ||
            !parse_unsigned(fields[4], &ignored) || !parse_unsigned(fields[5], &ignored) ||
            !parse_unsigned(fields[6], &ignored))
        {
            (void)fclose(file);
            return 0;
        }
        if (have_event && (event_id <= previous_event || tick < previous_tick))
        {
            (void)fclose(file);
            return 0;
        }
        previous_event = event_id;
        previous_tick = tick;
        have_event = 1;
        ++rows;
    }
    if (ferror(file) != 0 || fclose(file) != 0)
    {
        return 0;
    }
    *out_rows = rows;
    return 1;
}

enum
{
    MANIFEST_ARTIFACT_FORMAT = UINT64_C(1) << 0,
    MANIFEST_SCENARIO_VERSION = UINT64_C(1) << 1,
    MANIFEST_SCENARIO_ID = UINT64_C(1) << 2,
    MANIFEST_CONFIG_SIGNATURE = UINT64_C(1) << 3,
    MANIFEST_KERNEL_CONFIG = UINT64_C(1) << 4,
    MANIFEST_MASTER_SEED = UINT64_C(1) << 5,
    MANIFEST_PRNG_VERSION = UINT64_C(1) << 6,
    MANIFEST_HASH_VERSION = UINT64_C(1) << 7,
    MANIFEST_TICKS_REQUESTED = UINT64_C(1) << 8,
    MANIFEST_TICKS_COMPLETED = UINT64_C(1) << 9,
    MANIFEST_INITIAL_HASH = UINT64_C(1) << 10,
    MANIFEST_FINAL_HASH = UINT64_C(1) << 11,
    MANIFEST_TRACE_ROWS = UINT64_C(1) << 12,
    MANIFEST_EVENT_ROWS = UINT64_C(1) << 13,
    MANIFEST_FINAL_ALIVE = UINT64_C(1) << 14,
    MANIFEST_FINAL_PENDING = UINT64_C(1) << 15,
    MANIFEST_ENTITIES_CREATED = UINT64_C(1) << 16,
    MANIFEST_ENTITIES_DESTROYED = UINT64_C(1) << 17,
    MANIFEST_COMMANDS_SUBMITTED = UINT64_C(1) << 18,
    MANIFEST_COMMANDS_APPLIED = UINT64_C(1) << 19,
    MANIFEST_COMMANDS_REJECTED = UINT64_C(1) << 20,
    MANIFEST_EVENTS_EMITTED = UINT64_C(1) << 21,
    MANIFEST_RANDOM_STREAMS = UINT64_C(1) << 22,
    MANIFEST_RANDOM_U32 = UINT64_C(1) << 23,
    MANIFEST_TRACE_SIGNATURE = UINT64_C(1) << 24,
    MANIFEST_EVENTS_SIGNATURE = UINT64_C(1) << 25,
    MANIFEST_STATUS = UINT64_C(1) << 26
};

#define MANIFEST_REQUIRED_MASK ((UINT64_C(1) << 27) - UINT64_C(1))

enum
{
    REPORT_STATUS = UINT64_C(1) << 0,
    REPORT_SCENARIO_ID = UINT64_C(1) << 1,
    REPORT_MASTER_SEED = UINT64_C(1) << 2,
    REPORT_TICKS = UINT64_C(1) << 3,
    REPORT_CONFIG_SIGNATURE = UINT64_C(1) << 4,
    REPORT_INITIAL_HASH = UINT64_C(1) << 5,
    REPORT_FINAL_HASH = UINT64_C(1) << 6,
    REPORT_TRACE_ROWS = UINT64_C(1) << 7,
    REPORT_EVENT_ROWS = UINT64_C(1) << 8,
    REPORT_ALIVE = UINT64_C(1) << 9,
    REPORT_PENDING = UINT64_C(1) << 10,
    REPORT_ENTITIES_CREATED = UINT64_C(1) << 11,
    REPORT_ENTITIES_DESTROYED = UINT64_C(1) << 12,
    REPORT_COMMANDS_SUBMITTED = UINT64_C(1) << 13,
    REPORT_COMMANDS_APPLIED = UINT64_C(1) << 14,
    REPORT_COMMANDS_REJECTED = UINT64_C(1) << 15,
    REPORT_EVENTS_EMITTED = UINT64_C(1) << 16,
    REPORT_RANDOM_STREAMS = UINT64_C(1) << 17,
    REPORT_RANDOM_U32 = UINT64_C(1) << 18,
    REPORT_TRACE_SIGNATURE = UINT64_C(1) << 19,
    REPORT_EVENTS_SIGNATURE = UINT64_C(1) << 20,
    REPORT_WORKLOAD_VERSION = UINT64_C(1) << 21,
    REPORT_WORKLOAD_NOTE = UINT64_C(1) << 22
};

#define REPORT_REQUIRED_MASK ((UINT64_C(1) << 23) - UINT64_C(1))

static int scenario_id_is_valid(const char *value)
{
    size_t length;
    size_t index;

    if (value == NULL)
    {
        return 0;
    }
    length = strlen(value);
    if (length == 0U || length > K0_SCENARIO_ID_MAX_LENGTH)
    {
        return 0;
    }
    for (index = 0U; index < length; ++index)
    {
        unsigned char character = (unsigned char)value[index];

        if (!((character >= 'A' && character <= 'Z') ||
              (character >= 'a' && character <= 'z') ||
              (character >= '0' && character <= '9') ||
              character == '_' || character == '-'))
        {
            return 0;
        }
    }
    return 1;
}

static int parse_u32(const char *text, uint32_t *out_value)
{
    uint64_t value;

    if (!parse_unsigned(text, &value) || value > UINT32_MAX)
    {
        return 0;
    }
    *out_value = (uint32_t)value;
    return 1;
}

static int split_strict_key_value(char *line, char **out_key, char **out_value)
{
    char *equals;
    size_t length;

    if (line == NULL || out_key == NULL || out_value == NULL)
    {
        return 0;
    }
    length = strlen(line);
    if (length < 3U || line[length - 1U] != '\n' || strchr(line, '\r') != NULL)
    {
        return 0;
    }
    line[length - 1U] = '\0';
    equals = strchr(line, '=');
    if (equals == NULL || equals == line || equals[1] == '\0')
    {
        return 0;
    }
    *equals = '\0';
    *out_key = line;
    *out_value = equals + 1;
    return 1;
}

static int set_manifest_field(
    ArtifactManifest *manifest,
    const char *key,
    const char *value,
    uint64_t *out_bit)
{
    *out_bit = 0U;
    if (strcmp(key, "artifact_format_version") == 0 && parse_u32(value, &manifest->artifact_format_version) && manifest->artifact_format_version == 1U) *out_bit = MANIFEST_ARTIFACT_FORMAT;
    else if (strcmp(key, "scenario_version") == 0 && parse_u32(value, &manifest->scenario_version) && manifest->scenario_version == 1U) *out_bit = MANIFEST_SCENARIO_VERSION;
    else if (strcmp(key, "scenario_id") == 0 && scenario_id_is_valid(value))
    {
        (void)strcpy(manifest->scenario_id, value);
        *out_bit = MANIFEST_SCENARIO_ID;
    }
    else if (strcmp(key, "scenario_config_signature") == 0 && parse_unsigned(value, &manifest->scenario_config_signature)) *out_bit = MANIFEST_CONFIG_SIGNATURE;
    else if (strcmp(key, "kernel_config_version") == 0 && parse_u32(value, &manifest->kernel_config_version) && manifest->kernel_config_version == MINISNN_WORLDS_KERNEL_CONFIG_VERSION) *out_bit = MANIFEST_KERNEL_CONFIG;
    else if (strcmp(key, "master_seed") == 0 && parse_unsigned(value, &manifest->master_seed)) *out_bit = MANIFEST_MASTER_SEED;
    else if (strcmp(key, "prng_version") == 0 && parse_u32(value, &manifest->prng_version) && manifest->prng_version == MINISNN_WORLDS_KERNEL_PRNG_VERSION) *out_bit = MANIFEST_PRNG_VERSION;
    else if (strcmp(key, "state_hash_version") == 0 && parse_u32(value, &manifest->state_hash_version) && manifest->state_hash_version == MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION) *out_bit = MANIFEST_HASH_VERSION;
    else if (strcmp(key, "ticks_requested") == 0 && parse_unsigned(value, &manifest->ticks_requested)) *out_bit = MANIFEST_TICKS_REQUESTED;
    else if (strcmp(key, "ticks_completed") == 0 && parse_unsigned(value, &manifest->ticks_completed)) *out_bit = MANIFEST_TICKS_COMPLETED;
    else if (strcmp(key, "initial_state_hash") == 0 && parse_hex_hash(value, &manifest->initial_state_hash)) *out_bit = MANIFEST_INITIAL_HASH;
    else if (strcmp(key, "final_state_hash") == 0 && parse_hex_hash(value, &manifest->final_state_hash)) *out_bit = MANIFEST_FINAL_HASH;
    else if (strcmp(key, "trace_rows") == 0 && parse_unsigned(value, &manifest->trace_rows)) *out_bit = MANIFEST_TRACE_ROWS;
    else if (strcmp(key, "event_rows") == 0 && parse_unsigned(value, &manifest->event_rows)) *out_bit = MANIFEST_EVENT_ROWS;
    else if (strcmp(key, "final_alive_entities") == 0 && parse_unsigned(value, &manifest->final_alive_entities)) *out_bit = MANIFEST_FINAL_ALIVE;
    else if (strcmp(key, "final_pending_commands") == 0 && parse_unsigned(value, &manifest->final_pending_commands)) *out_bit = MANIFEST_FINAL_PENDING;
    else if (strcmp(key, "total_entities_created") == 0 && parse_unsigned(value, &manifest->total_entities_created)) *out_bit = MANIFEST_ENTITIES_CREATED;
    else if (strcmp(key, "total_entities_destroyed") == 0 && parse_unsigned(value, &manifest->total_entities_destroyed)) *out_bit = MANIFEST_ENTITIES_DESTROYED;
    else if (strcmp(key, "total_commands_submitted") == 0 && parse_unsigned(value, &manifest->total_commands_submitted)) *out_bit = MANIFEST_COMMANDS_SUBMITTED;
    else if (strcmp(key, "total_commands_applied") == 0 && parse_unsigned(value, &manifest->total_commands_applied)) *out_bit = MANIFEST_COMMANDS_APPLIED;
    else if (strcmp(key, "total_commands_rejected") == 0 && parse_unsigned(value, &manifest->total_commands_rejected)) *out_bit = MANIFEST_COMMANDS_REJECTED;
    else if (strcmp(key, "total_events_emitted") == 0 && parse_unsigned(value, &manifest->total_events_emitted)) *out_bit = MANIFEST_EVENTS_EMITTED;
    else if (strcmp(key, "random_streams") == 0 && parse_unsigned(value, &manifest->random_streams)) *out_bit = MANIFEST_RANDOM_STREAMS;
    else if (strcmp(key, "total_random_u32_generated") == 0 && parse_unsigned(value, &manifest->total_random_u32_generated)) *out_bit = MANIFEST_RANDOM_U32;
    else if (strcmp(key, "trace_file_signature") == 0 && parse_unsigned(value, &manifest->trace_signature)) *out_bit = MANIFEST_TRACE_SIGNATURE;
    else if (strcmp(key, "events_file_signature") == 0 && parse_unsigned(value, &manifest->events_signature)) *out_bit = MANIFEST_EVENTS_SIGNATURE;
    else if (strcmp(key, "status") == 0 && strcmp(value, "OK") == 0) *out_bit = MANIFEST_STATUS;

    return *out_bit != 0U;
}

static int read_manifest(const char *filename, ArtifactManifest *out_manifest)
{
    FILE *file = fopen(filename, "rb");
    char line[256];
    uint64_t seen = 0U;

    if (file == NULL || out_manifest == NULL)
    {
        if (file != NULL) (void)fclose(file);
        return 0;
    }
    memset(out_manifest, 0, sizeof(*out_manifest));
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *key;
        char *value;
        uint64_t bit;

        if (!split_strict_key_value(line, &key, &value) ||
            !set_manifest_field(out_manifest, key, value, &bit) ||
            (seen & bit) != 0U)
        {
            (void)fclose(file);
            return 0;
        }
        seen |= bit;
    }
    if (ferror(file) != 0 || fclose(file) != 0 || seen != MANIFEST_REQUIRED_MASK ||
        out_manifest->ticks_requested == 0U ||
        out_manifest->ticks_completed != out_manifest->ticks_requested)
    {
        return 0;
    }
    return 1;
}

static int set_report_field(
    ArtifactReport *report,
    const char *key,
    const char *value,
    uint64_t *out_bit)
{
    *out_bit = 0U;
    if (strcmp(key, "status") == 0 && strcmp(value, "OK") == 0) *out_bit = REPORT_STATUS;
    else if (strcmp(key, "scenario_id") == 0 && scenario_id_is_valid(value))
    {
        (void)strcpy(report->scenario_id, value);
        *out_bit = REPORT_SCENARIO_ID;
    }
    else if (strcmp(key, "master_seed") == 0 && parse_unsigned(value, &report->master_seed)) *out_bit = REPORT_MASTER_SEED;
    else if (strcmp(key, "ticks") == 0 && parse_unsigned(value, &report->ticks)) *out_bit = REPORT_TICKS;
    else if (strcmp(key, "scenario_config_signature") == 0 && parse_unsigned(value, &report->scenario_config_signature)) *out_bit = REPORT_CONFIG_SIGNATURE;
    else if (strcmp(key, "initial_state_hash") == 0 && parse_hex_hash(value, &report->initial_state_hash)) *out_bit = REPORT_INITIAL_HASH;
    else if (strcmp(key, "final_state_hash") == 0 && parse_hex_hash(value, &report->final_state_hash)) *out_bit = REPORT_FINAL_HASH;
    else if (strcmp(key, "trace_rows") == 0 && parse_unsigned(value, &report->trace_rows)) *out_bit = REPORT_TRACE_ROWS;
    else if (strcmp(key, "event_rows") == 0 && parse_unsigned(value, &report->event_rows)) *out_bit = REPORT_EVENT_ROWS;
    else if (strcmp(key, "alive_entities") == 0 && parse_unsigned(value, &report->alive_entities)) *out_bit = REPORT_ALIVE;
    else if (strcmp(key, "pending_commands") == 0 && parse_unsigned(value, &report->pending_commands)) *out_bit = REPORT_PENDING;
    else if (strcmp(key, "entities_created") == 0 && parse_unsigned(value, &report->entities_created)) *out_bit = REPORT_ENTITIES_CREATED;
    else if (strcmp(key, "entities_destroyed") == 0 && parse_unsigned(value, &report->entities_destroyed)) *out_bit = REPORT_ENTITIES_DESTROYED;
    else if (strcmp(key, "commands_submitted") == 0 && parse_unsigned(value, &report->commands_submitted)) *out_bit = REPORT_COMMANDS_SUBMITTED;
    else if (strcmp(key, "commands_applied") == 0 && parse_unsigned(value, &report->commands_applied)) *out_bit = REPORT_COMMANDS_APPLIED;
    else if (strcmp(key, "commands_rejected") == 0 && parse_unsigned(value, &report->commands_rejected)) *out_bit = REPORT_COMMANDS_REJECTED;
    else if (strcmp(key, "events_emitted") == 0 && parse_unsigned(value, &report->events_emitted)) *out_bit = REPORT_EVENTS_EMITTED;
    else if (strcmp(key, "random_streams") == 0 && parse_unsigned(value, &report->random_streams)) *out_bit = REPORT_RANDOM_STREAMS;
    else if (strcmp(key, "random_u32_generated") == 0 && parse_unsigned(value, &report->random_u32_generated)) *out_bit = REPORT_RANDOM_U32;
    else if (strcmp(key, "trace_file_signature") == 0 && parse_unsigned(value, &report->trace_signature)) *out_bit = REPORT_TRACE_SIGNATURE;
    else if (strcmp(key, "events_file_signature") == 0 && parse_unsigned(value, &report->events_signature)) *out_bit = REPORT_EVENTS_SIGNATURE;
    else if (strcmp(key, "workload_version") == 0 && parse_u32(value, &report->workload_version) && report->workload_version == 1U) *out_bit = REPORT_WORKLOAD_VERSION;
    else if (strcmp(key, "workload_note") == 0 && strcmp(value, "generic deterministic entity-command workload") == 0) *out_bit = REPORT_WORKLOAD_NOTE;
    return *out_bit != 0U;
}

static int read_report(const char *filename, ArtifactReport *out_report)
{
    static const char title[] = "miniSNN Worlds Kernel K0-D report\n";
    FILE *file = fopen(filename, "rb");
    char line[256];
    uint64_t seen = 0U;

    if (file == NULL || out_report == NULL || fgets(line, sizeof(line), file) == NULL ||
        strcmp(line, title) != 0)
    {
        if (file != NULL) (void)fclose(file);
        return 0;
    }
    memset(out_report, 0, sizeof(*out_report));
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *key;
        char *value;
        uint64_t bit;

        if (!split_strict_key_value(line, &key, &value) ||
            !set_report_field(out_report, key, value, &bit) ||
            (seen & bit) != 0U)
        {
            (void)fclose(file);
            return 0;
        }
        seen |= bit;
    }
    if (ferror(file) != 0 || fclose(file) != 0 || seen != REPORT_REQUIRED_MASK)
    {
        return 0;
    }
    return 1;
}

static int report_matches_manifest(const ArtifactReport *report, const ArtifactManifest *manifest)
{
    return strcmp(report->scenario_id, manifest->scenario_id) == 0 &&
           report->master_seed == manifest->master_seed &&
           report->ticks == manifest->ticks_completed &&
           report->scenario_config_signature == manifest->scenario_config_signature &&
           report->initial_state_hash == manifest->initial_state_hash &&
           report->final_state_hash == manifest->final_state_hash &&
           report->trace_rows == manifest->trace_rows &&
           report->event_rows == manifest->event_rows &&
           report->alive_entities == manifest->final_alive_entities &&
           report->pending_commands == manifest->final_pending_commands &&
           report->entities_created == manifest->total_entities_created &&
           report->entities_destroyed == manifest->total_entities_destroyed &&
           report->commands_submitted == manifest->total_commands_submitted &&
           report->commands_applied == manifest->total_commands_applied &&
           report->commands_rejected == manifest->total_commands_rejected &&
           report->events_emitted == manifest->total_events_emitted &&
           report->random_streams == manifest->random_streams &&
           report->random_u32_generated == manifest->total_random_u32_generated &&
           report->trace_signature == manifest->trace_signature &&
           report->events_signature == manifest->events_signature;
}

static int validate_set(const char *directory, const char *const *names)
{
    char trace[K0_SCENARIO_ARTIFACT_PATH_MAX];
    char events[K0_SCENARIO_ARTIFACT_PATH_MAX];
    char manifest[K0_SCENARIO_ARTIFACT_PATH_MAX];
    char report[K0_SCENARIO_ARTIFACT_PATH_MAX];
    uint64_t trace_rows;
    uint64_t initial_state_hash;
    K0ScenarioTraceRow last_row;
    uint64_t event_rows;
    uint64_t trace_signature;
    uint64_t events_signature;
    ArtifactManifest parsed;
    ArtifactReport parsed_report;

    if (!join_path(trace, sizeof(trace), directory, names[0]) ||
        !join_path(events, sizeof(events), directory, names[1]) ||
        !join_path(manifest, sizeof(manifest), directory, names[2]) ||
        !join_path(report, sizeof(report), directory, names[3]) ||
        !validate_trace_file(trace, &trace_rows, &initial_state_hash, &last_row) ||
        !validate_events_file(events, &event_rows) ||
        !read_manifest(manifest, &parsed) || !read_report(report, &parsed_report) ||
        !file_signature(trace, &trace_signature) || !file_signature(events, &events_signature))
    {
        return 0;
    }
    return trace_rows == parsed.trace_rows && event_rows == parsed.event_rows &&
           initial_state_hash == parsed.initial_state_hash &&
           last_row.tick == parsed.ticks_completed && last_row.state_hash == parsed.final_state_hash &&
           last_row.state_hash_version == parsed.state_hash_version &&
           last_row.prng_version == parsed.prng_version &&
           last_row.alive_entities == parsed.final_alive_entities &&
           last_row.pending_commands == parsed.final_pending_commands &&
           last_row.total_entities_created == parsed.total_entities_created &&
           last_row.total_entities_destroyed == parsed.total_entities_destroyed &&
           last_row.total_commands_submitted == parsed.total_commands_submitted &&
           last_row.total_commands_applied == parsed.total_commands_applied &&
           last_row.total_commands_rejected == parsed.total_commands_rejected &&
           last_row.total_events_emitted == parsed.total_events_emitted &&
           last_row.random_streams == parsed.random_streams &&
           last_row.total_random_u32_generated == parsed.total_random_u32_generated &&
           trace_signature == parsed.trace_signature && events_signature == parsed.events_signature &&
           report_matches_manifest(&parsed_report, &parsed);
}

int k0_scenario_artifacts_check_output(
    const char *output_directory,
    int overwrite,
    char *error_message,
    size_t error_message_size)
{
    char path[K0_SCENARIO_ARTIFACT_PATH_MAX];
    size_t index;

    if (output_directory == NULL || *output_directory == '\0' || strlen(output_directory) >= 900U ||
        !output_path_is_allowed(output_directory))
    {
        set_error(error_message, error_message_size, "invalid or prohibited output directory");
        return 0;
    }
    if (path_exists(output_directory) && !path_is_directory(output_directory))
    {
        set_error(error_message, error_message_size, "output path is not a directory");
        return 0;
    }
    if (!overwrite)
    {
        for (index = 0U; index < 4U; ++index)
        {
            if (!join_path(path, sizeof(path), output_directory, artifact_names[index]))
            {
                set_error(error_message, error_message_size, "output path is too long");
                return 0;
            }
            if (path_exists(path))
            {
                set_error(error_message, error_message_size, "final artifact already exists; use --overwrite");
                return 0;
            }
        }
    }
    set_error(error_message, error_message_size, "");
    return 1;
}

static void cleanup_paths(const char *directory, const char *const *names)
{
    char path[K0_SCENARIO_ARTIFACT_PATH_MAX];
    size_t index;

    for (index = 0U; index < 4U; ++index)
    {
        if (join_path(path, sizeof(path), directory, names[index]))
        {
            (void)remove(path);
        }
    }
}

static int publish_paths(const char *directory, int overwrite)
{
    char final_path[K0_SCENARIO_ARTIFACT_PATH_MAX];
    char temporary_path[K0_SCENARIO_ARTIFACT_PATH_MAX];
    char backup_path[K0_SCENARIO_ARTIFACT_PATH_MAX];
    size_t index;

    if (overwrite)
    {
        for (index = 0U; index < 4U; ++index)
        {
            if (!join_path(final_path, sizeof(final_path), directory, artifact_names[index]) ||
                !join_path(backup_path, sizeof(backup_path), directory, backup_names[index]))
            {
                return 0;
            }
            (void)remove(backup_path);
            if (path_exists(final_path) && rename(final_path, backup_path) != 0)
            {
                goto restore;
            }
        }
    }
    for (index = 0U; index < 4U; ++index)
    {
        if (!join_path(final_path, sizeof(final_path), directory, artifact_names[index]) ||
            !join_path(temporary_path, sizeof(temporary_path), directory, temporary_names[index]) ||
            rename(temporary_path, final_path) != 0)
        {
            goto restore;
        }
    }
    cleanup_paths(directory, backup_names);
    return 1;

restore:
    for (index = 0U; index < 4U; ++index)
    {
        if (join_path(final_path, sizeof(final_path), directory, artifact_names[index]) &&
            join_path(backup_path, sizeof(backup_path), directory, backup_names[index]))
        {
            if (path_exists(backup_path))
            {
                (void)remove(final_path);
                (void)rename(backup_path, final_path);
            }
        }
    }
    return 0;
}

int k0_scenario_artifacts_write(
    const char *output_directory,
    const K0ScenarioRunResult *result,
    int overwrite,
    char *error_message,
    size_t error_message_size)
{
    char trace[K0_SCENARIO_ARTIFACT_PATH_MAX];
    char events[K0_SCENARIO_ARTIFACT_PATH_MAX];
    char manifest[K0_SCENARIO_ARTIFACT_PATH_MAX];
    char report[K0_SCENARIO_ARTIFACT_PATH_MAX];
    uint64_t trace_signature;
    uint64_t events_signature;

    if (result == NULL || !k0_scenario_artifacts_check_output(output_directory, overwrite,
                                                               error_message, error_message_size) ||
        !ensure_directory(output_directory) ||
        !join_path(trace, sizeof(trace), output_directory, temporary_names[0]) ||
        !join_path(events, sizeof(events), output_directory, temporary_names[1]) ||
        !join_path(manifest, sizeof(manifest), output_directory, temporary_names[2]) ||
        !join_path(report, sizeof(report), output_directory, temporary_names[3]))
    {
        if (error_message != NULL && *error_message == '\0') set_error(error_message, error_message_size, "cannot prepare output directory");
        return 0;
    }
    cleanup_paths(output_directory, temporary_names);
    if (!write_trace_file(trace, result) || !write_events_file(events, result) ||
        !file_signature(trace, &trace_signature) || !file_signature(events, &events_signature) ||
        !write_manifest_file(manifest, result, trace_signature, events_signature) ||
        !write_report_file(report, result, trace_signature, events_signature) ||
        !validate_set(output_directory, temporary_names) ||
        !publish_paths(output_directory, overwrite))
    {
        cleanup_paths(output_directory, temporary_names);
        set_error(error_message, error_message_size, "artifact write, validation or publish failed");
        return 0;
    }
    set_error(error_message, error_message_size, "");
    return 1;
}

int k0_scenario_artifacts_validate_directory(
    const char *output_directory,
    char *error_message,
    size_t error_message_size)
{
    if (output_directory == NULL || !validate_set(output_directory, artifact_names))
    {
        set_error(error_message, error_message_size, "artifact validation failed");
        return 0;
    }
    set_error(error_message, error_message_size, "");
    return 1;
}
