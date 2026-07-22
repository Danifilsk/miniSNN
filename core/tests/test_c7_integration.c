#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "c7_audit_common.h"
#include "minisnn_internal.h"

static int fail(const char *message)
{
    fprintf(stderr, "C7 integration test failed: %s\n", message);
    return 0;
}

static void cleanup_checkpoint(const char *directory)
{
    static const char *const names[] =
    {
        "network_state.bin", "agent_io_state.bin", "sensor_encoder_state.bin",
        "action_decoder_state.bin", "agent_cycle_state.bin", "manifest.txt"
    };
    char path[384];
    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]); index++)
    {
        snprintf(path, sizeof(path), "%s/%s", directory, names[index]);
        remove(path);
    }
#ifdef _WIN32
    RemoveDirectoryA(directory);
#endif
}

static int ensure_directory(const char *directory)
{
#ifdef _WIN32
    return CreateDirectoryA(directory, NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    (void)directory;
    return 1;
#endif
}

static int actions_equal(const double *left, const double *right)
{
    for (uint32_t index = 0U; index < C7_AUDIT_ACTION_COUNT; index++)
        if (left[index] != right[index])
            return 0;
    return 1;
}

static int test_lif_tick_contract(void)
{
    C7AuditFixture fixture;
    MiniSNNAgentCycleDiagnostics diagnostics = {0};
    const double values[C7_AUDIT_SENSOR_COUNT] = {1.0, -0.5, 1.0};
    double action[C7_AUDIT_ACTION_COUNT] = {0};
    uint64_t global_after_first;
    int ok = c7_audit_fixture_create(&fixture, MINISNN_NEURON_MODEL_LIF,
                                     C7_AUDIT_MIN_NEURONS, 4U, 1, 1, 1, 1);
    if (!ok)
    {
        fail("criacao de fixture LIF");
        c7_audit_fixture_destroy(&fixture);
        return 0;
    }
    ok = c7_audit_submit_sensor(&fixture, 0U, values) &&
        c7_audit_run_pending(&fixture, &diagnostics) &&
        minisnn_agent_cycle_state(fixture.cycle) == MINISNN_AGENT_CYCLE_STATE_ACTION_PENDING &&
        diagnostics.global_tick == 0U && diagnostics.episode_tick == 0U &&
        diagnostics.brain_steps_executed == 4U &&
        !c7_audit_submit_sensor(&fixture, 1U, values) &&
        !minisnn_agent_cycle_reset_episode(fixture.cycle) &&
        c7_audit_consume_action(&fixture, 0U, action) &&
        !c7_audit_consume_action(&fixture, 0U, action) &&
        minisnn_agent_cycle_state(fixture.cycle) == MINISNN_AGENT_CYCLE_STATE_ACTION_PENDING;
    global_after_first = minisnn_agent_cycle_next_global_tick(fixture.cycle);
    ok = ok && global_after_first == 1U &&
        c7_audit_submit_feedback(&fixture, 0U, 1U, 0.25, 0) &&
        c7_audit_run_tick(&fixture, 1U, values, action, &diagnostics) &&
        diagnostics.global_tick == 1U && diagnostics.episode_tick == 1U &&
        diagnostics.feedback_events_delivered == 1U && diagnostics.reward_delivered == 0.25 &&
        minisnn_agent_cycle_total_ticks(fixture.cycle) == 2U &&
        minisnn_agent_cycle_total_actions(fixture.cycle) == 2U &&
        minisnn_agent_cycle_total_neural_steps(fixture.cycle) == 8U &&
        c7_audit_fixture_all_finite(&fixture) &&
        c7_audit_submit_feedback(&fixture, 1U, 2U, -0.10, 1) &&
        minisnn_agent_cycle_reset_episode(fixture.cycle) &&
        minisnn_agent_cycle_episode_tick(fixture.cycle) == 0U &&
        minisnn_agent_cycle_next_global_tick(fixture.cycle) == 2U;
    if (!ok)
        fail("contrato de tick, feedback e reset LIF");
    c7_audit_fixture_destroy(&fixture);
    return ok;
}

static int test_checkpoint_matrix_for_model(MiniSNNNeuronModel model, const char *name)
{
    C7AuditFixture continuous = {0};
    C7AuditFixture ready_source = {0};
    C7AuditFixture ready_resumed = {0};
    C7AuditFixture pending_source = {0};
    C7AuditFixture pending_resumed = {0};
    const double first[C7_AUDIT_SENSOR_COUNT] = {1.0, 0.25, 1.0};
    const double second[C7_AUDIT_SENSOR_COUNT] = {0.5, -0.25, 0.0};
    double continuous_action[C7_AUDIT_ACTION_COUNT] = {0};
    double ready_action[C7_AUDIT_ACTION_COUNT] = {0};
    double pending_action[C7_AUDIT_ACTION_COUNT] = {0};
    MiniSNNAgentCycleError error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    char ready_directory[256];
    char pending_directory[256];
    int ok;

    snprintf(ready_directory, sizeof(ready_directory), "build/c7_ready_%s", name);
    snprintf(pending_directory, sizeof(pending_directory), "build/c7_pending_%s", name);
    cleanup_checkpoint(ready_directory);
    cleanup_checkpoint(pending_directory);
    const int use_homeostasis = model == MINISNN_NEURON_MODEL_LIF;
    ok = ensure_directory(ready_directory) && ensure_directory(pending_directory) &&
        c7_audit_fixture_create(&continuous, model, C7_AUDIT_MIN_NEURONS, 4U,
                                1, 1, use_homeostasis, 1) &&
        c7_audit_fixture_create(&ready_source, model, C7_AUDIT_MIN_NEURONS, 4U,
                                1, 1, use_homeostasis, 1) &&
        c7_audit_fixture_create(&ready_resumed, model, C7_AUDIT_MIN_NEURONS, 4U,
                                1, 1, use_homeostasis, 1) &&
        c7_audit_fixture_create(&pending_source, model, C7_AUDIT_MIN_NEURONS, 4U,
                                1, 1, use_homeostasis, 1) &&
        c7_audit_fixture_create(&pending_resumed, model, C7_AUDIT_MIN_NEURONS, 4U,
                                1, 1, use_homeostasis, 1) &&
        c7_audit_run_tick(&continuous, 0U, first, continuous_action, NULL) &&
        c7_audit_run_tick(&continuous, 1U, second, continuous_action, NULL) &&
        c7_audit_run_tick(&ready_source, 0U, first, ready_action, NULL) &&
        minisnn_agent_cycle_save_checkpoint(ready_source.cycle, ready_directory, &error) &&
        minisnn_agent_cycle_load_checkpoint(ready_resumed.cycle, ready_directory, &error) &&
        c7_audit_run_tick(&ready_source, 1U, second, ready_action, NULL) &&
        c7_audit_run_tick(&ready_resumed, 1U, second, pending_action, NULL) &&
        actions_equal(ready_action, pending_action) &&
        minisnn_current_step(ready_source.network) == minisnn_current_step(ready_resumed.network) &&
        c7_audit_fixture_all_finite(&ready_resumed) &&
        c7_audit_submit_sensor(&pending_source, 0U, first) &&
        c7_audit_run_pending(&pending_source, NULL) &&
        minisnn_agent_cycle_save_checkpoint(pending_source.cycle, pending_directory, &error) &&
        minisnn_agent_cycle_load_checkpoint(pending_resumed.cycle, pending_directory, &error) &&
        minisnn_agent_cycle_state(pending_resumed.cycle) ==
            MINISNN_AGENT_CYCLE_STATE_ACTION_PENDING &&
        c7_audit_consume_action(&pending_source, 0U, ready_action) &&
        c7_audit_consume_action(&pending_resumed, 0U, pending_action) &&
        actions_equal(ready_action, pending_action) &&
        c7_audit_run_tick(&pending_source, 1U, second, ready_action, NULL) &&
        c7_audit_run_tick(&pending_resumed, 1U, second, pending_action, NULL) &&
        actions_equal(ready_action, pending_action) &&
        c7_audit_fixture_all_finite(&pending_resumed);
    if (!ok)
    {
        char message[128];
        snprintf(message, sizeof(message), "checkpoint READY/ACTION_PENDING (%s)", name);
        fail(message);
    }
    c7_audit_fixture_destroy(&continuous);
    c7_audit_fixture_destroy(&ready_source);
    c7_audit_fixture_destroy(&ready_resumed);
    c7_audit_fixture_destroy(&pending_source);
    c7_audit_fixture_destroy(&pending_resumed);
    cleanup_checkpoint(ready_directory);
    cleanup_checkpoint(pending_directory);
    return ok;
}

static int test_checkpoint_models(void)
{
    return test_checkpoint_matrix_for_model(MINISNN_NEURON_MODEL_LIF, "lif") &&
        test_checkpoint_matrix_for_model(MINISNN_NEURON_MODEL_ADEX, "adex") &&
        test_checkpoint_matrix_for_model(MINISNN_NEURON_MODEL_HODGKIN_HUXLEY, "hh");
}

static int test_fingerprints_and_fault_recovery(void)
{
    C7AuditFixture left = {0};
    C7AuditFixture right = {0};
    C7AuditFingerprint first;
    C7AuditFingerprint second;
    C7AuditFingerprint changed_seed;
    const double values[C7_AUDIT_SENSOR_COUNT] = {1.0, 0.0, 1.0};
    double left_action[C7_AUDIT_ACTION_COUNT] = {0};
    double right_action[C7_AUDIT_ACTION_COUNT] = {0};
    MiniSNNAgentCycleDiagnostics diagnostics = {0};
    int ok = c7_audit_fixture_create(&left, MINISNN_NEURON_MODEL_LIF,
                                     C7_AUDIT_MIN_NEURONS, 4U, 0, 0, 0, 0) &&
        c7_audit_fixture_create(&right, MINISNN_NEURON_MODEL_LIF,
                                C7_AUDIT_MIN_NEURONS, 4U, 0, 0, 0, 0);
    c7_audit_fingerprint_init(&first, &left, 7U);
    c7_audit_fingerprint_init(&second, &right, 7U);
    c7_audit_fingerprint_init(&changed_seed, &right, 8U);
    for (uint64_t tick = 0U; ok && tick < 4U; tick++)
    {
        ok = c7_audit_run_tick(&left, tick, values, left_action, &diagnostics) &&
            c7_audit_run_tick(&right, tick, values, right_action, &diagnostics) &&
            actions_equal(left_action, right_action);
        c7_audit_fingerprint_tick(&first, tick, values, left_action, &diagnostics, &left);
        c7_audit_fingerprint_tick(&second, tick, values, right_action, &diagnostics, &right);
    }
    ok = ok && c7_audit_fingerprint_value(&first) == c7_audit_fingerprint_value(&second) &&
        c7_audit_fingerprint_value(&first) != c7_audit_fingerprint_value(&changed_seed);
    if (ok)
    {
        left.network->net.neurons[0].model = (MiniSNNNeuronModel)99;
        ok = c7_audit_submit_sensor(&left, 4U, values) &&
            !c7_audit_run_pending(&left, NULL) &&
            minisnn_agent_cycle_state(left.cycle) == MINISNN_AGENT_CYCLE_STATE_FAULTED &&
            !minisnn_agent_io_action_pending(left.agent_io);
        left.network->net.neurons[0].model = MINISNN_NEURON_MODEL_LIF;
        ok = ok && minisnn_agent_cycle_reset_episode(left.cycle) &&
            minisnn_agent_cycle_state(left.cycle) == MINISNN_AGENT_CYCLE_STATE_READY;
    }
    if (!ok)
        fail("fingerprint deterministico ou recuperacao de falha");
    c7_audit_fixture_destroy(&left);
    c7_audit_fixture_destroy(&right);
    return ok;
}

int main(void)
{
    if (!test_lif_tick_contract() || !test_checkpoint_models() ||
        !test_fingerprints_and_fault_recovery())
        return 1;
    printf("C7 integrated interface validation OK\n");
    return 0;
}
