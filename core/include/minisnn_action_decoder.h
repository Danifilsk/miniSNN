#ifndef MINISNN_ACTION_DECODER_H
#define MINISNN_ACTION_DECODER_H

#include <stdint.h>

#include "minisnn_agent_io.h"

typedef struct MiniSNN MiniSNN;
typedef struct MiniSNNActionDecoder MiniSNNActionDecoder;

#define MINISNN_ACTION_DECODER_MAX_MAPPINGS 256U

typedef enum
{
    MINISNN_ACTION_DECODING_POPULATION_RATE = 0,
    MINISNN_ACTION_DECODING_BIPOLAR_DIFFERENCE,
    MINISNN_ACTION_DECODING_THRESHOLD,
    MINISNN_ACTION_DECODING_WTA_MEMBER
} MiniSNNActionDecodingMode;

/* Spikes use row-major order: spikes[brain_step * neuron_count + neuron]. */
typedef struct
{
    uint64_t tick;
    uint32_t neuron_count;
    uint32_t brain_step_count;
    uint8_t *spikes;
    uint8_t *captured_steps;
} MiniSNNNeuralActivityFrame;

typedef struct
{
    uint32_t action_channel_id;
    MiniSNNActionDecodingMode mode;

    uint32_t primary_neuron_start;
    uint32_t primary_neuron_count;
    uint32_t secondary_neuron_start;
    uint32_t secondary_neuron_count;

    double minimum_rate;
    double maximum_rate;

    double threshold;
    double active_value;
    double inactive_value;

    uint32_t competition_group_id;
    double minimum_activation;
    double minimum_confidence;
    double winner_value;
    double loser_value;
} MiniSNNActionDecodingSpec;

typedef struct
{
    uint64_t tick;
    uint32_t action_count;
    double *raw_scores;
    double *confidences;
    double *values;
    uint8_t *selected;
} MiniSNNActionDecodingDiagnostics;

typedef enum
{
    MINISNN_ACTION_DECODER_ERROR_NONE = 0,
    MINISNN_ACTION_DECODER_ERROR_INVALID_ARGUMENT,
    MINISNN_ACTION_DECODER_ERROR_INVALID_ACTION_SCHEMA,
    MINISNN_ACTION_DECODER_ERROR_UNKNOWN_ACTION_CHANNEL,
    MINISNN_ACTION_DECODER_ERROR_INVALID_NEURON_RANGE,
    MINISNN_ACTION_DECODER_ERROR_OVERLAPPING_NEURON_RANGE,
    MINISNN_ACTION_DECODER_ERROR_INVALID_DECODING_MODE,
    MINISNN_ACTION_DECODER_ERROR_INVALID_PARAMETER,
    MINISNN_ACTION_DECODER_ERROR_DIMENSION_MISMATCH,
    MINISNN_ACTION_DECODER_ERROR_INCOMPLETE_ACTIVITY_FRAME,
    MINISNN_ACTION_DECODER_ERROR_DUPLICATE_CAPTURED_STEP,
    MINISNN_ACTION_DECODER_ERROR_INVALID_SPIKE_VALUE,
    MINISNN_ACTION_DECODER_ERROR_NONFINITE_RESULT,
    MINISNN_ACTION_DECODER_ERROR_ACTION_FRAME_REJECTED,
    MINISNN_ACTION_DECODER_ERROR_SIGNATURE_MISMATCH,
    MINISNN_ACTION_DECODER_ERROR_FORMAT,
    MINISNN_ACTION_DECODER_ERROR_ALLOCATION,
    MINISNN_ACTION_DECODER_ERROR_IO
} MiniSNNActionDecoderError;

int minisnn_neural_activity_frame_init(
    MiniSNNNeuralActivityFrame *frame,
    uint32_t neuron_count,
    uint32_t brain_step_count,
    MiniSNNActionDecoderError *out_error);
void minisnn_neural_activity_frame_destroy(MiniSNNNeuralActivityFrame *frame);
int minisnn_neural_activity_frame_reset(
    MiniSNNNeuralActivityFrame *frame,
    uint64_t tick,
    MiniSNNActionDecoderError *out_error);
int minisnn_neural_activity_frame_copy(
    MiniSNNNeuralActivityFrame *destination,
    const MiniSNNNeuralActivityFrame *source,
    MiniSNNActionDecoderError *out_error);
int minisnn_neural_activity_frame_capture_step(
    MiniSNNNeuralActivityFrame *frame,
    uint32_t brain_step,
    const MiniSNN *network,
    MiniSNNActionDecoderError *out_error);
int minisnn_neural_activity_frame_set_step(
    MiniSNNNeuralActivityFrame *frame,
    uint32_t brain_step,
    const uint8_t *spikes,
    uint32_t spike_count,
    MiniSNNActionDecoderError *out_error);
int minisnn_neural_activity_frame_get_spike(
    const MiniSNNNeuralActivityFrame *frame,
    uint32_t brain_step,
    uint32_t neuron_id,
    uint8_t *out_spike,
    MiniSNNActionDecoderError *out_error);
int minisnn_neural_activity_frame_validate(
    const MiniSNNNeuralActivityFrame *frame,
    MiniSNNActionDecoderError *out_error);
int minisnn_neural_activity_frame_is_complete(
    const MiniSNNNeuralActivityFrame *frame,
    MiniSNNActionDecoderError *out_error);

int minisnn_action_decoding_diagnostics_init(
    MiniSNNActionDecodingDiagnostics *diagnostics,
    uint32_t action_count);
void minisnn_action_decoding_diagnostics_destroy(
    MiniSNNActionDecodingDiagnostics *diagnostics);
int minisnn_action_decoding_diagnostics_reset(
    MiniSNNActionDecodingDiagnostics *diagnostics);

MiniSNNActionDecoder *minisnn_action_decoder_create(
    const MiniSNNActionSchema *action_schema,
    const MiniSNNActionDecodingSpec *mappings,
    uint32_t mapping_count,
    uint32_t neuron_count,
    uint32_t brain_steps_per_tick,
    MiniSNNActionDecoderError *out_error);
void minisnn_action_decoder_destroy(MiniSNNActionDecoder **decoder_ptr);
void minisnn_action_decoder_reset(MiniSNNActionDecoder *decoder);

/* Contract dimensions and schema identity; no internal buffers are exposed. */
uint32_t minisnn_action_decoder_neuron_count(
    const MiniSNNActionDecoder *decoder);
uint32_t minisnn_action_decoder_brain_steps_per_tick(
    const MiniSNNActionDecoder *decoder);
uint32_t minisnn_action_decoder_action_count(
    const MiniSNNActionDecoder *decoder);
uint64_t minisnn_action_decoder_action_schema_signature(
    const MiniSNNActionDecoder *decoder);

uint64_t minisnn_action_decoding_mapping_signature(
    const MiniSNNActionDecoder *decoder);
uint64_t minisnn_action_decoder_contract_signature(
    const MiniSNNActionDecoder *decoder);
MiniSNNActionDecoderError minisnn_action_decoder_last_error(
    const MiniSNNActionDecoder *decoder);
const char *minisnn_action_decoder_error_string(
    MiniSNNActionDecoderError error);

int minisnn_action_decoder_decode(
    MiniSNNActionDecoder *decoder,
    const MiniSNNNeuralActivityFrame *activity,
    MiniSNNActionFrame *out_action,
    MiniSNNActionDecodingDiagnostics *out_diagnostics);
int minisnn_action_decoder_decode_to_agent_io(
    MiniSNNActionDecoder *decoder,
    const MiniSNNNeuralActivityFrame *activity,
    MiniSNNAgentIOContext *agent_io,
    MiniSNNActionDecodingDiagnostics *out_diagnostics);

int minisnn_action_decoder_write_file(
    const MiniSNNActionDecoder *decoder,
    const char *filename,
    MiniSNNActionDecoderError *out_error);
MiniSNNActionDecoder *minisnn_action_decoder_read_file(
    const char *filename,
    const MiniSNNActionSchema *action_schema,
    MiniSNNActionDecoderError *out_error);

#endif
