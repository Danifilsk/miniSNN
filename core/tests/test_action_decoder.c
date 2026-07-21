#include <math.h>
#include <stdio.h>
#include <string.h>

#include "minisnn.h"

#define DECODER_FILE "test_action_decoder.tmp"

static int fail(const char *message)
{
    fprintf(stderr, "Action decoder test failed: %s\n", message);
    return 0;
}

static int near(double left, double right)
{
    return fabs(left - right) < 1e-12;
}

static int diagnostics_copy(MiniSNNActionDecodingDiagnostics *destination,
                            const MiniSNNActionDecodingDiagnostics *source)
{
    if (destination == NULL || source == NULL ||
        destination->action_count != source->action_count ||
        destination->raw_scores == NULL || destination->confidences == NULL ||
        destination->values == NULL || destination->selected == NULL ||
        source->raw_scores == NULL || source->confidences == NULL ||
        source->values == NULL || source->selected == NULL)
        return 0;
    memcpy(destination->raw_scores, source->raw_scores,
           (size_t)source->action_count * sizeof(*source->raw_scores));
    memcpy(destination->confidences, source->confidences,
           (size_t)source->action_count * sizeof(*source->confidences));
    memcpy(destination->values, source->values,
           (size_t)source->action_count * sizeof(*source->values));
    memcpy(destination->selected, source->selected,
           (size_t)source->action_count * sizeof(*source->selected));
    destination->tick = source->tick;
    return 1;
}

static int diagnostics_equal(const MiniSNNActionDecodingDiagnostics *left,
                             const MiniSNNActionDecodingDiagnostics *right)
{
    if (left == NULL || right == NULL || left->tick != right->tick ||
        left->action_count != right->action_count)
        return 0;
    return memcmp(left->raw_scores, right->raw_scores,
                  (size_t)left->action_count * sizeof(*left->raw_scores)) == 0 &&
           memcmp(left->confidences, right->confidences,
                  (size_t)left->action_count * sizeof(*left->confidences)) == 0 &&
           memcmp(left->values, right->values,
                  (size_t)left->action_count * sizeof(*left->values)) == 0 &&
           memcmp(left->selected, right->selected,
                  (size_t)left->action_count * sizeof(*left->selected)) == 0;
}

static MiniSNNActionSchema *make_schema(const char *suffix,
                                         MiniSNNAgentIOError *out_error)
{
    static char names[5][32];
    MiniSNNActionChannelSpec channels[5];
    const double defaults[] = {0.25, -0.25, 0.0, 0.5, 0.5};

    for (uint32_t index = 0U; index < 5U; index++)
    {
        snprintf(names[index], sizeof(names[index]), "output_%u%s", index, suffix);
        channels[index].id = (index + 1U) * 10U;
        channels[index].name = names[index];
        channels[index].minimum = index == 1U ? -1.0 : 0.0;
        channels[index].maximum = 1.0;
        channels[index].default_value = defaults[index];
    }
    return minisnn_action_schema_create(channels, 5U, out_error);
}

static MiniSNNActionSchema *make_two_channel_schema(int reversed,
                                                     MiniSNNAgentIOError *out_error)
{
    const MiniSNNActionChannelSpec ordered[] =
    {
        {10U, "first", 0.0, 1.0, 0.0},
        {20U, "second", 0.0, 1.0, 0.0}
    };
    const MiniSNNActionChannelSpec reordered[] =
    {
        {20U, "second", 0.0, 1.0, 0.0},
        {10U, "first", 0.0, 1.0, 0.0}
    };

    return minisnn_action_schema_create(reversed ? reordered : ordered, 2U,
                                        out_error);
}

static void make_mappings(MiniSNNActionDecodingSpec mappings[5])
{
    memset(mappings, 0, 5U * sizeof(*mappings));
    mappings[0].action_channel_id = 10U;
    mappings[0].mode = MINISNN_ACTION_DECODING_POPULATION_RATE;
    mappings[0].primary_neuron_start = 0U;
    mappings[0].primary_neuron_count = 1U;
    mappings[0].minimum_rate = 0.25;
    mappings[0].maximum_rate = 0.75;

    mappings[1].action_channel_id = 20U;
    mappings[1].mode = MINISNN_ACTION_DECODING_BIPOLAR_DIFFERENCE;
    mappings[1].primary_neuron_start = 1U;
    mappings[1].primary_neuron_count = 1U;
    mappings[1].secondary_neuron_start = 2U;
    mappings[1].secondary_neuron_count = 1U;

    mappings[2].action_channel_id = 30U;
    mappings[2].mode = MINISNN_ACTION_DECODING_THRESHOLD;
    mappings[2].primary_neuron_start = 3U;
    mappings[2].primary_neuron_count = 1U;
    mappings[2].threshold = 0.5;
    mappings[2].active_value = 1.0;
    mappings[2].inactive_value = 0.0;

    mappings[3].action_channel_id = 40U;
    mappings[3].mode = MINISNN_ACTION_DECODING_WTA_MEMBER;
    mappings[3].primary_neuron_start = 4U;
    mappings[3].primary_neuron_count = 1U;
    mappings[3].competition_group_id = 7U;
    mappings[3].minimum_activation = 0.5;
    mappings[3].minimum_confidence = 0.0;
    mappings[3].winner_value = 1.0;
    mappings[3].loser_value = 0.0;

    mappings[4].action_channel_id = 50U;
    mappings[4].mode = MINISNN_ACTION_DECODING_WTA_MEMBER;
    mappings[4].primary_neuron_start = 5U;
    mappings[4].primary_neuron_count = 1U;
    mappings[4].competition_group_id = 7U;
    mappings[4].minimum_activation = 0.5;
    mappings[4].minimum_confidence = 0.0;
    mappings[4].winner_value = 1.0;
    mappings[4].loser_value = 0.0;
}

static int set_pattern(MiniSNNNeuralActivityFrame *activity, int tie)
{
    const uint8_t steps[4][6] =
    {
        {0, 1, 0, 0, 1, 1},
        {0, 1, 0, 0, 1, 1},
        {1, 1, 0, 1, 1, tie ? 1 : 0},
        {1, 1, 0, 1, 0, tie ? 0 : 0}
    };
    MiniSNNActionDecoderError error = MINISNN_ACTION_DECODER_ERROR_NONE;

    if (!minisnn_neural_activity_frame_reset(activity, 42U, &error))
        return 0;
    for (uint32_t step = 0U; step < 4U; step++)
    {
        if (!minisnn_neural_activity_frame_set_step(activity, step, steps[step],
                                                     6U, &error))
            return 0;
    }
    return 1;
}

static int test_activity_frame(void)
{
    MiniSNNActionDecoderError error = MINISNN_ACTION_DECODER_ERROR_NONE;
    MiniSNNNeuralActivityFrame frame = {0};
    MiniSNNNeuralActivityFrame copy = {0};
    uint8_t spikes[] = {0U, 1U, 0U};
    uint8_t invalid[] = {0U, 2U, 0U};
    uint8_t spike = 0U;

    if (!minisnn_neural_activity_frame_init(&frame, 3U, 2U, &error) ||
        !minisnn_neural_activity_frame_init(&copy, 3U, 2U, &error) ||
        !minisnn_neural_activity_frame_set_step(&frame, 0U, spikes, 3U, &error) ||
        minisnn_neural_activity_frame_set_step(&frame, 0U, spikes, 3U, &error) ||
        error != MINISNN_ACTION_DECODER_ERROR_DUPLICATE_CAPTURED_STEP ||
        minisnn_neural_activity_frame_set_step(&frame, 1U, invalid, 3U, &error) ||
        error != MINISNN_ACTION_DECODER_ERROR_INVALID_SPIKE_VALUE ||
        minisnn_neural_activity_frame_is_complete(&frame, &error) ||
        error != MINISNN_ACTION_DECODER_ERROR_INCOMPLETE_ACTIVITY_FRAME ||
        !minisnn_neural_activity_frame_set_step(&frame, 1U, spikes, 3U, &error) ||
        !minisnn_neural_activity_frame_copy(&copy, &frame, &error) ||
        !minisnn_neural_activity_frame_get_spike(&copy, 1U, 1U, &spike, &error) ||
        spike != 1U || !minisnn_neural_activity_frame_reset(&copy, 99U, &error) ||
        copy.tick != 99U || copy.captured_steps[0] != 0U)
    {
        minisnn_neural_activity_frame_destroy(&frame);
        minisnn_neural_activity_frame_destroy(&copy);
        return fail("activity frame");
    }
    minisnn_neural_activity_frame_destroy(&frame);
    minisnn_neural_activity_frame_destroy(&copy);
    return 1;
}

static int test_modes_atomicity_and_signatures(void)
{
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNActionDecoderError error = MINISNN_ACTION_DECODER_ERROR_NONE;
    MiniSNNActionSchema *schema = make_schema("", &io_error);
    MiniSNNActionSchema *renamed = make_schema("_renamed", &io_error);
    MiniSNNActionDecodingSpec mappings[5];
    MiniSNNActionDecodingSpec changed[5];
    MiniSNNActionDecoder *decoder = NULL;
    MiniSNNActionDecoder *same = NULL;
    MiniSNNActionDecoder *loaded = NULL;
    MiniSNNActionDecoder *name_independent = NULL;
    MiniSNNActionDecoder *changed_decoder = NULL;
    MiniSNNNeuralActivityFrame activity = {0};
    MiniSNNActionFrame action = {0};
    MiniSNNActionFrame previous = {0};
    MiniSNNActionDecodingDiagnostics diagnostics = {0};
    uint64_t known_signature;

    make_mappings(mappings);
    memcpy(changed, mappings, sizeof(changed));
    changed[0].maximum_rate = 0.9;
    if (schema == NULL || renamed == NULL ||
        !minisnn_neural_activity_frame_init(&activity, 6U, 4U, &error) ||
        !minisnn_action_frame_init(&action, 5U) ||
        !minisnn_action_frame_init(&previous, 5U) ||
        !minisnn_action_decoding_diagnostics_init(&diagnostics, 5U))
        goto failure;
    decoder = minisnn_action_decoder_create(schema, mappings, 5U, 6U, 4U, &error);
    same = minisnn_action_decoder_create(schema, mappings, 5U, 6U, 4U, &error);
    name_independent = minisnn_action_decoder_create(renamed, mappings, 5U, 6U, 4U,
                                                       &error);
    if (decoder == NULL || same == NULL || name_independent == NULL ||
        !set_pattern(&activity, 0) ||
        !minisnn_action_decoder_decode(decoder, &activity, &action, &diagnostics) ||
        action.tick != 42U || !near(action.values[0], 0.5) ||
        !near(action.values[1], 1.0) || !near(action.values[2], 1.0) ||
        !near(action.values[3], 1.0) || !near(action.values[4], 0.0) ||
        diagnostics.selected[3] != 1U || diagnostics.selected[4] != 0U ||
        !near(diagnostics.raw_scores[0], 0.5) ||
        minisnn_action_decoding_mapping_signature(decoder) !=
            minisnn_action_decoding_mapping_signature(same) ||
        minisnn_action_decoding_mapping_signature(decoder) ==
            minisnn_action_decoding_mapping_signature(name_independent))
        goto failure;
    changed_decoder = minisnn_action_decoder_create(schema, changed, 5U, 6U, 4U, &error);
    if (changed_decoder == NULL || minisnn_action_decoding_mapping_signature(decoder) ==
                                   minisnn_action_decoding_mapping_signature(changed_decoder))
        goto failure;
    known_signature = minisnn_action_decoding_mapping_signature(decoder);
    if (known_signature != UINT64_C(1630198257262049785))
    {
        fprintf(stderr, "unexpected known action signature: %llu\n",
                (unsigned long long)known_signature);
        goto failure;
    }
    if (!minisnn_action_frame_set_values(&previous, action.tick, action.values, 5U,
                                         &io_error))
        goto failure;
    if (!minisnn_neural_activity_frame_reset(&activity, 43U, &error))
        goto failure;
    if (minisnn_action_decoder_decode(decoder, &activity, &action, &diagnostics) ||
        minisnn_action_decoder_last_error(decoder) !=
            MINISNN_ACTION_DECODER_ERROR_INCOMPLETE_ACTIVITY_FRAME)
        goto failure;
    if (action.tick != previous.tick || memcmp(action.values, previous.values,
                                                5U * sizeof(*action.values)) != 0)
        goto failure;
    if (!set_pattern(&activity, 1) ||
        !minisnn_action_decoder_decode(decoder, &activity, &action, &diagnostics))
        goto failure;
    if (!near(action.values[3], 1.0) || !near(action.values[4], 0.0))
    {
        goto failure;
    }
    if (!minisnn_action_decoder_write_file(decoder, DECODER_FILE, &error))
        goto failure;
    loaded = minisnn_action_decoder_read_file(DECODER_FILE, schema, &error);
    if (loaded == NULL || minisnn_action_decoding_mapping_signature(loaded) !=
                              minisnn_action_decoding_mapping_signature(decoder))
        goto failure;
    remove(DECODER_FILE);
    minisnn_action_decoder_destroy(&decoder);
    minisnn_action_decoder_destroy(&same);
    minisnn_action_decoder_destroy(&loaded);
    minisnn_action_decoder_destroy(&name_independent);
    minisnn_action_decoder_destroy(&changed_decoder);
    minisnn_neural_activity_frame_destroy(&activity);
    minisnn_action_frame_destroy(&action);
    minisnn_action_frame_destroy(&previous);
    minisnn_action_decoding_diagnostics_destroy(&diagnostics);
    minisnn_action_schema_destroy(&schema);
    minisnn_action_schema_destroy(&renamed);
    return 1;

failure:
    minisnn_action_decoder_destroy(&decoder);
    minisnn_action_decoder_destroy(&same);
    minisnn_action_decoder_destroy(&loaded);
    minisnn_action_decoder_destroy(&name_independent);
    minisnn_action_decoder_destroy(&changed_decoder);
    minisnn_neural_activity_frame_destroy(&activity);
    minisnn_action_frame_destroy(&action);
    minisnn_action_frame_destroy(&previous);
    minisnn_action_decoding_diagnostics_destroy(&diagnostics);
    minisnn_action_schema_destroy(&schema);
    minisnn_action_schema_destroy(&renamed);
    remove(DECODER_FILE);
    return fail("modes, atomicidade ou assinaturas");
}

static int test_defaults_agent_io_and_capture(void)
{
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNActionDecoderError error = MINISNN_ACTION_DECODER_ERROR_NONE;
    MiniSNNActionSchema *schema = make_schema("", &io_error);
    MiniSNNSensorChannelSpec sensor_channels[] = {{1U, "input", 0.0, 1.0, 0.0}};
    MiniSNNSensorSchema *sensor_schema = minisnn_sensor_schema_create(sensor_channels, 1U,
                                                                        &io_error);
    MiniSNNActionDecodingSpec mappings[5];
    MiniSNNActionDecoder *decoder = NULL;
    MiniSNNAgentIOContext *context = NULL;
    MiniSNNNeuralActivityFrame activity = {0};
    MiniSNNActionFrame action = {0};
    MiniSNNSensorFrame sensor = {0};
    MiniSNN *network = NULL;

    make_mappings(mappings);
    if (schema == NULL || sensor_schema == NULL ||
        !minisnn_neural_activity_frame_init(&activity, 6U, 4U, &error) ||
        !minisnn_action_frame_init(&action, 5U) ||
        !minisnn_sensor_frame_init(&sensor, 1U))
        goto failure;
    decoder = minisnn_action_decoder_create(schema, mappings, 5U, 6U, 4U, &error);
    context = minisnn_agent_io_create(sensor_schema, schema, &io_error);
    if (decoder == NULL || context == NULL || !set_pattern(&activity, 0))
        goto failure;
    for (size_t index = 0U; index < 24U; index++)
        activity.spikes[index] = 0U;
    if (!minisnn_action_decoder_decode(decoder, &activity, &action, NULL) ||
        !near(action.values[0], 0.0) || !near(action.values[1], -0.25) ||
        !near(action.values[2], 0.0) || !near(action.values[3], 0.5) ||
        !near(action.values[4], 0.5))
        goto failure;
    sensor.values[0] = 0.0;
    sensor.tick = activity.tick;
    if (minisnn_action_decoder_decode_to_agent_io(decoder, &activity, context, NULL) ||
        minisnn_action_decoder_last_error(decoder) !=
            MINISNN_ACTION_DECODER_ERROR_ACTION_FRAME_REJECTED)
        goto failure;
    if (!minisnn_agent_io_submit_sensor_frame(context, &sensor) ||
        !minisnn_agent_io_consume_sensor_frame(context, &sensor) ||
        !minisnn_action_decoder_decode_to_agent_io(decoder, &activity, context, NULL) ||
        !minisnn_agent_io_finish_tick(context) ||
        !minisnn_agent_io_consume_action_frame(context, &action))
        goto failure;

    for (MiniSNNNeuronModel model = MINISNN_NEURON_MODEL_LIF;
         model <= MINISNN_NEURON_MODEL_HODGKIN_HUXLEY; model++)
    {
        MiniSNNConfig config = minisnn_default_config();
        int spike = 0;
        config.neuron_count = 6;
        config.neuron_model = model;
        network = minisnn_create_with_config(&config);
        if (network == NULL || !minisnn_set_input(network, 0, 20.0) ||
            minisnn_step(network) < 0 ||
            !minisnn_neural_activity_frame_reset(&activity, (uint64_t)model, &error) ||
            !minisnn_neural_activity_frame_capture_step(&activity, 0U, network, &error) ||
            !minisnn_get_spike(network, 0, &spike) || (spike != 0 && spike != 1))
            goto failure;
        minisnn_destroy(&network);
    }
    minisnn_destroy(&network);
    minisnn_action_decoder_destroy(&decoder);
    minisnn_agent_io_destroy(&context);
    minisnn_neural_activity_frame_destroy(&activity);
    minisnn_action_frame_destroy(&action);
    minisnn_sensor_frame_destroy(&sensor);
    minisnn_action_schema_destroy(&schema);
    minisnn_sensor_schema_destroy(&sensor_schema);
    return 1;

failure:
    minisnn_destroy(&network);
    minisnn_action_decoder_destroy(&decoder);
    minisnn_agent_io_destroy(&context);
    minisnn_neural_activity_frame_destroy(&activity);
    minisnn_action_frame_destroy(&action);
    minisnn_sensor_frame_destroy(&sensor);
    minisnn_action_schema_destroy(&schema);
    minisnn_sensor_schema_destroy(&sensor_schema);
    return fail("defaults, AgentIO ou captura publica");
}

static int expect_creation_error(const MiniSNNActionSchema *schema,
                                 const MiniSNNActionDecodingSpec *mappings,
                                 uint32_t mapping_count, uint32_t neuron_count,
                                 uint32_t brain_steps,
                                 MiniSNNActionDecoderError expected)
{
    MiniSNNActionDecoderError error = MINISNN_ACTION_DECODER_ERROR_NONE;
    MiniSNNActionDecoder *decoder = minisnn_action_decoder_create(
        schema, mappings, mapping_count, neuron_count, brain_steps, &error);

    if (decoder != NULL || error != expected)
    {
        minisnn_action_decoder_destroy(&decoder);
        return 0;
    }
    return 1;
}

static int test_creation_contracts_and_signatures(void)
{
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNActionDecoderError error = MINISNN_ACTION_DECODER_ERROR_NONE;
    MiniSNNActionSchema *schema = make_schema("", &io_error);
    MiniSNNActionDecodingSpec mappings[5];
    MiniSNNActionDecodingSpec altered[5];
    MiniSNNActionDecoder *base = NULL;
    MiniSNNActionDecoder *inactive_changed = NULL;
    MiniSNNActionDecoder *active_changed = NULL;

    if (schema == NULL)
        goto failure;
    make_mappings(mappings);
    memcpy(altered, mappings, sizeof(altered));
    altered[0].action_channel_id = 999U;
    if (!expect_creation_error(schema, altered, 5U, 6U, 4U,
                               MINISNN_ACTION_DECODER_ERROR_UNKNOWN_ACTION_CHANNEL))
        goto failure;
    memcpy(altered, mappings, sizeof(altered));
    altered[0].primary_neuron_start = 6U;
    if (!expect_creation_error(schema, altered, 5U, 6U, 4U,
                               MINISNN_ACTION_DECODER_ERROR_INVALID_NEURON_RANGE))
        goto failure;
    memcpy(altered, mappings, sizeof(altered));
    altered[1].primary_neuron_start = 0U;
    if (!expect_creation_error(schema, altered, 5U, 6U, 4U,
                               MINISNN_ACTION_DECODER_ERROR_OVERLAPPING_NEURON_RANGE))
        goto failure;
    memcpy(altered, mappings, sizeof(altered));
    altered[1].action_channel_id = mappings[0].action_channel_id;
    if (!expect_creation_error(schema, altered, 5U, 6U, 4U,
                               MINISNN_ACTION_DECODER_ERROR_INVALID_PARAMETER))
        goto failure;
    if (!expect_creation_error(schema, &mappings[3], 1U, 6U, 4U,
                               MINISNN_ACTION_DECODER_ERROR_INVALID_PARAMETER))
        goto failure;

    base = minisnn_action_decoder_create(schema, mappings, 5U, 6U, 4U, &error);
    memcpy(altered, mappings, sizeof(altered));
    altered[0].threshold = 0.123;
    inactive_changed = minisnn_action_decoder_create(schema, altered, 5U, 6U, 4U,
                                                      &error);
    memcpy(altered, mappings, sizeof(altered));
    altered[0].minimum_rate = 0.1;
    active_changed = minisnn_action_decoder_create(schema, altered, 5U, 6U, 4U,
                                                    &error);
    if (base == NULL || inactive_changed == NULL || active_changed == NULL ||
        minisnn_action_decoding_mapping_signature(base) !=
            minisnn_action_decoding_mapping_signature(inactive_changed) ||
        minisnn_action_decoding_mapping_signature(base) ==
            minisnn_action_decoding_mapping_signature(active_changed))
        goto failure;

    minisnn_action_decoder_destroy(&base);
    minisnn_action_decoder_destroy(&inactive_changed);
    minisnn_action_decoder_destroy(&active_changed);
    minisnn_action_schema_destroy(&schema);
    return 1;

failure:
    minisnn_action_decoder_destroy(&base);
    minisnn_action_decoder_destroy(&inactive_changed);
    minisnn_action_decoder_destroy(&active_changed);
    minisnn_action_schema_destroy(&schema);
    return fail("contratos de criacao ou assinatura");
}

static int test_wta_contracts(void)
{
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNActionDecoderError error = MINISNN_ACTION_DECODER_ERROR_NONE;
    MiniSNNActionSchema *schema = make_schema("", &io_error);
    MiniSNNActionDecodingSpec independent[4] = {{0}};
    MiniSNNActionDecodingSpec guarded[2] = {{0}};
    MiniSNNActionDecoder *independent_decoder = NULL;
    MiniSNNActionDecoder *guarded_decoder = NULL;
    MiniSNNNeuralActivityFrame activity = {0};
    MiniSNNActionFrame action = {0};
    const uint8_t independent_spikes[] = {1U, 0U, 0U, 1U};
    const uint8_t activation_first[] = {1U, 0U};
    const uint8_t activation_second[] = {0U, 0U};
    const uint8_t confidence_second[] = {0U, 1U};

    if (schema == NULL || !minisnn_action_frame_init(&action, 5U) ||
        !minisnn_neural_activity_frame_init(&activity, 4U, 1U, &error))
        goto failure;
    for (uint32_t index = 0U; index < 4U; index++)
    {
        independent[index].action_channel_id = (index + 1U) * 10U;
        independent[index].mode = MINISNN_ACTION_DECODING_WTA_MEMBER;
        independent[index].primary_neuron_start = index;
        independent[index].primary_neuron_count = 1U;
        independent[index].competition_group_id = index < 2U ? 1U : 2U;
        independent[index].minimum_activation = 0.0;
        independent[index].minimum_confidence = 0.0;
        independent[index].winner_value = 1.0;
        independent[index].loser_value = 0.0;
    }
    independent_decoder = minisnn_action_decoder_create(schema, independent, 4U, 4U, 1U,
                                                         &error);
    if (independent_decoder == NULL ||
        !minisnn_neural_activity_frame_set_step(&activity, 0U, independent_spikes, 4U,
                                                 &error) ||
        !minisnn_action_decoder_decode(independent_decoder, &activity, &action, NULL) ||
        !near(action.values[0], 1.0) || !near(action.values[1], 0.0) ||
        !near(action.values[2], 0.0) || !near(action.values[3], 1.0) ||
        !near(action.values[4], 0.5))
        goto failure;
    minisnn_action_decoder_destroy(&independent_decoder);
    minisnn_neural_activity_frame_destroy(&activity);

    for (uint32_t index = 0U; index < 2U; index++)
    {
        guarded[index].action_channel_id = (index + 1U) * 10U;
        guarded[index].mode = MINISNN_ACTION_DECODING_WTA_MEMBER;
        guarded[index].primary_neuron_start = index;
        guarded[index].primary_neuron_count = 1U;
        guarded[index].competition_group_id = 9U;
        guarded[index].minimum_activation = 0.75;
        guarded[index].minimum_confidence = 0.0;
        guarded[index].winner_value = 1.0;
        guarded[index].loser_value = 0.0;
    }
    if (!minisnn_neural_activity_frame_init(&activity, 2U, 2U, &error))
        goto failure;
    guarded_decoder = minisnn_action_decoder_create(schema, guarded, 2U, 2U, 2U, &error);
    if (guarded_decoder == NULL ||
        !minisnn_neural_activity_frame_set_step(&activity, 0U, activation_first, 2U,
                                                 &error) ||
        !minisnn_neural_activity_frame_set_step(&activity, 1U, activation_second, 2U,
                                                 &error) ||
        !minisnn_action_decoder_decode(guarded_decoder, &activity, &action, NULL) ||
        !near(action.values[0], 0.25) || !near(action.values[1], -0.25))
        goto failure;
    minisnn_action_decoder_destroy(&guarded_decoder);
    if (!minisnn_neural_activity_frame_reset(&activity, 1U, &error))
        goto failure;
    guarded[0].minimum_activation = 0.0;
    guarded[1].minimum_activation = 0.0;
    guarded[0].minimum_confidence = 0.1;
    guarded[1].minimum_confidence = 0.1;
    guarded_decoder = minisnn_action_decoder_create(schema, guarded, 2U, 2U, 2U, &error);
    if (guarded_decoder == NULL ||
        !minisnn_neural_activity_frame_set_step(&activity, 0U, activation_first, 2U,
                                                 &error) ||
        !minisnn_neural_activity_frame_set_step(&activity, 1U, confidence_second, 2U,
                                                 &error) ||
        !minisnn_action_decoder_decode(guarded_decoder, &activity, &action, NULL) ||
        !near(action.values[0], 0.25) || !near(action.values[1], -0.25))
        goto failure;

    minisnn_action_decoder_destroy(&independent_decoder);
    minisnn_action_decoder_destroy(&guarded_decoder);
    minisnn_neural_activity_frame_destroy(&activity);
    minisnn_action_frame_destroy(&action);
    minisnn_action_schema_destroy(&schema);
    return 1;

failure:
    minisnn_action_decoder_destroy(&independent_decoder);
    minisnn_action_decoder_destroy(&guarded_decoder);
    minisnn_neural_activity_frame_destroy(&activity);
    minisnn_action_frame_destroy(&action);
    minisnn_action_schema_destroy(&schema);
    return fail("contratos WTA");
}

static int test_agent_io_schema_signature_contract(void)
{
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNActionDecoderError error = MINISNN_ACTION_DECODER_ERROR_NONE;
    const MiniSNNSensorChannelSpec sensor_channels[] = {{1U, "input", 0.0, 1.0, 0.0}};
    const double sensor_values[] = {0.0};
    const uint8_t spikes[] = {1U, 1U};
    MiniSNNActionDecodingSpec mappings[2] = {{0}};
    MiniSNNSensorSchema *sensor_schema = NULL;
    MiniSNNActionSchema *decoder_schema = NULL;
    MiniSNNActionSchema *equivalent_schema = NULL;
    MiniSNNActionSchema *reversed_schema = NULL;
    MiniSNNActionDecoder *decoder = NULL;
    MiniSNNAgentIOContext *compatible = NULL;
    MiniSNNAgentIOContext *incompatible = NULL;
    MiniSNNNeuralActivityFrame activity = {0};
    MiniSNNActionFrame action = {0};
    MiniSNNSensorFrame sensor = {0};
    MiniSNNActionDecodingDiagnostics diagnostics = {0};
    MiniSNNActionDecodingDiagnostics previous = {0};

    mappings[0].action_channel_id = 10U;
    mappings[0].mode = MINISNN_ACTION_DECODING_POPULATION_RATE;
    mappings[0].primary_neuron_count = 1U;
    mappings[0].minimum_rate = 0.0;
    mappings[0].maximum_rate = 1.0;
    mappings[1].action_channel_id = 20U;
    mappings[1].mode = MINISNN_ACTION_DECODING_THRESHOLD;
    mappings[1].primary_neuron_start = 1U;
    mappings[1].primary_neuron_count = 1U;
    mappings[1].threshold = 0.5;
    mappings[1].active_value = 1.0;
    mappings[1].inactive_value = 0.0;
    sensor_schema = minisnn_sensor_schema_create(sensor_channels, 1U, &io_error);
    decoder_schema = make_two_channel_schema(0, &io_error);
    equivalent_schema = make_two_channel_schema(0, &io_error);
    reversed_schema = make_two_channel_schema(1, &io_error);
    if (sensor_schema == NULL || decoder_schema == NULL || equivalent_schema == NULL ||
        reversed_schema == NULL || !minisnn_neural_activity_frame_init(&activity, 2U, 1U,
                                                                         &error) ||
        !minisnn_action_frame_init(&action, 2U) || !minisnn_sensor_frame_init(&sensor, 1U) ||
        !minisnn_action_decoding_diagnostics_init(&diagnostics, 2U) ||
        !minisnn_action_decoding_diagnostics_init(&previous, 2U))
        goto failure;
    decoder = minisnn_action_decoder_create(decoder_schema, mappings, 2U, 2U, 1U, &error);
    compatible = minisnn_agent_io_create(sensor_schema, equivalent_schema, &io_error);
    incompatible = minisnn_agent_io_create(sensor_schema, reversed_schema, &io_error);
    if (decoder == NULL || compatible == NULL || incompatible == NULL ||
        minisnn_agent_io_action_schema_signature(NULL) != 0U ||
        minisnn_agent_io_action_schema_signature(compatible) !=
            minisnn_action_schema_signature(decoder_schema) ||
        minisnn_agent_io_action_schema_signature(incompatible) ==
            minisnn_action_schema_signature(decoder_schema) ||
        !minisnn_neural_activity_frame_set_step(&activity, 0U, spikes, 2U, &error) ||
        !minisnn_action_decoder_decode(decoder, &activity, &action, &diagnostics) ||
        !diagnostics_copy(&previous, &diagnostics) ||
        !minisnn_sensor_frame_set_values(&sensor, activity.tick, sensor_values, 1U,
                                         &io_error) ||
        !minisnn_agent_io_submit_sensor_frame(incompatible, &sensor) ||
        !minisnn_agent_io_consume_sensor_frame(incompatible, &sensor))
        goto failure;
    if (minisnn_action_decoder_decode_to_agent_io(decoder, &activity, incompatible,
                                                   &diagnostics) ||
        minisnn_action_decoder_last_error(decoder) !=
            MINISNN_ACTION_DECODER_ERROR_SIGNATURE_MISMATCH ||
        !diagnostics_equal(&diagnostics, &previous) ||
        minisnn_agent_io_last_error(incompatible) != MINISNN_AGENT_IO_ERROR_NONE ||
        minisnn_agent_io_finish_tick(incompatible))
        goto failure;
    if (!minisnn_agent_io_submit_sensor_frame(compatible, &sensor) ||
        !minisnn_agent_io_consume_sensor_frame(compatible, &sensor) ||
        !minisnn_action_decoder_decode_to_agent_io(decoder, &activity, compatible, NULL) ||
        !minisnn_agent_io_finish_tick(compatible) ||
        !minisnn_agent_io_consume_action_frame(compatible, &action))
        goto failure;

    minisnn_action_decoding_diagnostics_destroy(&previous);
    minisnn_action_decoding_diagnostics_destroy(&diagnostics);
    minisnn_sensor_frame_destroy(&sensor);
    minisnn_action_frame_destroy(&action);
    minisnn_neural_activity_frame_destroy(&activity);
    minisnn_agent_io_destroy(&compatible);
    minisnn_agent_io_destroy(&incompatible);
    minisnn_action_decoder_destroy(&decoder);
    minisnn_action_schema_destroy(&decoder_schema);
    minisnn_action_schema_destroy(&equivalent_schema);
    minisnn_action_schema_destroy(&reversed_schema);
    minisnn_sensor_schema_destroy(&sensor_schema);
    return 1;

failure:
    minisnn_action_decoding_diagnostics_destroy(&previous);
    minisnn_action_decoding_diagnostics_destroy(&diagnostics);
    minisnn_sensor_frame_destroy(&sensor);
    minisnn_action_frame_destroy(&action);
    minisnn_neural_activity_frame_destroy(&activity);
    minisnn_agent_io_destroy(&compatible);
    minisnn_agent_io_destroy(&incompatible);
    minisnn_action_decoder_destroy(&decoder);
    minisnn_action_schema_destroy(&decoder_schema);
    minisnn_action_schema_destroy(&equivalent_schema);
    minisnn_action_schema_destroy(&reversed_schema);
    minisnn_sensor_schema_destroy(&sensor_schema);
    return fail("assinatura de schema AgentIO");
}

static int tamper_mapping_signature_file(void)
{
    FILE *file = fopen(DECODER_FILE, "r+b");
    char line[256];
    const char *prefix = "mapping_signature=";

    if (file == NULL)
        return 0;
    while (fgets(line, (int)sizeof(line), file) != NULL)
    {
        if (strncmp(line, prefix, strlen(prefix)) == 0)
        {
            long offset = ftell(file) - (long)strlen(line) + (long)strlen(prefix);
            int replacement = line[strlen(prefix)] == '0' ? '1' : '0';
            if (fseek(file, offset, SEEK_SET) != 0 || fputc(replacement, file) == EOF)
            {
                fclose(file);
                return 0;
            }
            return fclose(file) == 0;
        }
    }
    fclose(file);
    return 0;
}

static int overwrite_decoder_file(const char *contents)
{
    FILE *file = fopen(DECODER_FILE, "wb");
    int write_ok;

    if (file == NULL)
        return 0;
    write_ok = fputs(contents, file) != EOF;
    return fclose(file) == 0 && write_ok;
}

static int test_file_rejections(void)
{
    MiniSNNAgentIOError io_error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNActionDecoderError error = MINISNN_ACTION_DECODER_ERROR_NONE;
    MiniSNNActionSchema *schema = make_schema("", &io_error);
    MiniSNNActionDecodingSpec mappings[5];
    MiniSNNActionDecoder *decoder = NULL;
    MiniSNNActionDecoder *loaded = NULL;

    make_mappings(mappings);
    decoder = minisnn_action_decoder_create(schema, mappings, 5U, 6U, 4U, &error);
    if (schema == NULL || decoder == NULL ||
        !minisnn_action_decoder_write_file(decoder, DECODER_FILE, &error) ||
        !tamper_mapping_signature_file())
        goto failure;
    loaded = minisnn_action_decoder_read_file(DECODER_FILE, schema, &error);
    if (loaded != NULL || error != MINISNN_ACTION_DECODER_ERROR_SIGNATURE_MISMATCH)
        goto failure;
    if (!overwrite_decoder_file("not_a_decoder\n"))
        goto failure;
    loaded = minisnn_action_decoder_read_file(DECODER_FILE, schema, &error);
    if (loaded != NULL || error != MINISNN_ACTION_DECODER_ERROR_FORMAT)
        goto failure;
    if (!overwrite_decoder_file("minisnn_action_decoder_v1\n"))
        goto failure;
    loaded = minisnn_action_decoder_read_file(DECODER_FILE, schema, &error);
    if (loaded != NULL || error != MINISNN_ACTION_DECODER_ERROR_FORMAT)
        goto failure;

    minisnn_action_decoder_destroy(&decoder);
    minisnn_action_schema_destroy(&schema);
    remove(DECODER_FILE);
    return 1;

failure:
    minisnn_action_decoder_destroy(&loaded);
    minisnn_action_decoder_destroy(&decoder);
    minisnn_action_schema_destroy(&schema);
    remove(DECODER_FILE);
    return fail("rejeicao de arquivo");
}

int main(void)
{
    if (!test_activity_frame() || !test_modes_atomicity_and_signatures() ||
        !test_defaults_agent_io_and_capture() ||
        !test_creation_contracts_and_signatures() || !test_wta_contracts() ||
        !test_agent_io_schema_signature_contract() || !test_file_rejections())
        return 1;
    printf("Action decoder validation OK\n");
    return 0;
}
