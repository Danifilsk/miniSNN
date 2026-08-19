#include "g0_event_log.h"

#include <stdio.h>
#include <string.h>

static int copy_field(char *out, size_t capacity, const char *value)
{
    size_t length;

    if (out == NULL || capacity == 0U || value == NULL)
        return 0;
    length = strlen(value);
    if (length >= capacity)
        return 0;
    memcpy(out, value, length + 1U);
    return 1;
}

void g0_event_log_init(G0EventLog *log)
{
    if (log != NULL)
    {
        memset(log, 0, sizeof(*log));
        log->next_sequence = UINT64_C(1);
    }
}

void g0_event_log_clear(G0EventLog *log)
{
    g0_event_log_init(log);
}

size_t g0_event_log_count(const G0EventLog *log)
{
    return log == NULL ? 0U : log->count;
}

int g0_event_log_at(const G0EventLog *log, size_t index, G0LogEntry *out_entry)
{
    size_t offset;

    if (log == NULL || out_entry == NULL || index >= log->count)
        return 0;
    offset = (log->first + index) % G0_EVENT_LOG_CAPACITY;
    *out_entry = log->entries[offset];
    return 1;
}

int g0_event_log_append(
    G0EventLog *log,
    uint64_t tick,
    G0LogCategory category,
    int has_actor,
    uint64_t actor,
    const char *event,
    const char *result,
    const char *reason,
    const char *text)
{
    G0LogEntry entry;
    size_t offset;

    if (log == NULL || category < G0_LOG_CATEGORY_SYSTEM ||
        category > G0_LOG_CATEGORY_DEATH || event == NULL || result == NULL ||
        reason == NULL || text == NULL || log->next_sequence == UINT64_MAX)
    {
        return 0;
    }
    memset(&entry, 0, sizeof(entry));
    entry.sequence = log->next_sequence++;
    entry.tick = tick;
    entry.actor = actor;
    entry.has_actor = has_actor != 0;
    entry.category = category;
    if (!copy_field(entry.event, sizeof(entry.event), event) ||
        !copy_field(entry.result, sizeof(entry.result), result) ||
        !copy_field(entry.reason, sizeof(entry.reason), reason) ||
        !copy_field(entry.text, sizeof(entry.text), text))
    {
        --log->next_sequence;
        return 0;
    }
    if (log->count == G0_EVENT_LOG_CAPACITY)
    {
        offset = log->first;
        log->first = (log->first + 1U) % G0_EVENT_LOG_CAPACITY;
    }
    else
    {
        offset = (log->first + log->count) % G0_EVENT_LOG_CAPACITY;
        ++log->count;
    }
    log->entries[offset] = entry;
    return 1;
}

const char *g0_log_category_name(G0LogCategory category)
{
    switch (category)
    {
        case G0_LOG_CATEGORY_SYSTEM: return "SYSTEM";
        case G0_LOG_CATEGORY_ACTION: return "ACTION";
        case G0_LOG_CATEGORY_FOOD: return "FOOD";
        case G0_LOG_CATEGORY_ENERGY: return "ENERGY";
        case G0_LOG_CATEGORY_ERROR: return "ERROR";
        case G0_LOG_CATEGORY_NEURAL: return "NEURAL";
        case G0_LOG_CATEGORY_REWARD: return "REWARD";
        case G0_LOG_CATEGORY_TRAINING: return "TRAINING";
        case G0_LOG_CATEGORY_DEATH: return "DEATH";
        default: return "UNKNOWN";
    }
}

static int csv_field(FILE *file, const char *value)
{
    const unsigned char *cursor = (const unsigned char *)value;

    if (file == NULL || value == NULL || fputc('"', file) == EOF)
        return 0;
    while (*cursor != '\0')
    {
        if (*cursor == '"' && fputc('"', file) == EOF)
            return 0;
        if (fputc(*cursor, file) == EOF)
            return 0;
        ++cursor;
    }
    return fputc('"', file) != EOF;
}

int g0_event_log_export_csv(const G0EventLog *log, const char *filename)
{
    FILE *file;
    size_t index;

    if (log == NULL || filename == NULL || filename[0] == '\0')
        return 0;
    file = fopen(filename, "w");
    if (file == NULL)
        return 0;
    if (fprintf(file, "sequence,tick,category,actor,event,result,reason,text\n") < 0)
        goto fail;
    for (index = 0U; index < log->count; ++index)
    {
        G0LogEntry entry;
        if (!g0_event_log_at(log, index, &entry) ||
            fprintf(file, "%llu,%llu,%s,",
                    (unsigned long long)entry.sequence,
                    (unsigned long long)entry.tick,
                    g0_log_category_name(entry.category)) < 0)
        {
            goto fail;
        }
        if (entry.has_actor)
        {
            if (fprintf(file, "%llu,", (unsigned long long)entry.actor) < 0)
                goto fail;
        }
        else if (fputc(',', file) == EOF)
        {
            goto fail;
        }
        if (!csv_field(file, entry.event) || fputc(',', file) == EOF ||
            !csv_field(file, entry.result) || fputc(',', file) == EOF ||
            !csv_field(file, entry.reason) || fputc(',', file) == EOF ||
            !csv_field(file, entry.text) || fputc('\n', file) == EOF)
        {
            goto fail;
        }
    }
    if (fclose(file) != 0)
        return 0;
    return 1;
fail:
    (void)fclose(file);
    return 0;
}
