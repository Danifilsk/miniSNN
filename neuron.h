#ifndef NEURON_H
#define NEURON_H

#include "config.h"

typedef struct
{
    // Potencial de membrana
    double V;

    // 1 = disparou neste passo
    int spike;

} LIFNeuron;

// Inicializa um neurônio
void lif_init(LIFNeuron *n);

// Atualiza um passo do modelo LIF
int lif_update(LIFNeuron *n, double I);

#endif