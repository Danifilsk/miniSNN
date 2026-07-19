#include <math.h>
#include <stdio.h>
#include <string.h>

#include "minisnn.h"

#define ENCODER_FILE "build/test_sensor_encoder.txt"

static int fail(const char *message)
{
    remove(ENCODER_FILE);
    printf("FAIL: %s\n", message);
    return 0;
}

static int nearly_equal(double left, double right)
{
    return fabs(left - right) < 1e-12;
}

static MiniSNNSensorSchema *make_schema(const char *first_name,
                                        MiniSNNAgentIOError *out_error)
{
    MiniSNNSensorChannelSpec channels[] =
    {
        {10U, first_name, 0.0, 1.0, 0.25},
        {20U, "signal_two", -2.0, 2.0, 0.0}
    };
    return minisnn_sensor_schema_create(channels, 2U, out_error);
}

static int set_sensor(MiniSNNSensorFrame *frame, uint64_t tick,
                      double first, double second)
{
    const double values[] = {first, second};
    MiniSNNAgentIOError error = MINISNN_AGENT_IO_ERROR_NONE;
    return minisnn_sensor_frame_set_values(frame, tick, values, 2U, &error);
}

static int current_at(const MiniSNNNeuralInputFrame *frame, uint32_t step,
                      uint32_t neuron, double expected)
{
    double current = 0.0;
    MiniSNNSensorEncoderError error = MINISNN_SENSOR_ENCODER_ERROR_NONE;
    return minisnn_neural_input_frame_get_current(frame, step, neuron, &current,
                                                  &error) &&
           nearly_equal(current, expected);
}

static int test_linear_bipolar_and_ranges(void)
{
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorEncoderError error = MINISNN_SENSOR_ENCODER_ERROR_NONE;
    MiniSNNSensorSchema *schema = make_schema("signal one", &io_error);
    MiniSNNSensorEncodingSpec mappings[] =
    {
        {10U, 0U, 2U, MINISNN_SENSOR_ENCODING_LINEAR_CURRENT,
         10.0, 1.0, 0.0, 0.0, 0U},
        {20U, 2U, 2U, MINISNN_SENSOR_ENCODING_BIPOLAR_CURRENT,
         4.0, -1.0, 0.0, 0.0, 0U},
        {10U, 4U, 1U, MINISNN_SENSOR_ENCODING_LINEAR_CURRENT,
         20.0, -1.0, 0.0, 0.0, 0U}
    };
    MiniSNNSensorEncoder *encoder = NULL;
    MiniSNNSensorFrame sensor = {0};
    MiniSNNNeuralInputFrame output = {0};

    if (schema == NULL || !minisnn_sensor_frame_init(&sensor, 2U) ||
        !minisnn_neural_input_frame_init(&output, 6U, 3U, &error))
        goto failure;
    encoder = minisnn_sensor_encoder_create(schema, mappings, 3U, 6U, 3U, &error);
    if (encoder == NULL || !set_sensor(&sensor, 4U, 0.0, -2.0) ||
        !minisnn_sensor_encoder_encode_frame(encoder, &sensor, &output) ||
        output.tick != 4U)
        goto failure;
    for (uint32_t step = 0U; step < 3U; step++)
    {
        if (!current_at(&output, step, 0U, 1.0) ||
            !current_at(&output, step, 1U, 1.0) ||
            !current_at(&output, step, 2U, -5.0) ||
            !current_at(&output, step, 3U, -5.0) ||
            !current_at(&output, step, 4U, -1.0) ||
            !current_at(&output, step, 5U, 0.0))
            goto failure;
    }
    if (!set_sensor(&sensor, 5U, 0.5, 0.0) ||
        !minisnn_sensor_encoder_encode_frame(encoder, &sensor, &output))
        goto failure;
    for (uint32_t step = 0U; step < 3U; step++)
    {
        if (!current_at(&output, step, 0U, 6.0) ||
            !current_at(&output, step, 2U, -1.0) ||
            !current_at(&output, step, 4U, 9.0))
            goto failure;
    }
    if (!set_sensor(&sensor, 6U, 1.0, 2.0) ||
        !minisnn_sensor_encoder_encode_frame(encoder, &sensor, &output))
        goto failure;
    if (!current_at(&output, 0U, 0U, 11.0) ||
        !current_at(&output, 0U, 2U, 3.0) ||
        !current_at(&output, 0U, 4U, 19.0))
        goto failure;

    minisnn_sensor_encoder_destroy(&encoder);
    minisnn_sensor_frame_destroy(&sensor);
    minisnn_neural_input_frame_destroy(&output);
    minisnn_sensor_schema_destroy(&schema);
    return 1;

failure:
    minisnn_sensor_encoder_destroy(&encoder);
    minisnn_sensor_frame_destroy(&sensor);
    minisnn_neural_input_frame_destroy(&output);
    minisnn_sensor_schema_destroy(&schema);
    return fail("linear, bipolar ou ranges invalidos");
}

static int test_validation_and_signature(void)
{
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorEncoderError error = MINISNN_SENSOR_ENCODER_ERROR_NONE;
    MiniSNNSensorSchema *schema = make_schema("signal one", &io_error);
    MiniSNNSensorEncodingSpec known[] =
    {
        {10U, 0U, 2U, MINISNN_SENSOR_ENCODING_LINEAR_CURRENT,
         10.0, 1.0, 99.0, 0.75, 17U},
        {20U, 2U, 2U, MINISNN_SENSOR_ENCODING_DETERMINISTIC_RATE,
         99.0, -5.0, 7.0, 0.5, 3U}
    };
    MiniSNNSensorEncodingSpec changed_inactive[2];
    MiniSNNSensorEncoder *encoder = NULL;
    MiniSNNSensorEncoder *same = NULL;
    MiniSNNSensorEncoder *changed = NULL;
    MiniSNNSensorEncodingSpec invalid;

    if (schema == NULL)
        return fail("schema de assinatura nao criado");
    encoder = minisnn_sensor_encoder_create(schema, known, 2U, 4U, 4U, &error);
    same = minisnn_sensor_encoder_create(schema, known, 2U, 4U, 4U, &error);
    memcpy(changed_inactive, known, sizeof(known));
    changed_inactive[0].pulse_current = 1234.0;
    changed_inactive[0].maximum_rate = 0.125;
    changed_inactive[0].phase_offset = 999U;
    changed_inactive[1].gain = -456.0;
    changed_inactive[1].bias = 765.0;
    changed = minisnn_sensor_encoder_create(schema, changed_inactive, 2U, 4U, 4U,
                                            &error);
    if (encoder == NULL || same == NULL || changed == NULL ||
        minisnn_sensor_encoding_mapping_signature(encoder) !=
            UINT64_C(7996300235072591673) ||
        minisnn_sensor_encoder_contract_signature(encoder) !=
            UINT64_C(8203056402127860223) ||
        minisnn_sensor_encoding_mapping_signature(encoder) !=
            minisnn_sensor_encoding_mapping_signature(same) ||
        minisnn_sensor_encoding_mapping_signature(encoder) !=
            minisnn_sensor_encoding_mapping_signature(changed))
        goto failure;
    changed_inactive[0].gain = 11.0;
    minisnn_sensor_encoder_destroy(&changed);
    changed = minisnn_sensor_encoder_create(schema, changed_inactive, 2U, 4U, 4U,
                                            &error);
    if (changed == NULL || minisnn_sensor_encoding_mapping_signature(encoder) ==
        minisnn_sensor_encoding_mapping_signature(changed))
        goto failure;

    invalid = known[0];
    invalid.sensor_channel_id = 999U;
    if (minisnn_sensor_encoder_create(schema, &invalid, 1U, 4U, 4U, &error) != NULL ||
        error != MINISNN_SENSOR_ENCODER_ERROR_UNKNOWN_SENSOR_CHANNEL)
        goto failure;
    invalid = known[0];
    invalid.target_neuron_start = 4U;
    if (minisnn_sensor_encoder_create(schema, &invalid, 1U, 4U, 4U, &error) != NULL ||
        error != MINISNN_SENSOR_ENCODER_ERROR_INVALID_NEURON_RANGE)
        goto failure;
    invalid = known[1];
    invalid.target_neuron_start = 1U;
    if (minisnn_sensor_encoder_create(schema, (MiniSNNSensorEncodingSpec[]){known[0], invalid},
                                      2U, 4U, 4U, &error) != NULL ||
        error != MINISNN_SENSOR_ENCODER_ERROR_OVERLAPPING_NEURON_RANGE)
        goto failure;
    invalid = known[1];
    invalid.pulse_current = NAN;
    if (minisnn_sensor_encoder_create(schema, &invalid, 1U, 4U, 4U, &error) != NULL ||
        error != MINISNN_SENSOR_ENCODER_ERROR_INVALID_PARAMETER)
        goto failure;
    invalid = known[0];
    invalid.gain = INFINITY;
    if (minisnn_sensor_encoder_create(schema, &invalid, 1U, 4U, 4U, &error) != NULL ||
        error != MINISNN_SENSOR_ENCODER_ERROR_INVALID_PARAMETER)
        goto failure;
    invalid = known[0];
    invalid.mode = (MiniSNNSensorEncodingMode)99;
    if (minisnn_sensor_encoder_create(schema, &invalid, 1U, 4U, 4U, &error) != NULL ||
        error != MINISNN_SENSOR_ENCODER_ERROR_INVALID_ENCODING_MODE)
        goto failure;

    minisnn_sensor_encoder_destroy(&encoder);
    minisnn_sensor_encoder_destroy(&same);
    minisnn_sensor_encoder_destroy(&changed);
    minisnn_sensor_schema_destroy(&schema);
    return 1;

failure:
    minisnn_sensor_encoder_destroy(&encoder);
    minisnn_sensor_encoder_destroy(&same);
    minisnn_sensor_encoder_destroy(&changed);
    minisnn_sensor_schema_destroy(&schema);
    return fail("validacao ou assinatura do encoder invalidas");
}

static int test_rate_reset_atomicity_and_names(void)
{
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorEncoderError error = MINISNN_SENSOR_ENCODER_ERROR_NONE;
    MiniSNNSensorSchema *schema = make_schema("signal one", &io_error);
    MiniSNNSensorSchema *renamed = make_schema("unrelated label", &io_error);
    MiniSNNSensorEncodingSpec mapping =
    {
        10U, 0U, 2U, MINISNN_SENSOR_ENCODING_DETERMINISTIC_RATE,
        0.0, 0.0, 9.0, 0.3, 0U
    };
    MiniSNNSensorEncoder *first = NULL;
    MiniSNNSensorEncoder *second = NULL;
    MiniSNNSensorEncoder *name_independent = NULL;
    MiniSNNSensorFrame sensor = {0};
    MiniSNNSensorFrame invalid_sensor = {0};
    MiniSNNNeuralInputFrame output = {0};
    MiniSNNNeuralInputFrame snapshot = {0};
    MiniSNNNeuralInputFrame renamed_output = {0};
    int pulse_count = 0;

    if (schema == NULL || renamed == NULL || !minisnn_sensor_frame_init(&sensor, 2U) ||
        !minisnn_sensor_frame_init(&invalid_sensor, 2U) ||
        !minisnn_neural_input_frame_init(&output, 2U, 4U, &error) ||
        !minisnn_neural_input_frame_init(&snapshot, 2U, 4U, &error) ||
        !minisnn_neural_input_frame_init(&renamed_output, 2U, 4U, &error))
        goto failure;
    first = minisnn_sensor_encoder_create(schema, &mapping, 1U, 2U, 4U, &error);
    second = minisnn_sensor_encoder_create(schema, &mapping, 1U, 2U, 4U, &error);
    name_independent = minisnn_sensor_encoder_create(renamed, &mapping, 1U, 2U, 4U,
                                                      &error);
    if (first == NULL || second == NULL || name_independent == NULL ||
        !set_sensor(&sensor, 1U, 1.0, 0.0) ||
        !minisnn_sensor_encoder_encode_frame(first, &sensor, &output) ||
        !minisnn_sensor_encoder_encode_frame(second, &sensor, &snapshot) ||
        !minisnn_sensor_encoder_encode_frame(name_independent, &sensor, &renamed_output))
        goto failure;
    for (uint32_t step = 0U; step < 4U; step++)
    {
        double current = 0.0;
        if (!minisnn_neural_input_frame_get_current(&output, step, 0U, &current,
                                                    &error) ||
            !nearly_equal(current, snapshot.currents[(size_t)step * 2U]) ||
            !nearly_equal(current, renamed_output.currents[(size_t)step * 2U]))
            goto failure;
        if (current > 0.0)
            pulse_count++;
    }
    if (pulse_count != 1)
        goto failure;
    if (!minisnn_sensor_encoder_encode_frame(first, &sensor, &output) ||
        memcmp(output.currents, snapshot.currents,
               8U * sizeof(*output.currents)) == 0)
        goto failure;
    minisnn_sensor_encoder_reset(first);
    if (!minisnn_sensor_encoder_encode_frame(first, &sensor, &output) ||
        memcmp(output.currents, snapshot.currents,
               8U * sizeof(*output.currents)) != 0)
        goto failure;
    invalid_sensor.tick = 2U;
    invalid_sensor.values[0] = NAN;
    invalid_sensor.values[1] = 0.0;
    if (!minisnn_neural_input_frame_copy(&snapshot, &output, &error) ||
        minisnn_sensor_encoder_encode_frame(first, &invalid_sensor, &output) ||
        minisnn_sensor_encoder_last_error(first) !=
            MINISNN_SENSOR_ENCODER_ERROR_NONFINITE_RESULT ||
        output.tick != snapshot.tick ||
        memcmp(output.currents, snapshot.currents,
               8U * sizeof(*output.currents)) != 0)
        goto failure;
    minisnn_sensor_encoder_reset(first);
    if (!minisnn_sensor_encoder_encode_frame(first, &sensor, &output) ||
        memcmp(output.currents, snapshot.currents,
               8U * sizeof(*output.currents)) != 0)
        goto failure;

    minisnn_sensor_encoder_destroy(&first);
    minisnn_sensor_encoder_destroy(&second);
    minisnn_sensor_encoder_destroy(&name_independent);
    minisnn_sensor_frame_destroy(&sensor);
    minisnn_sensor_frame_destroy(&invalid_sensor);
    minisnn_neural_input_frame_destroy(&output);
    minisnn_neural_input_frame_destroy(&snapshot);
    minisnn_neural_input_frame_destroy(&renamed_output);
    minisnn_sensor_schema_destroy(&schema);
    minisnn_sensor_schema_destroy(&renamed);
    return 1;

failure:
    minisnn_sensor_encoder_destroy(&first);
    minisnn_sensor_encoder_destroy(&second);
    minisnn_sensor_encoder_destroy(&name_independent);
    minisnn_sensor_frame_destroy(&sensor);
    minisnn_sensor_frame_destroy(&invalid_sensor);
    minisnn_neural_input_frame_destroy(&output);
    minisnn_neural_input_frame_destroy(&snapshot);
    minisnn_neural_input_frame_destroy(&renamed_output);
    minisnn_sensor_schema_destroy(&schema);
    minisnn_sensor_schema_destroy(&renamed);
    return fail("rate deterministico, reset, atomicidade ou nomes invalidos");
}

static int test_constant_channel_and_models(void)
{
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorEncoderError error = MINISNN_SENSOR_ENCODER_ERROR_NONE;
    MiniSNNSensorChannelSpec channels[] = {{30U, "constant", 5.0, 5.0, 5.0}};
    MiniSNNSensorSchema *schema = minisnn_sensor_schema_create(channels, 1U,
                                                                 &io_error);
    MiniSNNSensorEncodingSpec mappings[] =
    {
        {30U, 0U, 1U, MINISNN_SENSOR_ENCODING_LINEAR_CURRENT,
         10.0, 2.0, 0.0, 0.0, 0U},
        {30U, 1U, 1U, MINISNN_SENSOR_ENCODING_DETERMINISTIC_RATE,
         0.0, 0.0, 7.0, 1.0, 0U}
    };
    MiniSNNSensorEncoder *encoder = NULL;
    MiniSNNSensorFrame sensor = {0};
    MiniSNNNeuralInputFrame frame = {0};

    if (schema == NULL || !minisnn_sensor_frame_init(&sensor, 1U) ||
        !minisnn_neural_input_frame_init(&frame, 2U, 2U, &error))
        goto failure;
    encoder = minisnn_sensor_encoder_create(schema, mappings, 2U, 2U, 2U, &error);
    sensor.tick = 3U;
    sensor.values[0] = 5.0;
    if (encoder == NULL || !minisnn_sensor_encoder_encode_frame(encoder, &sensor,
                                                                 &frame) ||
        !current_at(&frame, 0U, 0U, 2.0) || !current_at(&frame, 1U, 0U, 2.0) ||
        !current_at(&frame, 0U, 1U, 0.0) || !current_at(&frame, 1U, 1U, 0.0))
        goto failure;

    for (MiniSNNNeuronModel model = MINISNN_NEURON_MODEL_LIF;
         model <= MINISNN_NEURON_MODEL_HODGKIN_HUXLEY; model++)
    {
        MiniSNNConfig config = minisnn_default_config();
        MiniSNN *network;
        double voltage = 0.0;

        config.neuron_count = 2;
        config.neuron_model = model;
        network = minisnn_create_with_config(&config);
        if (network == NULL || !minisnn_neural_input_frame_apply_step(&frame, 0U,
                                                                        network, &error) ||
            minisnn_step(network) < 0 || !minisnn_get_voltage(network, 0U,
                                                                 &voltage) ||
            !isfinite(voltage))
        {
            minisnn_destroy(&network);
            goto failure;
        }
        minisnn_destroy(&network);
    }

    minisnn_sensor_encoder_destroy(&encoder);
    minisnn_sensor_frame_destroy(&sensor);
    minisnn_neural_input_frame_destroy(&frame);
    minisnn_sensor_schema_destroy(&schema);
    return 1;

failure:
    minisnn_sensor_encoder_destroy(&encoder);
    minisnn_sensor_frame_destroy(&sensor);
    minisnn_neural_input_frame_destroy(&frame);
    minisnn_sensor_schema_destroy(&schema);
    return fail("canal constante ou modelos neuronais invalidos");
}

static int test_agent_io_apply_and_serialization(void)
{
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorEncoderError error = MINISNN_SENSOR_ENCODER_ERROR_NONE;
    MiniSNNSensorSchema *schema = make_schema("signal one", &io_error);
    MiniSNNSensorChannelSpec incompatible_channels[] =
    {
        {10U, "signal one", 0.0, 2.0, 0.0},
        {20U, "signal_two", -2.0, 2.0, 0.0}
    };
    MiniSNNSensorSchema *incompatible = minisnn_sensor_schema_create(
        incompatible_channels, 2U, &io_error);
    MiniSNNActionChannelSpec action_channels[] = {{1U, "out", 0.0, 1.0, 0.0}};
    MiniSNNActionSchema *action = minisnn_action_schema_create(action_channels, 1U,
                                                                 &io_error);
    MiniSNNSensorEncodingSpec mapping =
    {
        10U, 0U, 1U, MINISNN_SENSOR_ENCODING_LINEAR_CURRENT,
        100.0, 0.0, 0.0, 0.0, 0U
    };
    MiniSNNSensorEncoder *encoder = NULL;
    MiniSNNSensorEncoder *loaded = NULL;
    MiniSNNAgentIOContext *context = NULL;
    MiniSNNSensorFrame sensor = {0};
    MiniSNNNeuralInputFrame output = {0};
    MiniSNNNeuralInputFrame wrong = {0};
    MiniSNNNeuralInputFrame zero = {0};
    MiniSNN *network = NULL;
    MiniSNNConfig config;
    double voltage = 0.0;
    FILE *invalid_file = NULL;
    int invalid_write_failed;

    if (schema == NULL || incompatible == NULL || action == NULL ||
        !minisnn_sensor_frame_init(&sensor, 2U) ||
        !minisnn_neural_input_frame_init(&output, 2U, 2U, &error) ||
        !minisnn_neural_input_frame_init(&wrong, 3U, 2U, &error) ||
        !minisnn_neural_input_frame_init(&zero, 2U, 2U, &error))
        goto failure;
    encoder = minisnn_sensor_encoder_create(schema, &mapping, 1U, 2U, 2U, &error);
    context = minisnn_agent_io_create(schema, action, &io_error);
    if (encoder == NULL || context == NULL || !set_sensor(&sensor, 7U, 1.0, 0.0) ||
        minisnn_sensor_encoder_encode_from_agent_io(encoder, context, &output) ||
        minisnn_sensor_encoder_last_error(encoder) !=
            MINISNN_SENSOR_ENCODER_ERROR_FRAME_UNAVAILABLE ||
        !minisnn_agent_io_submit_sensor_frame(context, &sensor) ||
        minisnn_sensor_encoder_encode_from_agent_io(encoder, context, &wrong) ||
        minisnn_sensor_encoder_last_error(encoder) !=
            MINISNN_SENSOR_ENCODER_ERROR_DIMENSION_MISMATCH ||
        !minisnn_sensor_encoder_encode_from_agent_io(encoder, context, &output) ||
        minisnn_sensor_encoder_encode_from_agent_io(encoder, context, &output) ||
        minisnn_sensor_encoder_last_error(encoder) !=
            MINISNN_SENSOR_ENCODER_ERROR_FRAME_ALREADY_CONSUMED)
        goto failure;
    if (!minisnn_sensor_encoder_write_file(encoder, ENCODER_FILE, &error))
        goto failure;
    if (minisnn_sensor_encoder_read_file(ENCODER_FILE, incompatible, &error) != NULL ||
        error != MINISNN_SENSOR_ENCODER_ERROR_SIGNATURE_MISMATCH)
        goto failure;
    loaded = minisnn_sensor_encoder_read_file(ENCODER_FILE, schema, &error);
    if (loaded == NULL || minisnn_sensor_encoding_mapping_signature(loaded) !=
        minisnn_sensor_encoding_mapping_signature(encoder))
        goto failure;
    minisnn_sensor_encoder_destroy(&loaded);
    loaded = minisnn_sensor_encoder_read_file(ENCODER_FILE, schema, &error);
    if (loaded == NULL)
        goto failure;
    minisnn_sensor_encoder_destroy(&loaded);
    invalid_file = fopen(ENCODER_FILE, "wb");
    if (invalid_file == NULL)
        goto failure;
    invalid_write_failed = fputs("invalid\n", invalid_file) == EOF;
    if (fclose(invalid_file) != 0)
        invalid_write_failed = 1;
    invalid_file = NULL;
    if (invalid_write_failed)
        goto failure;
    if (minisnn_sensor_encoder_read_file(ENCODER_FILE, schema, &error) != NULL ||
        error != MINISNN_SENSOR_ENCODER_ERROR_FORMAT)
        goto failure;
    config = minisnn_default_config();
    config.neuron_count = 2;
    network = minisnn_create_with_config(&config);
    if (network == NULL || minisnn_current_step(network) != 0 ||
        !minisnn_neural_input_frame_apply_step(&output, 0U, network, &error) ||
        minisnn_current_step(network) != 0 || minisnn_step(network) < 0 ||
        !minisnn_get_voltage(network, 0, &voltage) || voltage <= -65.0 ||
        !minisnn_neural_input_frame_apply_step(&zero, 0U, network, &error) ||
        minisnn_step(network) < 0 || !minisnn_get_voltage(network, 0, &voltage) ||
        !nearly_equal(voltage, -64.5025))
        goto failure;

    minisnn_destroy(&network);
    minisnn_sensor_encoder_destroy(&encoder);
    minisnn_sensor_encoder_destroy(&loaded);
    minisnn_agent_io_destroy(&context);
    minisnn_sensor_frame_destroy(&sensor);
    minisnn_neural_input_frame_destroy(&output);
    minisnn_neural_input_frame_destroy(&wrong);
    minisnn_neural_input_frame_destroy(&zero);
    minisnn_sensor_schema_destroy(&schema);
    minisnn_sensor_schema_destroy(&incompatible);
    minisnn_action_schema_destroy(&action);
    remove(ENCODER_FILE);
    return 1;

failure:
    if (invalid_file != NULL)
        fclose(invalid_file);
    minisnn_destroy(&network);
    minisnn_sensor_encoder_destroy(&encoder);
    minisnn_sensor_encoder_destroy(&loaded);
    minisnn_agent_io_destroy(&context);
    minisnn_sensor_frame_destroy(&sensor);
    minisnn_neural_input_frame_destroy(&output);
    minisnn_neural_input_frame_destroy(&wrong);
    minisnn_neural_input_frame_destroy(&zero);
    minisnn_sensor_schema_destroy(&schema);
    minisnn_sensor_schema_destroy(&incompatible);
    minisnn_action_schema_destroy(&action);
    return fail("agent I/O, aplicacao ou serializacao invalidos");
}

static int test_phase_offset_and_apply_errors(void)
{
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorEncoderError error = MINISNN_SENSOR_ENCODER_ERROR_NONE;
    MiniSNNSensorSchema *schema = make_schema("signal one", &io_error);
    MiniSNNSensorEncodingSpec rate =
    {
        10U, 0U, 1U, MINISNN_SENSOR_ENCODING_DETERMINISTIC_RATE,
        0.0, 0.0, 8.0, 0.5, 999U
    };
    MiniSNNSensorEncoder *encoder = NULL;
    MiniSNNSensorEncoder *loaded = NULL;
    MiniSNNSensorFrame sensor = {0};
    MiniSNNNeuralInputFrame frame = {0};
    MiniSNN *network = NULL;
    MiniSNN *wrong_network = NULL;
    MiniSNNConfig config;
    double voltage = 0.0;

    if (schema == NULL || !minisnn_sensor_frame_init(&sensor, 2U) ||
        !minisnn_neural_input_frame_init(&frame, 2U, 1U, &error))
        goto failure;
    encoder = minisnn_sensor_encoder_create(schema, &rate, 1U, 2U, 1U, &error);
    if (encoder == NULL || !set_sensor(&sensor, 1U, 1.0, 0.0) ||
        !minisnn_sensor_encoder_encode_frame(encoder, &sensor, &frame) ||
        !minisnn_sensor_encoder_write_file(encoder, ENCODER_FILE, &error))
        goto failure;
    loaded = minisnn_sensor_encoder_read_file(ENCODER_FILE, schema, &error);
    if (loaded == NULL || minisnn_sensor_encoding_mapping_signature(loaded) !=
                              minisnn_sensor_encoding_mapping_signature(encoder))
        goto failure;
    minisnn_sensor_encoder_destroy(&loaded);
    rate.phase_offset = 0U;
    loaded = minisnn_sensor_encoder_create(schema, &rate, 1U, 2U, 1U, &error);
    if (loaded == NULL)
        goto failure;
    minisnn_sensor_encoder_destroy(&loaded);
    rate.phase_offset = 1000U;
    if (minisnn_sensor_encoder_create(schema, &rate, 1U, 2U, 1U, &error) != NULL ||
        error != MINISNN_SENSOR_ENCODER_ERROR_INVALID_PARAMETER)
        goto failure;

    config = minisnn_default_config();
    config.neuron_count = 2;
    network = minisnn_create_with_config(&config);
    if (network == NULL ||
        minisnn_neural_input_frame_apply_step(NULL, 0U, network, &error) ||
        error != MINISNN_SENSOR_ENCODER_ERROR_INVALID_ARGUMENT ||
        minisnn_neural_input_frame_apply_step(&frame, 0U, NULL, &error) ||
        error != MINISNN_SENSOR_ENCODER_ERROR_INVALID_ARGUMENT ||
        minisnn_neural_input_frame_apply_step(&frame, 0U, network, NULL) ||
        !minisnn_set_input(network, 0, 5.0) ||
        minisnn_neural_input_frame_apply_step(&frame, 1U, network, &error) ||
        error != MINISNN_SENSOR_ENCODER_ERROR_DIMENSION_MISMATCH ||
        minisnn_current_step(network) != 0 || minisnn_step(network) < 0 ||
        !minisnn_get_voltage(network, 0U, &voltage) || !nearly_equal(voltage, -64.975))
        goto failure;

    frame.currents[0] = NAN;
    if (!minisnn_set_input(network, 1, 5.0) ||
        minisnn_neural_input_frame_apply_step(&frame, 0U, network, &error) ||
        error != MINISNN_SENSOR_ENCODER_ERROR_NONFINITE_RESULT)
        goto failure;
    frame.currents[0] = 8.0;
    wrong_network = minisnn_create(1);
    if (wrong_network == NULL || !minisnn_set_input(wrong_network, 0, 5.0) ||
        minisnn_neural_input_frame_apply_step(&frame, 0U, wrong_network, &error) ||
        error != MINISNN_SENSOR_ENCODER_ERROR_DIMENSION_MISMATCH ||
        minisnn_step(wrong_network) < 0 || !minisnn_get_voltage(wrong_network, 0U,
                                                                  &voltage) ||
        !nearly_equal(voltage, -64.975))
        goto failure;
    if (!minisnn_neural_input_frame_apply_step(&frame, 0U, network, &error) ||
        error != MINISNN_SENSOR_ENCODER_ERROR_NONE || minisnn_current_step(network) != 1)
        goto failure;

    minisnn_destroy(&network);
    minisnn_destroy(&wrong_network);
    minisnn_sensor_encoder_destroy(&encoder);
    minisnn_sensor_encoder_destroy(&loaded);
    minisnn_sensor_frame_destroy(&sensor);
    minisnn_neural_input_frame_destroy(&frame);
    minisnn_sensor_schema_destroy(&schema);
    remove(ENCODER_FILE);
    return 1;

failure:
    minisnn_destroy(&network);
    minisnn_destroy(&wrong_network);
    minisnn_sensor_encoder_destroy(&encoder);
    minisnn_sensor_encoder_destroy(&loaded);
    minisnn_sensor_frame_destroy(&sensor);
    minisnn_neural_input_frame_destroy(&frame);
    minisnn_sensor_schema_destroy(&schema);
    return fail("phase_offset ou erros de aplicacao invalidos");
}

int main(void)
{
    if (!test_linear_bipolar_and_ranges() || !test_validation_and_signature() ||
        !test_rate_reset_atomicity_and_names() || !test_constant_channel_and_models() ||
        !test_agent_io_apply_and_serialization() || !test_phase_offset_and_apply_errors())
        return 1;
    printf("Sensor encoder validation OK\n");
    return 0;
}
