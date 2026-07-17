#include <stdio.h>
#include <string.h>

#include "minisnn.h"
#include "neuron_model.h"
#include "scenario_config.h"
#include "scenario_runner.h"
#include "scenario_runtime.h"

static int fail(const char *message)
{
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

int main(void)
{
    MiniSNNConfig network_config = minisnn_default_config();
    MiniSNN *snn;
    ScenarioConfig scenario;
    ScenarioRuntimeStep runtime_step;
    ScenarioRunResult run_result;
    char error[256];
    double synaptic_current;

    network_config.neuron_count = 2;
    snn = minisnn_create_with_config(&network_config);
    if (snn == NULL || !minisnn_connect_delayed(snn, 0, 1, 200.0, 1) ||
        !minisnn_set_input(snn, 0, 10000.0))
        return fail("setup");
    neuron_model_test_fail_after_calls(1);
    if (minisnn_step(snn) != -1 || minisnn_current_step(snn) != 0 ||
        !minisnn_get_synaptic_current(snn, 1, &synaptic_current) ||
        synaptic_current != 0.0)
        return fail("public API did not propagate atomic step error");
    minisnn_destroy(&snn);

    scenario_config_default(&scenario);
    scenario.neurons = 2;
    scenario.source_count = 1;
    scenario.record_neuron = 0;
    scenario.steps = 2;
    snprintf(scenario.topology, sizeof(scenario.topology), "chain");
    snn = minisnn_create(2);
    if (snn == NULL)
        return fail("runtime setup");
    neuron_model_test_fail_after_calls(0);
    memset(&runtime_step, 0x7f, sizeof(runtime_step));
    if (scenario_runtime_step(snn, &scenario, 0, 0, &runtime_step,
                              error, sizeof(error)) ||
        minisnn_current_step(snn) != 0 ||
        strstr(error, "minisnn_step") == NULL)
        return fail("scenario runtime did not stop on model error");
    minisnn_destroy(&snn);

    snprintf(scenario.run_name, sizeof(scenario.run_name),
             "test_c5_step_error");
    scenario.auto_unique_run = 0;
    scenario.history_enabled = 0;
    neuron_model_test_fail_after_calls(0);
    if (scenario_runner_execute(&scenario, NULL, &run_result,
                                error, sizeof(error)) ||
        strstr(error, "simulacao") == NULL)
        return fail("scenario runner did not propagate model error");

    printf("Neuron model error propagation validation OK\n");
    return 0;
}
