#ifndef NEURON_H
#define NEURON_H

#include "minisnn_types.h"

typedef MiniSNNNeuronType NeuronType;

#define NEURON_EXCITATORY MINISNN_NEURON_EXCITATORY
#define NEURON_INHIBITORY MINISNN_NEURON_INHIBITORY

typedef struct
{
    double dt;
    double tau;
    double v_rest;
    double v_reset;
    double v_threshold;
    double resistance;
} LIFParameters;

typedef struct
{
    // Potencial de membrana
    double V;

    // 1 = disparou neste passo
    int spike;

    NeuronType type;

} LIFNeuron;

// Inicializa um neurônio
void lif_parameters_default(LIFParameters *out_parameters);

int lif_parameters_are_valid(
    const LIFParameters *parameters);

void lif_init_with_parameters(
    LIFNeuron *neuron,
    const LIFParameters *parameters);

int lif_update_with_parameters(
    LIFNeuron *neuron,
    double current,
    const LIFParameters *parameters);

void lif_init(LIFNeuron *n);

// Atualiza um passo do modelo LIF
int lif_update(LIFNeuron *n, double I);

#endif
