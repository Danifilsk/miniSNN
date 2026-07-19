#include "minisnn_sensor_encoder.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minisnn.h"

#define SENSOR_ENCODER_FNV_OFFSET UINT64_C(14695981039346656037)
#define SENSOR_ENCODER_FNV_PRIME UINT64_C(1099511628211)
#define SENSOR_ENCODER_MAPPING_VERSION "minisnn_sensor_encoding_mapping_v1"
#define SENSOR_ENCODER_CONTRACT_VERSION "minisnn_sensor_encoder_contract_v1"
#define SENSOR_ENCODER_TEXT_VERSION "minisnn_sensor_encoder_v1"
#define SENSOR_ENCODER_TEXT_LINE_MAX 1024U
#define SENSOR_ENCODER_PHASE_SCALE 1000U

struct MiniSNNSensorEncoder
{
    MiniSNNSensorSchema *sensor_schema;
    MiniSNNSensorEncodingSpec *mappings;
    uint32_t *channel_indices;
    double *phases;
    double *next_phases;
    double *scratch_currents;
    MiniSNNSensorFrame agent_frame;
    uint32_t mapping_count;
    uint32_t neuron_count;
    uint32_t brain_steps_per_tick;
    uint64_t mapping_signature;
    uint64_t contract_signature;
    MiniSNNSensorEncoderError last_error;
};

static void set_error(MiniSNNSensorEncoderError *out_error,
                      MiniSNNSensorEncoderError error)
{
    if (out_error != NULL)
        *out_error = error;
}

static void encoder_error(MiniSNNSensorEncoder *encoder,
                          MiniSNNSensorEncoderError error)
{
    if (encoder != NULL)
        encoder->last_error = error;
}

static int frame_value_count(uint32_t neuron_count, uint32_t brain_steps,
                             size_t *out_count)
{
    if (out_count == NULL || neuron_count == 0U || brain_steps == 0U ||
        (size_t)neuron_count > SIZE_MAX / (size_t)brain_steps)
        return 0;
    *out_count = (size_t)neuron_count * (size_t)brain_steps;
    return 1;
}

static void hash_byte(uint64_t *hash, unsigned char value)
{
    *hash ^= (uint64_t)value;
    *hash *= SENSOR_ENCODER_FNV_PRIME;
}

static void hash_text(uint64_t *hash, const char *text)
{
    for (size_t index = 0U; text[index] != '\0'; index++)
        hash_byte(hash, (unsigned char)text[index]);
}

static void hash_u32(uint64_t *hash, uint32_t value)
{
    for (unsigned int shift = 0U; shift < 32U; shift += 8U)
        hash_byte(hash, (unsigned char)((value >> shift) & 0xffU));
}

static void hash_u64(uint64_t *hash, uint64_t value)
{
    for (unsigned int shift = 0U; shift < 64U; shift += 8U)
        hash_byte(hash, (unsigned char)((value >> shift) & 0xffU));
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

static void hash_double(uint64_t *hash, double value)
{
    hash_u64(hash, double_bits(value));
}

static int valid_mode(MiniSNNSensorEncodingMode mode)
{
    return mode == MINISNN_SENSOR_ENCODING_LINEAR_CURRENT ||
           mode == MINISNN_SENSOR_ENCODING_BIPOLAR_CURRENT ||
           mode == MINISNN_SENSOR_ENCODING_DETERMINISTIC_RATE;
}

static int specs_overlap(const MiniSNNSensorEncodingSpec *left,
                         const MiniSNNSensorEncodingSpec *right)
{
    uint32_t left_end = left->target_neuron_start + left->target_neuron_count;
    uint32_t right_end = right->target_neuron_start + right->target_neuron_count;
    return left->target_neuron_start < right_end && right->target_neuron_start < left_end;
}

static int find_channel_index(const MiniSNNSensorSchema *schema, uint32_t id,
                              uint32_t *out_index)
{
    uint32_t count = minisnn_sensor_schema_channel_count(schema);

    for (uint32_t index = 0U; index < count; index++)
    {
        MiniSNNSensorChannelSpec channel;
        if (!minisnn_sensor_schema_get_channel(schema, index, &channel))
            return 0;
        if (channel.id == id)
        {
            *out_index = index;
            return 1;
        }
    }
    return 0;
}

static MiniSNNSensorSchema *clone_sensor_schema(
    const MiniSNNSensorSchema *schema, MiniSNNSensorEncoderError *out_error)
{
    uint32_t count = minisnn_sensor_schema_channel_count(schema);
    MiniSNNSensorChannelSpec *channels;
    MiniSNNSensorSchema *copy;
    MiniSNNAgentIOError schema_error = MINISNN_AGENT_IO_ERROR_NONE;

    if (schema == NULL || count == 0U)
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_INVALID_SCHEMA);
        return NULL;
    }
    channels = calloc(count, sizeof(*channels));
    if (channels == NULL)
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_ALLOCATION);
        return NULL;
    }
    for (uint32_t index = 0U; index < count; index++)
    {
        if (!minisnn_sensor_schema_get_channel(schema, index, &channels[index]))
        {
            free(channels);
            set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_INVALID_SCHEMA);
            return NULL;
        }
    }
    copy = minisnn_sensor_schema_create(channels, count, &schema_error);
    free(channels);
    if (copy == NULL)
    {
        set_error(out_error, schema_error == MINISNN_AGENT_IO_ERROR_ALLOCATION ?
                  MINISNN_SENSOR_ENCODER_ERROR_ALLOCATION :
                  MINISNN_SENSOR_ENCODER_ERROR_INVALID_SCHEMA);
    }
    return copy;
}

static int validate_spec(const MiniSNNSensorSchema *schema,
                         const MiniSNNSensorEncodingSpec *spec,
                         uint32_t neuron_count, uint32_t *out_channel_index,
                         MiniSNNSensorEncoderError *out_error)
{
    if (spec == NULL || out_channel_index == NULL || !valid_mode(spec->mode))
    {
        set_error(out_error, spec != NULL ?
                  MINISNN_SENSOR_ENCODER_ERROR_INVALID_ENCODING_MODE :
                  MINISNN_SENSOR_ENCODER_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (!find_channel_index(schema, spec->sensor_channel_id, out_channel_index))
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_UNKNOWN_SENSOR_CHANNEL);
        return 0;
    }
    if (spec->target_neuron_count == 0U ||
        spec->target_neuron_start >= neuron_count ||
        spec->target_neuron_count > neuron_count - spec->target_neuron_start)
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_INVALID_NEURON_RANGE);
        return 0;
    }
    if (!isfinite(spec->gain) || !isfinite(spec->bias) ||
        !isfinite(spec->pulse_current) || !isfinite(spec->maximum_rate))
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_INVALID_PARAMETER);
        return 0;
    }
    if (spec->mode == MINISNN_SENSOR_ENCODING_DETERMINISTIC_RATE &&
        (spec->pulse_current <= 0.0 || spec->maximum_rate <= 0.0 ||
         spec->maximum_rate > 1.0 || spec->phase_offset >= SENSOR_ENCODER_PHASE_SCALE))
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_INVALID_PARAMETER);
        return 0;
    }
    return 1;
}

static uint64_t mapping_signature(const MiniSNNSensorEncoder *encoder)
{
    uint64_t hash = SENSOR_ENCODER_FNV_OFFSET;

    hash_text(&hash, SENSOR_ENCODER_MAPPING_VERSION);
    hash_u64(&hash, minisnn_sensor_schema_signature(encoder->sensor_schema));
    hash_u32(&hash, encoder->neuron_count);
    hash_u32(&hash, encoder->brain_steps_per_tick);
    hash_u32(&hash, encoder->mapping_count);
    for (uint32_t index = 0U; index < encoder->mapping_count; index++)
    {
        const MiniSNNSensorEncodingSpec *spec = &encoder->mappings[index];

        hash_u32(&hash, spec->sensor_channel_id);
        hash_u32(&hash, spec->target_neuron_start);
        hash_u32(&hash, spec->target_neuron_count);
        hash_u32(&hash, (uint32_t)spec->mode);
        if (spec->mode == MINISNN_SENSOR_ENCODING_DETERMINISTIC_RATE)
        {
            hash_double(&hash, spec->pulse_current);
            hash_double(&hash, spec->maximum_rate);
            hash_u32(&hash, spec->phase_offset);
        }
        else
        {
            hash_double(&hash, spec->gain);
            hash_double(&hash, spec->bias);
        }
    }
    return hash;
}

static uint64_t contract_signature(const MiniSNNSensorEncoder *encoder)
{
    uint64_t hash = SENSOR_ENCODER_FNV_OFFSET;
    hash_text(&hash, SENSOR_ENCODER_CONTRACT_VERSION);
    hash_u64(&hash, encoder->mapping_signature);
    return hash;
}

static int validate_input_frame_dimensions(const MiniSNNSensorEncoder *encoder,
                                           const MiniSNNNeuralInputFrame *frame,
                                           MiniSNNSensorEncoderError *out_error)
{
    if (encoder == NULL || !minisnn_neural_input_frame_validate(frame, out_error))
        return 0;
    if (frame->neuron_count != encoder->neuron_count ||
        frame->brain_step_count != encoder->brain_steps_per_tick)
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_DIMENSION_MISMATCH);
        return 0;
    }
    return 1;
}

static int validate_sensor_frame(const MiniSNNSensorEncoder *encoder,
                                 const MiniSNNSensorFrame *frame,
                                 MiniSNNSensorEncoderError *out_error)
{
    uint32_t count;

    if (encoder == NULL || frame == NULL || frame->values == NULL)
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    count = minisnn_sensor_schema_channel_count(encoder->sensor_schema);
    if (frame->value_count != count)
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_DIMENSION_MISMATCH);
        return 0;
    }
    for (uint32_t index = 0U; index < count; index++)
    {
        MiniSNNSensorChannelSpec channel;
        double value = frame->values[index];

        if (!minisnn_sensor_schema_get_channel(encoder->sensor_schema, index,
                                                &channel))
        {
            set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_INVALID_SCHEMA);
            return 0;
        }
        if (!isfinite(value))
        {
            set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_NONFINITE_RESULT);
            return 0;
        }
        if (value < channel.minimum || value > channel.maximum)
        {
            set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_INVALID_PARAMETER);
            return 0;
        }
    }
    return 1;
}

static double normalize_linear(double value, double minimum, double maximum)
{
    return minimum == maximum ? 0.0 : (value - minimum) / (maximum - minimum);
}

static int encode_to_scratch(MiniSNNSensorEncoder *encoder,
                             const MiniSNNSensorFrame *frame,
                             MiniSNNSensorEncoderError *out_error)
{
    size_t count;

    if (!frame_value_count(encoder->neuron_count, encoder->brain_steps_per_tick,
                           &count))
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_DIMENSION_MISMATCH);
        return 0;
    }
    memset(encoder->scratch_currents, 0, count * sizeof(*encoder->scratch_currents));
    if (encoder->mapping_count > 0U)
        memcpy(encoder->next_phases, encoder->phases,
               (size_t)encoder->mapping_count * sizeof(*encoder->next_phases));
    for (uint32_t mapping_index = 0U; mapping_index < encoder->mapping_count;
         mapping_index++)
    {
        const MiniSNNSensorEncodingSpec *spec = &encoder->mappings[mapping_index];
        MiniSNNSensorChannelSpec channel;
        double normalized;
        double phase = encoder->phases[mapping_index];

        if (!minisnn_sensor_schema_get_channel(encoder->sensor_schema,
                                                encoder->channel_indices[mapping_index],
                                                &channel))
        {
            set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_INVALID_SCHEMA);
            return 0;
        }
        normalized = normalize_linear(frame->values[encoder->channel_indices[mapping_index]],
                                      channel.minimum, channel.maximum);
        for (uint32_t step = 0U; step < encoder->brain_steps_per_tick; step++)
        {
            double current;
            if (spec->mode == MINISNN_SENSOR_ENCODING_LINEAR_CURRENT)
                current = spec->bias + spec->gain * normalized;
            else if (spec->mode == MINISNN_SENSOR_ENCODING_BIPOLAR_CURRENT)
                current = spec->bias + spec->gain * (2.0 * normalized - 1.0);
            else
            {
                phase += normalized * spec->maximum_rate;
                current = 0.0;
                if (phase >= 1.0)
                {
                    phase -= floor(phase);
                    current = spec->pulse_current;
                }
            }
            if (!isfinite(current) || !isfinite(phase))
            {
                set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_NONFINITE_RESULT);
                return 0;
            }
            for (uint32_t neuron = spec->target_neuron_start;
                 neuron < spec->target_neuron_start + spec->target_neuron_count;
                 neuron++)
            {
                encoder->scratch_currents[(size_t)step * encoder->neuron_count + neuron] =
                    current;
            }
        }
        encoder->next_phases[mapping_index] = phase;
    }
    return 1;
}

int minisnn_neural_input_frame_init(MiniSNNNeuralInputFrame *frame,
                                    uint32_t neuron_count,
                                    uint32_t brain_step_count,
                                    MiniSNNSensorEncoderError *out_error)
{
    size_t count;

    if (frame == NULL || frame->currents != NULL ||
        !frame_value_count(neuron_count, brain_step_count, &count))
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    frame->currents = calloc(count, sizeof(*frame->currents));
    if (frame->currents == NULL)
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_ALLOCATION);
        return 0;
    }
    frame->tick = 0U;
    frame->neuron_count = neuron_count;
    frame->brain_step_count = brain_step_count;
    set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_NONE);
    return 1;
}

void minisnn_neural_input_frame_destroy(MiniSNNNeuralInputFrame *frame)
{
    if (frame == NULL)
        return;
    free(frame->currents);
    memset(frame, 0, sizeof(*frame));
}

int minisnn_neural_input_frame_validate(const MiniSNNNeuralInputFrame *frame,
                                        MiniSNNSensorEncoderError *out_error)
{
    size_t count;

    if (frame == NULL || frame->currents == NULL ||
        !frame_value_count(frame->neuron_count, frame->brain_step_count, &count))
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    for (size_t index = 0U; index < count; index++)
    {
        if (!isfinite(frame->currents[index]))
        {
            set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_NONFINITE_RESULT);
            return 0;
        }
    }
    set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_NONE);
    return 1;
}

int minisnn_neural_input_frame_reset(MiniSNNNeuralInputFrame *frame,
                                     MiniSNNSensorEncoderError *out_error)
{
    size_t count;
    if (!minisnn_neural_input_frame_validate(frame, out_error) ||
        !frame_value_count(frame->neuron_count, frame->brain_step_count, &count))
        return 0;
    memset(frame->currents, 0, count * sizeof(*frame->currents));
    frame->tick = 0U;
    set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_NONE);
    return 1;
}

int minisnn_neural_input_frame_get_current(
    const MiniSNNNeuralInputFrame *frame, uint32_t brain_step,
    uint32_t neuron_id, double *out_current, MiniSNNSensorEncoderError *out_error)
{
    if (out_current == NULL || !minisnn_neural_input_frame_validate(frame, out_error))
    {
        if (out_current == NULL)
            set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (brain_step >= frame->brain_step_count || neuron_id >= frame->neuron_count)
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_DIMENSION_MISMATCH);
        return 0;
    }
    *out_current = frame->currents[(size_t)brain_step * frame->neuron_count + neuron_id];
    set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_NONE);
    return 1;
}

int minisnn_neural_input_frame_copy(MiniSNNNeuralInputFrame *destination,
                                    const MiniSNNNeuralInputFrame *source,
                                    MiniSNNSensorEncoderError *out_error)
{
    size_t count;
    if (!minisnn_neural_input_frame_validate(source, out_error) ||
        !minisnn_neural_input_frame_validate(destination, out_error))
        return 0;
    if (destination->neuron_count != source->neuron_count ||
        destination->brain_step_count != source->brain_step_count)
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_DIMENSION_MISMATCH);
        return 0;
    }
    frame_value_count(source->neuron_count, source->brain_step_count, &count);
    memcpy(destination->currents, source->currents,
           count * sizeof(*destination->currents));
    destination->tick = source->tick;
    set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_NONE);
    return 1;
}

MiniSNNSensorEncoder *minisnn_sensor_encoder_create(
    const MiniSNNSensorSchema *sensor_schema,
    const MiniSNNSensorEncodingSpec *mappings, uint32_t mapping_count,
    uint32_t neuron_count, uint32_t brain_steps_per_tick,
    MiniSNNSensorEncoderError *out_error)
{
    MiniSNNSensorEncoder *encoder;
    MiniSNNSensorSchema *schema_copy;
    size_t scratch_count;

    if (sensor_schema == NULL || neuron_count == 0U || brain_steps_per_tick == 0U ||
        mapping_count > MINISNN_SENSOR_ENCODER_MAX_MAPPINGS ||
        (mapping_count > 0U && mappings == NULL) ||
        !frame_value_count(neuron_count, brain_steps_per_tick, &scratch_count))
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_INVALID_ARGUMENT);
        return NULL;
    }
    schema_copy = clone_sensor_schema(sensor_schema, out_error);
    if (schema_copy == NULL)
        return NULL;
    encoder = calloc(1U, sizeof(*encoder));
    if (encoder == NULL)
    {
        minisnn_sensor_schema_destroy(&schema_copy);
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_ALLOCATION);
        return NULL;
    }
    encoder->sensor_schema = schema_copy;
    encoder->mapping_count = mapping_count;
    encoder->neuron_count = neuron_count;
    encoder->brain_steps_per_tick = brain_steps_per_tick;
    if (mapping_count > 0U)
    {
        encoder->mappings = calloc(mapping_count, sizeof(*encoder->mappings));
        encoder->channel_indices = calloc(mapping_count, sizeof(*encoder->channel_indices));
        encoder->phases = calloc(mapping_count, sizeof(*encoder->phases));
        encoder->next_phases = calloc(mapping_count, sizeof(*encoder->next_phases));
    }
    encoder->scratch_currents = calloc(scratch_count, sizeof(*encoder->scratch_currents));
    if (encoder->scratch_currents == NULL ||
        (mapping_count > 0U && (encoder->mappings == NULL ||
         encoder->channel_indices == NULL || encoder->phases == NULL ||
         encoder->next_phases == NULL)) ||
        !minisnn_sensor_frame_init(&encoder->agent_frame,
                                   minisnn_sensor_schema_channel_count(encoder->sensor_schema)))
    {
        minisnn_sensor_encoder_destroy(&encoder);
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_ALLOCATION);
        return NULL;
    }
    for (uint32_t index = 0U; index < mapping_count; index++)
    {
        MiniSNNSensorEncoderError error = MINISNN_SENSOR_ENCODER_ERROR_NONE;
        if (!validate_spec(encoder->sensor_schema, &mappings[index], neuron_count,
                           &encoder->channel_indices[index], &error))
        {
            minisnn_sensor_encoder_destroy(&encoder);
            set_error(out_error, error);
            return NULL;
        }
        for (uint32_t previous = 0U; previous < index; previous++)
        {
            if (specs_overlap(&mappings[index], &mappings[previous]))
            {
                minisnn_sensor_encoder_destroy(&encoder);
                set_error(out_error,
                          MINISNN_SENSOR_ENCODER_ERROR_OVERLAPPING_NEURON_RANGE);
                return NULL;
            }
        }
        encoder->mappings[index] = mappings[index];
        if (mappings[index].mode == MINISNN_SENSOR_ENCODING_DETERMINISTIC_RATE)
            encoder->phases[index] = (double)mappings[index].phase_offset /
                                     (double)SENSOR_ENCODER_PHASE_SCALE;
    }
    encoder->mapping_signature = mapping_signature(encoder);
    encoder->contract_signature = contract_signature(encoder);
    encoder->last_error = MINISNN_SENSOR_ENCODER_ERROR_NONE;
    set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_NONE);
    return encoder;
}

void minisnn_sensor_encoder_destroy(MiniSNNSensorEncoder **encoder_ptr)
{
    MiniSNNSensorEncoder *encoder;
    if (encoder_ptr == NULL || *encoder_ptr == NULL)
        return;
    encoder = *encoder_ptr;
    minisnn_sensor_schema_destroy(&encoder->sensor_schema);
    minisnn_sensor_frame_destroy(&encoder->agent_frame);
    free(encoder->mappings);
    free(encoder->channel_indices);
    free(encoder->phases);
    free(encoder->next_phases);
    free(encoder->scratch_currents);
    free(encoder);
    *encoder_ptr = NULL;
}

void minisnn_sensor_encoder_reset(MiniSNNSensorEncoder *encoder)
{
    if (encoder == NULL)
        return;
    for (uint32_t index = 0U; index < encoder->mapping_count; index++)
    {
        encoder->phases[index] = encoder->mappings[index].mode ==
                                  MINISNN_SENSOR_ENCODING_DETERMINISTIC_RATE ?
            (double)encoder->mappings[index].phase_offset /
                (double)SENSOR_ENCODER_PHASE_SCALE : 0.0;
    }
    minisnn_sensor_frame_reset(&encoder->agent_frame, encoder->sensor_schema, NULL);
    encoder->last_error = MINISNN_SENSOR_ENCODER_ERROR_NONE;
}

uint64_t minisnn_sensor_encoding_mapping_signature(
    const MiniSNNSensorEncoder *encoder)
{
    return encoder != NULL ? encoder->mapping_signature : 0U;
}

uint64_t minisnn_sensor_encoder_contract_signature(
    const MiniSNNSensorEncoder *encoder)
{
    return encoder != NULL ? encoder->contract_signature : 0U;
}

MiniSNNSensorEncoderError minisnn_sensor_encoder_last_error(
    const MiniSNNSensorEncoder *encoder)
{
    return encoder != NULL ? encoder->last_error :
        MINISNN_SENSOR_ENCODER_ERROR_INVALID_ARGUMENT;
}

const char *minisnn_sensor_encoder_error_string(MiniSNNSensorEncoderError error)
{
    static const char *const messages[] =
    {
        "ok", "argumento invalido", "schema invalido", "canal de sensor desconhecido",
        "range de neuronios invalido", "range de neuronios sobreposto",
        "modo de encoding invalido", "parametro invalido", "dimensao incompativel",
        "frame indisponivel", "frame ja consumido", "resultado nao finito",
        "assinatura incompativel", "formato incompativel", "falha de alocacao",
        "erro de E/S"
    };
    return (unsigned int)error < sizeof(messages) / sizeof(messages[0]) ?
        messages[error] : "erro de encoder desconhecido";
}

int minisnn_sensor_encoder_encode_frame(
    MiniSNNSensorEncoder *encoder, const MiniSNNSensorFrame *sensor_frame,
    MiniSNNNeuralInputFrame *out_frame)
{
    MiniSNNSensorEncoderError error = MINISNN_SENSOR_ENCODER_ERROR_NONE;
    size_t count;

    if (encoder == NULL)
        return 0;
    if (!validate_input_frame_dimensions(encoder, out_frame, &error) ||
        !validate_sensor_frame(encoder, sensor_frame, &error) ||
        !encode_to_scratch(encoder, sensor_frame, &error) ||
        !frame_value_count(encoder->neuron_count, encoder->brain_steps_per_tick,
                           &count))
    {
        encoder_error(encoder, error == MINISNN_SENSOR_ENCODER_ERROR_NONE ?
                      MINISNN_SENSOR_ENCODER_ERROR_NONFINITE_RESULT : error);
        return 0;
    }
    memcpy(out_frame->currents, encoder->scratch_currents,
           count * sizeof(*out_frame->currents));
    if (encoder->mapping_count > 0U)
        memcpy(encoder->phases, encoder->next_phases,
               (size_t)encoder->mapping_count * sizeof(*encoder->phases));
    out_frame->tick = sensor_frame->tick;
    encoder->last_error = MINISNN_SENSOR_ENCODER_ERROR_NONE;
    return 1;
}

int minisnn_sensor_encoder_encode_from_agent_io(
    MiniSNNSensorEncoder *encoder, MiniSNNAgentIOContext *agent_io,
    MiniSNNNeuralInputFrame *out_frame)
{
    MiniSNNSensorEncoderError error = MINISNN_SENSOR_ENCODER_ERROR_NONE;

    if (encoder == NULL)
        return 0;
    if (agent_io == NULL || !validate_input_frame_dimensions(encoder, out_frame, &error))
    {
        encoder_error(encoder, error == MINISNN_SENSOR_ENCODER_ERROR_NONE ?
                      MINISNN_SENSOR_ENCODER_ERROR_INVALID_ARGUMENT : error);
        return 0;
    }
    if (minisnn_agent_io_sensor_schema_signature(agent_io) !=
        minisnn_sensor_schema_signature(encoder->sensor_schema))
    {
        encoder_error(encoder, MINISNN_SENSOR_ENCODER_ERROR_SIGNATURE_MISMATCH);
        return 0;
    }
    if (!minisnn_agent_io_consume_sensor_frame(agent_io, &encoder->agent_frame))
    {
        encoder_error(encoder, minisnn_agent_io_last_error(agent_io) ==
                      MINISNN_AGENT_IO_ERROR_SENSOR_ALREADY_CONSUMED ?
                      MINISNN_SENSOR_ENCODER_ERROR_FRAME_ALREADY_CONSUMED :
                      MINISNN_SENSOR_ENCODER_ERROR_FRAME_UNAVAILABLE);
        return 0;
    }
    return minisnn_sensor_encoder_encode_frame(encoder, &encoder->agent_frame,
                                               out_frame);
}

int minisnn_neural_input_frame_apply_step(
    const MiniSNNNeuralInputFrame *frame, uint32_t brain_step, MiniSNN *network,
    MiniSNNSensorEncoderError *out_error)
{
    if (out_error == NULL || frame == NULL || network == NULL)
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (!minisnn_neural_input_frame_validate(frame, out_error))
        return 0;
    if (brain_step >= frame->brain_step_count ||
        minisnn_neuron_count(network) != (int)frame->neuron_count)
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_DIMENSION_MISMATCH);
        return 0;
    }
    minisnn_clear_inputs(network);
    for (uint32_t neuron = 0U; neuron < frame->neuron_count; neuron++)
    {
        if (!minisnn_set_input(network, (int)neuron,
                                frame->currents[(size_t)brain_step *
                                                frame->neuron_count + neuron]))
        {
            set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_INVALID_ARGUMENT);
            return 0;
        }
    }
    set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_NONE);
    return 1;
}

static int write_mapping(FILE *file, const MiniSNNSensorEncodingSpec *spec)
{
    return fprintf(file, "mapping=%u|%u|%u|%u|%016llx|%016llx|%016llx|%016llx|%u\n",
                   spec->sensor_channel_id, spec->target_neuron_start,
                   spec->target_neuron_count, (unsigned int)spec->mode,
                   (unsigned long long)double_bits(spec->gain),
                   (unsigned long long)double_bits(spec->bias),
                   (unsigned long long)double_bits(spec->pulse_current),
                   (unsigned long long)double_bits(spec->maximum_rate),
                   spec->phase_offset) >= 0;
}

int minisnn_sensor_encoder_write_file(
    const MiniSNNSensorEncoder *encoder, const char *filename,
    MiniSNNSensorEncoderError *out_error)
{
    FILE *file;
    if (encoder == NULL || filename == NULL || filename[0] == '\0')
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    file = fopen(filename, "wb");
    if (file == NULL)
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_IO);
        return 0;
    }
    if (fprintf(file, "%s\nsensor_schema_signature=%016llx\nneuron_count=%u\n"
                     "brain_steps_per_tick=%u\nmapping_count=%u\n",
                SENSOR_ENCODER_TEXT_VERSION,
                (unsigned long long)minisnn_sensor_schema_signature(encoder->sensor_schema),
                encoder->neuron_count, encoder->brain_steps_per_tick,
                encoder->mapping_count) < 0)
        goto io_failure;
    for (uint32_t index = 0U; index < encoder->mapping_count; index++)
    {
        if (!write_mapping(file, &encoder->mappings[index]))
            goto io_failure;
    }
    {
        int write_failed = fprintf(
            file, "mapping_signature=%016llx\ncontract_signature=%016llx\n",
            (unsigned long long)encoder->mapping_signature,
            (unsigned long long)encoder->contract_signature) < 0;

        if (fclose(file) != 0)
            write_failed = 1;
        if (write_failed)
        {
            set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_IO);
            return 0;
        }
    }
    set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_NONE);
    return 1;

io_failure:
    fclose(file);
    set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_IO);
    return 0;
}

static int read_line(FILE *file, char *line, size_t line_size)
{
    size_t length;
    if (fgets(line, (int)line_size, file) == NULL)
        return 0;
    length = strlen(line);
    if (length == 0U || line[length - 1U] != '\n')
        return 0;
    line[length - 1U] = '\0';
    return 1;
}

static int parse_u32(const char *text, uint32_t *out_value)
{
    char *end = NULL;
    unsigned long value;
    if (text == NULL || text[0] == '\0')
        return 0;
    value = strtoul(text, &end, 10);
    if (end == NULL || *end != '\0' || value > UINT32_MAX)
        return 0;
    *out_value = (uint32_t)value;
    return 1;
}

static int parse_hex64(const char *text, uint64_t *out_value)
{
    char *end = NULL;
    unsigned long long value;
    if (text == NULL || strlen(text) != 16U)
        return 0;
    value = strtoull(text, &end, 16);
    if (end == NULL || *end != '\0')
        return 0;
    *out_value = (uint64_t)value;
    return 1;
}

static int parse_mapping(const char *line, MiniSNNSensorEncodingSpec *out_spec)
{
    char copy[SENSOR_ENCODER_TEXT_LINE_MAX];
    char *fields[9] = {0};
    char *cursor;
    uint32_t raw_mode;
    uint64_t bits[4];

    if (line == NULL || out_spec == NULL || strncmp(line, "mapping=", 8U) != 0 ||
        strlen(line + 8U) >= sizeof(copy))
        return 0;
    strcpy(copy, line + 8U);
    cursor = copy;
    for (int index = 0; index < 9; index++)
    {
        fields[index] = cursor;
        cursor = index == 8 ? NULL : strchr(cursor, '|');
        if (cursor == NULL && index != 8)
            return 0;
        if (cursor != NULL)
            *cursor++ = '\0';
    }
    if (strchr(fields[8], '|') != NULL ||
        !parse_u32(fields[0], &out_spec->sensor_channel_id) ||
        !parse_u32(fields[1], &out_spec->target_neuron_start) ||
        !parse_u32(fields[2], &out_spec->target_neuron_count) ||
        !parse_u32(fields[3], &raw_mode) ||
        !parse_hex64(fields[4], &bits[0]) || !parse_hex64(fields[5], &bits[1]) ||
        !parse_hex64(fields[6], &bits[2]) || !parse_hex64(fields[7], &bits[3]) ||
        !parse_u32(fields[8], &out_spec->phase_offset))
        return 0;
    out_spec->mode = (MiniSNNSensorEncodingMode)raw_mode;
    out_spec->gain = bits_double(bits[0]);
    out_spec->bias = bits_double(bits[1]);
    out_spec->pulse_current = bits_double(bits[2]);
    out_spec->maximum_rate = bits_double(bits[3]);
    return 1;
}

MiniSNNSensorEncoder *minisnn_sensor_encoder_read_file(
    const char *filename, const MiniSNNSensorSchema *sensor_schema,
    MiniSNNSensorEncoderError *out_error)
{
    FILE *file;
    char line[SENSOR_ENCODER_TEXT_LINE_MAX];
    uint64_t schema_signature;
    uint64_t saved_mapping_signature;
    uint64_t saved_contract_signature;
    uint32_t neuron_count;
    uint32_t brain_steps;
    uint32_t mapping_count;
    MiniSNNSensorEncodingSpec *mappings = NULL;
    MiniSNNSensorEncoder *encoder = NULL;

    if (filename == NULL || sensor_schema == NULL)
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_INVALID_ARGUMENT);
        return NULL;
    }
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_IO);
        return NULL;
    }
    if (!read_line(file, line, sizeof(line)) || strcmp(line, SENSOR_ENCODER_TEXT_VERSION) != 0 ||
        !read_line(file, line, sizeof(line)) ||
        strncmp(line, "sensor_schema_signature=", 24U) != 0 ||
        !parse_hex64(line + 24U, &schema_signature) ||
        !read_line(file, line, sizeof(line)) || strncmp(line, "neuron_count=", 13U) != 0 ||
        !parse_u32(line + 13U, &neuron_count) ||
        !read_line(file, line, sizeof(line)) || strncmp(line, "brain_steps_per_tick=", 21U) != 0 ||
        !parse_u32(line + 21U, &brain_steps) ||
        !read_line(file, line, sizeof(line)) || strncmp(line, "mapping_count=", 14U) != 0 ||
        !parse_u32(line + 14U, &mapping_count) ||
        mapping_count > MINISNN_SENSOR_ENCODER_MAX_MAPPINGS)
        goto format_failure;
    if (schema_signature != minisnn_sensor_schema_signature(sensor_schema))
    {
        fclose(file);
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_SIGNATURE_MISMATCH);
        return NULL;
    }
    if (mapping_count > 0U)
    {
        mappings = calloc(mapping_count, sizeof(*mappings));
        if (mappings == NULL)
        {
            fclose(file);
            set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_ALLOCATION);
            return NULL;
        }
    }
    for (uint32_t index = 0U; index < mapping_count; index++)
    {
        if (!read_line(file, line, sizeof(line)) || !parse_mapping(line, &mappings[index]))
            goto format_failure;
    }
    if (!read_line(file, line, sizeof(line)) || strncmp(line, "mapping_signature=", 18U) != 0 ||
        !parse_hex64(line + 18U, &saved_mapping_signature) ||
        !read_line(file, line, sizeof(line)) || strncmp(line, "contract_signature=", 19U) != 0 ||
        !parse_hex64(line + 19U, &saved_contract_signature) ||
        read_line(file, line, sizeof(line)))
        goto format_failure;
    fclose(file);
    encoder = minisnn_sensor_encoder_create(sensor_schema, mappings, mapping_count,
                                            neuron_count, brain_steps, out_error);
    free(mappings);
    if (encoder == NULL)
        return NULL;
    if (encoder->mapping_signature != saved_mapping_signature ||
        encoder->contract_signature != saved_contract_signature)
    {
        minisnn_sensor_encoder_destroy(&encoder);
        set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_SIGNATURE_MISMATCH);
        return NULL;
    }
    set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_NONE);
    return encoder;

format_failure:
    fclose(file);
    free(mappings);
    set_error(out_error, MINISNN_SENSOR_ENCODER_ERROR_FORMAT);
    return NULL;
}
