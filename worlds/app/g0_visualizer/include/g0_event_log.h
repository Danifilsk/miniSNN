#ifndef G0_EVENT_LOG_H
#define G0_EVENT_LOG_H

#include <stddef.h>
#include <stdint.h>

#define G0_EVENT_LOG_CAPACITY 256U
#define G0_EVENT_LOG_EVENT_MAX 48U
#define G0_EVENT_LOG_RESULT_MAX 32U
#define G0_EVENT_LOG_REASON_MAX 48U
#define G0_EVENT_LOG_TEXT_MAX 160U

typedef enum
{
    G0_LOG_CATEGORY_SYSTEM = 0,
    G0_LOG_CATEGORY_ACTION,
    G0_LOG_CATEGORY_FOOD,
    G0_LOG_CATEGORY_ENERGY,
    G0_LOG_CATEGORY_ERROR,
    G0_LOG_CATEGORY_NEURAL,
    G0_LOG_CATEGORY_REWARD,
    G0_LOG_CATEGORY_TRAINING,
    G0_LOG_CATEGORY_DEATH
} G0LogCategory;

typedef struct
{
    uint64_t sequence;
    uint64_t tick;
    uint64_t actor;
    int has_actor;
    G0LogCategory category;
    char event[G0_EVENT_LOG_EVENT_MAX];
    char result[G0_EVENT_LOG_RESULT_MAX];
    char reason[G0_EVENT_LOG_REASON_MAX];
    char text[G0_EVENT_LOG_TEXT_MAX];
} G0LogEntry;

typedef struct
{
    G0LogEntry entries[G0_EVENT_LOG_CAPACITY];
    size_t first;
    size_t count;
    uint64_t next_sequence;
} G0EventLog;

void g0_event_log_init(G0EventLog *log);
void g0_event_log_clear(G0EventLog *log);
size_t g0_event_log_count(const G0EventLog *log);
int g0_event_log_at(const G0EventLog *log, size_t index, G0LogEntry *out_entry);
int g0_event_log_append(
    G0EventLog *log,
    uint64_t tick,
    G0LogCategory category,
    int has_actor,
    uint64_t actor,
    const char *event,
    const char *result,
    const char *reason,
    const char *text);
const char *g0_log_category_name(G0LogCategory category);
int g0_event_log_export_csv(const G0EventLog *log, const char *filename);

#endif
