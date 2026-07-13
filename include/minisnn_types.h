#ifndef MINISNN_TYPES_H
#define MINISNN_TYPES_H

#include <stddef.h>

typedef enum
{
    MINISNN_NEURON_EXCITATORY = 0,
    MINISNN_NEURON_INHIBITORY = 1
} MiniSNNNeuronType;

typedef enum
{
    MINISNN_PLASTICITY_NONE = 0,
    MINISNN_PLASTICITY_STDP_PAIR_TRACE = 1
} MiniSNNPlasticityRule;

typedef struct
{
    int enabled;
    MiniSNNPlasticityRule rule;
    double a_plus;
    double a_minus;
    double tau_plus;
    double tau_minus;
    double trace_increment;
    double weight_min;
    double weight_max;
} MiniSNNPlasticityConfig;

typedef struct
{
    size_t source;
    size_t target;
    MiniSNNNeuronType source_type;
    MiniSNNNeuronType target_type;
    double weight;
    unsigned int delay;
    int plasticity_eligible;
} MiniSNNConnectionInfo;

typedef struct
{
    unsigned long long potentiation_events;
    unsigned long long depression_events;
    unsigned long long clamp_min_events;
    unsigned long long clamp_max_events;
    size_t eligible_connections;
    size_t modified_connections;
    double total_signed_change;
    double total_absolute_change;
    double max_absolute_change;
} MiniSNNPlasticityStats;

#endif
