#include "minisnn_agent_io.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AGENT_IO_FNV_OFFSET UINT64_C(14695981039346656037)
#define AGENT_IO_FNV_PRIME UINT64_C(1099511628211)
#define AGENT_IO_SENSOR_SCHEMA_VERSION "minisnn_agent_io_sensor_schema_v1"
#define AGENT_IO_ACTION_SCHEMA_VERSION "minisnn_agent_io_action_schema_v1"
#define AGENT_IO_CONTRACT_VERSION "minisnn_agent_io_contract_v1"
#define AGENT_IO_TEXT_LINE_MAX 1024U

typedef struct
{
    uint32_t id;
    char *name;
    double minimum;
    double maximum;
    double default_value;
} AgentIOChannel;

typedef struct
{
    uint32_t channel_count;
    AgentIOChannel *channels;
    uint64_t signature;
} AgentIOSchema;

struct MiniSNNSensorSchema
{
    AgentIOSchema data;
};

struct MiniSNNActionSchema
{
    AgentIOSchema data;
};

struct MiniSNNAgentIOContext
{
    AgentIOSchema sensor_schema;
    AgentIOSchema action_schema;
    MiniSNNSensorFrame pending_sensor;
    MiniSNNActionFrame pending_action;
    MiniSNNSensorFrame last_sensor;
    MiniSNNActionFrame last_action;
    uint64_t active_tick;
    uint64_t last_finished_tick;
    uint64_t signature;
    int sensor_submitted;
    int sensor_consumed;
    int action_submitted;
    int action_consumed;
    int has_finished_tick;
    MiniSNNAgentIOError last_error;
};

static void set_error(MiniSNNAgentIOError *out_error, MiniSNNAgentIOError error)
{
    if (out_error != NULL)
        *out_error = error;
}

static void context_error(MiniSNNAgentIOContext *context,
                          MiniSNNAgentIOError error)
{
    if (context != NULL)
        context->last_error = error;
}

static void hash_byte(uint64_t *hash, unsigned char value)
{
    *hash ^= (uint64_t)value;
    *hash *= AGENT_IO_FNV_PRIME;
}

static void hash_bytes(uint64_t *hash, const char *text)
{
    size_t length = strlen(text);

    for (size_t index = 0; index < length; index++)
        hash_byte(hash, (unsigned char)text[index]);
}

static void hash_u32(uint64_t *hash, uint32_t value)
{
    for (unsigned int shift = 0; shift < 32U; shift += 8U)
        hash_byte(hash, (unsigned char)((value >> shift) & 0xffU));
}

static void hash_u64(uint64_t *hash, uint64_t value)
{
    for (unsigned int shift = 0; shift < 64U; shift += 8U)
        hash_byte(hash, (unsigned char)((value >> shift) & 0xffU));
}

static void hash_double(uint64_t *hash, double value)
{
    uint64_t bits = 0U;

    memcpy(&bits, &value, sizeof(bits));
    hash_u64(hash, bits);
}

static uint64_t double_bits(double value)
{
    uint64_t bits = 0U;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static double bits_double(uint64_t bits)
{
    double value = 0.0;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint64_t schema_signature(const AgentIOSchema *schema,
                                 const char *version)
{
    uint64_t hash = AGENT_IO_FNV_OFFSET;

    hash_bytes(&hash, version);
    hash_u32(&hash, schema->channel_count);
    for (uint32_t index = 0; index < schema->channel_count; index++)
    {
        const AgentIOChannel *channel = &schema->channels[index];

        hash_u32(&hash, channel->id);
        hash_u32(&hash, (uint32_t)strlen(channel->name));
        hash_bytes(&hash, channel->name);
        hash_double(&hash, channel->minimum);
        hash_double(&hash, channel->maximum);
        hash_double(&hash, channel->default_value);
    }
    return hash;
}

static void schema_destroy_data(AgentIOSchema *schema)
{
    if (schema == NULL)
        return;
    if (schema->channels != NULL)
    {
        for (uint32_t index = 0; index < schema->channel_count; index++)
            free(schema->channels[index].name);
    }
    free(schema->channels);
    memset(schema, 0, sizeof(*schema));
}

static int valid_name(const char *name, MiniSNNAgentIOError *out_error)
{
    size_t length;

    if (name == NULL || name[0] == '\0')
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_CHANNEL_NAME_EMPTY);
        return 0;
    }
    length = strlen(name);
    if (length > MINISNN_AGENT_IO_MAX_CHANNEL_NAME_LENGTH)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_CHANNEL_NAME_TOO_LONG);
        return 0;
    }
    for (size_t index = 0; index < length; index++)
    {
        unsigned char value = (unsigned char)name[index];

        if (value < 0x20U || value > 0x7eU)
        {
            set_error(out_error, MINISNN_AGENT_IO_ERROR_CHANNEL_NAME_INVALID);
            return 0;
        }
    }
    return 1;
}

static int valid_channel(uint32_t id, const char *name, double minimum,
                         double maximum, double default_value,
                         MiniSNNAgentIOError *out_error)
{
    if (!valid_name(name, out_error))
        return 0;
    if (!isfinite(minimum) || !isfinite(maximum) || !isfinite(default_value))
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_NONFINITE_VALUE);
        return 0;
    }
    if (minimum > maximum || default_value < minimum ||
        default_value > maximum)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_CHANNEL_LIMITS_INVALID);
        return 0;
    }
    (void)id;
    return 1;
}

static int schema_has_duplicate(const AgentIOSchema *schema, uint32_t current,
                                uint32_t id, const char *name,
                                MiniSNNAgentIOError *out_error)
{
    for (uint32_t previous = 0; previous < current; previous++)
    {
        if (schema->channels[previous].id == id)
        {
            set_error(out_error, MINISNN_AGENT_IO_ERROR_CHANNEL_ID_DUPLICATE);
            return 1;
        }
        if (strcmp(schema->channels[previous].name, name) == 0)
        {
            set_error(out_error,
                      MINISNN_AGENT_IO_ERROR_CHANNEL_NAME_DUPLICATE);
            return 1;
        }
    }
    return 0;
}

static int schema_allocate(AgentIOSchema *schema, uint32_t channel_count,
                           MiniSNNAgentIOError *out_error)
{
    if (schema == NULL)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (channel_count == 0U)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_SCHEMA_EMPTY);
        return 0;
    }
    if (channel_count > MINISNN_AGENT_IO_MAX_CHANNELS)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_SCHEMA_TOO_LARGE);
        return 0;
    }
    schema->channels = NULL;
    schema->channel_count = 0U;
    schema->channels = calloc(channel_count, sizeof(*schema->channels));
    if (schema->channels == NULL)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_ALLOCATION);
        return 0;
    }
    schema->channel_count = channel_count;
    return 1;
}

static int schema_set_channel(AgentIOSchema *schema, uint32_t index,
                              uint32_t id, const char *name, double minimum,
                              double maximum, double default_value,
                              MiniSNNAgentIOError *out_error)
{
    AgentIOChannel *channel;
    size_t name_length;

    if (schema == NULL || index >= schema->channel_count ||
        !valid_channel(id, name, minimum, maximum, default_value, out_error) ||
        schema_has_duplicate(schema, index, id, name, out_error))
        return 0;
    name_length = strlen(name);
    channel = &schema->channels[index];
    channel->name = malloc(name_length + 1U);
    if (channel->name == NULL)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_ALLOCATION);
        return 0;
    }
    memcpy(channel->name, name, name_length + 1U);
    channel->id = id;
    channel->minimum = minimum;
    channel->maximum = maximum;
    channel->default_value = default_value;
    return 1;
}

static int schema_clone(AgentIOSchema *destination,
                        const AgentIOSchema *source,
                        const char *version,
                        MiniSNNAgentIOError *out_error)
{
    AgentIOSchema copy = {0};

    if (destination == NULL || source == NULL || source->channels == NULL ||
        source->channel_count == 0U)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (!schema_allocate(&copy, source->channel_count, out_error))
        return 0;
    for (uint32_t index = 0; index < copy.channel_count; index++)
    {
        const AgentIOChannel *source_channel = &source->channels[index];

        if (!schema_set_channel(&copy, index, source_channel->id,
                                source_channel->name, source_channel->minimum,
                                source_channel->maximum,
                                source_channel->default_value, out_error))
        {
            schema_destroy_data(&copy);
            return 0;
        }
    }
    copy.signature = schema_signature(&copy, version);
    *destination = copy;
    return 1;
}

static int frame_allocate(double **out_values, uint32_t value_count)
{
    if (out_values == NULL || value_count == 0U ||
        value_count > MINISNN_AGENT_IO_MAX_CHANNELS)
        return 0;
    *out_values = calloc(value_count, sizeof(**out_values));
    return *out_values != NULL;
}

static int frame_values_finite(const double *values, uint32_t value_count,
                               MiniSNNAgentIOError *out_error)
{
    if (values == NULL || value_count == 0U ||
        value_count > MINISNN_AGENT_IO_MAX_CHANNELS)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    for (uint32_t index = 0; index < value_count; index++)
    {
        if (!isfinite(values[index]))
        {
            set_error(out_error, MINISNN_AGENT_IO_ERROR_NONFINITE_VALUE);
            return 0;
        }
    }
    return 1;
}

static int frame_matches_schema(const double *values, uint32_t value_count,
                                const AgentIOSchema *schema,
                                MiniSNNAgentIOError *out_error)
{
    if (schema == NULL || schema->channels == NULL ||
        value_count != schema->channel_count)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_VALUE_COUNT_MISMATCH);
        return 0;
    }
    if (!frame_values_finite(values, value_count, out_error))
        return 0;
    for (uint32_t index = 0; index < value_count; index++)
    {
        if (values[index] < schema->channels[index].minimum ||
            values[index] > schema->channels[index].maximum)
        {
            set_error(out_error, MINISNN_AGENT_IO_ERROR_VALUE_OUT_OF_RANGE);
            return 0;
        }
    }
    return 1;
}

static int copy_frame_values(double *destination, uint32_t destination_count,
                             uint64_t *destination_tick,
                             const double *source, uint32_t source_count,
                             uint64_t source_tick)
{
    if (destination == NULL || destination_tick == NULL || source == NULL ||
        destination_count != source_count)
        return 0;
    memcpy(destination, source, (size_t)source_count * sizeof(*source));
    *destination_tick = source_tick;
    return 1;
}

static int append_encoded_name(char *out, size_t out_size, const char *name)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t used = 0;

    for (size_t index = 0; name[index] != '\0'; index++)
    {
        unsigned char value = (unsigned char)name[index];
        int unescaped = (value >= 'A' && value <= 'Z') ||
                        (value >= 'a' && value <= 'z') ||
                        (value >= '0' && value <= '9') || value == '_' ||
                        value == '-' || value == '.';

        if (used + (unescaped ? 1U : 3U) >= out_size)
            return 0;
        if (unescaped)
            out[used++] = (char)value;
        else
        {
            out[used++] = '%';
            out[used++] = hex[value >> 4U];
            out[used++] = hex[value & 0x0fU];
        }
    }
    out[used] = '\0';
    return 1;
}

static int hex_value(char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    return -1;
}

static int decode_name(const char *encoded, char *out, size_t out_size)
{
    size_t used = 0;

    for (size_t index = 0; encoded[index] != '\0'; index++)
    {
        unsigned char value;

        if (encoded[index] == '%')
        {
            int high;
            int low;

            if (encoded[index + 1U] == '\0' || encoded[index + 2U] == '\0')
                return 0;
            high = hex_value(encoded[index + 1U]);
            low = hex_value(encoded[index + 2U]);
            if (high < 0 || low < 0)
                return 0;
            value = (unsigned char)((high << 4U) | low);
            index += 2U;
        }
        else
            value = (unsigned char)encoded[index];
        if (value < 0x20U || value > 0x7eU || used + 1U >= out_size)
            return 0;
        out[used++] = (char)value;
    }
    out[used] = '\0';
    return used > 0U;
}

static int write_schema_file(const AgentIOSchema *schema, const char *filename,
                             const char *header,
                             MiniSNNAgentIOError *out_error)
{
    FILE *file;

    if (schema == NULL || schema->channels == NULL || filename == NULL ||
        filename[0] == '\0')
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    file = fopen(filename, "wb");
    if (file == NULL)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_IO);
        return 0;
    }
    if (fprintf(file, "%s\nchannel_count=%u\n", header,
                schema->channel_count) < 0)
    {
        fclose(file);
        set_error(out_error, MINISNN_AGENT_IO_ERROR_IO);
        return 0;
    }
    for (uint32_t index = 0; index < schema->channel_count; index++)
    {
        const AgentIOChannel *channel = &schema->channels[index];
        char encoded_name[MINISNN_AGENT_IO_MAX_CHANNEL_NAME_LENGTH * 3U + 1U];

        if (!append_encoded_name(encoded_name, sizeof(encoded_name),
                                 channel->name) ||
            fprintf(file, "channel=%u|%s|%016llx|%016llx|%016llx\n",
                    channel->id, encoded_name,
                    (unsigned long long)double_bits(channel->minimum),
                    (unsigned long long)double_bits(channel->maximum),
                    (unsigned long long)double_bits(channel->default_value)) < 0)
        {
            fclose(file);
            set_error(out_error, MINISNN_AGENT_IO_ERROR_IO);
            return 0;
        }
    }
    if (fclose(file) != 0)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_IO);
        return 0;
    }
    set_error(out_error, MINISNN_AGENT_IO_ERROR_NONE);
    return 1;
}

static int read_line(FILE *file, char *line, size_t line_size)
{
    size_t length;

    if (fgets(line, (int)line_size, file) == NULL)
        return 0;
    length = strlen(line);
    while (length > 0U && (line[length - 1U] == '\n' ||
                           line[length - 1U] == '\r'))
        line[--length] = '\0';
    return 1;
}

static int parse_u32(const char *text, uint32_t *out_value)
{
    char *end = NULL;
    unsigned long value;

    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value > UINT32_MAX)
        return 0;
    *out_value = (uint32_t)value;
    return 1;
}

static int parse_u64_hex(const char *text, uint64_t *out_value)
{
    char *end = NULL;
    unsigned long long value;

    errno = 0;
    value = strtoull(text, &end, 16);
    if (errno != 0 || end == text || *end != '\0')
        return 0;
    *out_value = (uint64_t)value;
    return 1;
}

static int parse_channel_line(AgentIOSchema *schema, uint32_t index,
                              char *line, MiniSNNAgentIOError *out_error)
{
    char *fields[5] = {0};
    char *cursor;
    char decoded_name[MINISNN_AGENT_IO_MAX_CHANNEL_NAME_LENGTH + 1U];
    uint32_t id;
    uint64_t minimum_bits;
    uint64_t maximum_bits;
    uint64_t default_bits;

    if (strncmp(line, "channel=", 8U) != 0)
        return 0;
    cursor = line + 8U;
    for (unsigned int field = 0; field < 5U; field++)
    {
        fields[field] = cursor;
        if (field < 4U)
        {
            cursor = strchr(cursor, '|');
            if (cursor == NULL)
                return 0;
            *cursor++ = '\0';
        }
    }
    if (strchr(fields[4], '|') != NULL || !parse_u32(fields[0], &id) ||
        !decode_name(fields[1], decoded_name, sizeof(decoded_name)) ||
        !parse_u64_hex(fields[2], &minimum_bits) ||
        !parse_u64_hex(fields[3], &maximum_bits) ||
        !parse_u64_hex(fields[4], &default_bits))
        return 0;
    return schema_set_channel(schema, index, id, decoded_name,
                              bits_double(minimum_bits),
                              bits_double(maximum_bits),
                              bits_double(default_bits), out_error);
}

static int read_schema_file(AgentIOSchema *out_schema, const char *filename,
                            const char *header,
                            MiniSNNAgentIOError *out_error)
{
    AgentIOSchema schema = {0};
    FILE *file;
    char line[AGENT_IO_TEXT_LINE_MAX];
    uint32_t channel_count;

    set_error(out_error, MINISNN_AGENT_IO_ERROR_NONE);
    if (out_schema == NULL || filename == NULL || filename[0] == '\0')
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_IO);
        return 0;
    }
    if (!read_line(file, line, sizeof(line)) || strcmp(line, header) != 0 ||
        !read_line(file, line, sizeof(line)) ||
        strncmp(line, "channel_count=", 14U) != 0 ||
        !parse_u32(line + 14U, &channel_count))
    {
        fclose(file);
        schema_destroy_data(&schema);
        set_error(out_error, MINISNN_AGENT_IO_ERROR_FORMAT);
        return 0;
    }
    if (!schema_allocate(&schema, channel_count, out_error))
    {
        fclose(file);
        schema_destroy_data(&schema);
        if (out_error == NULL || *out_error != MINISNN_AGENT_IO_ERROR_ALLOCATION)
            set_error(out_error, MINISNN_AGENT_IO_ERROR_FORMAT);
        return 0;
    }
    for (uint32_t index = 0; index < channel_count; index++)
    {
        if (!read_line(file, line, sizeof(line)) ||
            !parse_channel_line(&schema, index, line, out_error))
        {
            fclose(file);
            schema_destroy_data(&schema);
            set_error(out_error, MINISNN_AGENT_IO_ERROR_FORMAT);
            return 0;
        }
    }
    if (read_line(file, line, sizeof(line)))
    {
        fclose(file);
        schema_destroy_data(&schema);
        set_error(out_error, MINISNN_AGENT_IO_ERROR_FORMAT);
        return 0;
    }
    {
        int io_error = ferror(file);

        if (fclose(file) != 0)
            io_error = 1;
        if (io_error)
        {
            schema_destroy_data(&schema);
            set_error(out_error, MINISNN_AGENT_IO_ERROR_IO);
            return 0;
        }
    }
    schema.signature = schema_signature(&schema, header);
    *out_schema = schema;
    set_error(out_error, MINISNN_AGENT_IO_ERROR_NONE);
    return 1;
}

MiniSNNSensorSchema *minisnn_sensor_schema_create(
    const MiniSNNSensorChannelSpec *channels, uint32_t channel_count,
    MiniSNNAgentIOError *out_error)
{
    MiniSNNSensorSchema *schema;

    if (channels == NULL)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return NULL;
    }
    schema = calloc(1U, sizeof(*schema));
    if (schema == NULL || !schema_allocate(&schema->data, channel_count, out_error))
    {
        free(schema);
        if (schema == NULL)
            set_error(out_error, MINISNN_AGENT_IO_ERROR_ALLOCATION);
        return NULL;
    }
    for (uint32_t index = 0; index < channel_count; index++)
    {
        if (!schema_set_channel(&schema->data, index, channels[index].id,
                                channels[index].name, channels[index].minimum,
                                channels[index].maximum,
                                channels[index].default_value, out_error))
        {
            minisnn_sensor_schema_destroy(&schema);
            return NULL;
        }
    }
    schema->data.signature = schema_signature(&schema->data,
                                               AGENT_IO_SENSOR_SCHEMA_VERSION);
    set_error(out_error, MINISNN_AGENT_IO_ERROR_NONE);
    return schema;
}

MiniSNNActionSchema *minisnn_action_schema_create(
    const MiniSNNActionChannelSpec *channels, uint32_t channel_count,
    MiniSNNAgentIOError *out_error)
{
    MiniSNNActionSchema *schema;

    if (channels == NULL)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return NULL;
    }
    schema = calloc(1U, sizeof(*schema));
    if (schema == NULL || !schema_allocate(&schema->data, channel_count, out_error))
    {
        free(schema);
        if (schema == NULL)
            set_error(out_error, MINISNN_AGENT_IO_ERROR_ALLOCATION);
        return NULL;
    }
    for (uint32_t index = 0; index < channel_count; index++)
    {
        if (!schema_set_channel(&schema->data, index, channels[index].id,
                                channels[index].name, channels[index].minimum,
                                channels[index].maximum,
                                channels[index].default_value, out_error))
        {
            minisnn_action_schema_destroy(&schema);
            return NULL;
        }
    }
    schema->data.signature = schema_signature(&schema->data,
                                               AGENT_IO_ACTION_SCHEMA_VERSION);
    set_error(out_error, MINISNN_AGENT_IO_ERROR_NONE);
    return schema;
}

void minisnn_sensor_schema_destroy(MiniSNNSensorSchema **schema_ptr)
{
    if (schema_ptr == NULL || *schema_ptr == NULL)
        return;
    schema_destroy_data(&(*schema_ptr)->data);
    free(*schema_ptr);
    *schema_ptr = NULL;
}

void minisnn_action_schema_destroy(MiniSNNActionSchema **schema_ptr)
{
    if (schema_ptr == NULL || *schema_ptr == NULL)
        return;
    schema_destroy_data(&(*schema_ptr)->data);
    free(*schema_ptr);
    *schema_ptr = NULL;
}

uint32_t minisnn_sensor_schema_channel_count(const MiniSNNSensorSchema *schema)
{
    return schema != NULL ? schema->data.channel_count : 0U;
}

uint32_t minisnn_action_schema_channel_count(const MiniSNNActionSchema *schema)
{
    return schema != NULL ? schema->data.channel_count : 0U;
}

int minisnn_sensor_schema_get_channel(const MiniSNNSensorSchema *schema,
                                      uint32_t index,
                                      MiniSNNSensorChannelSpec *out_channel)
{
    const AgentIOChannel *channel;

    if (schema == NULL || out_channel == NULL ||
        index >= schema->data.channel_count)
        return 0;
    channel = &schema->data.channels[index];
    out_channel->id = channel->id;
    out_channel->name = channel->name;
    out_channel->minimum = channel->minimum;
    out_channel->maximum = channel->maximum;
    out_channel->default_value = channel->default_value;
    return 1;
}

int minisnn_action_schema_get_channel(const MiniSNNActionSchema *schema,
                                      uint32_t index,
                                      MiniSNNActionChannelSpec *out_channel)
{
    const AgentIOChannel *channel;

    if (schema == NULL || out_channel == NULL ||
        index >= schema->data.channel_count)
        return 0;
    channel = &schema->data.channels[index];
    out_channel->id = channel->id;
    out_channel->name = channel->name;
    out_channel->minimum = channel->minimum;
    out_channel->maximum = channel->maximum;
    out_channel->default_value = channel->default_value;
    return 1;
}

uint64_t minisnn_sensor_schema_signature(const MiniSNNSensorSchema *schema)
{
    return schema != NULL ? schema->data.signature : 0U;
}

uint64_t minisnn_action_schema_signature(const MiniSNNActionSchema *schema)
{
    return schema != NULL ? schema->data.signature : 0U;
}

int minisnn_sensor_frame_init(MiniSNNSensorFrame *frame, uint32_t value_count)
{
    double *values = NULL;

    if (frame == NULL || !frame_allocate(&values, value_count))
        return 0;
    free(frame->values);
    frame->tick = 0U;
    frame->value_count = value_count;
    frame->values = values;
    return 1;
}

int minisnn_action_frame_init(MiniSNNActionFrame *frame, uint32_t value_count)
{
    double *values = NULL;

    if (frame == NULL || !frame_allocate(&values, value_count))
        return 0;
    free(frame->values);
    frame->tick = 0U;
    frame->value_count = value_count;
    frame->values = values;
    return 1;
}

void minisnn_sensor_frame_destroy(MiniSNNSensorFrame *frame)
{
    if (frame == NULL)
        return;
    free(frame->values);
    memset(frame, 0, sizeof(*frame));
}

void minisnn_action_frame_destroy(MiniSNNActionFrame *frame)
{
    if (frame == NULL)
        return;
    free(frame->values);
    memset(frame, 0, sizeof(*frame));
}

int minisnn_sensor_frame_set_values(MiniSNNSensorFrame *frame, uint64_t tick,
                                    const double *values, uint32_t value_count,
                                    MiniSNNAgentIOError *out_error)
{
    if (frame == NULL || frame->values == NULL || values == NULL)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (frame->value_count != value_count)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_VALUE_COUNT_MISMATCH);
        return 0;
    }
    if (!frame_values_finite(values, value_count, out_error))
        return 0;
    memcpy(frame->values, values, (size_t)value_count * sizeof(*values));
    frame->tick = tick;
    set_error(out_error, MINISNN_AGENT_IO_ERROR_NONE);
    return 1;
}

int minisnn_action_frame_set_values(MiniSNNActionFrame *frame, uint64_t tick,
                                    const double *values, uint32_t value_count,
                                    MiniSNNAgentIOError *out_error)
{
    if (frame == NULL || frame->values == NULL || values == NULL)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (frame->value_count != value_count)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_VALUE_COUNT_MISMATCH);
        return 0;
    }
    if (!frame_values_finite(values, value_count, out_error))
        return 0;
    memcpy(frame->values, values, (size_t)value_count * sizeof(*values));
    frame->tick = tick;
    set_error(out_error, MINISNN_AGENT_IO_ERROR_NONE);
    return 1;
}

int minisnn_sensor_frame_reset(MiniSNNSensorFrame *frame,
                               const MiniSNNSensorSchema *schema,
                               MiniSNNAgentIOError *out_error)
{
    if (frame == NULL || schema == NULL || frame->values == NULL ||
        schema->data.channels == NULL)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (frame->value_count != schema->data.channel_count)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_VALUE_COUNT_MISMATCH);
        return 0;
    }
    for (uint32_t index = 0; index < frame->value_count; index++)
        frame->values[index] = schema->data.channels[index].default_value;
    frame->tick = 0U;
    set_error(out_error, MINISNN_AGENT_IO_ERROR_NONE);
    return 1;
}

int minisnn_action_frame_reset(MiniSNNActionFrame *frame,
                               const MiniSNNActionSchema *schema,
                               MiniSNNAgentIOError *out_error)
{
    if (frame == NULL || schema == NULL || frame->values == NULL ||
        schema->data.channels == NULL)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (frame->value_count != schema->data.channel_count)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_VALUE_COUNT_MISMATCH);
        return 0;
    }
    for (uint32_t index = 0; index < frame->value_count; index++)
        frame->values[index] = schema->data.channels[index].default_value;
    frame->tick = 0U;
    set_error(out_error, MINISNN_AGENT_IO_ERROR_NONE);
    return 1;
}

MiniSNNAgentIOContext *minisnn_agent_io_create(
    const MiniSNNSensorSchema *sensor_schema,
    const MiniSNNActionSchema *action_schema,
    MiniSNNAgentIOError *out_error)
{
    MiniSNNAgentIOContext *context;
    uint64_t signature = AGENT_IO_FNV_OFFSET;

    if (sensor_schema == NULL || action_schema == NULL)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return NULL;
    }
    context = calloc(1U, sizeof(*context));
    if (context == NULL ||
        !schema_clone(&context->sensor_schema, &sensor_schema->data,
                      AGENT_IO_SENSOR_SCHEMA_VERSION, out_error) ||
        !schema_clone(&context->action_schema, &action_schema->data,
                      AGENT_IO_ACTION_SCHEMA_VERSION, out_error) ||
        !minisnn_sensor_frame_init(&context->pending_sensor,
                                   sensor_schema->data.channel_count) ||
        !minisnn_action_frame_init(&context->pending_action,
                                   action_schema->data.channel_count) ||
        !minisnn_sensor_frame_init(&context->last_sensor,
                                   sensor_schema->data.channel_count) ||
        !minisnn_action_frame_init(&context->last_action,
                                   action_schema->data.channel_count))
    {
        if (context != NULL)
            minisnn_agent_io_destroy(&context);
        set_error(out_error, MINISNN_AGENT_IO_ERROR_ALLOCATION);
        return NULL;
    }
    hash_bytes(&signature, AGENT_IO_CONTRACT_VERSION);
    hash_u64(&signature, context->sensor_schema.signature);
    hash_u64(&signature, context->action_schema.signature);
    context->signature = signature;
    minisnn_agent_io_reset(context);
    set_error(out_error, MINISNN_AGENT_IO_ERROR_NONE);
    return context;
}

void minisnn_agent_io_destroy(MiniSNNAgentIOContext **context_ptr)
{
    MiniSNNAgentIOContext *context;

    if (context_ptr == NULL || *context_ptr == NULL)
        return;
    context = *context_ptr;
    schema_destroy_data(&context->sensor_schema);
    schema_destroy_data(&context->action_schema);
    minisnn_sensor_frame_destroy(&context->pending_sensor);
    minisnn_action_frame_destroy(&context->pending_action);
    minisnn_sensor_frame_destroy(&context->last_sensor);
    minisnn_action_frame_destroy(&context->last_action);
    free(context);
    *context_ptr = NULL;
}

void minisnn_agent_io_reset(MiniSNNAgentIOContext *context)
{
    if (context == NULL)
        return;
    for (uint32_t index = 0; index < context->sensor_schema.channel_count;
         index++)
    {
        context->pending_sensor.values[index] =
            context->sensor_schema.channels[index].default_value;
        context->last_sensor.values[index] =
            context->sensor_schema.channels[index].default_value;
    }
    for (uint32_t index = 0; index < context->action_schema.channel_count;
         index++)
    {
        context->pending_action.values[index] =
            context->action_schema.channels[index].default_value;
        context->last_action.values[index] =
            context->action_schema.channels[index].default_value;
    }
    context->pending_sensor.tick = 0U;
    context->pending_action.tick = 0U;
    context->last_sensor.tick = 0U;
    context->last_action.tick = 0U;
    context->active_tick = 0U;
    context->last_finished_tick = 0U;
    context->sensor_submitted = 0;
    context->sensor_consumed = 0;
    context->action_submitted = 0;
    context->action_consumed = 0;
    context->has_finished_tick = 0;
    context->last_error = MINISNN_AGENT_IO_ERROR_NONE;
}

uint64_t minisnn_agent_io_contract_signature(
    const MiniSNNAgentIOContext *context)
{
    return context != NULL ? context->signature : 0U;
}

MiniSNNAgentIOError minisnn_agent_io_last_error(
    const MiniSNNAgentIOContext *context)
{
    return context != NULL ? context->last_error
                           : MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT;
}

const char *minisnn_agent_io_error_string(MiniSNNAgentIOError error)
{
    static const char *const messages[] =
    {
        "ok", "argumento invalido", "falha de alocacao", "schema vazio",
        "schema excede o limite", "id de canal duplicado",
        "nome de canal vazio", "nome de canal longo demais",
        "nome de canal invalido", "nome de canal duplicado",
        "limites de canal invalidos",
        "valor nao finito", "quantidade de valores incompativel",
        "valor fora do intervalo", "tick repetido", "tick regressivo",
        "sensor ja submetido", "acao antes do sensor", "acao ja submetida",
        "tick da acao incompativel", "tick ainda nao esta pronto",
        "tick ja finalizado", "sensor ainda nao esta disponivel",
        "sensor ja consumido", "sensor ainda nao foi consumido",
        "acao ainda nao esta disponivel", "acao ja consumida",
        "acao anterior ainda nao foi consumida", "erro de E/S",
        "formato de schema incompativel"
    };

    if ((unsigned int)error >= sizeof(messages) / sizeof(messages[0]))
        return "erro de interface desconhecido";
    return messages[error];
}

int minisnn_agent_io_submit_sensor_frame(MiniSNNAgentIOContext *context,
                                          const MiniSNNSensorFrame *frame)
{
    MiniSNNAgentIOError error = MINISNN_AGENT_IO_ERROR_NONE;

    if (context == NULL)
        return 0;
    if (frame == NULL)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (context->sensor_submitted)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_SENSOR_ALREADY_SUBMITTED);
        return 0;
    }
    if (context->has_finished_tick)
    {
        if (!context->action_consumed)
        {
            context_error(context,
                          MINISNN_AGENT_IO_ERROR_PREVIOUS_ACTION_NOT_CONSUMED);
            return 0;
        }
        if (frame->tick < context->last_finished_tick)
        {
            context_error(context, MINISNN_AGENT_IO_ERROR_TICK_REGRESSIVE);
            return 0;
        }
        if (frame->tick == context->last_finished_tick)
        {
            context_error(context, MINISNN_AGENT_IO_ERROR_TICK_REPEATED);
            return 0;
        }
    }
    if (!frame_matches_schema(frame->values, frame->value_count,
                              &context->sensor_schema, &error))
    {
        context_error(context, error);
        return 0;
    }
    if (!copy_frame_values(context->pending_sensor.values,
                           context->pending_sensor.value_count,
                           &context->pending_sensor.tick, frame->values,
                           frame->value_count, frame->tick))
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    context->active_tick = frame->tick;
    context->sensor_submitted = 1;
    context->sensor_consumed = 0;
    context->last_error = MINISNN_AGENT_IO_ERROR_NONE;
    return 1;
}

int minisnn_agent_io_consume_sensor_frame(MiniSNNAgentIOContext *context,
                                           MiniSNNSensorFrame *out_frame)
{
    if (context == NULL)
        return 0;
    if (!context->sensor_submitted)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_SENSOR_NOT_AVAILABLE);
        return 0;
    }
    if (context->sensor_consumed)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_SENSOR_ALREADY_CONSUMED);
        return 0;
    }
    if (out_frame == NULL || out_frame->values == NULL)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (out_frame->value_count != context->pending_sensor.value_count)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_VALUE_COUNT_MISMATCH);
        return 0;
    }
    if (!copy_frame_values(out_frame->values, out_frame->value_count,
                           &out_frame->tick, context->pending_sensor.values,
                           context->pending_sensor.value_count,
                           context->pending_sensor.tick))
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    context->sensor_consumed = 1;
    context->last_error = MINISNN_AGENT_IO_ERROR_NONE;
    return 1;
}

int minisnn_agent_io_submit_action_frame(MiniSNNAgentIOContext *context,
                                          const MiniSNNActionFrame *frame)
{
    MiniSNNAgentIOError error = MINISNN_AGENT_IO_ERROR_NONE;

    if (context == NULL)
        return 0;
    if (frame == NULL)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (!context->sensor_submitted)
    {
        context_error(context,
                      context->has_finished_tick &&
                              frame->tick == context->last_finished_tick ?
                          MINISNN_AGENT_IO_ERROR_TICK_FINISHED :
                          MINISNN_AGENT_IO_ERROR_ACTION_BEFORE_SENSOR);
        return 0;
    }
    if (!context->sensor_consumed)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_SENSOR_NOT_CONSUMED);
        return 0;
    }
    if (context->action_submitted)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_ACTION_ALREADY_SUBMITTED);
        return 0;
    }
    if (frame->tick != context->active_tick)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_ACTION_TICK_MISMATCH);
        return 0;
    }
    if (!frame_matches_schema(frame->values, frame->value_count,
                              &context->action_schema, &error))
    {
        context_error(context, error);
        return 0;
    }
    if (!copy_frame_values(context->pending_action.values,
                           context->pending_action.value_count,
                           &context->pending_action.tick, frame->values,
                           frame->value_count, frame->tick))
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    context->action_submitted = 1;
    context->last_error = MINISNN_AGENT_IO_ERROR_NONE;
    return 1;
}

int minisnn_agent_io_finish_tick(MiniSNNAgentIOContext *context)
{
    if (context == NULL)
        return 0;
    if (!context->sensor_submitted || !context->sensor_consumed ||
        !context->action_submitted)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_TICK_NOT_READY);
        return 0;
    }
    if (!copy_frame_values(context->last_sensor.values,
                           context->last_sensor.value_count,
                           &context->last_sensor.tick,
                           context->pending_sensor.values,
                           context->pending_sensor.value_count,
                           context->pending_sensor.tick) ||
        !copy_frame_values(context->last_action.values,
                           context->last_action.value_count,
                           &context->last_action.tick,
                           context->pending_action.values,
                           context->pending_action.value_count,
                           context->pending_action.tick))
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    context->last_finished_tick = context->active_tick;
    context->has_finished_tick = 1;
    context->sensor_submitted = 0;
    context->sensor_consumed = 0;
    context->action_submitted = 0;
    context->action_consumed = 0;
    context->last_error = MINISNN_AGENT_IO_ERROR_NONE;
    return 1;
}

int minisnn_agent_io_consume_action_frame(MiniSNNAgentIOContext *context,
                                           MiniSNNActionFrame *out_frame)
{
    if (context == NULL)
        return 0;
    if (!context->has_finished_tick)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_ACTION_NOT_AVAILABLE);
        return 0;
    }
    if (context->action_consumed)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_ACTION_ALREADY_CONSUMED);
        return 0;
    }
    if (out_frame == NULL || out_frame->values == NULL)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (out_frame->value_count != context->last_action.value_count)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_VALUE_COUNT_MISMATCH);
        return 0;
    }
    if (!copy_frame_values(out_frame->values, out_frame->value_count,
                           &out_frame->tick, context->last_action.values,
                           context->last_action.value_count,
                           context->last_action.tick))
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    context->action_consumed = 1;
    context->last_error = MINISNN_AGENT_IO_ERROR_NONE;
    return 1;
}

int minisnn_agent_io_copy_last_sensor_frame(MiniSNNAgentIOContext *context,
                                             MiniSNNSensorFrame *out_frame)
{
    if (context == NULL)
        return 0;
    if (!context->has_finished_tick)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_SENSOR_NOT_AVAILABLE);
        return 0;
    }
    if (out_frame == NULL || out_frame->values == NULL)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (out_frame->value_count != context->last_sensor.value_count)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_VALUE_COUNT_MISMATCH);
        return 0;
    }
    if (!copy_frame_values(out_frame->values, out_frame->value_count,
                           &out_frame->tick, context->last_sensor.values,
                           context->last_sensor.value_count,
                           context->last_sensor.tick))
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    context->last_error = MINISNN_AGENT_IO_ERROR_NONE;
    return 1;
}

int minisnn_agent_io_copy_last_action_frame(MiniSNNAgentIOContext *context,
                                             MiniSNNActionFrame *out_frame)
{
    if (context == NULL)
        return 0;
    if (!context->has_finished_tick)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_ACTION_NOT_AVAILABLE);
        return 0;
    }
    if (out_frame == NULL || out_frame->values == NULL)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (out_frame->value_count != context->last_action.value_count)
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_VALUE_COUNT_MISMATCH);
        return 0;
    }
    if (!copy_frame_values(out_frame->values, out_frame->value_count,
                           &out_frame->tick, context->last_action.values,
                           context->last_action.value_count,
                           context->last_action.tick))
    {
        context_error(context, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    context->last_error = MINISNN_AGENT_IO_ERROR_NONE;
    return 1;
}

int minisnn_sensor_schema_write_file(const MiniSNNSensorSchema *schema,
                                     const char *filename,
                                     MiniSNNAgentIOError *out_error)
{
    if (schema == NULL)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    return write_schema_file(&schema->data, filename,
                             AGENT_IO_SENSOR_SCHEMA_VERSION, out_error);
}

int minisnn_action_schema_write_file(const MiniSNNActionSchema *schema,
                                     const char *filename,
                                     MiniSNNAgentIOError *out_error)
{
    if (schema == NULL)
    {
        set_error(out_error, MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    return write_schema_file(&schema->data, filename,
                             AGENT_IO_ACTION_SCHEMA_VERSION, out_error);
}

MiniSNNSensorSchema *minisnn_sensor_schema_read_file(
    const char *filename, MiniSNNAgentIOError *out_error)
{
    MiniSNNSensorSchema *schema = calloc(1U, sizeof(*schema));

    if (schema == NULL || !read_schema_file(&schema->data, filename,
                                            AGENT_IO_SENSOR_SCHEMA_VERSION,
                                            out_error))
    {
        free(schema);
        if (schema == NULL)
            set_error(out_error, MINISNN_AGENT_IO_ERROR_ALLOCATION);
        return NULL;
    }
    return schema;
}

MiniSNNActionSchema *minisnn_action_schema_read_file(
    const char *filename, MiniSNNAgentIOError *out_error)
{
    MiniSNNActionSchema *schema = calloc(1U, sizeof(*schema));

    if (schema == NULL || !read_schema_file(&schema->data, filename,
                                            AGENT_IO_ACTION_SCHEMA_VERSION,
                                            out_error))
    {
        free(schema);
        if (schema == NULL)
            set_error(out_error, MINISNN_AGENT_IO_ERROR_ALLOCATION);
        return NULL;
    }
    return schema;
}
