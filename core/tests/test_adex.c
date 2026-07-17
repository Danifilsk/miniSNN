#include <math.h>
#include <stdio.h>
#include <string.h>

#include "neuron_model.h"

static int close_enough(double a, double b, double tolerance)
{
    return fabs(a - b) <= tolerance;
}

static int fail(const char *message)
{
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

int main(void)
{
    AdExParameters parameters;
    NeuronModelConfig config;
    Neuron neuron;
    Neuron before;
    NeuronStepContext context = {0.0, 0, 0.0};
    double expected_v;
    double expected_w;
    int first_interval = -1;
    int last_interval = -1;
    int previous_spike = -1;

    adex_parameters_default(&parameters);
    if (!close_enough(parameters.capacitance, 200.0, 1e-12) ||
        !close_enough(parameters.b, 60.0, 1e-12))
        return fail("defaults");
    neuron_model_config_adex(&config, &parameters);
    if (!neuron_model_validate_config(&config) ||
        neuron_model_capabilities(config.model).supports_homeostatic_threshold ||
        !neuron_model_capabilities(config.model).supports_adaptation_state)
        return fail("validation or capabilities");

    parameters.capacitance = 0.0;
    neuron_model_config_adex(&config, &parameters);
    if (neuron_model_validate_config(&config))
        return fail("invalid capacitance accepted");
    adex_parameters_default(&parameters);
    neuron_model_config_adex(&config, &parameters);

    if (!neuron_model_init(&neuron, &config) ||
        neuron.model != MINISNN_NEURON_MODEL_ADEX ||
        !close_enough(neuron.V, parameters.e_leak, 1e-12) ||
        neuron.state.adex.w != 0.0)
        return fail("initialization");

    context.current = 100.0;
    expected_v = parameters.e_leak + parameters.dt *
        (parameters.g_leak * parameters.delta_t *
         exp((parameters.e_leak - parameters.v_threshold) /
             parameters.delta_t) + context.current) /
        parameters.capacitance;
    expected_w = 0.0;
    if (neuron_model_step(&neuron, &config, &context) != 0 ||
        !close_enough(neuron.V, expected_v, 1e-12) ||
        !close_enough(neuron.state.adex.w, expected_w, 1e-12))
        return fail("manual Euler step");

    neuron.V = parameters.v_peak - 0.01;
    neuron.state.adex.w = 3.0;
    context.current = 100000.0;
    if (neuron_model_step(&neuron, &config, &context) != 1 ||
        neuron.V != parameters.v_reset ||
        neuron.state.adex.w <= 3.0 + parameters.b)
        return fail("spike reset and adaptation increment");

    neuron.type = NEURON_INHIBITORY;
    neuron.V = -10.0;
    neuron.spike = 1;
    if (!neuron_model_reset(&neuron, &config) ||
        neuron.type != NEURON_INHIBITORY ||
        neuron.model != MINISNN_NEURON_MODEL_ADEX ||
        neuron.V != parameters.e_leak || neuron.state.adex.w != 0.0 ||
        neuron.spike != 0)
        return fail("reset");

    before = neuron;
    context.current = NAN;
    if (neuron_model_step(&neuron, &config, &context) != -1 ||
        memcmp(&before, &neuron, sizeof(neuron)) != 0)
        return fail("atomic non-finite error");

    if (!neuron_model_reset(&neuron, &config))
        return fail("reset before constant current");
    context.current = 500.0;
    for (int step = 0; step < 20000; step++)
    {
        int spike = neuron_model_step(&neuron, &config, &context);
        if (spike < 0 || !isfinite(neuron.V) ||
            !isfinite(neuron.state.adex.w))
            return fail("constant-current stability");
        if (spike)
        {
            if (previous_spike >= 0)
            {
                int interval = step - previous_spike;
                if (first_interval < 0) first_interval = interval;
                last_interval = interval;
            }
            previous_spike = step;
        }
    }
    if (first_interval <= 0 || last_interval < first_interval)
        return fail("frequency adaptation");

    printf("AdEx numerical validation OK\n");
    return 0;
}
