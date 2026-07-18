#include <math.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minisnn_agent_io.h"

#define SENSOR_FILE "build/test_agent_io_sensor_schema.txt"
#define SENSOR_FILE_2 "build/test_agent_io_sensor_schema_2.txt"
#define ACTION_FILE "build/test_agent_io_action_schema.txt"

static int fail(const char *message)
{
    remove(SENSOR_FILE);
    remove(SENSOR_FILE_2);
    remove(ACTION_FILE);
    printf("FAIL: %s\n", message);
    return 0;
}

static MiniSNNSensorSchema *create_sensor_schema(MiniSNNAgentIOError *error)
{
    MiniSNNSensorChannelSpec channels[] =
    {
        {10U, "signal one", 0.0, 1.0, 0.25},
        {20U, "signal_two", -2.0, 2.0, 0.0}
    };

    return minisnn_sensor_schema_create(channels, 2U, error);
}

static MiniSNNActionSchema *create_action_schema(MiniSNNAgentIOError *error)
{
    MiniSNNActionChannelSpec channels[] =
    {
        {30U, "command_a", -1.0, 1.0, 0.0},
        {40U, "command_b", 0.0, 4.0, 2.0}
    };

    return minisnn_action_schema_create(channels, 2U, error);
}

static int set_sensor(MiniSNNSensorFrame *frame, uint64_t tick,
                      double first, double second)
{
    const double values[] = {first, second};
    MiniSNNAgentIOError error = MINISNN_AGENT_IO_ERROR_NONE;

    return minisnn_sensor_frame_set_values(frame, tick, values, 2U, &error);
}

static int set_action(MiniSNNActionFrame *frame, uint64_t tick,
                      double first, double second)
{
    const double values[] = {first, second};
    MiniSNNAgentIOError error = MINISNN_AGENT_IO_ERROR_NONE;

    return minisnn_action_frame_set_values(frame, tick, values, 2U, &error);
}

static int test_schema_contracts(void)
{
    MiniSNNAgentIOError error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorSchema *sensor = NULL;
    MiniSNNSensorSchema *same_sensor = NULL;
    MiniSNNActionSchema *action = NULL;
    MiniSNNSensorChannelSpec channel;
    MiniSNNSensorChannelSpec invalid[] = {{1U, "x", 0.0, 1.0, 0.5}};
    char *external_name;

    external_name = malloc(32U);
    if (external_name == NULL)
        return fail("nao foi possivel preparar nome externo");
    strcpy(external_name, "external_copy");
    invalid[0].name = external_name;
    sensor = minisnn_sensor_schema_create(invalid, 1U, &error);
    free(external_name);
    if (sensor == NULL || !minisnn_sensor_schema_get_channel(sensor, 0U,
                                                              &channel) ||
        strcmp(channel.name, "external_copy") != 0)
    {
        minisnn_sensor_schema_destroy(&sensor);
        return fail("schema nao copiou o nome externo");
    }
    minisnn_sensor_schema_destroy(&sensor);

    sensor = create_sensor_schema(&error);
    same_sensor = create_sensor_schema(&error);
    action = create_action_schema(&error);
    if (sensor == NULL || same_sensor == NULL || action == NULL ||
        minisnn_sensor_schema_channel_count(sensor) != 2U ||
        minisnn_action_schema_channel_count(action) != 2U ||
        minisnn_sensor_schema_signature(sensor) == 0U ||
        minisnn_sensor_schema_signature(sensor) !=
            UINT64_C(12815672321792322842) ||
        minisnn_sensor_schema_signature(sensor) !=
            minisnn_sensor_schema_signature(same_sensor) ||
        !minisnn_sensor_schema_get_channel(sensor, 0U, &channel) ||
        channel.id != 10U || strcmp(channel.name, "signal one") != 0)
    {
        minisnn_sensor_schema_destroy(&sensor);
        minisnn_sensor_schema_destroy(&same_sensor);
        minisnn_action_schema_destroy(&action);
        return fail("schema valido ou assinatura deterministica incorretos");
    }
    minisnn_sensor_schema_destroy(&sensor);
    minisnn_sensor_schema_destroy(&same_sensor);
    minisnn_action_schema_destroy(&action);

    for (int repeat = 0; repeat < 3; repeat++)
    {
        sensor = create_sensor_schema(&error);
        action = create_action_schema(&error);
        if (sensor == NULL || action == NULL)
        {
            minisnn_sensor_schema_destroy(&sensor);
            minisnn_action_schema_destroy(&action);
            return fail("criacao repetida de schemas falhou");
        }
        minisnn_sensor_schema_destroy(&sensor);
        minisnn_action_schema_destroy(&action);
    }

    if (minisnn_sensor_schema_create(invalid, 0U, &error) != NULL ||
        error != MINISNN_AGENT_IO_ERROR_SCHEMA_EMPTY)
        return fail("schema vazio foi aceito");
    {
        MiniSNNSensorChannelSpec duplicate_id[] =
        {
            {1U, "a", 0.0, 1.0, 0.0},
            {1U, "b", 0.0, 1.0, 0.0}
        };
        MiniSNNSensorChannelSpec duplicate_name[] =
        {
            {1U, "same", 0.0, 1.0, 0.0},
            {2U, "same", 0.0, 1.0, 0.0}
        };
        MiniSNNSensorChannelSpec invalid_limits[] =
        {
            {1U, "bad", 2.0, 1.0, 1.0}
        };
        MiniSNNSensorChannelSpec invalid_default[] =
        {
            {1U, "bad_default", 0.0, 1.0, 2.0}
        };
        MiniSNNSensorChannelSpec invalid_nan[] =
        {
            {1U, "bad_nan", NAN, 1.0, 0.0}
        };
        MiniSNNSensorChannelSpec invalid_inf[] =
        {
            {1U, "bad_inf", 0.0, INFINITY, 0.0}
        };

        if (minisnn_sensor_schema_create(duplicate_id, 2U, &error) != NULL ||
            error != MINISNN_AGENT_IO_ERROR_CHANNEL_ID_DUPLICATE ||
            minisnn_sensor_schema_create(duplicate_name, 2U, &error) != NULL ||
            error != MINISNN_AGENT_IO_ERROR_CHANNEL_NAME_DUPLICATE ||
            minisnn_sensor_schema_create(invalid_limits, 1U, &error) != NULL ||
            error != MINISNN_AGENT_IO_ERROR_CHANNEL_LIMITS_INVALID ||
            minisnn_sensor_schema_create(invalid_default, 1U, &error) != NULL ||
            error != MINISNN_AGENT_IO_ERROR_CHANNEL_LIMITS_INVALID ||
            minisnn_sensor_schema_create(invalid_nan, 1U, &error) != NULL ||
            error != MINISNN_AGENT_IO_ERROR_NONFINITE_VALUE ||
            minisnn_sensor_schema_create(invalid_inf, 1U, &error) != NULL ||
            error != MINISNN_AGENT_IO_ERROR_NONFINITE_VALUE)
            return fail("schema invalido nao foi rejeitado corretamente");
    }
    return 1;
}

static int test_signatures_and_serialization(void)
{
    MiniSNNAgentIOError error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorSchema *sensor = create_sensor_schema(&error);
    MiniSNNActionSchema *action = create_action_schema(&error);
    MiniSNNSensorSchema *loaded_sensor = NULL;
    MiniSNNActionSchema *loaded_action = NULL;
    MiniSNNSensorChannelSpec changed[] =
    {
        {10U, "signal one", 0.0, 1.0, 0.5},
        {20U, "signal_two", -2.0, 2.0, 0.0}
    };
    MiniSNNSensorSchema *changed_sensor;
    FILE *file;

    if (sensor == NULL || action == NULL ||
        !minisnn_sensor_schema_write_file(sensor, SENSOR_FILE, &error) ||
        !minisnn_action_schema_write_file(action, ACTION_FILE, &error))
    {
        minisnn_sensor_schema_destroy(&sensor);
        minisnn_action_schema_destroy(&action);
        return fail("nao foi possivel escrever schemas");
    }
    loaded_sensor = minisnn_sensor_schema_read_file(SENSOR_FILE, &error);
    loaded_action = minisnn_action_schema_read_file(ACTION_FILE, &error);
    changed_sensor = minisnn_sensor_schema_create(changed, 2U, &error);
    if (loaded_sensor == NULL || loaded_action == NULL || changed_sensor == NULL ||
        minisnn_sensor_schema_signature(sensor) !=
            minisnn_sensor_schema_signature(loaded_sensor) ||
        minisnn_action_schema_signature(action) !=
            minisnn_action_schema_signature(loaded_action) ||
        minisnn_sensor_schema_signature(sensor) ==
            minisnn_sensor_schema_signature(changed_sensor))
    {
        minisnn_sensor_schema_destroy(&sensor);
        minisnn_action_schema_destroy(&action);
        minisnn_sensor_schema_destroy(&loaded_sensor);
        minisnn_action_schema_destroy(&loaded_action);
        minisnn_sensor_schema_destroy(&changed_sensor);
        return fail("round-trip ou mudanca de assinatura incorretos");
    }
    minisnn_sensor_schema_destroy(&changed_sensor);
    minisnn_sensor_schema_destroy(&loaded_sensor);
    minisnn_action_schema_destroy(&loaded_action);
    file = fopen(SENSOR_FILE, "wb");
    if (file == NULL)
    {
        minisnn_sensor_schema_destroy(&sensor);
        minisnn_action_schema_destroy(&action);
        return fail("nao foi possivel corromper schema temporario");
    }
    fputs("incompatible_schema_v0\nchannel_count=1\n", file);
    fclose(file);
    if (minisnn_sensor_schema_read_file(SENSOR_FILE, &error) != NULL ||
        error != MINISNN_AGENT_IO_ERROR_FORMAT)
    {
        minisnn_sensor_schema_destroy(&sensor);
        minisnn_action_schema_destroy(&action);
        return fail("arquivo incompativel foi aceito");
    }
    minisnn_sensor_schema_destroy(&sensor);
    minisnn_action_schema_destroy(&action);
    remove(SENSOR_FILE);
    remove(ACTION_FILE);
    return 1;
}

static int files_match(const char *first_name, const char *second_name)
{
    FILE *first = fopen(first_name, "rb");
    FILE *second = fopen(second_name, "rb");
    int first_byte;
    int second_byte;

    if (first == NULL || second == NULL)
    {
        if (first != NULL)
            fclose(first);
        if (second != NULL)
            fclose(second);
        return 0;
    }
    do
    {
        first_byte = fgetc(first);
        second_byte = fgetc(second);
        if (first_byte != second_byte)
        {
            fclose(first);
            fclose(second);
            return 0;
        }
    } while (first_byte != EOF);
    fclose(first);
    fclose(second);
    return 1;
}

static int test_ascii_names_and_reader_errors(void)
{
    MiniSNNAgentIOError error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorChannelSpec allowed[] =
    {
        {1U, "space % pipe|", -1.0, 1.0, 0.0}
    };
    MiniSNNSensorChannelSpec control[] =
    {
        {1U, "bad\x01", -1.0, 1.0, 0.0}
    };
    MiniSNNSensorChannelSpec del[] =
    {
        {1U, "bad\x7f", -1.0, 1.0, 0.0}
    };
    MiniSNNSensorChannelSpec extended[] =
    {
        {1U, "bad\x80", -1.0, 1.0, 0.0}
    };
    MiniSNNSensorSchema *schema = NULL;
    MiniSNNSensorSchema *loaded = NULL;
    MiniSNNSensorChannelSpec channel;
    FILE *file;

    if (minisnn_sensor_schema_create(control, 1U, &error) != NULL ||
        error != MINISNN_AGENT_IO_ERROR_CHANNEL_NAME_INVALID ||
        minisnn_sensor_schema_create(del, 1U, &error) != NULL ||
        error != MINISNN_AGENT_IO_ERROR_CHANNEL_NAME_INVALID ||
        minisnn_sensor_schema_create(extended, 1U, &error) != NULL ||
        error != MINISNN_AGENT_IO_ERROR_CHANNEL_NAME_INVALID)
        return fail("nomes fora do contrato ASCII foram aceitos");

    setlocale(LC_CTYPE, "");
    schema = minisnn_sensor_schema_create(allowed, 1U, &error);
    if (schema == NULL ||
        !minisnn_sensor_schema_write_file(schema, SENSOR_FILE, &error))
    {
        minisnn_sensor_schema_destroy(&schema);
        return fail("nao foi possivel serializar nome ASCII permitido");
    }
    setlocale(LC_CTYPE, "C");
    if (!minisnn_sensor_schema_write_file(schema, SENSOR_FILE_2, &error) ||
        !files_match(SENSOR_FILE, SENSOR_FILE_2))
    {
        minisnn_sensor_schema_destroy(&schema);
        return fail("serializacao dependeu do locale ou foi instavel");
    }
    file = fopen(SENSOR_FILE, "rb");
    if (file == NULL)
    {
        minisnn_sensor_schema_destroy(&schema);
        return fail("nao foi possivel reler schema ASCII");
    }
    {
        char contents[512] = {0};
        size_t length = fread(contents, 1U, sizeof(contents) - 1U, file);

        fclose(file);
        if (length == 0U || strstr(contents, "space%20%25%20pipe%7C") == NULL)
        {
            minisnn_sensor_schema_destroy(&schema);
            return fail("nome ASCII nao foi percent-encoded corretamente");
        }
    }
    loaded = minisnn_sensor_schema_read_file(SENSOR_FILE, &error);
    if (loaded == NULL || !minisnn_sensor_schema_get_channel(loaded, 0U,
                                                               &channel) ||
        strcmp(channel.name, allowed[0].name) != 0)
    {
        minisnn_sensor_schema_destroy(&schema);
        minisnn_sensor_schema_destroy(&loaded);
        return fail("round-trip do nome ASCII falhou");
    }
    minisnn_sensor_schema_destroy(&loaded);

    file = fopen(SENSOR_FILE, "wb");
    if (file == NULL)
    {
        minisnn_sensor_schema_destroy(&schema);
        return fail("nao foi possivel criar schema malformado");
    }
    fputs("schema_malformado\n", file);
    fclose(file);
    error = MINISNN_AGENT_IO_ERROR_IO;
    loaded = minisnn_sensor_schema_read_file(SENSOR_FILE, &error);
    if (loaded != NULL || error != MINISNN_AGENT_IO_ERROR_FORMAT)
    {
        minisnn_sensor_schema_destroy(&schema);
        minisnn_sensor_schema_destroy(&loaded);
        return fail("reader nao sobrescreveu erro sujo com FORMAT");
    }
    minisnn_sensor_schema_destroy(&schema);
    remove(SENSOR_FILE);
    remove(SENSOR_FILE_2);
    return 1;
}

static int test_frame_public_errors(void)
{
    MiniSNNAgentIOError error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorSchema *sensor_schema = create_sensor_schema(&error);
    MiniSNNActionSchema *action_schema = create_action_schema(&error);
    MiniSNNSensorFrame sensor = {0};
    MiniSNNSensorFrame short_sensor = {0};
    MiniSNNSensorFrame sensor_without_buffer = {19U, 2U, NULL};
    MiniSNNActionFrame action = {0};
    MiniSNNActionFrame short_action = {0};
    MiniSNNActionFrame action_without_buffer = {19U, 2U, NULL};
    const double sensor_values[] = {0.5, -1.0};
    const double action_values[] = {0.2, 3.0};

    if (sensor_schema == NULL || action_schema == NULL ||
        !minisnn_sensor_frame_init(&sensor, 2U) ||
        !minisnn_sensor_frame_init(&short_sensor, 1U) ||
        !minisnn_action_frame_init(&action, 2U) ||
        !minisnn_action_frame_init(&short_action, 1U) ||
        !minisnn_sensor_frame_set_values(&sensor, 17U, sensor_values, 2U,
                                          &error) ||
        !minisnn_action_frame_set_values(&action, 17U, action_values, 2U,
                                          &error))
        goto failure;

    if (minisnn_sensor_frame_set_values(NULL, 18U, sensor_values, 2U,
                                         &error) ||
        error != MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT ||
        minisnn_action_frame_set_values(NULL, 18U, action_values, 2U,
                                         &error) ||
        error != MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT ||
        minisnn_sensor_frame_set_values(&sensor_without_buffer, 18U,
                                         sensor_values, 2U, &error) ||
        error != MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT ||
        minisnn_action_frame_set_values(&action_without_buffer, 18U,
                                         action_values, 2U, &error) ||
        error != MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT ||
        minisnn_sensor_frame_set_values(&sensor, 18U, NULL, 2U, &error) ||
        error != MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT ||
        minisnn_action_frame_set_values(&action, 18U, NULL, 2U, &error) ||
        error != MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT ||
        minisnn_sensor_frame_set_values(&sensor, 18U, sensor_values, 1U,
                                         &error) ||
        error != MINISNN_AGENT_IO_ERROR_VALUE_COUNT_MISMATCH ||
        minisnn_action_frame_set_values(&action, 18U, action_values, 1U,
                                         &error) ||
        error != MINISNN_AGENT_IO_ERROR_VALUE_COUNT_MISMATCH ||
        sensor.tick != 17U || sensor.values[0] != 0.5 ||
        sensor.values[1] != -1.0 || action.tick != 17U ||
        action.values[0] != 0.2 || action.values[1] != 3.0)
        goto failure;

    if (minisnn_sensor_frame_reset(NULL, sensor_schema, &error) ||
        error != MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT ||
        minisnn_action_frame_reset(NULL, action_schema, &error) ||
        error != MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT ||
        minisnn_sensor_frame_reset(&sensor, NULL, &error) ||
        error != MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT ||
        minisnn_action_frame_reset(&action, NULL, &error) ||
        error != MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT ||
        minisnn_sensor_frame_reset(&sensor_without_buffer, sensor_schema,
                                   &error) ||
        error != MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT ||
        minisnn_action_frame_reset(&action_without_buffer, action_schema,
                                   &error) ||
        error != MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT ||
        minisnn_sensor_frame_reset(&short_sensor, sensor_schema, &error) ||
        error != MINISNN_AGENT_IO_ERROR_VALUE_COUNT_MISMATCH ||
        minisnn_action_frame_reset(&short_action, action_schema, &error) ||
        error != MINISNN_AGENT_IO_ERROR_VALUE_COUNT_MISMATCH ||
        sensor.tick != 17U || sensor.values[0] != 0.5 ||
        sensor.values[1] != -1.0 || action.tick != 17U ||
        action.values[0] != 0.2 || action.values[1] != 3.0)
        goto failure;

    minisnn_sensor_schema_destroy(&sensor_schema);
    minisnn_action_schema_destroy(&action_schema);
    minisnn_sensor_frame_destroy(&sensor);
    minisnn_sensor_frame_destroy(&short_sensor);
    minisnn_action_frame_destroy(&action);
    minisnn_action_frame_destroy(&short_action);
    return 1;

failure:
    minisnn_sensor_schema_destroy(&sensor_schema);
    minisnn_action_schema_destroy(&action_schema);
    minisnn_sensor_frame_destroy(&sensor);
    minisnn_sensor_frame_destroy(&short_sensor);
    minisnn_action_frame_destroy(&action);
    minisnn_action_frame_destroy(&short_action);
    return fail("erros publicos de frame ou atomicidade invalidos");
}

static int test_frames_and_context(void)
{
    MiniSNNAgentIOError error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorSchema *sensor = create_sensor_schema(&error);
    MiniSNNActionSchema *action = create_action_schema(&error);
    MiniSNNAgentIOContext *first = NULL;
    MiniSNNAgentIOContext *second = NULL;
    MiniSNNSensorFrame sensor_frame = {0};
    MiniSNNSensorFrame short_sensor_frame = {0};
    MiniSNNActionFrame action_frame = {0};
    MiniSNNActionFrame short_action_frame = {0};
    MiniSNNSensorFrame copied_sensor = {0};
    MiniSNNActionFrame copied_action = {0};
    const double nonfinite[] = {NAN, 0.0};
    const double outside[] = {2.0, 0.0};

    if (sensor == NULL || action == NULL ||
        !minisnn_sensor_frame_init(&sensor_frame, 2U) ||
        !minisnn_sensor_frame_init(&short_sensor_frame, 1U) ||
        !minisnn_action_frame_init(&action_frame, 2U) ||
        !minisnn_action_frame_init(&short_action_frame, 1U) ||
        !minisnn_sensor_frame_init(&copied_sensor, 2U) ||
        !minisnn_action_frame_init(&copied_action, 2U) ||
        !set_sensor(&sensor_frame, 5U, 0.5, -1.0) ||
        !set_action(&action_frame, 5U, 0.2, 3.0))
    {
        minisnn_sensor_schema_destroy(&sensor);
        minisnn_action_schema_destroy(&action);
        minisnn_sensor_frame_destroy(&sensor_frame);
        minisnn_sensor_frame_destroy(&short_sensor_frame);
        minisnn_action_frame_destroy(&action_frame);
        minisnn_action_frame_destroy(&short_action_frame);
        minisnn_sensor_frame_destroy(&copied_sensor);
        minisnn_action_frame_destroy(&copied_action);
        return fail("nao foi possivel preparar frames validos");
    }
    first = minisnn_agent_io_create(sensor, action, &error);
    second = minisnn_agent_io_create(sensor, action, &error);
    if (!minisnn_sensor_frame_reset(&sensor_frame, sensor, &error) ||
        sensor_frame.values[0] != 0.25 || sensor_frame.values[1] != 0.0 ||
        !minisnn_action_frame_reset(&action_frame, action, &error) ||
        action_frame.values[0] != 0.0 || action_frame.values[1] != 2.0 ||
        !set_sensor(&sensor_frame, 5U, 0.5, -1.0) ||
        !set_action(&action_frame, 5U, 0.2, 3.0))
        goto failure;
    minisnn_sensor_schema_destroy(&sensor);
    minisnn_action_schema_destroy(&action);
    if (first == NULL || second == NULL ||
        minisnn_agent_io_contract_signature(first) == 0U ||
        minisnn_agent_io_contract_signature(first) !=
            minisnn_agent_io_contract_signature(second))
        goto failure;

    if (minisnn_agent_io_submit_action_frame(first, &action_frame) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_ACTION_BEFORE_SENSOR ||
        minisnn_agent_io_last_error(second) != MINISNN_AGENT_IO_ERROR_NONE)
        goto failure;

    if (minisnn_agent_io_submit_sensor_frame(first, NULL) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT ||
        minisnn_agent_io_copy_last_sensor_frame(first, &copied_sensor) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_SENSOR_NOT_AVAILABLE ||
        minisnn_agent_io_consume_sensor_frame(first, &copied_sensor) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_SENSOR_NOT_AVAILABLE ||
        minisnn_agent_io_consume_action_frame(first, &copied_action) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_ACTION_NOT_AVAILABLE)
        goto failure;

    short_sensor_frame.tick = 5U;
    short_sensor_frame.values[0] = 0.5;
    if (minisnn_agent_io_submit_sensor_frame(first, &short_sensor_frame) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_VALUE_COUNT_MISMATCH)
        goto failure;

    if (minisnn_sensor_frame_set_values(&sensor_frame, 5U, nonfinite, 2U,
                                        &error) ||
        error != MINISNN_AGENT_IO_ERROR_NONFINITE_VALUE ||
        sensor_frame.values[0] != 0.5)
        goto failure;
    sensor_frame.values[0] = NAN;
    if (minisnn_agent_io_submit_sensor_frame(first, &sensor_frame) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_NONFINITE_VALUE ||
        !set_sensor(&sensor_frame, 5U, 0.5, -1.0) ||
        minisnn_agent_io_submit_sensor_frame(first, &sensor_frame) != 1)
        goto failure;

    sensor_frame.values[0] = 0.9;
    if (minisnn_agent_io_submit_sensor_frame(first, &sensor_frame) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_SENSOR_ALREADY_SUBMITTED ||
        minisnn_agent_io_submit_action_frame(first, &action_frame) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_SENSOR_NOT_CONSUMED ||
        minisnn_agent_io_consume_sensor_frame(first, NULL) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT ||
        minisnn_agent_io_consume_sensor_frame(first, &short_sensor_frame) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_VALUE_COUNT_MISMATCH ||
        copied_sensor.tick != 0U || copied_sensor.values[0] != 0.0 ||
        !minisnn_agent_io_consume_sensor_frame(first, &copied_sensor) ||
        copied_sensor.tick != 5U || copied_sensor.values[0] != 0.5 ||
        minisnn_agent_io_consume_sensor_frame(first, &copied_sensor) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_SENSOR_ALREADY_CONSUMED ||
        !set_action(&action_frame, 6U, 0.2, 3.0) ||
        minisnn_agent_io_submit_action_frame(first, &action_frame) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_ACTION_TICK_MISMATCH ||
        !set_action(&action_frame, 5U, 2.0, 3.0) ||
        minisnn_agent_io_submit_action_frame(first, &action_frame) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_VALUE_OUT_OF_RANGE ||
        !set_action(&action_frame, 5U, 0.2, 3.0) ||
        !minisnn_agent_io_submit_action_frame(first, &action_frame) ||
        minisnn_agent_io_consume_action_frame(first, &copied_action) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_ACTION_NOT_AVAILABLE ||
        !minisnn_agent_io_finish_tick(first))
        goto failure;

    if (!minisnn_agent_io_copy_last_sensor_frame(first, &copied_sensor) ||
        !minisnn_agent_io_copy_last_action_frame(first, &copied_action) ||
        copied_sensor.tick != 5U || copied_sensor.values[0] != 0.5 ||
        copied_action.values[1] != 3.0 ||
        minisnn_agent_io_submit_action_frame(first, &action_frame) ||
        minisnn_agent_io_last_error(first) != MINISNN_AGENT_IO_ERROR_TICK_FINISHED)
        goto failure;

    copied_action.tick = 99U;
    copied_action.values[0] = -0.75;
    if (minisnn_agent_io_consume_action_frame(first, NULL) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT ||
        minisnn_agent_io_consume_action_frame(first, &short_action_frame) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_VALUE_COUNT_MISMATCH ||
        copied_action.tick != 99U || copied_action.values[0] != -0.75 ||
        !set_sensor(&sensor_frame, 6U, 0.25, 0.0) ||
        minisnn_agent_io_submit_sensor_frame(first, &sensor_frame) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_PREVIOUS_ACTION_NOT_CONSUMED ||
        !minisnn_agent_io_consume_action_frame(first, &copied_action) ||
        copied_action.tick != 5U || copied_action.values[1] != 3.0 ||
        minisnn_agent_io_consume_action_frame(first, &copied_action) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_ACTION_ALREADY_CONSUMED)
        goto failure;

    if (!set_sensor(&sensor_frame, 5U, 0.5, -1.0) ||
        minisnn_agent_io_submit_sensor_frame(first, &sensor_frame) ||
        minisnn_agent_io_last_error(first) != MINISNN_AGENT_IO_ERROR_TICK_REPEATED ||
        !set_sensor(&sensor_frame, 4U, 0.5, -1.0) ||
        minisnn_agent_io_submit_sensor_frame(first, &sensor_frame) ||
        minisnn_agent_io_last_error(first) != MINISNN_AGENT_IO_ERROR_TICK_REGRESSIVE)
        goto failure;

    if (!set_sensor(&sensor_frame, 6U, 0.25, 0.0) ||
        !set_action(&action_frame, 6U, 0.0, 2.0) ||
        !minisnn_agent_io_submit_sensor_frame(first, &sensor_frame) ||
        !minisnn_agent_io_consume_sensor_frame(first, &copied_sensor) ||
        !minisnn_agent_io_submit_action_frame(first, &action_frame) ||
        !minisnn_agent_io_finish_tick(first))
        goto failure;

    if (!set_sensor(&sensor_frame, 1U, 0.25, 0.0) ||
        !set_action(&action_frame, 1U, 0.0, 2.0) ||
        !minisnn_agent_io_submit_sensor_frame(second, &sensor_frame) ||
        !minisnn_agent_io_consume_sensor_frame(second, &copied_sensor) ||
        !minisnn_agent_io_submit_action_frame(second, &action_frame) ||
        !minisnn_agent_io_finish_tick(second))
        goto failure;
    minisnn_agent_io_reset(first);
    if (!minisnn_agent_io_copy_last_action_frame(second, &copied_action) ||
        copied_action.tick != 1U ||
        minisnn_agent_io_copy_last_action_frame(first, &copied_action) ||
        minisnn_agent_io_last_error(first) !=
            MINISNN_AGENT_IO_ERROR_ACTION_NOT_AVAILABLE ||
        !set_sensor(&sensor_frame, 1U, 0.25, 0.0) ||
        !minisnn_agent_io_submit_sensor_frame(first, &sensor_frame))
        goto failure;

    if (minisnn_sensor_frame_reset(&sensor_frame, NULL, &error) ||
        error != MINISNN_AGENT_IO_ERROR_INVALID_ARGUMENT)
        goto failure;
    if (!minisnn_sensor_frame_set_values(&sensor_frame, 9U, outside, 2U,
                                         &error) ||
        sensor_frame.values[0] != 2.0)
        goto failure;

    minisnn_agent_io_destroy(&first);
    minisnn_agent_io_destroy(&first);
    minisnn_agent_io_destroy(&second);
    minisnn_sensor_frame_destroy(&sensor_frame);
    minisnn_sensor_frame_destroy(&short_sensor_frame);
    minisnn_action_frame_destroy(&action_frame);
    minisnn_action_frame_destroy(&short_action_frame);
    minisnn_sensor_frame_destroy(&copied_sensor);
    minisnn_action_frame_destroy(&copied_action);
    return 1;

failure:
    minisnn_agent_io_destroy(&first);
    minisnn_agent_io_destroy(&second);
    minisnn_sensor_frame_destroy(&sensor_frame);
    minisnn_sensor_frame_destroy(&short_sensor_frame);
    minisnn_action_frame_destroy(&action_frame);
    minisnn_action_frame_destroy(&short_action_frame);
    minisnn_sensor_frame_destroy(&copied_sensor);
    minisnn_action_frame_destroy(&copied_action);
    return fail("contrato de frame, tick, isolamento ou atomicidade invalido");
}

int main(void)
{
    if (!test_schema_contracts() || !test_signatures_and_serialization() ||
        !test_ascii_names_and_reader_errors() || !test_frame_public_errors() ||
        !test_frames_and_context())
        return 1;
    printf("Agent I/O contracts validation OK\n");
    return 0;
}
