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
    HodgkinHuxleyParameters parameters;
    NeuronModelConfig config;
    Neuron neuron;
    Neuron copy;
    NeuronStepContext context = {0.0, 0, 0.0};
    int spike_count = 0;

    hodgkin_huxley_parameters_default(&parameters);
    if (parameters.capacitance != 1.0 || parameters.g_na != 120.0 ||
        parameters.dt != 0.01)
        return fail("defaults");
    neuron_model_config_hodgkin_huxley(&config, &parameters);
    if (!neuron_model_validate_config(&config) ||
        !neuron_model_capabilities(config.model).supports_hh_gates ||
        neuron_model_capabilities(config.model).supports_homeostatic_threshold)
        return fail("validation or capabilities");
    if (!close_enough(hodgkin_huxley_alpha_m(-40.0), 1.0, 1e-10) ||
        !close_enough(hodgkin_huxley_alpha_n(-55.0), 0.1, 1e-10))
        return fail("stable singular rates");

    parameters.g_na = -1.0;
    neuron_model_config_hodgkin_huxley(&config, &parameters);
    if (neuron_model_validate_config(&config))
        return fail("negative conductance accepted");
    hodgkin_huxley_parameters_default(&parameters);
    neuron_model_config_hodgkin_huxley(&config, &parameters);

    if (!neuron_model_init(&neuron, &config) ||
        neuron.model != MINISNN_NEURON_MODEL_HODGKIN_HUXLEY ||
        neuron.state.hh.m < 0.0 || neuron.state.hh.m > 1.0 ||
        neuron.state.hh.h < 0.0 || neuron.state.hh.h > 1.0 ||
        neuron.state.hh.n < 0.0 || neuron.state.hh.n > 1.0)
        return fail("equilibrium initialization");

    copy = neuron;
    if (neuron_model_step(&neuron, &config, &context) != 0 ||
        !isfinite(neuron.V) || fabs(neuron.V - copy.V) > 0.01)
        return fail("zero-current rest step");

    context.current = 10.0;
    for (int step = 0; step < 5000; step++)
    {
        int spike = neuron_model_step(&neuron, &config, &context);
        if (spike < 0 || !isfinite(neuron.V) ||
            neuron.state.hh.m < 0.0 || neuron.state.hh.m > 1.0 ||
            neuron.state.hh.h < 0.0 || neuron.state.hh.h > 1.0 ||
            neuron.state.hh.n < 0.0 || neuron.state.hh.n > 1.0)
            return fail("RK4 state bounds");
        spike_count += spike;
    }
    if (spike_count < 1)
        return fail("suprathreshold pulse did not spike");

    neuron.type = NEURON_INHIBITORY;
    if (!neuron_model_reset(&neuron, &config) ||
        neuron.type != NEURON_INHIBITORY ||
        neuron.model != MINISNN_NEURON_MODEL_HODGKIN_HUXLEY ||
        neuron.V != parameters.v_init || neuron.spike != 0)
        return fail("reset");

    copy = neuron;
    context.current = INFINITY;
    if (neuron_model_step(&neuron, &config, &context) != -1 ||
        memcmp(&copy, &neuron, sizeof(neuron)) != 0)
        return fail("atomic non-finite error");

    printf("Hodgkin-Huxley numerical validation OK\n");
    return 0;
}
