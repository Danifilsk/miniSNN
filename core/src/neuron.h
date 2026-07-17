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

typedef MiniSNNAdExConfig AdExParameters;
typedef MiniSNNHodgkinHuxleyConfig HodgkinHuxleyParameters;

typedef struct
{
    int reserved;
} LIFState;

typedef struct
{
    double w;
} AdExState;

typedef struct
{
    double m;
    double h;
    double n;
    double previous_V;
} HodgkinHuxleyState;

typedef struct
{
    /* Estado observavel comum a todos os modelos. */
    double V;
    int spike;

    NeuronType type;
    MiniSNNNeuronModel model;

    union
    {
        LIFState lif;
        AdExState adex;
        HodgkinHuxleyState hh;
    } state;

} Neuron;

typedef struct
{
    MiniSNNNeuronModel model;

    union
    {
        LIFParameters lif;
        AdExParameters adex;
        HodgkinHuxleyParameters hh;
    } data;

} NeuronModelConfig;

typedef struct
{
    double current;
    int has_adaptive_threshold;
    double adaptive_threshold;
} NeuronStepContext;

/* Compatibilidade da implementacao LIF existente. */
typedef Neuron LIFNeuron;

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

int lif_update_with_threshold(
    LIFNeuron *neuron,
    double current,
    const LIFParameters *parameters,
    double effective_threshold);

void lif_init(LIFNeuron *n);

// Atualiza um passo do modelo LIF
int lif_update(LIFNeuron *n, double I);

#endif
