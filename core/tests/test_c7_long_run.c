#include <math.h>
#include <stdio.h>
#include <string.h>

#include "c7_audit_common.h"

#define C7_LONG_NEURAL_STEPS_PER_MODEL 100000U
#define C7_LONG_BRAIN_STEPS 100U
#define C7_LONG_TICKS (C7_LONG_NEURAL_STEPS_PER_MODEL / C7_LONG_BRAIN_STEPS)

static int fail(const char *message)
{
    fprintf(stderr, "C7 long-run test failed: %s\n", message);
    return 0;
}

static int ensure_directory(const char *directory)
{
    return c7_audit_ensure_directory(directory);
}

static void cleanup_checkpoint(const char *directory)
{
    c7_audit_remove_checkpoint_directory(directory);
}

static void values_for_tick(uint64_t tick, double values[C7_AUDIT_SENSOR_COUNT])
{
    values[0] = (tick % 3U) == 0U ? 1.0 : 0.5;
    values[1] = (tick % 3U) == 1U ? -0.5 : 0.25;
    values[2] = (tick & UINT64_C(1)) == 0U ? 1.0 : 0.0;
}

static int actions_equal(const double *left, const double *right)
{
    for (uint32_t index = 0U; index < C7_AUDIT_ACTION_COUNT; index++)
        if (left[index] != right[index])
            return 0;
    return 1;
}

static int replace_from_checkpoint(
    C7AuditFixture *fixture,
    MiniSNNNeuronModel model,
    const char *directory,
    int pending,
    uint64_t tick,
    double in_out_action[C7_AUDIT_ACTION_COUNT])
{
    C7AuditFixture resumed = {0};
    MiniSNNAgentCycleError error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    double action[C7_AUDIT_ACTION_COUNT] = {0};
    const int use_homeostasis = model == MINISNN_NEURON_MODEL_LIF;

    if (!minisnn_agent_cycle_save_checkpoint(fixture->cycle, directory, &error) ||
        !c7_audit_fixture_create(&resumed, model, C7_AUDIT_MIN_NEURONS,
                                 C7_LONG_BRAIN_STEPS, 1, 1, use_homeostasis, 1) ||
        !minisnn_agent_cycle_load_checkpoint(resumed.cycle, directory, &error))
    {
        c7_audit_fixture_destroy(&resumed);
        return 0;
    }
    if (pending)
    {
        if (!c7_audit_consume_action(fixture, tick, in_out_action) ||
            !c7_audit_consume_action(&resumed, tick, action) ||
            !actions_equal(action, in_out_action))
        {
            c7_audit_fixture_destroy(&resumed);
            return 0;
        }
    }
    minisnn_agent_cycle_destroy(&fixture->cycle);
    minisnn_action_decoder_destroy(&fixture->decoder);
    minisnn_sensor_encoder_destroy(&fixture->encoder);
    minisnn_agent_io_destroy(&fixture->agent_io);
    minisnn_action_schema_destroy(&fixture->action_schema);
    minisnn_sensor_schema_destroy(&fixture->sensor_schema);
    minisnn_destroy(&fixture->network);
    *fixture = resumed;
    return 1;
}

static int run_model(MiniSNNNeuronModel model, const char *name)
{
    C7AuditFixture fixture = {0};
    C7AuditFingerprint fingerprint;
    const int use_homeostasis = model == MINISNN_NEURON_MODEL_LIF;
    char ready_directory[256];
    char pending_directory[256];
    uint64_t expected_actions = 0U;
    uint64_t total_spikes = 0U;
    uint64_t nondefault_actions = 0U;
    uint64_t failed_tick = UINT64_MAX;
    int ok = c7_audit_fixture_create(&fixture, model, C7_AUDIT_MIN_NEURONS,
                                     C7_LONG_BRAIN_STEPS, 1, 1, use_homeostasis, 1);

    snprintf(ready_directory, sizeof(ready_directory), "build/c7_long_ready_%s", name);
    snprintf(pending_directory, sizeof(pending_directory), "build/c7_long_pending_%s", name);
    cleanup_checkpoint(ready_directory);
    cleanup_checkpoint(pending_directory);
    ok = ok && ensure_directory(ready_directory) && ensure_directory(pending_directory);
    c7_audit_fingerprint_init(&fingerprint, &fixture, UINT64_C(1000) + (uint64_t)model);
    for (uint64_t tick = 0U; ok && tick < C7_LONG_TICKS; tick++)
    {
        double values[C7_AUDIT_SENSOR_COUNT];
        double actions[C7_AUDIT_ACTION_COUNT] = {0};
        MiniSNNAgentCycleDiagnostics diagnostics = {0};
        values_for_tick(tick, values);

        if (tick == 64U)
        {
            ok = c7_audit_submit_sensor(&fixture, tick, values) &&
                c7_audit_run_pending(&fixture, &diagnostics) &&
                replace_from_checkpoint(&fixture, model, pending_directory, 1, tick, actions);
        }
        else
            ok = c7_audit_run_tick(&fixture, tick, values, actions, &diagnostics);
        if (!ok || !c7_audit_fixture_all_finite(&fixture))
        {
            failed_tick = tick;
            break;
        }
        expected_actions++;
        total_spikes += diagnostics.total_spikes;
        for (uint32_t index = 0U; index < C7_AUDIT_ACTION_COUNT; index++)
            if (actions[index] != 0.0)
            {
                nondefault_actions++;
                break;
            }
        c7_audit_fingerprint_tick(&fingerprint, tick, values, actions, &diagnostics, &fixture);
        if (tick == 32U)
            ok = replace_from_checkpoint(&fixture, model, ready_directory, 0, tick, actions);
        if (ok && (tick % 500U) == 499U)
        {
            ok = c7_audit_submit_feedback(&fixture, tick, tick + 1U, -0.25, 1) &&
                minisnn_agent_cycle_reset_episode(fixture.cycle);
        }
        else if (ok)
        {
            const double reward = (tick % 3U) == 0U ? 0.20 :
                ((tick % 3U) == 1U ? -0.10 : 0.0);
            ok = c7_audit_submit_feedback(&fixture, tick, tick + 1U, reward, 0);
        }
    }
    ok = ok && minisnn_agent_cycle_total_actions(fixture.cycle) == expected_actions &&
        minisnn_agent_cycle_total_neural_steps(fixture.cycle) == C7_LONG_NEURAL_STEPS_PER_MODEL &&
        total_spikes > 0U && nondefault_actions > 0U &&
        c7_audit_fingerprint_value(&fingerprint) != 0U &&
        c7_audit_fixture_all_finite(&fixture);
    if (!ok)
    {
        fprintf(stderr, "C7 long-run detail model=%s tick=%llu actions=%llu steps=%llu\n",
                name, (unsigned long long)failed_tick,
                (unsigned long long)expected_actions,
                (unsigned long long)minisnn_agent_cycle_total_neural_steps(fixture.cycle));
        fail("100000 passos por modelo, checkpoint ou reset");
    }
    c7_audit_fixture_destroy(&fixture);
    cleanup_checkpoint(ready_directory);
    cleanup_checkpoint(pending_directory);
    return ok;
}

static int test_lifecycle_stress(void)
{
    int ok = 1;
    for (uint32_t index = 0U; ok && index < 150U; index++)
    {
        C7AuditFixture fixture = {0};
        const double values[C7_AUDIT_SENSOR_COUNT] = {1.0, 0.0, 1.0};
        double actions[C7_AUDIT_ACTION_COUNT] = {0};
        MiniSNNNeuronModel model = (MiniSNNNeuronModel)(index % 3U);
        ok = c7_audit_fixture_create(&fixture, model, C7_AUDIT_MIN_NEURONS, 2U,
                                     0, 0, 0, 0) &&
            c7_audit_run_tick(&fixture, 0U, values, actions, NULL) &&
            c7_audit_fixture_all_finite(&fixture);
        c7_audit_fixture_destroy(&fixture);
    }
    if (!ok)
        fail("stress de create/run/destroy");
    return ok;
}

int main(void)
{
    if (!run_model(MINISNN_NEURON_MODEL_LIF, "lif") ||
        !run_model(MINISNN_NEURON_MODEL_ADEX, "adex") ||
        !run_model(MINISNN_NEURON_MODEL_HODGKIN_HUXLEY, "hh") ||
        !test_lifecycle_stress())
        return 1;
    printf("C7 long-run validation OK\n");
    return 0;
}
