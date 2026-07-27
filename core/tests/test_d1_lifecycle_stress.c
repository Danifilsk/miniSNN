#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "test_allocation.h"
#include "c7_audit_common.h"
#include "minisnn.h"
#include "minisnn_internal.h"

#define NETWORK_CYCLES 10000U
#define FRAME_CYCLES 1000U
#define SCHEMA_CYCLES 500U
#define CHECKPOINT_CYCLES 100U
#define SCHEMA_PATH "build/tests/d1_b_lifecycle_sensor_schema.txt"
#define CHECKPOINT_DIRECTORY "build/tests/d1_b_lifecycle_checkpoint"

static int fail(const char *message)
{
    fprintf(stderr, "D1-B lifecycle stress failed: %s\n", message);
    remove(SCHEMA_PATH);
    c7_audit_remove_checkpoint_directory(CHECKPOINT_DIRECTORY);
    return 0;
}

static int test_network_lifecycle(void)
{
    for (uint32_t index = 0U; index < NETWORK_CYCLES; index++)
    {
        MiniSNNConfig config = minisnn_default_config();
        MiniSNN *network;

        config.neuron_count = 2;
        config.neuron_model = (MiniSNNNeuronModel)(index % 3U);
        if (config.neuron_model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY)
            config.dt = 0.01;
        network = minisnn_create_with_config(&config);
        if (network == NULL || !minisnn_connect_delayed(network, 0, 1, 1.0, 1) ||
            !minisnn_reset_transient_state(network) ||
            minisnn_step(network) < 0)
        {
            minisnn_destroy(&network);
            return fail("create/reset/step/destroy de rede");
        }
        minisnn_destroy(&network);
        minisnn_destroy(&network);
        if (network != NULL)
            return fail("destroy nao tornou ponteiro nulo");
    }
    return 1;
}

static int test_frames_and_schemas(void)
{
    const MiniSNNSensorChannelSpec channels[] =
    {
        {1U, "signal", -1.0, 1.0, 0.0}
    };
    MiniSNNAgentIOError error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorSchema *schema = minisnn_sensor_schema_create(channels, 1U, &error);
    MiniSNNSensorFrame frame = {0};
    double value = 0.5;

    if (schema == NULL || !minisnn_sensor_frame_init(&frame, 1U))
    {
        minisnn_sensor_schema_destroy(&schema);
        return fail("preparo de frame/schema");
    }
    for (uint32_t index = 0U; index < FRAME_CYCLES; index++)
    {
        if (!minisnn_sensor_frame_set_values(&frame, index, &value, 1U, &error) ||
            !minisnn_sensor_frame_reset(&frame, schema, &error) ||
            frame.tick != 0U || frame.values[0] != 0.0)
        {
            minisnn_sensor_frame_destroy(&frame);
            minisnn_sensor_schema_destroy(&schema);
            return fail("reset repetido de frame");
        }
    }
    minisnn_sensor_frame_destroy(&frame);
    minisnn_sensor_schema_destroy(&schema);

    for (uint32_t index = 0U; index < SCHEMA_CYCLES; index++)
    {
        MiniSNNSensorSchema *written;
        MiniSNNSensorSchema *loaded;

        written = minisnn_sensor_schema_create(channels, 1U, &error);
        if (written == NULL ||
            !minisnn_sensor_schema_write_file(written, SCHEMA_PATH, &error))
        {
            minisnn_sensor_schema_destroy(&written);
            return fail("escrita repetida de schema");
        }
        loaded = minisnn_sensor_schema_read_file(SCHEMA_PATH, &error);
        if (loaded == NULL ||
            minisnn_sensor_schema_signature(written) !=
                minisnn_sensor_schema_signature(loaded))
        {
            minisnn_sensor_schema_destroy(&written);
            minisnn_sensor_schema_destroy(&loaded);
            return fail("round-trip repetido de schema");
        }
        minisnn_sensor_schema_destroy(&written);
        minisnn_sensor_schema_destroy(&loaded);
    }
    remove(SCHEMA_PATH);
    return 1;
}

static int test_cycle_checkpoint_lifecycle(void)
{
    for (uint32_t index = 0U; index < CHECKPOINT_CYCLES; index++)
    {
        C7AuditFixture first = {0};
        C7AuditFixture restored = {0};
        MiniSNNAgentCycleError error = MINISNN_AGENT_CYCLE_ERROR_NONE;
        const double input[C7_AUDIT_SENSOR_COUNT] = {1.0, 0.0, 1.0};
        double action[C7_AUDIT_ACTION_COUNT] = {0};

        c7_audit_remove_checkpoint_directory(CHECKPOINT_DIRECTORY);
        if (!c7_audit_fixture_create(&first, MINISNN_NEURON_MODEL_LIF,
                                     C7_AUDIT_MIN_NEURONS, 2U, 1, 1, 1, 1) ||
            !c7_audit_run_tick(&first, 0U, input, action, NULL) ||
            !minisnn_agent_cycle_reset_episode(first.cycle) ||
            !minisnn_agent_cycle_save_checkpoint(first.cycle,
                                                 CHECKPOINT_DIRECTORY, &error) ||
            !c7_audit_fixture_create(&restored, MINISNN_NEURON_MODEL_LIF,
                                     C7_AUDIT_MIN_NEURONS, 2U, 1, 1, 1, 1) ||
            !minisnn_agent_cycle_load_checkpoint(restored.cycle,
                                                 CHECKPOINT_DIRECTORY, &error) ||
            !c7_audit_fixture_all_finite(&restored))
        {
            c7_audit_fixture_destroy(&first);
            c7_audit_fixture_destroy(&restored);
            return fail("checkpoint C7 repetido");
        }
        c7_audit_fixture_destroy(&first);
        c7_audit_fixture_destroy(&restored);
    }
    c7_audit_remove_checkpoint_directory(CHECKPOINT_DIRECTORY);
    return 1;
}

static int test_allocation_failures_are_atomic(void)
{
    MiniSNNConfig config = minisnn_default_config();
    MiniSNN *baseline;
    MiniSNN *failed;
    MiniSNNSensorChannelSpec channel = {1U, "signal", 0.0, 1.0, 0.0};
    MiniSNNAgentIOError error = MINISNN_AGENT_IO_ERROR_NONE;
    MiniSNNSensorSchema *schema;
    double voltage_before;
    double voltage_after;
    uint64_t signature;

    config.neuron_count = 2;
    baseline = minisnn_create_with_config(&config);
    if (baseline == NULL || !minisnn_get_voltage(baseline, 0, &voltage_before))
    {
        minisnn_destroy(&baseline);
        return fail("rede base para falha de alocacao");
    }
    minisnn_test_allocation_fail_after(0U);
    failed = minisnn_create_with_config(&config);
    minisnn_test_allocation_reset();
    if (failed != NULL || !minisnn_get_voltage(baseline, 0, &voltage_after) ||
        voltage_before != voltage_after)
    {
        minisnn_destroy(&failed);
        minisnn_destroy(&baseline);
        return fail("falha de alocacao alterou rede existente");
    }

    schema = minisnn_sensor_schema_create(&channel, 1U, &error);
    if (schema == NULL)
    {
        minisnn_destroy(&baseline);
        return fail("schema base para falha de alocacao");
    }
    signature = minisnn_sensor_schema_signature(schema);
    minisnn_test_allocation_fail_after(0U);
    failed = NULL;
    if (minisnn_sensor_schema_create(&channel, 1U, &error) != NULL ||
        error != MINISNN_AGENT_IO_ERROR_ALLOCATION ||
        minisnn_sensor_schema_signature(schema) != signature)
    {
        minisnn_test_allocation_reset();
        minisnn_sensor_schema_destroy(&schema);
        minisnn_destroy(&baseline);
        return fail("falha de schema nao foi atomica ou observavel");
    }
    minisnn_test_allocation_reset();
    minisnn_sensor_schema_destroy(&schema);
    minisnn_destroy(&baseline);
    return 1;
}

static int test_c7_allocation_failures_are_recoverable(void)
{
    for (size_t failure_index = 0U; failure_index < 40U; failure_index++)
    {
        C7AuditFixture fixture = {0};
        C7AuditFixture usable = {0};

        minisnn_test_allocation_fail_after(failure_index);
        (void)c7_audit_fixture_create(&fixture, MINISNN_NEURON_MODEL_LIF,
                                      C7_AUDIT_MIN_NEURONS, 2U, 1, 1, 1, 1);
        minisnn_test_allocation_reset();
        c7_audit_fixture_destroy(&fixture);
        if (!c7_audit_fixture_create(&usable, MINISNN_NEURON_MODEL_LIF,
                                     C7_AUDIT_MIN_NEURONS, 2U, 1, 1, 1, 1))
        {
            c7_audit_fixture_destroy(&usable);
            return fail("falha de alocacao C7 deixou o processo inutilizavel");
        }
        c7_audit_fixture_destroy(&usable);
    }
    return 1;
}

static int test_counter_guards(void)
{
    C7AuditFixture fixture = {0};
    const double input[C7_AUDIT_SENSOR_COUNT] = {1.0, 0.0, 1.0};
    MiniSNN *network;
    MiniSNNConfig config = minisnn_default_config();
    int ok;

    if (!c7_audit_fixture_create(&fixture, MINISNN_NEURON_MODEL_LIF,
                                 C7_AUDIT_MIN_NEURONS, 1U, 0, 0, 0, 0) ||
        !minisnn_test_agent_cycle_set_counters(
            fixture.cycle, 0U, UINT64_MAX, UINT64_MAX, 0U, 0U, 0U, 0U, 0U) ||
        !c7_audit_submit_sensor(&fixture, UINT64_MAX, input) ||
        minisnn_agent_cycle_run_tick(fixture.cycle, NULL) ||
        minisnn_agent_cycle_last_error(fixture.cycle) !=
            MINISNN_AGENT_CYCLE_ERROR_COUNTER_OVERFLOW)
    {
        c7_audit_fixture_destroy(&fixture);
        return fail("overflow do ciclo nao foi rejeitado");
    }
    c7_audit_fixture_destroy(&fixture);

    config.neuron_count = 1;
    network = minisnn_create_with_config(&config);
    if (network == NULL)
        return fail("rede para guarda de passo");
    network->net.step = INT_MAX;
    ok = minisnn_step(network) == -1 && minisnn_current_step(network) == INT_MAX;
    minisnn_destroy(&network);
    return ok ? 1 : fail("overflow do passo neural nao foi rejeitado");
}

int main(int argc, char **argv)
{
    if (argc == 2)
    {
        if (strcmp(argv[1], "network") == 0)
            return test_network_lifecycle() ? 0 : 1;
        if (strcmp(argv[1], "frames") == 0)
            return test_frames_and_schemas() ? 0 : 1;
        if (strcmp(argv[1], "checkpoint") == 0)
            return test_cycle_checkpoint_lifecycle() ? 0 : 1;
        if (strcmp(argv[1], "allocation") == 0)
            return test_allocation_failures_are_atomic() ? 0 : 1;
        if (strcmp(argv[1], "c7-allocation") == 0)
            return test_c7_allocation_failures_are_recoverable() ? 0 : 1;
        if (strcmp(argv[1], "counters") == 0)
            return test_counter_guards() ? 0 : 1;
        return fail("nome de bloco desconhecido");
    }
    if (!test_network_lifecycle() || !test_frames_and_schemas() ||
        !test_cycle_checkpoint_lifecycle() ||
        !test_allocation_failures_are_atomic() ||
        !test_c7_allocation_failures_are_recoverable() || !test_counter_guards())
        return 1;

    printf("D1-B lifecycle, allocation and counter stress OK\n");
    return 0;
}
