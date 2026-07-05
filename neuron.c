#include "neuron.h"

void lif_init(LIFNeuron *n)
{
    n->V = V_REST;
    n->spike = 0;
}

int lif_update(LIFNeuron *n, double I)
{
    // Integração de Euler do modelo LIF
    double dV = (-(n->V - V_REST) + R * I) / TAU;

    n->V += DT * dV;

    if (n->V >= V_THRESH)
    {
        n->V = V_RESET;
        n->spike = 1;
        return 1;
    }

    n->spike = 0;
    return 0;
}