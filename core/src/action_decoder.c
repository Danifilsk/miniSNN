#include "minisnn_action_decoder.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minisnn.h"

#define ACTION_DECODER_FNV_OFFSET UINT64_C(14695981039346656037)
#define ACTION_DECODER_FNV_PRIME UINT64_C(1099511628211)
#define ACTION_DECODER_MAPPING_VERSION "minisnn_action_decoding_mapping_v1"
#define ACTION_DECODER_CONTRACT_VERSION "minisnn_action_decoder_contract_v1"
#define ACTION_DECODER_TEXT_VERSION "minisnn_action_decoder_v1"
#define ACTION_DECODER_TEXT_LINE_MAX 1024U

struct MiniSNNActionDecoder
{
    MiniSNNActionSchema *action_schema;
    MiniSNNActionDecodingSpec *mappings;
    uint32_t *channel_indices;
    double *scratch_values;
    double *scratch_scores;
    double *scratch_confidences;
    uint8_t *scratch_selected;
    uint32_t mapping_count;
    uint32_t neuron_count;
    uint32_t brain_steps_per_tick;
    uint64_t mapping_signature;
    uint64_t contract_signature;
    MiniSNNActionDecoderError last_error;
};

static void set_error(MiniSNNActionDecoderError *out_error,
                      MiniSNNActionDecoderError error)
{
    if (out_error != NULL)
        *out_error = error;
}

static void decoder_error(MiniSNNActionDecoder *decoder,
                          MiniSNNActionDecoderError error)
{
    if (decoder != NULL)
        decoder->last_error = error;
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
    *hash *= ACTION_DECODER_FNV_PRIME;
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

static int valid_mode(MiniSNNActionDecodingMode mode)
{
    return mode == MINISNN_ACTION_DECODING_POPULATION_RATE ||
           mode == MINISNN_ACTION_DECODING_BIPOLAR_DIFFERENCE ||
           mode == MINISNN_ACTION_DECODING_THRESHOLD ||
           mode == MINISNN_ACTION_DECODING_WTA_MEMBER;
}

static int range_valid(uint32_t start, uint32_t count, uint32_t neuron_count)
{
    return count > 0U && start < neuron_count && count <= neuron_count - start;
}

static int ranges_overlap(uint32_t left_start, uint32_t left_count,
                          uint32_t right_start, uint32_t right_count)
{
    uint32_t left_end = left_start + left_count;
    uint32_t right_end = right_start + right_count;
    return left_start < right_end && right_start < left_end;
}

static int find_channel_index(const MiniSNNActionSchema *schema, uint32_t id,
                              uint32_t *out_index)
{
    uint32_t count = minisnn_action_schema_channel_count(schema);

    if (out_index == NULL)
        return 0;
    for (uint32_t index = 0U; index < count; index++)
    {
        MiniSNNActionChannelSpec channel;
        if (!minisnn_action_schema_get_channel(schema, index, &channel))
            return 0;
        if (channel.id == id)
        {
            *out_index = index;
            return 1;
        }
    }
    return 0;
}

static int get_channel(const MiniSNNActionDecoder *decoder, uint32_t index,
                       MiniSNNActionChannelSpec *out_channel)
{
    return decoder != NULL && minisnn_action_schema_get_channel(
        decoder->action_schema, index, out_channel);
}

static MiniSNNActionSchema *clone_action_schema(
    const MiniSNNActionSchema *schema, MiniSNNActionDecoderError *out_error)
{
    uint32_t count;
    MiniSNNActionChannelSpec *channels;
    MiniSNNActionSchema *copy;
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;

    if (schema == NULL || (count = minisnn_action_schema_channel_count(schema)) == 0U)
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_ACTION_SCHEMA);
        return NULL;
    }
    channels = calloc(count, sizeof(*channels));
    if (channels == NULL)
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_ALLOCATION);
        return NULL;
    }
    for (uint32_t index = 0U; index < count; index++)
    {
        if (!minisnn_action_schema_get_channel(schema, index, &channels[index]))
        {
            free(channels);
            set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_ACTION_SCHEMA);
            return NULL;
        }
    }
    copy = minisnn_action_schema_create(channels, count, &io_error);
    free(channels);
    if (copy == NULL)
    {
        set_error(out_error, io_error == MINISNN_AGENT_IO_ERROR_ALLOCATION ?
                  MINISNN_ACTION_DECODER_ERROR_ALLOCATION :
                  MINISNN_ACTION_DECODER_ERROR_INVALID_ACTION_SCHEMA);
    }
    return copy;
}

static int value_in_channel(const MiniSNNActionChannelSpec *channel, double value)
{
    return isfinite(value) && value >= channel->minimum && value <= channel->maximum;
}

static int validate_spec(const MiniSNNActionSchema *schema,
                         const MiniSNNActionDecodingSpec *spec,
                         uint32_t neuron_count, uint32_t *out_channel_index,
                         MiniSNNActionDecoderError *out_error)
{
    MiniSNNActionChannelSpec channel;

    if (spec == NULL || out_channel_index == NULL)
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (!valid_mode(spec->mode))
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_DECODING_MODE);
        return 0;
    }
    if (!find_channel_index(schema, spec->action_channel_id, out_channel_index) ||
        !minisnn_action_schema_get_channel(schema, *out_channel_index, &channel))
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_UNKNOWN_ACTION_CHANNEL);
        return 0;
    }
    if (!range_valid(spec->primary_neuron_start, spec->primary_neuron_count,
                     neuron_count))
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_NEURON_RANGE);
        return 0;
    }
    if (spec->mode == MINISNN_ACTION_DECODING_BIPOLAR_DIFFERENCE)
    {
        if (!range_valid(spec->secondary_neuron_start, spec->secondary_neuron_count,
                         neuron_count))
        {
            set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_NEURON_RANGE);
            return 0;
        }
        if (ranges_overlap(spec->primary_neuron_start, spec->primary_neuron_count,
                           spec->secondary_neuron_start,
                           spec->secondary_neuron_count))
        {
            set_error(out_error, MINISNN_ACTION_DECODER_ERROR_OVERLAPPING_NEURON_RANGE);
            return 0;
        }
    }
    if (spec->mode == MINISNN_ACTION_DECODING_POPULATION_RATE)
    {
        if (!isfinite(spec->minimum_rate) || !isfinite(spec->maximum_rate) ||
            spec->minimum_rate < 0.0 || spec->minimum_rate >= spec->maximum_rate ||
            spec->maximum_rate > 1.0)
        {
            set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_PARAMETER);
            return 0;
        }
    }
    if (spec->mode == MINISNN_ACTION_DECODING_THRESHOLD)
    {
        if (!isfinite(spec->threshold) || spec->threshold < 0.0 ||
            spec->threshold > 1.0 || !value_in_channel(&channel, spec->active_value) ||
            !value_in_channel(&channel, spec->inactive_value))
        {
            set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_PARAMETER);
            return 0;
        }
    }
    if (spec->mode == MINISNN_ACTION_DECODING_WTA_MEMBER)
    {
        if (!isfinite(spec->minimum_activation) || !isfinite(spec->minimum_confidence) ||
            spec->minimum_activation < 0.0 || spec->minimum_activation > 1.0 ||
            spec->minimum_confidence < 0.0 || spec->minimum_confidence > 1.0 ||
            !value_in_channel(&channel, spec->winner_value) ||
            !value_in_channel(&channel, spec->loser_value))
        {
            set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_PARAMETER);
            return 0;
        }
    }
    return 1;
}

static int spec_ranges_overlap(const MiniSNNActionDecodingSpec *left,
                               const MiniSNNActionDecodingSpec *right)
{
    if (ranges_overlap(left->primary_neuron_start, left->primary_neuron_count,
                       right->primary_neuron_start, right->primary_neuron_count))
        return 1;
    if (left->mode == MINISNN_ACTION_DECODING_BIPOLAR_DIFFERENCE &&
        ranges_overlap(left->secondary_neuron_start, left->secondary_neuron_count,
                       right->primary_neuron_start, right->primary_neuron_count))
        return 1;
    if (right->mode == MINISNN_ACTION_DECODING_BIPOLAR_DIFFERENCE &&
        ranges_overlap(left->primary_neuron_start, left->primary_neuron_count,
                       right->secondary_neuron_start, right->secondary_neuron_count))
        return 1;
    return left->mode == MINISNN_ACTION_DECODING_BIPOLAR_DIFFERENCE &&
           right->mode == MINISNN_ACTION_DECODING_BIPOLAR_DIFFERENCE &&
           ranges_overlap(left->secondary_neuron_start, left->secondary_neuron_count,
                          right->secondary_neuron_start,
                          right->secondary_neuron_count);
}

static uint64_t mapping_signature(const MiniSNNActionDecoder *decoder)
{
    uint64_t hash = ACTION_DECODER_FNV_OFFSET;

    hash_text(&hash, ACTION_DECODER_MAPPING_VERSION);
    hash_u64(&hash, minisnn_action_schema_signature(decoder->action_schema));
    hash_u32(&hash, decoder->neuron_count);
    hash_u32(&hash, decoder->brain_steps_per_tick);
    hash_u32(&hash, decoder->mapping_count);
    for (uint32_t index = 0U; index < decoder->mapping_count; index++)
    {
        const MiniSNNActionDecodingSpec *spec = &decoder->mappings[index];

        hash_u32(&hash, spec->action_channel_id);
        hash_u32(&hash, (uint32_t)spec->mode);
        hash_u32(&hash, spec->primary_neuron_start);
        hash_u32(&hash, spec->primary_neuron_count);
        if (spec->mode == MINISNN_ACTION_DECODING_POPULATION_RATE)
        {
            hash_double(&hash, spec->minimum_rate);
            hash_double(&hash, spec->maximum_rate);
        }
        else if (spec->mode == MINISNN_ACTION_DECODING_BIPOLAR_DIFFERENCE)
        {
            hash_u32(&hash, spec->secondary_neuron_start);
            hash_u32(&hash, spec->secondary_neuron_count);
        }
        else if (spec->mode == MINISNN_ACTION_DECODING_THRESHOLD)
        {
            hash_double(&hash, spec->threshold);
            hash_double(&hash, spec->active_value);
            hash_double(&hash, spec->inactive_value);
        }
        else
        {
            hash_u32(&hash, spec->competition_group_id);
            hash_double(&hash, spec->minimum_activation);
            hash_double(&hash, spec->minimum_confidence);
            hash_double(&hash, spec->winner_value);
            hash_double(&hash, spec->loser_value);
        }
    }
    return hash;
}

static uint64_t contract_signature(const MiniSNNActionDecoder *decoder)
{
    uint64_t hash = ACTION_DECODER_FNV_OFFSET;
    hash_text(&hash, ACTION_DECODER_CONTRACT_VERSION);
    hash_u64(&hash, decoder->mapping_signature);
    return hash;
}

int minisnn_neural_activity_frame_validate(
    const MiniSNNNeuralActivityFrame *frame, MiniSNNActionDecoderError *out_error)
{
    size_t count;

    if (frame == NULL || frame->spikes == NULL || frame->captured_steps == NULL ||
        !frame_value_count(frame->neuron_count, frame->brain_step_count, &count))
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    for (uint32_t step = 0U; step < frame->brain_step_count; step++)
    {
        if (frame->captured_steps[step] > 1U)
        {
            set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_SPIKE_VALUE);
            return 0;
        }
    }
    for (size_t index = 0U; index < count; index++)
    {
        if (frame->spikes[index] > 1U)
        {
            set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_SPIKE_VALUE);
            return 0;
        }
    }
    set_error(out_error, MINISNN_ACTION_DECODER_ERROR_NONE);
    return 1;
}

int minisnn_neural_activity_frame_init(
    MiniSNNNeuralActivityFrame *frame, uint32_t neuron_count,
    uint32_t brain_step_count, MiniSNNActionDecoderError *out_error)
{
    size_t count;
    uint8_t *spikes;
    uint8_t *captured;

    if (frame == NULL || !frame_value_count(neuron_count, brain_step_count, &count))
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    spikes = calloc(count, sizeof(*spikes));
    captured = calloc(brain_step_count, sizeof(*captured));
    if (spikes == NULL || captured == NULL)
    {
        free(spikes);
        free(captured);
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_ALLOCATION);
        return 0;
    }
    minisnn_neural_activity_frame_destroy(frame);
    frame->tick = 0U;
    frame->neuron_count = neuron_count;
    frame->brain_step_count = brain_step_count;
    frame->spikes = spikes;
    frame->captured_steps = captured;
    set_error(out_error, MINISNN_ACTION_DECODER_ERROR_NONE);
    return 1;
}

void minisnn_neural_activity_frame_destroy(MiniSNNNeuralActivityFrame *frame)
{
    if (frame == NULL)
        return;
    free(frame->spikes);
    free(frame->captured_steps);
    memset(frame, 0, sizeof(*frame));
}

int minisnn_neural_activity_frame_reset(
    MiniSNNNeuralActivityFrame *frame, uint64_t tick,
    MiniSNNActionDecoderError *out_error)
{
    size_t count;
    if (!minisnn_neural_activity_frame_validate(frame, out_error) ||
        !frame_value_count(frame->neuron_count, frame->brain_step_count, &count))
        return 0;
    memset(frame->spikes, 0, count * sizeof(*frame->spikes));
    memset(frame->captured_steps, 0,
           (size_t)frame->brain_step_count * sizeof(*frame->captured_steps));
    frame->tick = tick;
    set_error(out_error, MINISNN_ACTION_DECODER_ERROR_NONE);
    return 1;
}

int minisnn_neural_activity_frame_copy(
    MiniSNNNeuralActivityFrame *destination,
    const MiniSNNNeuralActivityFrame *source,
    MiniSNNActionDecoderError *out_error)
{
    size_t count;
    if (destination == NULL || !minisnn_neural_activity_frame_validate(source, out_error))
        return 0;
    if (destination->neuron_count != source->neuron_count ||
        destination->brain_step_count != source->brain_step_count ||
        destination->spikes == NULL || destination->captured_steps == NULL ||
        !frame_value_count(source->neuron_count, source->brain_step_count, &count))
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_DIMENSION_MISMATCH);
        return 0;
    }
    memcpy(destination->spikes, source->spikes, count * sizeof(*source->spikes));
    memcpy(destination->captured_steps, source->captured_steps,
           (size_t)source->brain_step_count * sizeof(*source->captured_steps));
    destination->tick = source->tick;
    set_error(out_error, MINISNN_ACTION_DECODER_ERROR_NONE);
    return 1;
}

int minisnn_neural_activity_frame_set_step(
    MiniSNNNeuralActivityFrame *frame, uint32_t brain_step,
    const uint8_t *spikes, uint32_t spike_count,
    MiniSNNActionDecoderError *out_error)
{
    if (!minisnn_neural_activity_frame_validate(frame, out_error) || spikes == NULL)
    {
        if (spikes == NULL)
            set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (brain_step >= frame->brain_step_count || spike_count != frame->neuron_count)
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_DIMENSION_MISMATCH);
        return 0;
    }
    if (frame->captured_steps[brain_step] != 0U)
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_DUPLICATE_CAPTURED_STEP);
        return 0;
    }
    for (uint32_t neuron = 0U; neuron < spike_count; neuron++)
    {
        if (spikes[neuron] > 1U)
        {
            set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_SPIKE_VALUE);
            return 0;
        }
    }
    memcpy(&frame->spikes[(size_t)brain_step * frame->neuron_count], spikes,
           (size_t)spike_count * sizeof(*spikes));
    frame->captured_steps[brain_step] = 1U;
    set_error(out_error, MINISNN_ACTION_DECODER_ERROR_NONE);
    return 1;
}

int minisnn_neural_activity_frame_capture_step(
    MiniSNNNeuralActivityFrame *frame, uint32_t brain_step,
    const MiniSNN *network, MiniSNNActionDecoderError *out_error)
{
    uint8_t *scratch;

    if (!minisnn_neural_activity_frame_validate(frame, out_error) || network == NULL)
    {
        if (network == NULL)
            set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (brain_step >= frame->brain_step_count ||
        minisnn_neuron_count(network) != (int)frame->neuron_count)
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_DIMENSION_MISMATCH);
        return 0;
    }
    if (frame->captured_steps[brain_step] != 0U)
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_DUPLICATE_CAPTURED_STEP);
        return 0;
    }
    scratch = calloc(frame->neuron_count, sizeof(*scratch));
    if (scratch == NULL)
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_ALLOCATION);
        return 0;
    }
    for (uint32_t neuron = 0U; neuron < frame->neuron_count; neuron++)
    {
        int spike = 0;
        if (!minisnn_get_spike(network, (int)neuron, &spike) ||
            (spike != 0 && spike != 1))
        {
            free(scratch);
            set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_SPIKE_VALUE);
            return 0;
        }
        scratch[neuron] = (uint8_t)spike;
    }
    memcpy(&frame->spikes[(size_t)brain_step * frame->neuron_count], scratch,
           (size_t)frame->neuron_count * sizeof(*scratch));
    free(scratch);
    frame->captured_steps[brain_step] = 1U;
    set_error(out_error, MINISNN_ACTION_DECODER_ERROR_NONE);
    return 1;
}

int minisnn_neural_activity_frame_get_spike(
    const MiniSNNNeuralActivityFrame *frame, uint32_t brain_step,
    uint32_t neuron_id, uint8_t *out_spike, MiniSNNActionDecoderError *out_error)
{
    if (!minisnn_neural_activity_frame_validate(frame, out_error) || out_spike == NULL)
    {
        if (out_spike == NULL)
            set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (brain_step >= frame->brain_step_count || neuron_id >= frame->neuron_count)
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_DIMENSION_MISMATCH);
        return 0;
    }
    *out_spike = frame->spikes[(size_t)brain_step * frame->neuron_count + neuron_id];
    set_error(out_error, MINISNN_ACTION_DECODER_ERROR_NONE);
    return 1;
}

int minisnn_neural_activity_frame_is_complete(
    const MiniSNNNeuralActivityFrame *frame, MiniSNNActionDecoderError *out_error)
{
    if (!minisnn_neural_activity_frame_validate(frame, out_error))
        return 0;
    for (uint32_t step = 0U; step < frame->brain_step_count; step++)
    {
        if (frame->captured_steps[step] == 0U)
        {
            set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INCOMPLETE_ACTIVITY_FRAME);
            return 0;
        }
    }
    set_error(out_error, MINISNN_ACTION_DECODER_ERROR_NONE);
    return 1;
}

int minisnn_action_decoding_diagnostics_init(
    MiniSNNActionDecodingDiagnostics *diagnostics, uint32_t action_count)
{
    double *scores;
    double *confidences;
    double *values;
    uint8_t *selected;
    if (diagnostics == NULL || action_count == 0U)
        return 0;
    scores = calloc(action_count, sizeof(*scores));
    confidences = calloc(action_count, sizeof(*confidences));
    values = calloc(action_count, sizeof(*values));
    selected = calloc(action_count, sizeof(*selected));
    if (scores == NULL || confidences == NULL || values == NULL || selected == NULL)
    {
        free(scores); free(confidences); free(values); free(selected);
        return 0;
    }
    minisnn_action_decoding_diagnostics_destroy(diagnostics);
    diagnostics->action_count = action_count;
    diagnostics->raw_scores = scores;
    diagnostics->confidences = confidences;
    diagnostics->values = values;
    diagnostics->selected = selected;
    diagnostics->tick = 0U;
    return 1;
}

void minisnn_action_decoding_diagnostics_destroy(
    MiniSNNActionDecodingDiagnostics *diagnostics)
{
    if (diagnostics == NULL)
        return;
    free(diagnostics->raw_scores);
    free(diagnostics->confidences);
    free(diagnostics->values);
    free(diagnostics->selected);
    memset(diagnostics, 0, sizeof(*diagnostics));
}

int minisnn_action_decoding_diagnostics_reset(
    MiniSNNActionDecodingDiagnostics *diagnostics)
{
    if (diagnostics == NULL || diagnostics->action_count == 0U ||
        diagnostics->raw_scores == NULL || diagnostics->confidences == NULL ||
        diagnostics->values == NULL || diagnostics->selected == NULL)
        return 0;
    memset(diagnostics->raw_scores, 0,
           (size_t)diagnostics->action_count * sizeof(*diagnostics->raw_scores));
    memset(diagnostics->confidences, 0,
           (size_t)diagnostics->action_count * sizeof(*diagnostics->confidences));
    memset(diagnostics->values, 0,
           (size_t)diagnostics->action_count * sizeof(*diagnostics->values));
    memset(diagnostics->selected, 0,
           (size_t)diagnostics->action_count * sizeof(*diagnostics->selected));
    diagnostics->tick = 0U;
    return 1;
}

MiniSNNActionDecoder *minisnn_action_decoder_create(
    const MiniSNNActionSchema *action_schema,
    const MiniSNNActionDecodingSpec *mappings, uint32_t mapping_count,
    uint32_t neuron_count, uint32_t brain_steps_per_tick,
    MiniSNNActionDecoderError *out_error)
{
    MiniSNNActionDecoder *decoder;
    MiniSNNActionSchema *schema_copy;
    uint32_t action_count;

    if (action_schema == NULL || neuron_count == 0U || brain_steps_per_tick == 0U ||
        mapping_count > MINISNN_ACTION_DECODER_MAX_MAPPINGS ||
        (mapping_count > 0U && mappings == NULL))
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_ARGUMENT);
        return NULL;
    }
    schema_copy = clone_action_schema(action_schema, out_error);
    if (schema_copy == NULL)
        return NULL;
    action_count = minisnn_action_schema_channel_count(schema_copy);
    decoder = calloc(1U, sizeof(*decoder));
    if (decoder == NULL)
    {
        minisnn_action_schema_destroy(&schema_copy);
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_ALLOCATION);
        return NULL;
    }
    decoder->action_schema = schema_copy;
    decoder->mapping_count = mapping_count;
    decoder->neuron_count = neuron_count;
    decoder->brain_steps_per_tick = brain_steps_per_tick;
    decoder->scratch_values = calloc(action_count, sizeof(*decoder->scratch_values));
    decoder->scratch_scores = calloc(action_count, sizeof(*decoder->scratch_scores));
    decoder->scratch_confidences = calloc(action_count,
                                          sizeof(*decoder->scratch_confidences));
    decoder->scratch_selected = calloc(action_count, sizeof(*decoder->scratch_selected));
    if (mapping_count > 0U)
    {
        decoder->mappings = calloc(mapping_count, sizeof(*decoder->mappings));
        decoder->channel_indices = calloc(mapping_count,
                                          sizeof(*decoder->channel_indices));
    }
    if (decoder->scratch_values == NULL || decoder->scratch_scores == NULL ||
        decoder->scratch_confidences == NULL || decoder->scratch_selected == NULL ||
        (mapping_count > 0U && (decoder->mappings == NULL ||
                                decoder->channel_indices == NULL)))
    {
        minisnn_action_decoder_destroy(&decoder);
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_ALLOCATION);
        return NULL;
    }
    for (uint32_t index = 0U; index < mapping_count; index++)
    {
        MiniSNNActionDecoderError error = MINISNN_ACTION_DECODER_ERROR_NONE;
        if (!validate_spec(schema_copy, &mappings[index], neuron_count,
                           &decoder->channel_indices[index], &error))
        {
            minisnn_action_decoder_destroy(&decoder);
            set_error(out_error, error);
            return NULL;
        }
        for (uint32_t previous = 0U; previous < index; previous++)
        {
            if (mappings[index].action_channel_id == mappings[previous].action_channel_id)
            {
                minisnn_action_decoder_destroy(&decoder);
                set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_PARAMETER);
                return NULL;
            }
            if (spec_ranges_overlap(&mappings[index], &mappings[previous]))
            {
                minisnn_action_decoder_destroy(&decoder);
                set_error(out_error,
                          MINISNN_ACTION_DECODER_ERROR_OVERLAPPING_NEURON_RANGE);
                return NULL;
            }
        }
        decoder->mappings[index] = mappings[index];
    }
    for (uint32_t index = 0U; index < mapping_count; index++)
    {
        if (decoder->mappings[index].mode == MINISNN_ACTION_DECODING_WTA_MEMBER)
        {
            uint32_t members = 0U;
            for (uint32_t other = 0U; other < mapping_count; other++)
            {
                if (decoder->mappings[other].mode == MINISNN_ACTION_DECODING_WTA_MEMBER &&
                    decoder->mappings[other].competition_group_id ==
                        decoder->mappings[index].competition_group_id)
                    members++;
            }
            if (members < 2U)
            {
                minisnn_action_decoder_destroy(&decoder);
                set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_PARAMETER);
                return NULL;
            }
        }
    }
    decoder->mapping_signature = mapping_signature(decoder);
    decoder->contract_signature = contract_signature(decoder);
    decoder->last_error = MINISNN_ACTION_DECODER_ERROR_NONE;
    set_error(out_error, MINISNN_ACTION_DECODER_ERROR_NONE);
    return decoder;
}

void minisnn_action_decoder_destroy(MiniSNNActionDecoder **decoder_ptr)
{
    MiniSNNActionDecoder *decoder;
    if (decoder_ptr == NULL || *decoder_ptr == NULL)
        return;
    decoder = *decoder_ptr;
    minisnn_action_schema_destroy(&decoder->action_schema);
    free(decoder->mappings);
    free(decoder->channel_indices);
    free(decoder->scratch_values);
    free(decoder->scratch_scores);
    free(decoder->scratch_confidences);
    free(decoder->scratch_selected);
    free(decoder);
    *decoder_ptr = NULL;
}

void minisnn_action_decoder_reset(MiniSNNActionDecoder *decoder)
{
    if (decoder == NULL)
        return;
    decoder->last_error = MINISNN_ACTION_DECODER_ERROR_NONE;
}

uint64_t minisnn_action_decoding_mapping_signature(
    const MiniSNNActionDecoder *decoder)
{
    return decoder != NULL ? decoder->mapping_signature : 0U;
}

uint64_t minisnn_action_decoder_contract_signature(
    const MiniSNNActionDecoder *decoder)
{
    return decoder != NULL ? decoder->contract_signature : 0U;
}

MiniSNNActionDecoderError minisnn_action_decoder_last_error(
    const MiniSNNActionDecoder *decoder)
{
    return decoder != NULL ? decoder->last_error :
        MINISNN_ACTION_DECODER_ERROR_INVALID_ARGUMENT;
}

const char *minisnn_action_decoder_error_string(MiniSNNActionDecoderError error)
{
    static const char *const messages[] =
    {
        "ok", "argumento invalido", "schema de acao invalido",
        "canal de acao desconhecido", "range de neuronios invalido",
        "range de neuronios sobreposto", "modo de decoding invalido",
        "parametro invalido", "dimensao incompativel",
        "frame de atividade incompleto", "passo ja capturado",
        "valor de spike invalido", "resultado nao finito",
        "frame de acao rejeitado", "assinatura incompativel",
        "formato incompativel", "falha de alocacao", "erro de E/S"
    };
    return (unsigned int)error < sizeof(messages) / sizeof(messages[0]) ?
        messages[error] : "erro de decoder desconhecido";
}

static double population_rate(const MiniSNNNeuralActivityFrame *activity,
                              uint32_t start, uint32_t count)
{
    uint64_t spikes = 0U;
    for (uint32_t step = 0U; step < activity->brain_step_count; step++)
    {
        for (uint32_t neuron = start; neuron < start + count; neuron++)
            spikes += activity->spikes[(size_t)step * activity->neuron_count + neuron];
    }
    return (double)spikes / ((double)count * (double)activity->brain_step_count);
}

static int validate_output(const MiniSNNActionDecoder *decoder,
                           const MiniSNNActionFrame *frame,
                           const MiniSNNActionDecodingDiagnostics *diagnostics,
                           MiniSNNActionDecoderError *out_error)
{
    uint32_t action_count = minisnn_action_schema_channel_count(decoder->action_schema);
    if (frame == NULL || frame->values == NULL || frame->value_count != action_count)
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_DIMENSION_MISMATCH);
        return 0;
    }
    if (diagnostics != NULL && (diagnostics->action_count != action_count ||
        diagnostics->raw_scores == NULL || diagnostics->confidences == NULL ||
        diagnostics->values == NULL || diagnostics->selected == NULL))
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_DIMENSION_MISMATCH);
        return 0;
    }
    return 1;
}

static int prepare_defaults(MiniSNNActionDecoder *decoder,
                            MiniSNNActionDecoderError *out_error)
{
    uint32_t action_count = minisnn_action_schema_channel_count(decoder->action_schema);
    for (uint32_t index = 0U; index < action_count; index++)
    {
        MiniSNNActionChannelSpec channel;
        if (!get_channel(decoder, index, &channel))
        {
            set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_ACTION_SCHEMA);
            return 0;
        }
        decoder->scratch_values[index] = channel.default_value;
        decoder->scratch_scores[index] = 0.0;
        decoder->scratch_confidences[index] = 0.0;
        decoder->scratch_selected[index] = 0U;
    }
    return 1;
}

int minisnn_action_decoder_decode(
    MiniSNNActionDecoder *decoder, const MiniSNNNeuralActivityFrame *activity,
    MiniSNNActionFrame *out_action,
    MiniSNNActionDecodingDiagnostics *out_diagnostics)
{
    MiniSNNActionDecoderError error = MINISNN_ACTION_DECODER_ERROR_NONE;
    uint32_t action_count;
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;

    if (decoder == NULL)
        return 0;
    if (!minisnn_neural_activity_frame_is_complete(activity, &error) ||
        activity->neuron_count != decoder->neuron_count ||
        activity->brain_step_count != decoder->brain_steps_per_tick ||
        !validate_output(decoder, out_action, out_diagnostics, &error) ||
        !prepare_defaults(decoder, &error))
    {
        if (error == MINISNN_ACTION_DECODER_ERROR_NONE)
            error = MINISNN_ACTION_DECODER_ERROR_DIMENSION_MISMATCH;
        decoder_error(decoder, error);
        return 0;
    }
    action_count = minisnn_action_schema_channel_count(decoder->action_schema);
    for (uint32_t index = 0U; index < decoder->mapping_count; index++)
    {
        const MiniSNNActionDecodingSpec *spec = &decoder->mappings[index];
        uint32_t channel_index = decoder->channel_indices[index];
        MiniSNNActionChannelSpec channel;
        double primary_rate = population_rate(activity, spec->primary_neuron_start,
                                              spec->primary_neuron_count);

        if (!get_channel(decoder, channel_index, &channel))
        {
            decoder_error(decoder, MINISNN_ACTION_DECODER_ERROR_INVALID_ACTION_SCHEMA);
            return 0;
        }
        if (spec->mode == MINISNN_ACTION_DECODING_POPULATION_RATE)
        {
            double normalized = (primary_rate - spec->minimum_rate) /
                                (spec->maximum_rate - spec->minimum_rate);
            if (normalized < 0.0)
                normalized = 0.0;
            else if (normalized > 1.0)
                normalized = 1.0;
            decoder->scratch_values[channel_index] = channel.minimum +
                normalized * (channel.maximum - channel.minimum);
            decoder->scratch_scores[channel_index] = primary_rate;
        }
        else if (spec->mode == MINISNN_ACTION_DECODING_BIPOLAR_DIFFERENCE)
        {
            double secondary_rate = population_rate(activity,
                spec->secondary_neuron_start, spec->secondary_neuron_count);
            double difference = primary_rate - secondary_rate;
            decoder->scratch_scores[channel_index] = difference;
            decoder->scratch_confidences[channel_index] = fabs(difference);
            if (primary_rate == 0.0 && secondary_rate == 0.0)
                decoder->scratch_values[channel_index] = channel.default_value;
            else
                decoder->scratch_values[channel_index] = channel.minimum +
                    ((difference + 1.0) * 0.5) * (channel.maximum - channel.minimum);
        }
        else if (spec->mode == MINISNN_ACTION_DECODING_THRESHOLD)
        {
            int selected = primary_rate >= spec->threshold;
            decoder->scratch_scores[channel_index] = primary_rate;
            decoder->scratch_confidences[channel_index] = fabs(primary_rate -
                                                                 spec->threshold);
            decoder->scratch_selected[channel_index] = (uint8_t)selected;
            decoder->scratch_values[channel_index] = selected ? spec->active_value :
                                                                spec->inactive_value;
        }
        else
        {
            decoder->scratch_scores[channel_index] = primary_rate;
        }
    }
    for (uint32_t index = 0U; index < decoder->mapping_count; index++)
    {
        const MiniSNNActionDecodingSpec *member = &decoder->mappings[index];
        uint32_t winner = index;
        uint32_t members = 0U;
        double best_rate;
        double runner_rate = 0.0;
        int has_runner = 0;
        int valid_winner;
        int seen = 0;

        if (member->mode != MINISNN_ACTION_DECODING_WTA_MEMBER)
            continue;
        for (uint32_t previous = 0U; previous < index; previous++)
        {
            if (decoder->mappings[previous].mode == MINISNN_ACTION_DECODING_WTA_MEMBER &&
                decoder->mappings[previous].competition_group_id ==
                    member->competition_group_id)
            {
                seen = 1;
                break;
            }
        }
        if (seen)
            continue;
        best_rate = decoder->scratch_scores[decoder->channel_indices[index]];
        for (uint32_t other = 0U; other < decoder->mapping_count; other++)
        {
            const MiniSNNActionDecodingSpec *candidate = &decoder->mappings[other];
            double rate;
            if (candidate->mode != MINISNN_ACTION_DECODING_WTA_MEMBER ||
                candidate->competition_group_id != member->competition_group_id)
                continue;
            members++;
            rate = decoder->scratch_scores[decoder->channel_indices[other]];
            if (other != winner && (rate > best_rate ||
                (rate == best_rate && candidate->action_channel_id <
                 decoder->mappings[winner].action_channel_id)))
            {
                runner_rate = best_rate;
                has_runner = 1;
                winner = other;
                best_rate = rate;
            }
            else if (other != winner && (!has_runner || rate > runner_rate))
            {
                runner_rate = rate;
                has_runner = 1;
            }
        }
        if (members < 2U)
        {
            decoder_error(decoder, MINISNN_ACTION_DECODER_ERROR_INVALID_PARAMETER);
            return 0;
        }
        valid_winner = best_rate >= decoder->mappings[winner].minimum_activation &&
                       (best_rate - (has_runner ? runner_rate : 0.0)) >=
                           decoder->mappings[winner].minimum_confidence;
        for (uint32_t other = 0U; other < decoder->mapping_count; other++)
        {
            const MiniSNNActionDecodingSpec *candidate = &decoder->mappings[other];
            uint32_t channel_index;
            MiniSNNActionChannelSpec channel;
            double confidence;
            if (candidate->mode != MINISNN_ACTION_DECODING_WTA_MEMBER ||
                candidate->competition_group_id != member->competition_group_id)
                continue;
            channel_index = decoder->channel_indices[other];
            confidence = best_rate - (has_runner ? runner_rate : 0.0);
            decoder->scratch_confidences[channel_index] = confidence;
            decoder->scratch_selected[channel_index] = (uint8_t)(valid_winner &&
                                                                    other == winner);
            if (!valid_winner)
            {
                if (!get_channel(decoder, channel_index, &channel))
                {
                    decoder_error(decoder,
                                  MINISNN_ACTION_DECODER_ERROR_INVALID_ACTION_SCHEMA);
                    return 0;
                }
                decoder->scratch_values[channel_index] = channel.default_value;
            }
            else
                decoder->scratch_values[channel_index] = other == winner ?
                    candidate->winner_value : candidate->loser_value;
        }
    }
    for (uint32_t index = 0U; index < action_count; index++)
    {
        MiniSNNActionChannelSpec channel;
        if (!get_channel(decoder, index, &channel) ||
            !value_in_channel(&channel, decoder->scratch_values[index]) ||
            !isfinite(decoder->scratch_scores[index]) ||
            !isfinite(decoder->scratch_confidences[index]))
        {
            decoder_error(decoder, MINISNN_ACTION_DECODER_ERROR_NONFINITE_RESULT);
            return 0;
        }
    }
    if (!minisnn_action_frame_set_values(out_action, activity->tick,
                                         decoder->scratch_values, action_count,
                                         &io_error))
    {
        decoder_error(decoder, MINISNN_ACTION_DECODER_ERROR_ACTION_FRAME_REJECTED);
        return 0;
    }
    if (out_diagnostics != NULL)
    {
        memcpy(out_diagnostics->raw_scores, decoder->scratch_scores,
               (size_t)action_count * sizeof(*decoder->scratch_scores));
        memcpy(out_diagnostics->confidences, decoder->scratch_confidences,
               (size_t)action_count * sizeof(*decoder->scratch_confidences));
        memcpy(out_diagnostics->values, decoder->scratch_values,
               (size_t)action_count * sizeof(*decoder->scratch_values));
        memcpy(out_diagnostics->selected, decoder->scratch_selected,
               (size_t)action_count * sizeof(*decoder->scratch_selected));
        out_diagnostics->tick = activity->tick;
    }
    decoder->last_error = MINISNN_ACTION_DECODER_ERROR_NONE;
    return 1;
}

int minisnn_action_decoder_decode_to_agent_io(
    MiniSNNActionDecoder *decoder, const MiniSNNNeuralActivityFrame *activity,
    MiniSNNAgentIOContext *agent_io,
    MiniSNNActionDecodingDiagnostics *out_diagnostics)
{
    MiniSNNActionFrame action = {0};
    MiniSNNActionDecodingDiagnostics scratch_diagnostics = {0};
    uint32_t count;
    int result = 0;

    if (decoder == NULL)
        return 0;
    if (agent_io == NULL)
    {
        decoder_error(decoder, MINISNN_ACTION_DECODER_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (minisnn_agent_io_action_schema_signature(agent_io) !=
        minisnn_action_schema_signature(decoder->action_schema))
    {
        decoder_error(decoder, MINISNN_ACTION_DECODER_ERROR_SIGNATURE_MISMATCH);
        return 0;
    }
    count = minisnn_action_schema_channel_count(decoder->action_schema);
    if (out_diagnostics != NULL &&
        (out_diagnostics->action_count != count || out_diagnostics->raw_scores == NULL ||
         out_diagnostics->confidences == NULL || out_diagnostics->values == NULL ||
         out_diagnostics->selected == NULL))
    {
        decoder_error(decoder, MINISNN_ACTION_DECODER_ERROR_DIMENSION_MISMATCH);
        return 0;
    }
    if (!minisnn_action_frame_init(&action, count))
    {
        decoder_error(decoder, MINISNN_ACTION_DECODER_ERROR_ALLOCATION);
        return 0;
    }
    if (out_diagnostics != NULL &&
        !minisnn_action_decoding_diagnostics_init(&scratch_diagnostics, count))
    {
        minisnn_action_frame_destroy(&action);
        decoder_error(decoder, MINISNN_ACTION_DECODER_ERROR_ALLOCATION);
        return 0;
    }
    if (minisnn_action_decoder_decode(decoder, activity, &action,
                                      out_diagnostics == NULL ? NULL :
                                                                &scratch_diagnostics) &&
        minisnn_agent_io_submit_action_frame(agent_io, &action))
    {
        result = 1;
        if (out_diagnostics != NULL)
        {
            memcpy(out_diagnostics->raw_scores, scratch_diagnostics.raw_scores,
                   (size_t)count * sizeof(*scratch_diagnostics.raw_scores));
            memcpy(out_diagnostics->confidences, scratch_diagnostics.confidences,
                   (size_t)count * sizeof(*scratch_diagnostics.confidences));
            memcpy(out_diagnostics->values, scratch_diagnostics.values,
                   (size_t)count * sizeof(*scratch_diagnostics.values));
            memcpy(out_diagnostics->selected, scratch_diagnostics.selected,
                   (size_t)count * sizeof(*scratch_diagnostics.selected));
            out_diagnostics->tick = scratch_diagnostics.tick;
        }
    }
    else if (minisnn_action_decoder_last_error(decoder) ==
             MINISNN_ACTION_DECODER_ERROR_NONE)
        decoder_error(decoder, MINISNN_ACTION_DECODER_ERROR_ACTION_FRAME_REJECTED);
    minisnn_action_decoding_diagnostics_destroy(&scratch_diagnostics);
    minisnn_action_frame_destroy(&action);
    return result;
}

static int write_mapping(FILE *file, const MiniSNNActionDecodingSpec *spec)
{
    return fprintf(file,
                   "mapping=%u|%u|%u|%u|%u|%u|%016llx|%016llx|%016llx|%016llx|%016llx|%u|%016llx|%016llx|%016llx|%016llx\n",
                   spec->action_channel_id, (unsigned int)spec->mode,
                   spec->primary_neuron_start, spec->primary_neuron_count,
                   spec->secondary_neuron_start, spec->secondary_neuron_count,
                   (unsigned long long)double_bits(spec->minimum_rate),
                   (unsigned long long)double_bits(spec->maximum_rate),
                   (unsigned long long)double_bits(spec->threshold),
                   (unsigned long long)double_bits(spec->active_value),
                   (unsigned long long)double_bits(spec->inactive_value),
                   spec->competition_group_id,
                   (unsigned long long)double_bits(spec->minimum_activation),
                   (unsigned long long)double_bits(spec->minimum_confidence),
                   (unsigned long long)double_bits(spec->winner_value),
                   (unsigned long long)double_bits(spec->loser_value)) >= 0;
}

int minisnn_action_decoder_write_file(
    const MiniSNNActionDecoder *decoder, const char *filename,
    MiniSNNActionDecoderError *out_error)
{
    FILE *file;
    int failed = 0;
    if (decoder == NULL || filename == NULL || filename[0] == '\0')
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    file = fopen(filename, "wb");
    if (file == NULL)
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_IO);
        return 0;
    }
    if (fprintf(file, "%s\naction_schema_signature=%016llx\nneuron_count=%u\n"
                      "brain_steps_per_tick=%u\nmapping_count=%u\n",
                ACTION_DECODER_TEXT_VERSION,
                (unsigned long long)minisnn_action_schema_signature(decoder->action_schema),
                decoder->neuron_count, decoder->brain_steps_per_tick,
                decoder->mapping_count) < 0)
        failed = 1;
    for (uint32_t index = 0U; !failed && index < decoder->mapping_count; index++)
        failed = !write_mapping(file, &decoder->mappings[index]);
    if (!failed && fprintf(file, "mapping_signature=%016llx\ncontract_signature=%016llx\n",
                           (unsigned long long)decoder->mapping_signature,
                           (unsigned long long)decoder->contract_signature) < 0)
        failed = 1;
    if (fclose(file) != 0)
        failed = 1;
    set_error(out_error, failed ? MINISNN_ACTION_DECODER_ERROR_IO :
                                 MINISNN_ACTION_DECODER_ERROR_NONE);
    return !failed;
}

static int read_line(FILE *file, char *line, size_t size)
{
    size_t length;
    if (fgets(line, (int)size, file) == NULL)
        return 0;
    length = strlen(line);
    if (length == 0U || line[length - 1U] != '\n')
        return 0;
    line[length - 1U] = '\0';
    return 1;
}

static int parse_u32_line(const char *line, const char *key, uint32_t *out_value)
{
    unsigned long value;
    char suffix;
    if (sscanf(line, "%*[^=]=%lu%c", &value, &suffix) != 1 ||
        strncmp(line, key, strlen(key)) != 0 || value > UINT32_MAX)
        return 0;
    *out_value = (uint32_t)value;
    return 1;
}

static int parse_hex_line(const char *line, const char *key, uint64_t *out_value)
{
    unsigned long long value;
    char suffix;
    if (strncmp(line, key, strlen(key)) != 0 ||
        sscanf(line + strlen(key), "%llx%c", &value, &suffix) != 1)
        return 0;
    *out_value = (uint64_t)value;
    return 1;
}

static int parse_mapping(const char *line, MiniSNNActionDecodingSpec *spec)
{
    unsigned int mode;
    unsigned long long minimum_rate, maximum_rate, threshold, active, inactive;
    unsigned long long minimum_activation, minimum_confidence, winner, loser;
    char suffix;
    int fields;

    if (strncmp(line, "mapping=", 8U) != 0)
        return 0;
    fields = sscanf(line + 8U,
        "%u|%u|%u|%u|%u|%u|%llx|%llx|%llx|%llx|%llx|%u|%llx|%llx|%llx|%llx%c",
        &spec->action_channel_id, &mode, &spec->primary_neuron_start,
        &spec->primary_neuron_count, &spec->secondary_neuron_start,
        &spec->secondary_neuron_count, &minimum_rate, &maximum_rate, &threshold,
        &active, &inactive, &spec->competition_group_id, &minimum_activation,
        &minimum_confidence, &winner, &loser, &suffix);
    if (fields != 16)
        return 0;
    spec->mode = (MiniSNNActionDecodingMode)mode;
    spec->minimum_rate = bits_double((uint64_t)minimum_rate);
    spec->maximum_rate = bits_double((uint64_t)maximum_rate);
    spec->threshold = bits_double((uint64_t)threshold);
    spec->active_value = bits_double((uint64_t)active);
    spec->inactive_value = bits_double((uint64_t)inactive);
    spec->minimum_activation = bits_double((uint64_t)minimum_activation);
    spec->minimum_confidence = bits_double((uint64_t)minimum_confidence);
    spec->winner_value = bits_double((uint64_t)winner);
    spec->loser_value = bits_double((uint64_t)loser);
    return 1;
}

MiniSNNActionDecoder *minisnn_action_decoder_read_file(
    const char *filename, const MiniSNNActionSchema *action_schema,
    MiniSNNActionDecoderError *out_error)
{
    FILE *file;
    char line[ACTION_DECODER_TEXT_LINE_MAX];
    uint64_t schema_signature, expected_mapping, expected_contract;
    uint32_t neuron_count, brain_steps, mapping_count;
    MiniSNNActionDecodingSpec *mappings = NULL;
    MiniSNNActionDecoder *decoder = NULL;

    if (filename == NULL || action_schema == NULL)
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_INVALID_ARGUMENT);
        return NULL;
    }
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_IO);
        return NULL;
    }
    if (!read_line(file, line, sizeof(line)) ||
        strcmp(line, ACTION_DECODER_TEXT_VERSION) != 0 ||
        !read_line(file, line, sizeof(line)) ||
        !parse_hex_line(line, "action_schema_signature=", &schema_signature) ||
        !read_line(file, line, sizeof(line)) ||
        !parse_u32_line(line, "neuron_count=", &neuron_count) ||
        !read_line(file, line, sizeof(line)) ||
        !parse_u32_line(line, "brain_steps_per_tick=", &brain_steps) ||
        !read_line(file, line, sizeof(line)) ||
        !parse_u32_line(line, "mapping_count=", &mapping_count) ||
        mapping_count > MINISNN_ACTION_DECODER_MAX_MAPPINGS)
        goto format_failure;
    if (schema_signature != minisnn_action_schema_signature(action_schema))
    {
        fclose(file);
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_SIGNATURE_MISMATCH);
        return NULL;
    }
    if (mapping_count > 0U)
    {
        mappings = calloc(mapping_count, sizeof(*mappings));
        if (mappings == NULL)
            goto allocation_failure;
    }
    for (uint32_t index = 0U; index < mapping_count; index++)
    {
        if (!read_line(file, line, sizeof(line)) || !parse_mapping(line, &mappings[index]))
            goto format_failure;
    }
    if (!read_line(file, line, sizeof(line)) ||
        !parse_hex_line(line, "mapping_signature=", &expected_mapping) ||
        !read_line(file, line, sizeof(line)) ||
        !parse_hex_line(line, "contract_signature=", &expected_contract) ||
        fgetc(file) != EOF)
        goto format_failure;
    fclose(file);
    file = NULL;
    decoder = minisnn_action_decoder_create(action_schema, mappings, mapping_count,
                                            neuron_count, brain_steps, out_error);
    free(mappings);
    if (decoder == NULL)
        return NULL;
    if (decoder->mapping_signature != expected_mapping ||
        decoder->contract_signature != expected_contract)
    {
        minisnn_action_decoder_destroy(&decoder);
        set_error(out_error, MINISNN_ACTION_DECODER_ERROR_SIGNATURE_MISMATCH);
        return NULL;
    }
    set_error(out_error, MINISNN_ACTION_DECODER_ERROR_NONE);
    return decoder;

allocation_failure:
    fclose(file);
    set_error(out_error, MINISNN_ACTION_DECODER_ERROR_ALLOCATION);
    return NULL;
format_failure:
    fclose(file);
    free(mappings);
    set_error(out_error, MINISNN_ACTION_DECODER_ERROR_FORMAT);
    return NULL;
}
