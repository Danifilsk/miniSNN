#include <math.h>
#include <stdio.h>

#include "neuron.h"

#define TEST_TOLERANCE 1e-12

static int fail(const char *message)
{
    printf("FAIL: %s\n", message);
    return 0;
}

static int close_enough(double actual, double expected)
{
    return fabs(actual - expected) <= TEST_TOLERANCE;
}

static LIFParameters simple_parameters(void)
{
    LIFParameters parameters;

    parameters.dt = 1.0;
    parameters.tau = 10.0;
    parameters.v_rest = -65.0;
    parameters.v_reset = -65.0;
    parameters.v_threshold = -50.0;
    parameters.resistance = 1.0;
    return parameters;
}

static double reference_euler_step(
    double voltage,
    double current,
    const LIFParameters *parameters)
{
    return voltage +
           (parameters->dt / parameters->tau) *
               (-(voltage - parameters->v_rest) +
                parameters->resistance * current);
}

static int check_initial_state(void)
{
    LIFNeuron neuron;
    LIFParameters parameters = simple_parameters();

    lif_init_with_parameters(&neuron, &parameters);

    if (!close_enough(neuron.V, parameters.v_rest) ||
        neuron.spike != 0 ||
        neuron.type != NEURON_EXCITATORY)
    {
        return fail("initial LIF state is incorrect");
    }

    return 1;
}

static int check_zero_current(void)
{
    LIFNeuron neuron;
    LIFParameters parameters = simple_parameters();

    lif_init_with_parameters(&neuron, &parameters);

    for (int step = 0; step < 100; step++)
    {
        if (lif_update_with_parameters(&neuron, 0.0, &parameters) != 0 ||
            neuron.spike != 0 ||
            !close_enough(neuron.V, parameters.v_rest))
        {
            return fail("zero current did not preserve resting potential");
        }
    }

    return 1;
}

static int check_one_step_and_trajectory(void)
{
    LIFNeuron neuron;
    LIFParameters parameters = simple_parameters();
    double expected = parameters.v_rest;
    const double current = 10.0;

    lif_init_with_parameters(&neuron, &parameters);

    for (int step = 0; step < 8; step++)
    {
        expected = reference_euler_step(expected, current, &parameters);

        if (lif_update_with_parameters(&neuron, current, &parameters) != 0 ||
            !close_enough(neuron.V, expected))
        {
            return fail("Euler trajectory differs from independent reference");
        }

        if (step == 0 && !close_enough(neuron.V, -64.0))
            return fail("one-step voltage is not -64.0");
    }

    return 1;
}

static int check_subthreshold_current(void)
{
    LIFNeuron neuron;
    LIFParameters parameters = simple_parameters();

    lif_init_with_parameters(&neuron, &parameters);

    for (int step = 0; step < 200; step++)
    {
        if (lif_update_with_parameters(&neuron, 10.0, &parameters) != 0)
            return fail("subthreshold current produced a spike");
    }

    if (!(neuron.V < parameters.v_threshold && neuron.V > parameters.v_rest))
        return fail("subthreshold voltage did not approach its equilibrium");

    return 1;
}

static int check_suprathreshold_current(void)
{
    LIFNeuron neuron;
    LIFParameters parameters = simple_parameters();
    int first_spike_update = -1;
    int total_spikes = 0;

    lif_init_with_parameters(&neuron, &parameters);

    for (int update = 1; update <= 100; update++)
    {
        int spike = lif_update_with_parameters(&neuron, 20.0, &parameters);

        if (spike)
        {
            total_spikes++;
            if (first_spike_update < 0)
                first_spike_update = update;

            if (neuron.spike != 1 ||
                !close_enough(neuron.V, parameters.v_reset))
            {
                return fail("spike did not set flag and reset voltage");
            }
        }
        else if (neuron.spike != 0)
        {
            return fail("spike flag was not cleared after non-spiking update");
        }
    }

    if (first_spike_update != 14)
        return fail("first suprathreshold spike did not occur on update 14");

    if (total_spikes != 7)
        return fail("suprathreshold horizon did not produce exactly 7 spikes");

    return 1;
}

static int check_inhibitory_current(void)
{
    LIFNeuron neuron;
    LIFParameters parameters = simple_parameters();

    lif_init_with_parameters(&neuron, &parameters);

    if (lif_update_with_parameters(&neuron, -10.0, &parameters) != 0 ||
        !close_enough(neuron.V, -66.0) ||
        !(neuron.V < parameters.v_rest))
    {
        return fail("negative current did not lower membrane potential");
    }

    return 1;
}

int main(void)
{
    if (!check_initial_state() ||
        !check_zero_current() ||
        !check_one_step_and_trajectory() ||
        !check_subthreshold_current() ||
        !check_suprathreshold_current() ||
        !check_inhibitory_current())
    {
        return 1;
    }

    printf("LIF numerical validation OK\n");
    return 0;
}
