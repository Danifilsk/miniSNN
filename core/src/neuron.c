#include <math.h>
#include <stddef.h>

#include "neuron.h"
#include "config.h"

void lif_parameters_default(LIFParameters *out_parameters)
{
    if (out_parameters == NULL)
        return;

    out_parameters->dt = DT;
    out_parameters->tau = TAU;
    out_parameters->v_rest = V_REST;
    out_parameters->v_reset = V_RESET;
    out_parameters->v_threshold = V_THRESH;
    out_parameters->resistance = R;
}

int lif_parameters_are_valid(
    const LIFParameters *parameters)
{
    if (parameters == NULL)
        return 0;

    if (!isfinite(parameters->dt) ||
        !isfinite(parameters->tau) ||
        !isfinite(parameters->v_rest) ||
        !isfinite(parameters->v_reset) ||
        !isfinite(parameters->v_threshold) ||
        !isfinite(parameters->resistance))
    {
        return 0;
    }

    if (parameters->dt <= 0.0 ||
        parameters->tau <= 0.0 ||
        parameters->resistance <= 0.0)
    {
        return 0;
    }

    if (parameters->v_rest >= parameters->v_threshold ||
        parameters->v_reset >= parameters->v_threshold)
    {
        return 0;
    }

    return 1;
}

void lif_init_with_parameters(
    LIFNeuron *neuron,
    const LIFParameters *parameters)
{
    if (neuron == NULL || !lif_parameters_are_valid(parameters))
        return;

    neuron->V = parameters->v_rest;
    neuron->spike = 0;
    neuron->type = NEURON_EXCITATORY;
}

int lif_update_with_parameters(
    LIFNeuron *neuron,
    double current,
    const LIFParameters *parameters)
{
    if (parameters == NULL)
        return 0;

    return lif_update_with_threshold(
        neuron, current, parameters, parameters->v_threshold);
}

int lif_update_with_threshold(
    LIFNeuron *neuron,
    double current,
    const LIFParameters *parameters,
    double effective_threshold)
{
    double dV;

    if (neuron == NULL || !lif_parameters_are_valid(parameters) ||
        !isfinite(effective_threshold) ||
        parameters->v_rest >= effective_threshold ||
        parameters->v_reset >= effective_threshold)
        return 0;

    dV = (-(neuron->V - parameters->v_rest) +
          parameters->resistance * current) /
         parameters->tau;

    neuron->V += parameters->dt * dV;

    if (neuron->V >= effective_threshold)
    {
        neuron->V = parameters->v_reset;
        neuron->spike = 1;
        return 1;
    }

    neuron->spike = 0;
    return 0;
}

void lif_init(LIFNeuron *n)
{
    LIFParameters parameters;

    lif_parameters_default(&parameters);
    lif_init_with_parameters(n, &parameters);
}

int lif_update(LIFNeuron *n, double I)
{
    LIFParameters parameters;

    lif_parameters_default(&parameters);
    return lif_update_with_parameters(n, I, &parameters);
}
