#ifndef MINISNN_SENSOR_ENCODER_H
#define MINISNN_SENSOR_ENCODER_H

#include <stdint.h>

#include "minisnn_agent_io.h"

typedef struct MiniSNN MiniSNN;
typedef struct MiniSNNSensorEncoder MiniSNNSensorEncoder;

#define MINISNN_SENSOR_ENCODER_MAX_MAPPINGS 256U

/* maximum_rate is expressed in pulses per neural step and must be in (0, 1].
 * phase_offset is expressed in thousandths of phase and must be in [0, 999]. */
typedef enum
{
    MINISNN_SENSOR_ENCODING_LINEAR_CURRENT = 0,
    MINISNN_SENSOR_ENCODING_BIPOLAR_CURRENT,
    MINISNN_SENSOR_ENCODING_DETERMINISTIC_RATE
} MiniSNNSensorEncodingMode;

typedef struct
{
    uint32_t sensor_channel_id;
    uint32_t target_neuron_start;
    uint32_t target_neuron_count;
    MiniSNNSensorEncodingMode mode;
    double gain;
    double bias;
    double pulse_current;
    double maximum_rate;
    uint32_t phase_offset;
} MiniSNNSensorEncodingSpec;

/* currents are stored in row-major order: currents[brain_step * neuron_count + neuron]. */
typedef struct
{
    uint64_t tick;
    uint32_t neuron_count;
    uint32_t brain_step_count;
    double *currents;
} MiniSNNNeuralInputFrame;

typedef enum
{
    MINISNN_SENSOR_ENCODER_ERROR_NONE = 0,
    MINISNN_SENSOR_ENCODER_ERROR_INVALID_ARGUMENT,
    MINISNN_SENSOR_ENCODER_ERROR_INVALID_SCHEMA,
    MINISNN_SENSOR_ENCODER_ERROR_UNKNOWN_SENSOR_CHANNEL,
    MINISNN_SENSOR_ENCODER_ERROR_INVALID_NEURON_RANGE,
    MINISNN_SENSOR_ENCODER_ERROR_OVERLAPPING_NEURON_RANGE,
    MINISNN_SENSOR_ENCODER_ERROR_INVALID_ENCODING_MODE,
    MINISNN_SENSOR_ENCODER_ERROR_INVALID_PARAMETER,
    MINISNN_SENSOR_ENCODER_ERROR_DIMENSION_MISMATCH,
    MINISNN_SENSOR_ENCODER_ERROR_FRAME_UNAVAILABLE,
    MINISNN_SENSOR_ENCODER_ERROR_FRAME_ALREADY_CONSUMED,
    MINISNN_SENSOR_ENCODER_ERROR_NONFINITE_RESULT,
    MINISNN_SENSOR_ENCODER_ERROR_SIGNATURE_MISMATCH,
    MINISNN_SENSOR_ENCODER_ERROR_FORMAT,
    MINISNN_SENSOR_ENCODER_ERROR_ALLOCATION,
    MINISNN_SENSOR_ENCODER_ERROR_IO
} MiniSNNSensorEncoderError;

int minisnn_neural_input_frame_init(
    MiniSNNNeuralInputFrame *frame,
    uint32_t neuron_count,
    uint32_t brain_step_count,
    MiniSNNSensorEncoderError *out_error);
void minisnn_neural_input_frame_destroy(MiniSNNNeuralInputFrame *frame);
int minisnn_neural_input_frame_reset(
    MiniSNNNeuralInputFrame *frame,
    MiniSNNSensorEncoderError *out_error);
int minisnn_neural_input_frame_get_current(
    const MiniSNNNeuralInputFrame *frame,
    uint32_t brain_step,
    uint32_t neuron_id,
    double *out_current,
    MiniSNNSensorEncoderError *out_error);
int minisnn_neural_input_frame_copy(
    MiniSNNNeuralInputFrame *destination,
    const MiniSNNNeuralInputFrame *source,
    MiniSNNSensorEncoderError *out_error);
int minisnn_neural_input_frame_validate(
    const MiniSNNNeuralInputFrame *frame,
    MiniSNNSensorEncoderError *out_error);

MiniSNNSensorEncoder *minisnn_sensor_encoder_create(
    const MiniSNNSensorSchema *sensor_schema,
    const MiniSNNSensorEncodingSpec *mappings,
    uint32_t mapping_count,
    uint32_t neuron_count,
    uint32_t brain_steps_per_tick,
    MiniSNNSensorEncoderError *out_error);
void minisnn_sensor_encoder_destroy(MiniSNNSensorEncoder **encoder_ptr);
void minisnn_sensor_encoder_reset(MiniSNNSensorEncoder *encoder);

uint64_t minisnn_sensor_encoding_mapping_signature(
    const MiniSNNSensorEncoder *encoder);
uint64_t minisnn_sensor_encoder_contract_signature(
    const MiniSNNSensorEncoder *encoder);
MiniSNNSensorEncoderError minisnn_sensor_encoder_last_error(
    const MiniSNNSensorEncoder *encoder);
const char *minisnn_sensor_encoder_error_string(
    MiniSNNSensorEncoderError error);

int minisnn_sensor_encoder_encode_frame(
    MiniSNNSensorEncoder *encoder,
    const MiniSNNSensorFrame *sensor_frame,
    MiniSNNNeuralInputFrame *out_frame);
int minisnn_sensor_encoder_encode_from_agent_io(
    MiniSNNSensorEncoder *encoder,
    MiniSNNAgentIOContext *agent_io,
    MiniSNNNeuralInputFrame *out_frame);

int minisnn_neural_input_frame_apply_step(
    const MiniSNNNeuralInputFrame *frame,
    uint32_t brain_step,
    MiniSNN *network,
    MiniSNNSensorEncoderError *out_error);

int minisnn_sensor_encoder_write_file(
    const MiniSNNSensorEncoder *encoder,
    const char *filename,
    MiniSNNSensorEncoderError *out_error);
MiniSNNSensorEncoder *minisnn_sensor_encoder_read_file(
    const char *filename,
    const MiniSNNSensorSchema *sensor_schema,
    MiniSNNSensorEncoderError *out_error);

#endif
