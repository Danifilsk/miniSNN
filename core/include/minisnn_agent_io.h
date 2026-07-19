#ifndef MINISNN_AGENT_IO_H
#define MINISNN_AGENT_IO_H

#include <stdint.h>

/* C7.1 is intentionally numeric and domain-neutral. */
#define MINISNN_AGENT_IO_MAX_CHANNELS 256U
#define MINISNN_AGENT_IO_MAX_CHANNEL_NAME_LENGTH 96U

typedef struct
{
    uint32_t id;
    const char *name;
    double minimum;
    double maximum;
    double default_value;
} MiniSNNSensorChannelSpec;

typedef struct
{
    uint32_t id;
    const char *name;
    double minimum;
    double maximum;
    double default_value;
} MiniSNNActionChannelSpec;

typedef struct MiniSNNSensorSchema MiniSNNSensorSchema;
typedef struct MiniSNNActionSchema MiniSNNActionSchema;
typedef struct MiniSNNAgentIOContext MiniSNNAgentIOContext;

typedef struct
{
    uint64_t tick;
    uint32_t value_count;
    double *values;
} MiniSNNSensorFrame;

typedef struct
{
    uint64_t tick;
    uint32_t value_count;
    double *values;
} MiniSNNActionFrame;

typedef enum
{
    MINISNN_AGENT_IO_ERROR_NONE = 0,
    MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT,
    MINISNN_AGENT_IO_ERROR_ALLOCATION,
    MINISNN_AGENT_IO_ERROR_SCHEMA_EMPTY,
    MINISNN_AGENT_IO_ERROR_SCHEMA_TOO_LARGE,
    MINISNN_AGENT_IO_ERROR_CHANNEL_ID_DUPLICATE,
    MINISNN_AGENT_IO_ERROR_CHANNEL_NAME_EMPTY,
    MINISNN_AGENT_IO_ERROR_CHANNEL_NAME_TOO_LONG,
    MINISNN_AGENT_IO_ERROR_CHANNEL_NAME_INVALID,
    MINISNN_AGENT_IO_ERROR_CHANNEL_NAME_DUPLICATE,
    MINISNN_AGENT_IO_ERROR_CHANNEL_LIMITS_INVALID,
    MINISNN_AGENT_IO_ERROR_NONFINITE_VALUE,
    MINISNN_AGENT_IO_ERROR_VALUE_COUNT_MISMATCH,
    MINISNN_AGENT_IO_ERROR_VALUE_OUT_OF_RANGE,
    MINISNN_AGENT_IO_ERROR_TICK_REPEATED,
    MINISNN_AGENT_IO_ERROR_TICK_REGRESSIVE,
    MINISNN_AGENT_IO_ERROR_SENSOR_ALREADY_SUBMITTED,
    MINISNN_AGENT_IO_ERROR_ACTION_BEFORE_SENSOR,
    MINISNN_AGENT_IO_ERROR_ACTION_ALREADY_SUBMITTED,
    MINISNN_AGENT_IO_ERROR_ACTION_TICK_MISMATCH,
    MINISNN_AGENT_IO_ERROR_TICK_NOT_READY,
    MINISNN_AGENT_IO_ERROR_TICK_FINISHED,
    MINISNN_AGENT_IO_ERROR_SENSOR_NOT_AVAILABLE,
    MINISNN_AGENT_IO_ERROR_SENSOR_ALREADY_CONSUMED,
    MINISNN_AGENT_IO_ERROR_SENSOR_NOT_CONSUMED,
    MINISNN_AGENT_IO_ERROR_ACTION_NOT_AVAILABLE,
    MINISNN_AGENT_IO_ERROR_ACTION_ALREADY_CONSUMED,
    MINISNN_AGENT_IO_ERROR_PREVIOUS_ACTION_NOT_CONSUMED,
    MINISNN_AGENT_IO_ERROR_IO,
    MINISNN_AGENT_IO_ERROR_FORMAT
} MiniSNNAgentIOError;

/* Schemas copy every channel name and preserve the caller supplied order. */
MiniSNNSensorSchema *minisnn_sensor_schema_create(
    const MiniSNNSensorChannelSpec *channels,
    uint32_t channel_count,
    MiniSNNAgentIOError *out_error);

MiniSNNActionSchema *minisnn_action_schema_create(
    const MiniSNNActionChannelSpec *channels,
    uint32_t channel_count,
    MiniSNNAgentIOError *out_error);

void minisnn_sensor_schema_destroy(MiniSNNSensorSchema **schema_ptr);
void minisnn_action_schema_destroy(MiniSNNActionSchema **schema_ptr);

uint32_t minisnn_sensor_schema_channel_count(
    const MiniSNNSensorSchema *schema);
uint32_t minisnn_action_schema_channel_count(
    const MiniSNNActionSchema *schema);

int minisnn_sensor_schema_get_channel(
    const MiniSNNSensorSchema *schema,
    uint32_t index,
    MiniSNNSensorChannelSpec *out_channel);
int minisnn_action_schema_get_channel(
    const MiniSNNActionSchema *schema,
    uint32_t index,
    MiniSNNActionChannelSpec *out_channel);

uint64_t minisnn_sensor_schema_signature(const MiniSNNSensorSchema *schema);
uint64_t minisnn_action_schema_signature(const MiniSNNActionSchema *schema);

/* Frames own their value arrays after successful init. Initialize with {0}. */
int minisnn_sensor_frame_init(MiniSNNSensorFrame *frame, uint32_t value_count);
int minisnn_action_frame_init(MiniSNNActionFrame *frame, uint32_t value_count);
void minisnn_sensor_frame_destroy(MiniSNNSensorFrame *frame);
void minisnn_action_frame_destroy(MiniSNNActionFrame *frame);
int minisnn_sensor_frame_set_values(
    MiniSNNSensorFrame *frame,
    uint64_t tick,
    const double *values,
    uint32_t value_count,
    MiniSNNAgentIOError *out_error);
int minisnn_action_frame_set_values(
    MiniSNNActionFrame *frame,
    uint64_t tick,
    const double *values,
    uint32_t value_count,
    MiniSNNAgentIOError *out_error);
int minisnn_sensor_frame_reset(
    MiniSNNSensorFrame *frame,
    const MiniSNNSensorSchema *schema,
    MiniSNNAgentIOError *out_error);
int minisnn_action_frame_reset(
    MiniSNNActionFrame *frame,
    const MiniSNNActionSchema *schema,
    MiniSNNAgentIOError *out_error);

/* The context copies schemas and submitted frames; it never borrows them. */
MiniSNNAgentIOContext *minisnn_agent_io_create(
    const MiniSNNSensorSchema *sensor_schema,
    const MiniSNNActionSchema *action_schema,
    MiniSNNAgentIOError *out_error);
void minisnn_agent_io_destroy(MiniSNNAgentIOContext **context_ptr);
void minisnn_agent_io_reset(MiniSNNAgentIOContext *context);

uint64_t minisnn_agent_io_contract_signature(
    const MiniSNNAgentIOContext *context);
uint64_t minisnn_agent_io_sensor_schema_signature(
    const MiniSNNAgentIOContext *context);
MiniSNNAgentIOError minisnn_agent_io_last_error(
    const MiniSNNAgentIOContext *context);
const char *minisnn_agent_io_error_string(MiniSNNAgentIOError error);

int minisnn_agent_io_submit_sensor_frame(
    MiniSNNAgentIOContext *context,
    const MiniSNNSensorFrame *frame);
int minisnn_agent_io_consume_sensor_frame(
    MiniSNNAgentIOContext *context,
    MiniSNNSensorFrame *out_frame);
int minisnn_agent_io_submit_action_frame(
    MiniSNNAgentIOContext *context,
    const MiniSNNActionFrame *frame);
int minisnn_agent_io_finish_tick(MiniSNNAgentIOContext *context);
int minisnn_agent_io_consume_action_frame(
    MiniSNNAgentIOContext *context,
    MiniSNNActionFrame *out_frame);
int minisnn_agent_io_copy_last_sensor_frame(
    MiniSNNAgentIOContext *context,
    MiniSNNSensorFrame *out_frame);
int minisnn_agent_io_copy_last_action_frame(
    MiniSNNAgentIOContext *context,
    MiniSNNActionFrame *out_frame);

/* Stable versioned text formats. Files are rejected when incompatible. */
int minisnn_sensor_schema_write_file(
    const MiniSNNSensorSchema *schema,
    const char *filename,
    MiniSNNAgentIOError *out_error);
int minisnn_action_schema_write_file(
    const MiniSNNActionSchema *schema,
    const char *filename,
    MiniSNNAgentIOError *out_error);
MiniSNNSensorSchema *minisnn_sensor_schema_read_file(
    const char *filename,
    MiniSNNAgentIOError *out_error);
MiniSNNActionSchema *minisnn_action_schema_read_file(
    const char *filename,
    MiniSNNAgentIOError *out_error);

#endif
