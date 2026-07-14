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

typedef enum
{
    MINISNN_LEARNING_MODE_DIRECT_STDP = 0,
    MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP = 1
} MiniSNNLearningMode;

typedef struct
{
    int enabled;
    MiniSNNPlasticityRule rule;
    MiniSNNLearningMode learning_mode;
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

typedef enum
{
    MINISNN_REWARD_MODE_RSTDP = 1
} MiniSNNRewardMode;

typedef struct
{
    int enabled;
    MiniSNNRewardMode mode;
    double learning_rate;
    double eligibility_tau;
    double eligibility_min;
    double eligibility_max;
    double reward_min;
    double reward_max;
    int clip_reward;
} MiniSNNRewardConfig;

typedef struct
{
    unsigned long long reward_event_count;
    unsigned long long positive_reward_event_count;
    unsigned long long negative_reward_event_count;
    unsigned long long zero_reward_event_count;
    unsigned long long eligibility_potentiation_events;
    unsigned long long eligibility_depression_events;
    unsigned long long eligibility_clamp_min_events;
    unsigned long long eligibility_clamp_max_events;
    unsigned long long reward_potentiation_events;
    unsigned long long reward_depression_events;
    unsigned long long weight_clamp_min_events;
    unsigned long long weight_clamp_max_events;
    size_t eligible_connection_count;
    size_t modified_connection_count;
    size_t last_active_eligibility_count;
    size_t last_modified_connection_count;
    unsigned int last_reward_component_count;
    double cumulative_raw_reward;
    double cumulative_applied_reward;
    double cumulative_positive_reward;
    double cumulative_negative_reward;
    double cumulative_absolute_reward;
    double total_signed_weight_change;
    double total_absolute_weight_change;
    double max_absolute_weight_change;
    double eligibility_final_mean;
    double eligibility_final_min;
    double eligibility_final_max;
    double eligibility_final_mean_absolute;
    double eligibility_max_absolute_observed;
    double last_raw_reward;
    double last_applied_reward;
    double last_weight_signed_change;
    double last_weight_absolute_change;
    unsigned long long last_weight_clamp_min_count;
    unsigned long long last_weight_clamp_max_count;
} MiniSNNRewardStats;

typedef struct
{
    int eligible;
    double eligibility;
    double max_absolute_eligibility;
    unsigned long long reward_update_count;
    double reward_signed_change;
    double reward_absolute_change;
} MiniSNNRewardConnectionStats;

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
