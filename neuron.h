#ifndef NEURON_H
#define NEURON_H

#include "config.h"

typedef enum
{
    NEURON_EXCITATORY = 0,
    NEURON_INHIBITORY = 1
} NeuronType;

typedef struct
{
    // Potencial de membrana
    double V;

    // 1 = disparou neste passo
    int spike;

    NeuronType type;

} LIFNeuron;

// Inicializa um neurônio
void lif_init(LIFNeuron *n);

// Atualiza um passo do modelo LIF
int lif_update(LIFNeuron *n, double I);

#endif
