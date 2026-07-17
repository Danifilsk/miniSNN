#ifndef MINISNN_TYPES_H
#define MINISNN_TYPES_H

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    MINISNN_NEURON_EXCITATORY = 0,
    MINISNN_NEURON_INHIBITORY = 1
} MiniSNNNeuronType;

typedef enum
{
    MINISNN_NEURON_MODEL_LIF = 0,
    MINISNN_NEURON_MODEL_ADEX = 1,
    MINISNN_NEURON_MODEL_HODGKIN_HUXLEY = 2
} MiniSNNNeuronModel;

typedef struct
{
    double capacitance;
    double g_leak;
    double e_leak;
    double delta_t;
    double v_threshold;
    double tau_w;
    double a;
    double b;
    double v_reset;
    double v_peak;
    double dt;
} MiniSNNAdExConfig;

typedef struct
{
    double capacitance;
    double g_na;
    double g_k;
    double g_leak;
    double e_na;
    double e_k;
    double e_leak;
    double v_init;
    double spike_threshold;
    double dt;
} MiniSNNHodgkinHuxleyConfig;

typedef struct
{
    int supports_voltage;
    int supports_spike_event;
    int supports_homeostatic_threshold;
    int supports_adaptation_state;
    int supports_hh_gates;
} MiniSNNNeuronModelCapabilities;

typedef struct
{
    double voltage;
    double adaptation;
    int spike;
} MiniSNNAdExState;

typedef struct
{
    double voltage;
    double m;
    double h;
    double n;
    int spike;
} MiniSNNHodgkinHuxleyState;

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

typedef struct
{
    uint64_t connection_key;
    size_t source;
    size_t target;
    double magnitude;
    unsigned int delay;
    unsigned int inherited_from;
} MiniSNNConnectionGene;

typedef enum
{
    MINISNN_TOPOLOGY_ADD = 0,
    MINISNN_TOPOLOGY_REMOVE = 1,
    MINISNN_TOPOLOGY_REWIRE = 2,
    MINISNN_TOPOLOGY_SET_DELAY = 3
} MiniSNNTopologyOperationType;

typedef struct
{
    MiniSNNTopologyOperationType type;
    size_t source;
    size_t target;
    size_t new_source;
    size_t new_target;
    double magnitude;
    unsigned int delay;
    int allow_self_connection;
} MiniSNNTopologyOperation;

#define MINISNN_TOPOLOGY_REASON_MAX 95

typedef struct
{
    size_t requested_operations;
    size_t applied_operations;
    size_t connections_added;
    size_t connections_removed;
    size_t connections_rewired;
    size_t delays_changed;
    uint64_t signature_before;
    uint64_t signature_after;
    int success;
    char reason[MINISNN_TOPOLOGY_REASON_MAX + 1];
} MiniSNNTopologyPatchResult;

typedef struct
{
    int enabled;
    unsigned int maintenance_interval_steps;
    unsigned int grace_period_steps;
    int pruning_enabled;
    double prune_weight_threshold;
    double prune_activity_threshold;
    size_t max_prunes_per_interval;
    int growth_enabled;
    size_t growth_candidate_count;
    double growth_score_threshold;
    size_t max_growth_per_interval;
    uint64_t growth_seed;
    double new_exc_weight;
    double new_inh_magnitude;
    unsigned int new_delay;
    size_t min_connections;
    size_t max_connections;
    int allow_self_connections;
    int allow_inh_to_inh;
} MiniSNNStructuralPlasticityConfig;

typedef struct
{
    unsigned long long maintenance_count;
    unsigned long long add_attempt_count;
    unsigned long long add_success_count;
    unsigned long long add_rejected_count;
    unsigned long long remove_attempt_count;
    unsigned long long remove_success_count;
    unsigned long long remove_rejected_count;
    unsigned long long rewire_count;
    unsigned long long delay_change_count;
    unsigned long long rebuild_count;
    size_t initial_connection_count;
    size_t current_connection_count;
    size_t minimum_connection_count_observed;
    size_t maximum_connection_count_observed;
    double cumulative_growth_score;
    double cumulative_pruned_usage;
    uint64_t initial_topology_signature;
    uint64_t current_topology_signature;
} MiniSNNStructuralStats;

typedef struct
{
    uint64_t connection_key;
    unsigned long long birth_step;
    unsigned long long last_structural_update_step;
    double max_absolute_weight;
    double activity_score;
    unsigned long long prune_candidate_count;
    unsigned int growth_origin;
} MiniSNNStructuralConnectionState;

typedef enum
{
    MINISNN_STRUCTURAL_RESET_STATE = 0,
    MINISNN_STRUCTURAL_RESTORE_INITIAL_TOPOLOGY = 1
} MiniSNNStructuralResetMode;

typedef struct
{
    unsigned long long step;
    unsigned long long event_index;
    MiniSNNTopologyOperationType type;
    size_t source;
    size_t target;
    size_t new_source;
    size_t new_target;
    uint64_t connection_key;
    uint64_t new_connection_key;
    double magnitude;
    unsigned int delay;
    double activity_score;
    double growth_score;
    unsigned long long age_steps;
    int applied;
    char reason[MINISNN_TOPOLOGY_REASON_MAX + 1];
    uint64_t signature_before;
    uint64_t signature_after;
} MiniSNNStructuralEvent;

#endif
