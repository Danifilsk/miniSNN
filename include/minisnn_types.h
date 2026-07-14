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

typedef struct
{
    int enabled;

    int intrinsic_enabled;
    double target_rate;
    double rate_tau;
    unsigned int update_interval_steps;

    double threshold_eta;
    double threshold_min;
    double threshold_max;

    int synaptic_scaling_enabled;
    double scaling_eta;
    double scaling_min_factor;
    double scaling_max_factor;
    double scaling_weight_min;
    double scaling_weight_max;

    int inhibitory_gain_enabled;
    double inhibitory_gain_initial;
    double inhibitory_gain_eta;
    double inhibitory_gain_min;
    double inhibitory_gain_max;
} MiniSNNHomeostasisConfig;

typedef struct
{
    unsigned long long update_count;

    unsigned long long threshold_increase_events;
    unsigned long long threshold_decrease_events;
    unsigned long long threshold_clamp_min_events;
    unsigned long long threshold_clamp_max_events;
    unsigned long long threshold_modified_neuron_count;

    unsigned long long scaling_events;
    unsigned long long scaling_connections_modified;
    unsigned long long scaling_clamp_min_events;
    unsigned long long scaling_clamp_max_events;
    unsigned long long scaling_zero_sum_skips;

    unsigned long long inhibitory_gain_increase_events;
    unsigned long long inhibitory_gain_decrease_events;
    unsigned long long inhibitory_gain_clamp_min_events;
    unsigned long long inhibitory_gain_clamp_max_events;

    double total_threshold_absolute_change;
    double total_scaling_absolute_change;
    double total_scaling_signed_change;

    double population_rate_sum;
    double population_rate_min;
    double population_rate_max;
    unsigned long long population_rate_sample_count;
    double rate_error_sum;
    double rate_error_absolute_sum;

    double scaling_factor_sum;
    double scaling_factor_min;
    double scaling_factor_max;
    unsigned long long scaling_factor_count;

    double inhibitory_gain_min_observed;
    double inhibitory_gain_max_observed;
    double final_population_rate;
    double final_inhibitory_gain;
} MiniSNNHomeostasisStats;

#endif
