#include "agent_cycle_checkpoint_internal.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "minisnn_internal.h"
#include "network.h"

#define CYCLE_NETWORK_MAGIC UINT32_C(0x4e433731) /* NC71 */
#define CYCLE_NETWORK_VERSION UINT32_C(2)
#define CYCLE_FNV_OFFSET UINT64_C(14695981039346656037)
#define CYCLE_FNV_PRIME UINT64_C(1099511628211)
#define CYCLE_MAX_STRUCTURAL_EVENTS UINT64_C(1000000)

_Static_assert(sizeof(uint64_t) == 8,
               "checkpoint format requires an eight-byte uint64_t");

static int write_u32(FILE *file, uint32_t value)
{
    unsigned char bytes[4];
    for (unsigned int shift = 0U; shift < 32U; shift += 8U)
        bytes[shift / 8U] = (unsigned char)((value >> shift) & 0xffU);
    return fwrite(bytes, 1U, sizeof(bytes), file) == sizeof(bytes);
}

static int write_u64(FILE *file, uint64_t value)
{
    unsigned char bytes[8];
    for (unsigned int shift = 0U; shift < 64U; shift += 8U)
        bytes[shift / 8U] = (unsigned char)((value >> shift) & 0xffU);
    return fwrite(bytes, 1U, sizeof(bytes), file) == sizeof(bytes);
}

static int read_u32(FILE *file, uint32_t *out_value)
{
    unsigned char bytes[4];
    uint32_t value = 0U;
    if (out_value == NULL || fread(bytes, 1U, sizeof(bytes), file) != sizeof(bytes))
        return 0;
    for (unsigned int shift = 0U; shift < 32U; shift += 8U)
        value |= (uint32_t)bytes[shift / 8U] << shift;
    *out_value = value;
    return 1;
}

static int read_u64(FILE *file, uint64_t *out_value)
{
    unsigned char bytes[8];
    uint64_t value = 0U;
    if (out_value == NULL || fread(bytes, 1U, sizeof(bytes), file) != sizeof(bytes))
        return 0;
    for (unsigned int shift = 0U; shift < 64U; shift += 8U)
        value |= (uint64_t)bytes[shift / 8U] << shift;
    *out_value = value;
    return 1;
}

/* The checkpoint format stores counters as exactly 64 bits. Keep the
 * serialized value separate from unsigned long long for LP64 portability. */
static int read_unsigned_long_long(FILE *file, unsigned long long *out_value)
{
    uint64_t serialized;
    if (file == NULL || out_value == NULL || !read_u64(file, &serialized) ||
        serialized > ULLONG_MAX)
        return 0;
    *out_value = (unsigned long long)serialized;
    return 1;
}

static uint64_t double_bits(double value)
{
    uint64_t bits = 0U;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static double bits_double(uint64_t bits)
{
    double value = 0.0;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int write_double(FILE *file, double value)
{
    return isfinite(value) && write_u64(file, double_bits(value));
}

/* Statistics may use infinities internally to represent an unobserved
 * minimum or maximum. Checkpoints never emit non-finite values; the neutral
 * zero is the stable representation of an as-yet unobserved statistic. */
static double checkpoint_stat_value(double value)
{
    return isfinite(value) ? value : 0.0;
}

static int read_double(FILE *file, double *out_value)
{
    uint64_t bits;
    if (!read_u64(file, &bits))
        return 0;
    *out_value = bits_double(bits);
    return isfinite(*out_value);
}

static size_t bounded_string_length(const char *text, size_t maximum)
{
    size_t length = 0U;
    if (text == NULL)
        return maximum;
    while (length < maximum && text[length] != '\0')
        length++;
    return length;
}

static void hash_byte(uint64_t *hash, unsigned char value)
{
    *hash ^= (uint64_t)value;
    *hash *= CYCLE_FNV_PRIME;
}

static void hash_u32(uint64_t *hash, uint32_t value)
{
    for (unsigned int shift = 0U; shift < 32U; shift += 8U)
        hash_byte(hash, (unsigned char)((value >> shift) & 0xffU));
}

static void hash_u64(uint64_t *hash, uint64_t value)
{
    for (unsigned int shift = 0U; shift < 64U; shift += 8U)
        hash_byte(hash, (unsigned char)((value >> shift) & 0xffU));
}

static int write_model(FILE *file, const NeuronModelConfig *config)
{
    if (file == NULL || config == NULL || !neuron_model_validate_config(config) ||
        !write_u32(file, (uint32_t)config->model))
        return 0;
    if (config->model == MINISNN_NEURON_MODEL_LIF)
    {
        const LIFParameters *p = &config->data.lif;
        return write_double(file, p->dt) && write_double(file, p->tau) &&
            write_double(file, p->v_rest) && write_double(file, p->v_reset) &&
            write_double(file, p->v_threshold) && write_double(file, p->resistance);
    }
    if (config->model == MINISNN_NEURON_MODEL_ADEX)
    {
        const AdExParameters *p = &config->data.adex;
        return write_double(file, p->capacitance) && write_double(file, p->g_leak) &&
            write_double(file, p->e_leak) && write_double(file, p->delta_t) &&
            write_double(file, p->v_threshold) && write_double(file, p->tau_w) &&
            write_double(file, p->a) && write_double(file, p->b) &&
            write_double(file, p->v_reset) && write_double(file, p->v_peak) &&
            write_double(file, p->dt);
    }
    {
        const HodgkinHuxleyParameters *p = &config->data.hh;
        return write_double(file, p->capacitance) && write_double(file, p->g_na) &&
            write_double(file, p->g_k) && write_double(file, p->g_leak) &&
            write_double(file, p->e_na) && write_double(file, p->e_k) &&
            write_double(file, p->e_leak) && write_double(file, p->v_init) &&
            write_double(file, p->spike_threshold) && write_double(file, p->dt);
    }
}

static int read_model(FILE *file, NetworkConfig *out_config)
{
    uint32_t model;
    if (file == NULL || out_config == NULL || !read_u32(file, &model))
        return 0;
    memset(out_config, 0, sizeof(*out_config));
    out_config->neuron_model = (MiniSNNNeuronModel)model;
    if (out_config->neuron_model == MINISNN_NEURON_MODEL_LIF)
    {
        LIFParameters *p = &out_config->lif;
        return read_double(file, &p->dt) && read_double(file, &p->tau) &&
            read_double(file, &p->v_rest) && read_double(file, &p->v_reset) &&
            read_double(file, &p->v_threshold) && read_double(file, &p->resistance);
    }
    if (out_config->neuron_model == MINISNN_NEURON_MODEL_ADEX)
    {
        AdExParameters *p = &out_config->adex;
        return read_double(file, &p->capacitance) && read_double(file, &p->g_leak) &&
            read_double(file, &p->e_leak) && read_double(file, &p->delta_t) &&
            read_double(file, &p->v_threshold) && read_double(file, &p->tau_w) &&
            read_double(file, &p->a) && read_double(file, &p->b) &&
            read_double(file, &p->v_reset) && read_double(file, &p->v_peak) &&
            read_double(file, &p->dt);
    }
    if (out_config->neuron_model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY)
    {
        HodgkinHuxleyParameters *p = &out_config->hodgkin_huxley;
        return read_double(file, &p->capacitance) && read_double(file, &p->g_na) &&
            read_double(file, &p->g_k) && read_double(file, &p->g_leak) &&
            read_double(file, &p->e_na) && read_double(file, &p->e_k) &&
            read_double(file, &p->e_leak) && read_double(file, &p->v_init) &&
            read_double(file, &p->spike_threshold) && read_double(file, &p->dt);
    }
    return 0;
}

static int write_plasticity_config(FILE *file, const MiniSNNPlasticityConfig *c)
{
    return c != NULL && write_u32(file, (uint32_t)c->enabled) &&
        write_u32(file, (uint32_t)c->rule) && write_u32(file, (uint32_t)c->learning_mode) &&
        write_double(file, c->a_plus) && write_double(file, c->a_minus) &&
        write_double(file, c->tau_plus) && write_double(file, c->tau_minus) &&
        write_double(file, c->trace_increment) && write_double(file, c->weight_min) &&
        write_double(file, c->weight_max);
}

static int read_plasticity_config(FILE *file, MiniSNNPlasticityConfig *c)
{
    uint32_t enabled, rule, mode;
    if (c == NULL || !read_u32(file, &enabled) || !read_u32(file, &rule) ||
        !read_u32(file, &mode))
        return 0;
    c->enabled = (int)enabled;
    c->rule = (MiniSNNPlasticityRule)rule;
    c->learning_mode = (MiniSNNLearningMode)mode;
    return read_double(file, &c->a_plus) && read_double(file, &c->a_minus) &&
        read_double(file, &c->tau_plus) && read_double(file, &c->tau_minus) &&
        read_double(file, &c->trace_increment) && read_double(file, &c->weight_min) &&
        read_double(file, &c->weight_max);
}

static int write_reward_config(FILE *file, const MiniSNNRewardConfig *c)
{
    return c != NULL && write_u32(file, (uint32_t)c->enabled) &&
        write_u32(file, (uint32_t)c->mode) && write_double(file, c->learning_rate) &&
        write_double(file, c->eligibility_tau) && write_double(file, c->eligibility_min) &&
        write_double(file, c->eligibility_max) && write_double(file, c->reward_min) &&
        write_double(file, c->reward_max) && write_u32(file, (uint32_t)c->clip_reward);
}

static int read_reward_config(FILE *file, MiniSNNRewardConfig *c)
{
    uint32_t enabled, mode, clip;
    if (c == NULL || !read_u32(file, &enabled) || !read_u32(file, &mode) ||
        !read_double(file, &c->learning_rate) || !read_double(file, &c->eligibility_tau) ||
        !read_double(file, &c->eligibility_min) || !read_double(file, &c->eligibility_max) ||
        !read_double(file, &c->reward_min) || !read_double(file, &c->reward_max) ||
        !read_u32(file, &clip))
        return 0;
    c->enabled = (int)enabled;
    c->mode = (MiniSNNRewardMode)mode;
    c->clip_reward = (int)clip;
    return enabled <= 1U && clip <= 1U;
}

static int write_homeostasis_config(FILE *file, const MiniSNNHomeostasisConfig *c)
{
    return c != NULL && write_u32(file, (uint32_t)c->enabled) &&
        write_u32(file, (uint32_t)c->intrinsic_enabled) &&
        write_double(file, c->target_rate) && write_double(file, c->rate_tau) &&
        write_u32(file, c->update_interval_steps) && write_double(file, c->threshold_eta) &&
        write_double(file, c->threshold_min) && write_double(file, c->threshold_max) &&
        write_u32(file, (uint32_t)c->synaptic_scaling_enabled) &&
        write_double(file, c->scaling_eta) && write_double(file, c->scaling_min_factor) &&
        write_double(file, c->scaling_max_factor) && write_double(file, c->scaling_weight_min) &&
        write_double(file, c->scaling_weight_max) &&
        write_u32(file, (uint32_t)c->inhibitory_gain_enabled) &&
        write_double(file, c->inhibitory_gain_initial) &&
        write_double(file, c->inhibitory_gain_eta) && write_double(file, c->inhibitory_gain_min) &&
        write_double(file, c->inhibitory_gain_max);
}

static int read_homeostasis_config(FILE *file, MiniSNNHomeostasisConfig *c)
{
    uint32_t enabled, intrinsic, scaling, gain;
    if (c == NULL || !read_u32(file, &enabled) || !read_u32(file, &intrinsic) ||
        !read_double(file, &c->target_rate) || !read_double(file, &c->rate_tau) ||
        !read_u32(file, &c->update_interval_steps) || !read_double(file, &c->threshold_eta) ||
        !read_double(file, &c->threshold_min) || !read_double(file, &c->threshold_max) ||
        !read_u32(file, &scaling) || !read_double(file, &c->scaling_eta) ||
        !read_double(file, &c->scaling_min_factor) || !read_double(file, &c->scaling_max_factor) ||
        !read_double(file, &c->scaling_weight_min) || !read_double(file, &c->scaling_weight_max) ||
        !read_u32(file, &gain) || !read_double(file, &c->inhibitory_gain_initial) ||
        !read_double(file, &c->inhibitory_gain_eta) || !read_double(file, &c->inhibitory_gain_min) ||
        !read_double(file, &c->inhibitory_gain_max))
        return 0;
    c->enabled = (int)enabled;
    c->intrinsic_enabled = (int)intrinsic;
    c->synaptic_scaling_enabled = (int)scaling;
    c->inhibitory_gain_enabled = (int)gain;
    return enabled <= 1U && intrinsic <= 1U && scaling <= 1U && gain <= 1U;
}

static int write_structural_config(FILE *file,
                                   const MiniSNNStructuralPlasticityConfig *c)
{
    return c != NULL && write_u32(file, (uint32_t)c->enabled) &&
        write_u32(file, c->maintenance_interval_steps) &&
        write_u32(file, c->grace_period_steps) &&
        write_u32(file, (uint32_t)c->pruning_enabled) &&
        write_double(file, c->prune_weight_threshold) &&
        write_double(file, c->prune_activity_threshold) &&
        write_u64(file, (uint64_t)c->max_prunes_per_interval) &&
        write_u32(file, (uint32_t)c->growth_enabled) &&
        write_u64(file, (uint64_t)c->growth_candidate_count) &&
        write_double(file, c->growth_score_threshold) &&
        write_u64(file, (uint64_t)c->max_growth_per_interval) &&
        write_u64(file, c->growth_seed) && write_double(file, c->new_exc_weight) &&
        write_double(file, c->new_inh_magnitude) && write_u32(file, c->new_delay) &&
        write_u64(file, (uint64_t)c->min_connections) &&
        write_u64(file, (uint64_t)c->max_connections) &&
        write_u32(file, (uint32_t)c->allow_self_connections) &&
        write_u32(file, (uint32_t)c->allow_inh_to_inh);
}

static int read_structural_config(FILE *file,
                                  MiniSNNStructuralPlasticityConfig *c)
{
    uint32_t enabled, pruning, growth, self, inh;
    uint64_t max_prunes, candidates, max_growth, minimum, maximum;
    if (c == NULL || !read_u32(file, &enabled) || !read_u32(file, &c->maintenance_interval_steps) ||
        !read_u32(file, &c->grace_period_steps) || !read_u32(file, &pruning) ||
        !read_double(file, &c->prune_weight_threshold) ||
        !read_double(file, &c->prune_activity_threshold) || !read_u64(file, &max_prunes) ||
        max_prunes > SIZE_MAX || !read_u32(file, &growth) ||
        !read_u64(file, &candidates) || candidates > SIZE_MAX ||
        !read_double(file, &c->growth_score_threshold) || !read_u64(file, &max_growth) ||
        max_growth > SIZE_MAX || !read_u64(file, &c->growth_seed) ||
        !read_double(file, &c->new_exc_weight) || !read_double(file, &c->new_inh_magnitude) ||
        !read_u32(file, &c->new_delay) || !read_u64(file, &minimum) || minimum > SIZE_MAX ||
        !read_u64(file, &maximum) || maximum > SIZE_MAX || !read_u32(file, &self) ||
        !read_u32(file, &inh))
        return 0;
    if (enabled > 1U || pruning > 1U || growth > 1U || self > 1U || inh > 1U)
        return 0;
    c->enabled = (int)enabled;
    c->pruning_enabled = (int)pruning;
    c->max_prunes_per_interval = (size_t)max_prunes;
    c->growth_enabled = (int)growth;
    c->growth_candidate_count = (size_t)candidates;
    c->max_growth_per_interval = (size_t)max_growth;
    c->min_connections = (size_t)minimum;
    c->max_connections = (size_t)maximum;
    c->allow_self_connections = (int)self;
    c->allow_inh_to_inh = (int)inh;
    return 1;
}

static int write_plasticity_stats(FILE *file, const MiniSNNPlasticityStats *stats)
{
    return stats != NULL && write_u64(file, stats->potentiation_events) &&
        write_u64(file, stats->depression_events) &&
        write_u64(file, stats->clamp_min_events) &&
        write_u64(file, stats->clamp_max_events) &&
        write_u64(file, (uint64_t)stats->eligible_connections) &&
        write_u64(file, (uint64_t)stats->modified_connections) &&
        write_double(file, stats->total_signed_change) &&
        write_double(file, stats->total_absolute_change) &&
        write_double(file, stats->max_absolute_change);
}

static int read_plasticity_stats(FILE *file, MiniSNNPlasticityStats *stats)
{
    uint64_t eligible, modified;
    return stats != NULL && read_unsigned_long_long(file, &stats->potentiation_events) &&
        read_unsigned_long_long(file, &stats->depression_events) &&
        read_unsigned_long_long(file, &stats->clamp_min_events) &&
        read_unsigned_long_long(file, &stats->clamp_max_events) && read_u64(file, &eligible) &&
        eligible <= SIZE_MAX && read_u64(file, &modified) && modified <= SIZE_MAX &&
        read_double(file, &stats->total_signed_change) &&
        read_double(file, &stats->total_absolute_change) &&
        read_double(file, &stats->max_absolute_change) &&
        ((stats->eligible_connections = (size_t)eligible),
         (stats->modified_connections = (size_t)modified), 1);
}

static int write_reward_stats(FILE *file, const MiniSNNRewardStats *stats)
{
    return stats != NULL &&
        write_u64(file, stats->reward_event_count) &&
        write_u64(file, stats->positive_reward_event_count) &&
        write_u64(file, stats->negative_reward_event_count) &&
        write_u64(file, stats->zero_reward_event_count) &&
        write_u64(file, stats->eligibility_potentiation_events) &&
        write_u64(file, stats->eligibility_depression_events) &&
        write_u64(file, stats->eligibility_clamp_min_events) &&
        write_u64(file, stats->eligibility_clamp_max_events) &&
        write_u64(file, stats->reward_potentiation_events) &&
        write_u64(file, stats->reward_depression_events) &&
        write_u64(file, stats->weight_clamp_min_events) &&
        write_u64(file, stats->weight_clamp_max_events) &&
        write_u64(file, (uint64_t)stats->eligible_connection_count) &&
        write_u64(file, (uint64_t)stats->modified_connection_count) &&
        write_u64(file, (uint64_t)stats->last_active_eligibility_count) &&
        write_u64(file, (uint64_t)stats->last_modified_connection_count) &&
        write_u32(file, stats->last_reward_component_count) &&
        write_double(file, stats->cumulative_raw_reward) &&
        write_double(file, stats->cumulative_applied_reward) &&
        write_double(file, stats->cumulative_positive_reward) &&
        write_double(file, stats->cumulative_negative_reward) &&
        write_double(file, stats->cumulative_absolute_reward) &&
        write_double(file, stats->total_signed_weight_change) &&
        write_double(file, stats->total_absolute_weight_change) &&
        write_double(file, stats->max_absolute_weight_change) &&
        write_double(file, stats->eligibility_final_mean) &&
        write_double(file, stats->eligibility_final_min) &&
        write_double(file, stats->eligibility_final_max) &&
        write_double(file, stats->eligibility_final_mean_absolute) &&
        write_double(file, stats->eligibility_max_absolute_observed) &&
        write_double(file, stats->last_raw_reward) &&
        write_double(file, stats->last_applied_reward) &&
        write_double(file, stats->last_weight_signed_change) &&
        write_double(file, stats->last_weight_absolute_change) &&
        write_u64(file, stats->last_weight_clamp_min_count) &&
        write_u64(file, stats->last_weight_clamp_max_count);
}

static int read_reward_stats(FILE *file, MiniSNNRewardStats *stats)
{
    uint64_t values[4];
    if (stats == NULL ||
        !read_unsigned_long_long(file, &stats->reward_event_count) ||
        !read_unsigned_long_long(file, &stats->positive_reward_event_count) ||
        !read_unsigned_long_long(file, &stats->negative_reward_event_count) ||
        !read_unsigned_long_long(file, &stats->zero_reward_event_count) ||
        !read_unsigned_long_long(file, &stats->eligibility_potentiation_events) ||
        !read_unsigned_long_long(file, &stats->eligibility_depression_events) ||
        !read_unsigned_long_long(file, &stats->eligibility_clamp_min_events) ||
        !read_unsigned_long_long(file, &stats->eligibility_clamp_max_events) ||
        !read_unsigned_long_long(file, &stats->reward_potentiation_events) ||
        !read_unsigned_long_long(file, &stats->reward_depression_events) ||
        !read_unsigned_long_long(file, &stats->weight_clamp_min_events) ||
        !read_unsigned_long_long(file, &stats->weight_clamp_max_events))
        return 0;
    for (size_t index = 0U; index < 4U; index++)
        if (!read_u64(file, &values[index]) || values[index] > SIZE_MAX)
            return 0;
    stats->eligible_connection_count = (size_t)values[0];
    stats->modified_connection_count = (size_t)values[1];
    stats->last_active_eligibility_count = (size_t)values[2];
    stats->last_modified_connection_count = (size_t)values[3];
    return read_u32(file, &stats->last_reward_component_count) &&
        read_double(file, &stats->cumulative_raw_reward) &&
        read_double(file, &stats->cumulative_applied_reward) &&
        read_double(file, &stats->cumulative_positive_reward) &&
        read_double(file, &stats->cumulative_negative_reward) &&
        read_double(file, &stats->cumulative_absolute_reward) &&
        read_double(file, &stats->total_signed_weight_change) &&
        read_double(file, &stats->total_absolute_weight_change) &&
        read_double(file, &stats->max_absolute_weight_change) &&
        read_double(file, &stats->eligibility_final_mean) &&
        read_double(file, &stats->eligibility_final_min) &&
        read_double(file, &stats->eligibility_final_max) &&
        read_double(file, &stats->eligibility_final_mean_absolute) &&
        read_double(file, &stats->eligibility_max_absolute_observed) &&
        read_double(file, &stats->last_raw_reward) &&
        read_double(file, &stats->last_applied_reward) &&
        read_double(file, &stats->last_weight_signed_change) &&
        read_double(file, &stats->last_weight_absolute_change) &&
        read_unsigned_long_long(file, &stats->last_weight_clamp_min_count) &&
        read_unsigned_long_long(file, &stats->last_weight_clamp_max_count);
}

static int write_homeostasis_stats(FILE *file, const MiniSNNHomeostasisStats *stats)
{
    return stats != NULL && write_u64(file, stats->update_count) &&
        write_u64(file, stats->threshold_increase_events) &&
        write_u64(file, stats->threshold_decrease_events) &&
        write_u64(file, stats->threshold_clamp_min_events) &&
        write_u64(file, stats->threshold_clamp_max_events) &&
        write_u64(file, stats->threshold_modified_neuron_count) &&
        write_u64(file, stats->scaling_events) &&
        write_u64(file, stats->scaling_connections_modified) &&
        write_u64(file, stats->scaling_clamp_min_events) &&
        write_u64(file, stats->scaling_clamp_max_events) &&
        write_u64(file, stats->scaling_zero_sum_skips) &&
        write_u64(file, stats->inhibitory_gain_increase_events) &&
        write_u64(file, stats->inhibitory_gain_decrease_events) &&
        write_u64(file, stats->inhibitory_gain_clamp_min_events) &&
        write_u64(file, stats->inhibitory_gain_clamp_max_events) &&
        write_double(file, checkpoint_stat_value(stats->total_threshold_absolute_change)) &&
        write_double(file, checkpoint_stat_value(stats->total_scaling_absolute_change)) &&
        write_double(file, checkpoint_stat_value(stats->total_scaling_signed_change)) &&
        write_double(file, checkpoint_stat_value(stats->population_rate_sum)) &&
        write_double(file, checkpoint_stat_value(stats->population_rate_min)) &&
        write_double(file, checkpoint_stat_value(stats->population_rate_max)) &&
        write_u64(file, stats->population_rate_sample_count) &&
        write_double(file, checkpoint_stat_value(stats->rate_error_sum)) &&
        write_double(file, checkpoint_stat_value(stats->rate_error_absolute_sum)) &&
        write_double(file, checkpoint_stat_value(stats->scaling_factor_sum)) &&
        write_double(file, checkpoint_stat_value(stats->scaling_factor_min)) &&
        write_double(file, checkpoint_stat_value(stats->scaling_factor_max)) &&
        write_u64(file, stats->scaling_factor_count) &&
        write_double(file, checkpoint_stat_value(stats->inhibitory_gain_min_observed)) &&
        write_double(file, checkpoint_stat_value(stats->inhibitory_gain_max_observed)) &&
        write_double(file, checkpoint_stat_value(stats->final_population_rate)) &&
        write_double(file, checkpoint_stat_value(stats->final_inhibitory_gain));
}

static int read_homeostasis_stats(FILE *file, MiniSNNHomeostasisStats *stats)
{
    return stats != NULL && read_unsigned_long_long(file, &stats->update_count) &&
        read_unsigned_long_long(file, &stats->threshold_increase_events) &&
        read_unsigned_long_long(file, &stats->threshold_decrease_events) &&
        read_unsigned_long_long(file, &stats->threshold_clamp_min_events) &&
        read_unsigned_long_long(file, &stats->threshold_clamp_max_events) &&
        read_unsigned_long_long(file, &stats->threshold_modified_neuron_count) &&
        read_unsigned_long_long(file, &stats->scaling_events) &&
        read_unsigned_long_long(file, &stats->scaling_connections_modified) &&
        read_unsigned_long_long(file, &stats->scaling_clamp_min_events) &&
        read_unsigned_long_long(file, &stats->scaling_clamp_max_events) &&
        read_unsigned_long_long(file, &stats->scaling_zero_sum_skips) &&
        read_unsigned_long_long(file, &stats->inhibitory_gain_increase_events) &&
        read_unsigned_long_long(file, &stats->inhibitory_gain_decrease_events) &&
        read_unsigned_long_long(file, &stats->inhibitory_gain_clamp_min_events) &&
        read_unsigned_long_long(file, &stats->inhibitory_gain_clamp_max_events) &&
        read_double(file, &stats->total_threshold_absolute_change) &&
        read_double(file, &stats->total_scaling_absolute_change) &&
        read_double(file, &stats->total_scaling_signed_change) &&
        read_double(file, &stats->population_rate_sum) &&
        read_double(file, &stats->population_rate_min) &&
        read_double(file, &stats->population_rate_max) &&
        read_unsigned_long_long(file, &stats->population_rate_sample_count) &&
        read_double(file, &stats->rate_error_sum) &&
        read_double(file, &stats->rate_error_absolute_sum) &&
        read_double(file, &stats->scaling_factor_sum) &&
        read_double(file, &stats->scaling_factor_min) &&
        read_double(file, &stats->scaling_factor_max) &&
        read_unsigned_long_long(file, &stats->scaling_factor_count) &&
        read_double(file, &stats->inhibitory_gain_min_observed) &&
        read_double(file, &stats->inhibitory_gain_max_observed) &&
        read_double(file, &stats->final_population_rate) &&
        read_double(file, &stats->final_inhibitory_gain);
}

static int write_structural_stats(FILE *file, const MiniSNNStructuralStats *stats)
{
    return stats != NULL && write_u64(file, stats->maintenance_count) &&
        write_u64(file, stats->add_attempt_count) &&
        write_u64(file, stats->add_success_count) &&
        write_u64(file, stats->add_rejected_count) &&
        write_u64(file, stats->remove_attempt_count) &&
        write_u64(file, stats->remove_success_count) &&
        write_u64(file, stats->remove_rejected_count) &&
        write_u64(file, stats->rewire_count) && write_u64(file, stats->delay_change_count) &&
        write_u64(file, stats->rebuild_count) &&
        write_u64(file, (uint64_t)stats->initial_connection_count) &&
        write_u64(file, (uint64_t)stats->current_connection_count) &&
        write_u64(file, (uint64_t)stats->minimum_connection_count_observed) &&
        write_u64(file, (uint64_t)stats->maximum_connection_count_observed) &&
        write_double(file, stats->cumulative_growth_score) &&
        write_double(file, stats->cumulative_pruned_usage) &&
        write_u64(file, stats->initial_topology_signature) &&
        write_u64(file, stats->current_topology_signature);
}

static int read_structural_stats(FILE *file, MiniSNNStructuralStats *stats)
{
    uint64_t counts[4];
    if (stats == NULL || !read_unsigned_long_long(file, &stats->maintenance_count) ||
        !read_unsigned_long_long(file, &stats->add_attempt_count) ||
        !read_unsigned_long_long(file, &stats->add_success_count) ||
        !read_unsigned_long_long(file, &stats->add_rejected_count) ||
        !read_unsigned_long_long(file, &stats->remove_attempt_count) ||
        !read_unsigned_long_long(file, &stats->remove_success_count) ||
        !read_unsigned_long_long(file, &stats->remove_rejected_count) ||
        !read_unsigned_long_long(file, &stats->rewire_count) ||
        !read_unsigned_long_long(file, &stats->delay_change_count) ||
        !read_unsigned_long_long(file, &stats->rebuild_count))
        return 0;
    for (size_t index = 0U; index < 4U; index++)
        if (!read_u64(file, &counts[index]) || counts[index] > SIZE_MAX)
            return 0;
    stats->initial_connection_count = (size_t)counts[0];
    stats->current_connection_count = (size_t)counts[1];
    stats->minimum_connection_count_observed = (size_t)counts[2];
    stats->maximum_connection_count_observed = (size_t)counts[3];
    return read_double(file, &stats->cumulative_growth_score) &&
        read_double(file, &stats->cumulative_pruned_usage) &&
        read_u64(file, &stats->initial_topology_signature) &&
        read_u64(file, &stats->current_topology_signature);
}

static int write_genome(FILE *file, const StructureGenome *genome)
{
    if (file == NULL || genome == NULL ||
        !write_u64(file, (uint64_t)genome->connection_count))
        return 0;
    for (size_t index = 0U; index < genome->connection_count; index++)
    {
        const MiniSNNConnectionGene *gene = &genome->connections[index];
        if (!write_u64(file, gene->connection_key) ||
            !write_u64(file, (uint64_t)gene->source) ||
            !write_u64(file, (uint64_t)gene->target) ||
            !write_double(file, gene->magnitude) || !write_u32(file, gene->delay) ||
            !write_u32(file, gene->inherited_from))
            return 0;
    }
    return 1;
}

static int read_genome(FILE *file, StructureGenome *genome)
{
    uint64_t count;
    MiniSNNConnectionGene *genes = NULL;
    int ok = 0;
    if (file == NULL || genome == NULL || !read_u64(file, &count) ||
        count > SIZE_MAX || count > UINT64_C(10000000))
        return 0;
    if (count > 0U)
    {
        genes = calloc((size_t)count, sizeof(*genes));
        if (genes == NULL)
            return 0;
    }
    for (size_t index = 0U; index < (size_t)count; index++)
    {
        uint64_t source, target;
        if (!read_u64(file, &genes[index].connection_key) ||
            !read_u64(file, &source) || source > SIZE_MAX ||
            !read_u64(file, &target) || target > SIZE_MAX ||
            !read_double(file, &genes[index].magnitude) ||
            !read_u32(file, &genes[index].delay) ||
            !read_u32(file, &genes[index].inherited_from))
            goto done;
        genes[index].source = (size_t)source;
        genes[index].target = (size_t)target;
    }
    ok = structure_genome_set(genome, genes, (size_t)count);
done:
    free(genes);
    return ok;
}

static int write_structural_event(FILE *file, const MiniSNNStructuralEvent *event)
{
    size_t reason_length;
    if (file == NULL || event == NULL ||
        (reason_length = bounded_string_length(event->reason, sizeof(event->reason))) >=
            sizeof(event->reason))
        return 0;
    return write_u64(file, event->step) && write_u64(file, event->event_index) &&
        write_u32(file, (uint32_t)event->type) &&
        write_u64(file, (uint64_t)event->source) && write_u64(file, (uint64_t)event->target) &&
        write_u64(file, (uint64_t)event->new_source) &&
        write_u64(file, (uint64_t)event->new_target) &&
        write_u64(file, event->connection_key) && write_u64(file, event->new_connection_key) &&
        write_double(file, event->magnitude) && write_u32(file, event->delay) &&
        write_double(file, event->activity_score) && write_double(file, event->growth_score) &&
        write_u64(file, event->age_steps) && write_u32(file, (uint32_t)event->applied) &&
        write_u32(file, (uint32_t)reason_length) &&
        fwrite(event->reason, 1U, reason_length, file) == reason_length &&
        write_u64(file, event->signature_before) && write_u64(file, event->signature_after);
}

static int read_structural_event(FILE *file, MiniSNNStructuralEvent *event)
{
    uint32_t type, applied, reason_length;
    uint64_t source, target, new_source, new_target;
    if (file == NULL || event == NULL ||
        !read_unsigned_long_long(file, &event->step) ||
        !read_unsigned_long_long(file, &event->event_index) || !read_u32(file, &type) ||
        type > (uint32_t)MINISNN_TOPOLOGY_SET_DELAY || !read_u64(file, &source) ||
        source > SIZE_MAX || !read_u64(file, &target) || target > SIZE_MAX ||
        !read_u64(file, &new_source) || new_source > SIZE_MAX ||
        !read_u64(file, &new_target) || new_target > SIZE_MAX ||
        !read_u64(file, &event->connection_key) || !read_u64(file, &event->new_connection_key) ||
        !read_double(file, &event->magnitude) || !read_u32(file, &event->delay) ||
        !read_double(file, &event->activity_score) || !read_double(file, &event->growth_score) ||
        !read_unsigned_long_long(file, &event->age_steps) ||
        !read_u32(file, &applied) || applied > 1U ||
        !read_u32(file, &reason_length) || reason_length > MINISNN_TOPOLOGY_REASON_MAX ||
        fread(event->reason, 1U, reason_length, file) != reason_length ||
        !read_u64(file, &event->signature_before) || !read_u64(file, &event->signature_after))
        return 0;
    event->type = (MiniSNNTopologyOperationType)type;
    event->source = (size_t)source;
    event->target = (size_t)target;
    event->new_source = (size_t)new_source;
    event->new_target = (size_t)new_target;
    event->applied = (int)applied;
    event->reason[reason_length] = '\0';
    return 1;
}

static int write_neuron(FILE *file, const Neuron *n)
{
    if (n == NULL || !write_u32(file, (uint32_t)n->type) ||
        !write_u32(file, (uint32_t)n->model) || !write_double(file, n->V) ||
        !write_u32(file, (uint32_t)n->spike))
        return 0;
    if (n->model == MINISNN_NEURON_MODEL_LIF)
        return write_u32(file, (uint32_t)n->state.lif.reserved);
    if (n->model == MINISNN_NEURON_MODEL_ADEX)
        return write_double(file, n->state.adex.w);
    return write_double(file, n->state.hh.m) && write_double(file, n->state.hh.h) &&
        write_double(file, n->state.hh.n) && write_double(file, n->state.hh.previous_V);
}

static int read_neuron(FILE *file, Neuron *n, MiniSNNNeuronModel expected)
{
    uint32_t type, model, spike, reserved;
    if (n == NULL || !read_u32(file, &type) || !read_u32(file, &model) ||
        type > (uint32_t)NEURON_INHIBITORY || model != (uint32_t)expected ||
        !read_double(file, &n->V) || !read_u32(file, &spike) || spike > 1U)
        return 0;
    n->type = (NeuronType)type;
    n->model = expected;
    n->spike = (int)spike;
    if (expected == MINISNN_NEURON_MODEL_LIF)
    {
        if (!read_u32(file, &reserved))
            return 0;
        n->state.lif.reserved = (int)reserved;
        return 1;
    }
    if (expected == MINISNN_NEURON_MODEL_ADEX)
        return read_double(file, &n->state.adex.w);
    return read_double(file, &n->state.hh.m) && read_double(file, &n->state.hh.h) &&
        read_double(file, &n->state.hh.n) && read_double(file, &n->state.hh.previous_V);
}

uint64_t minisnn_network_checkpoint_contract_signature(const MiniSNN *network)
{
    uint64_t hash = CYCLE_FNV_OFFSET;
    uint64_t topology = 0U;
    if (network == NULL || network->net.size <= 0 ||
        !neuron_model_validate_config(&network->net.model_config) ||
        !minisnn_get_topology_signature(network, &topology))
        return 0U;
    hash_u32(&hash, CYCLE_NETWORK_VERSION);
    hash_u32(&hash, (uint32_t)network->net.size);
    hash_u32(&hash, (uint32_t)network->net.max_synaptic_delay);
    hash_u64(&hash, neuron_model_config_signature(&network->net.model_config));
    hash_u64(&hash, topology);
    return hash;
}

int minisnn_network_checkpoint_write(const MiniSNN *network, FILE *file)
{
    const Network *net;
    size_t connection_count;
    if (network == NULL || file == NULL || (net = &network->net) == NULL ||
        net->size <= 0 || net->neurons == NULL || net->connections == NULL ||
        net->spikes == NULL || net->syn_current == NULL || net->used_syn_current == NULL ||
        net->pending_current == NULL || net->ext_current == NULL || net->plasticity == NULL ||
        net->reward == NULL || net->homeostasis == NULL ||
        !write_u32(file, CYCLE_NETWORK_MAGIC) || !write_u32(file, CYCLE_NETWORK_VERSION) ||
        !write_model(file, &net->model_config) || !write_u32(file, (uint32_t)net->size) ||
        !write_double(file, net->synaptic_decay) ||
        !write_u32(file, (uint32_t)net->max_synaptic_delay) ||
        !write_u32(file, (uint32_t)net->delay_cursor) ||
        !write_u64(file, (uint64_t)(unsigned int)net->step) ||
        !write_plasticity_config(file, &net->plasticity->config) ||
        !write_reward_config(file, &net->reward->config) ||
        !write_homeostasis_config(file, &net->homeostasis->config) ||
        !write_plasticity_stats(file, &net->plasticity->stats) ||
        !write_reward_stats(file, &net->reward->stats) ||
        !write_homeostasis_stats(file, &net->homeostasis->stats))
        return 0;
    for (int index = 0; index < net->size; index++)
        if (!write_neuron(file, &net->neurons[index]))
            return 0;
    connection_count = network_connection_count(net);
    if (!write_u64(file, (uint64_t)connection_count))
        return 0;
    for (int source = 0; source < net->size; source++)
        for (int index = 0; index < net->connections[source].count; index++)
        {
            const Connection *connection = &net->connections[source].list[index];
            if (!write_u32(file, (uint32_t)source) ||
                !write_u32(file, (uint32_t)connection->target) ||
                !write_double(file, connection->weight) ||
                !write_u32(file, (uint32_t)connection->delay))
                return 0;
        }
    for (int index = 0; index < net->size; index++)
    {
        double rate_trace = net->homeostasis->config.enabled ?
            net->homeostasis->rate_trace[index] : 0.0;
        double effective_threshold = net->homeostasis->config.enabled ?
            net->homeostasis->effective_threshold[index] : 0.0;
        double initial_threshold = net->homeostasis->config.enabled ?
            net->homeostasis->initial_threshold[index] : 0.0;
        double initial_sum = net->homeostasis->config.enabled ?
            net->homeostasis->initial_incoming_exc_sum[index] : 0.0;
        double current_sum = net->homeostasis->config.enabled ?
            net->homeostasis->current_incoming_exc_sum[index] : 0.0;
        uint32_t threshold_modified = net->homeostasis->config.enabled ?
            (uint32_t)net->homeostasis->threshold_modified[index] : 0U;
        if (!write_u32(file, (uint32_t)net->spikes[index]) ||
            !write_double(file, net->syn_current[index]) ||
            !write_double(file, net->used_syn_current[index]) ||
            !write_double(file, net->ext_current[index]) ||
            !write_double(file, net->plasticity->pre_trace[index]) ||
            !write_double(file, net->plasticity->post_trace[index]) ||
            !write_double(file, rate_trace) || !write_double(file, effective_threshold) ||
            !write_double(file, initial_threshold) || !write_double(file, initial_sum) ||
            !write_double(file, current_sum) || !write_u32(file, threshold_modified))
            return 0;
    }
    for (size_t index = 0U; index < (size_t)net->size * (size_t)net->max_synaptic_delay; index++)
        if (!write_double(file, net->pending_current[index]))
            return 0;
    if (!write_double(file, net->homeostasis->inhibitory_gain) ||
        !write_double(file, net->reward->pending_raw_reward) ||
        !write_u32(file, net->reward->pending_component_count) ||
        !write_u32(file, (uint32_t)net->reward->pending_queued) ||
        !write_u64(file, (uint64_t)net->reward->connection_count))
        return 0;
    for (size_t index = 0U; index < net->reward->connection_count; index++)
    {
        const RewardConnectionState *state = &net->reward->connections[index];
        if (!write_double(file, state->eligibility) ||
            !write_u64(file, state->last_update_step) ||
            !write_double(file, state->max_absolute_eligibility) ||
            !write_double(file, state->reward_signed_change) ||
            !write_double(file, state->reward_absolute_change) ||
            !write_u64(file, state->reward_update_count) ||
            !write_u32(file, (uint32_t)state->eligible) ||
            !write_u32(file, (uint32_t)state->modified))
            return 0;
    }
    /* Structural baselines, events, counters and PRNG state are serialized
     * explicitly. A restored structural reset therefore has the same initial
     * topology and diagnostics as the uninterrupted execution. */
    if (!write_u32(file, net->structural_plasticity != NULL ? 1U : 0U))
        return 0;
    if (net->structural_plasticity != NULL)
    {
        const StructuralPlasticityState *state = net->structural_plasticity;
        if (!write_structural_config(file, &state->config) ||
            !write_structural_stats(file, &state->stats) ||
            !write_genome(file, &state->initial_topology) ||
            !write_u64(file, state->prng.state) || !write_u64(file, state->prng.increment) ||
            !write_u64(file, (uint64_t)state->connection_state_count) ||
            !write_u64(file, (uint64_t)state->event_count))
            return 0;
        for (int index = 0; index < net->size; index++)
            if (!write_double(file, state->rate_traces[index]))
                return 0;
        for (size_t index = 0U; index < state->connection_state_count; index++)
        {
            const MiniSNNStructuralConnectionState *connection =
                &state->connection_states[index];
            if (!write_u64(file, connection->connection_key) ||
                !write_u64(file, connection->birth_step) ||
                !write_u64(file, connection->last_structural_update_step) ||
                !write_double(file, connection->max_absolute_weight) ||
                !write_double(file, connection->activity_score) ||
                !write_u64(file, connection->prune_candidate_count) ||
                !write_u32(file, connection->growth_origin))
                return 0;
        }
        for (size_t index = 0U; index < state->event_count; index++)
            if (!write_structural_event(file, &state->events[index]))
                return 0;
    }
    return 1;
}

int minisnn_network_checkpoint_load(MiniSNN *network, FILE *file)
{
    NetworkConfig config;
    MiniSNNPlasticityConfig plasticity_config;
    MiniSNNRewardConfig reward_config;
    MiniSNNHomeostasisConfig homeostasis_config;
    MiniSNNPlasticityStats plasticity_stats;
    MiniSNNRewardStats reward_stats;
    MiniSNNHomeostasisStats homeostasis_stats;
    Network restored = {0};
    uint32_t magic, version, size, max_delay, delay_cursor, pending_queued,
        structural_present;
    uint64_t saved_step, connection_count, reward_count;
    int ok = 0;

    if (network == NULL || file == NULL || !read_u32(file, &magic) ||
        magic != CYCLE_NETWORK_MAGIC || !read_u32(file, &version) ||
        version != CYCLE_NETWORK_VERSION || !read_model(file, &config) ||
        !read_u32(file, &size) || size == 0U || size > 100000U ||
        !read_double(file, &config.synaptic_decay) || !read_u32(file, &max_delay) ||
        max_delay == 0U || max_delay > 100000U || !read_u32(file, &delay_cursor) ||
        delay_cursor >= max_delay || !read_u64(file, &saved_step) || saved_step > INT_MAX ||
        !read_plasticity_config(file, &plasticity_config) ||
        !read_reward_config(file, &reward_config) ||
        !read_homeostasis_config(file, &homeostasis_config) ||
        !read_plasticity_stats(file, &plasticity_stats) ||
        !read_reward_stats(file, &reward_stats) ||
        !read_homeostasis_stats(file, &homeostasis_stats))
        goto done;
    config.max_synaptic_delay = (int)max_delay;
    if (!network_config_is_valid(&config) || !network_init_with_config(&restored, (int)size, &config))
        goto done;
    for (uint32_t index = 0U; index < size; index++)
        if (!read_neuron(file, &restored.neurons[index], config.neuron_model))
            goto done;
    if (!read_u64(file, &connection_count) ||
        connection_count > (uint64_t)size * (uint64_t)size)
        goto done;
    for (uint64_t index = 0U; index < connection_count; index++)
    {
        uint32_t source, target, delay;
        double weight;
        if (!read_u32(file, &source) || !read_u32(file, &target) ||
            !read_double(file, &weight) || !read_u32(file, &delay) || source >= size ||
            target >= size || delay == 0U || delay > max_delay ||
            !network_connect_delayed_ex(&restored, (int)source, (int)target, weight,
                                        (int)delay, 1))
            goto done;
    }
    if (!network_set_plasticity_config(&restored, &plasticity_config) ||
        !network_set_reward_config(&restored, &reward_config) ||
        !network_set_homeostasis_config(&restored, &homeostasis_config))
        goto done;
    restored.plasticity->stats = plasticity_stats;
    restored.reward->stats = reward_stats;
    restored.homeostasis->stats = homeostasis_stats;
    for (uint32_t index = 0U; index < size; index++)
    {
        uint32_t spike, threshold_modified;
        double rate_trace, effective_threshold, initial_threshold, initial_sum, current_sum;
        if (!read_u32(file, &spike) || spike > 1U ||
            !read_double(file, &restored.syn_current[index]) ||
            !read_double(file, &restored.used_syn_current[index]) ||
            !read_double(file, &restored.ext_current[index]) ||
            !read_double(file, &restored.plasticity->pre_trace[index]) ||
            !read_double(file, &restored.plasticity->post_trace[index]) ||
            !read_double(file, &rate_trace) || !read_double(file, &effective_threshold) ||
            !read_double(file, &initial_threshold) || !read_double(file, &initial_sum) ||
            !read_double(file, &current_sum) ||
            !read_u32(file, &threshold_modified) || threshold_modified > 1U)
            goto done;
        restored.spikes[index] = (int)spike;
        if (restored.homeostasis->config.enabled)
        {
            restored.homeostasis->rate_trace[index] = rate_trace;
            restored.homeostasis->effective_threshold[index] = effective_threshold;
            restored.homeostasis->initial_threshold[index] = initial_threshold;
            restored.homeostasis->initial_incoming_exc_sum[index] = initial_sum;
            restored.homeostasis->current_incoming_exc_sum[index] = current_sum;
            restored.homeostasis->threshold_modified[index] = (unsigned char)threshold_modified;
        }
    }
    for (size_t index = 0U; index < (size_t)size * (size_t)max_delay; index++)
        if (!read_double(file, &restored.pending_current[index]))
            goto done;
    if (!read_double(file, &restored.homeostasis->inhibitory_gain) ||
        !read_double(file, &restored.reward->pending_raw_reward) ||
        !read_u32(file, &restored.reward->pending_component_count) ||
        !read_u32(file, &pending_queued) || pending_queued > 1U ||
        !read_u64(file, &reward_count) || reward_count !=
            (restored.reward->config.enabled ? network_connection_count(&restored) : 0U))
        goto done;
    restored.reward->pending_queued = (int)pending_queued;
    for (size_t index = 0U; index < (size_t)reward_count; index++)
    {
        RewardConnectionState *state = &restored.reward->connections[index];
        uint32_t eligible, modified;
        if (!read_double(file, &state->eligibility) ||
            !read_unsigned_long_long(file, &state->last_update_step) ||
            !read_double(file, &state->max_absolute_eligibility) ||
            !read_double(file, &state->reward_signed_change) ||
            !read_double(file, &state->reward_absolute_change) ||
            !read_unsigned_long_long(file, &state->reward_update_count) ||
            !read_u32(file, &eligible) ||
            !read_u32(file, &modified) || eligible > 1U || modified > 1U)
            goto done;
        state->eligible = (unsigned char)eligible;
        state->modified = (unsigned char)modified;
    }
    if (!read_u32(file, &structural_present) || structural_present > 1U)
        goto done;
    if (structural_present != 0U)
    {
        MiniSNNStructuralPlasticityConfig structural_config;
        StructuralPlasticityState *structural_state;
        uint64_t structural_count, event_count;
        StructureGenome initial_topology = {0};
        if (!read_structural_config(file, &structural_config) ||
            !network_set_structural_plasticity_config(&restored, &structural_config) ||
            (structural_state = restored.structural_plasticity) == NULL ||
            !read_structural_stats(file, &structural_state->stats) ||
            !read_genome(file, &initial_topology) ||
            !read_u64(file, &structural_state->prng.state) ||
            !read_u64(file, &structural_state->prng.increment) ||
            !read_u64(file, &structural_count) ||
            structural_count != structural_state->connection_state_count ||
            !read_u64(file, &event_count) || event_count > CYCLE_MAX_STRUCTURAL_EVENTS)
        {
            structure_genome_destroy(&initial_topology);
            goto done;
        }
        for (uint32_t index = 0U; index < size; index++)
            if (!read_double(file, &structural_state->rate_traces[index]))
                goto done;
        for (size_t index = 0U; index < (size_t)structural_count; index++)
        {
            MiniSNNStructuralConnectionState *connection =
                &structural_state->connection_states[index];
            if (!read_u64(file, &connection->connection_key) ||
                !read_unsigned_long_long(file, &connection->birth_step) ||
                !read_unsigned_long_long(file,
                                         &connection->last_structural_update_step) ||
                !read_double(file, &connection->max_absolute_weight) ||
                !read_double(file, &connection->activity_score) ||
                !read_unsigned_long_long(file,
                                         &connection->prune_candidate_count) ||
                !read_u32(file, &connection->growth_origin) ||
                connection->growth_origin > 1U)
            {
                structure_genome_destroy(&initial_topology);
                goto done;
            }
        }
        for (size_t index = 0U; index < (size_t)event_count; index++)
        {
            MiniSNNStructuralEvent event = {0};
            if (!read_structural_event(file, &event) ||
                !structural_plasticity_append_event(structural_state, &event))
            {
                structure_genome_destroy(&initial_topology);
                goto done;
            }
            structural_state->events[structural_state->event_count - 1U].event_index =
                event.event_index;
        }
        structure_genome_destroy(&structural_state->initial_topology);
        structural_state->initial_topology = initial_topology;
    }
    if (fgetc(file) != EOF)
        goto done;
    restored.step = (int)saved_step;
    restored.delay_cursor = (int)delay_cursor;
    network_destroy(&network->net);
    network->net = restored;
    memset(&restored, 0, sizeof(restored));
    ok = 1;
done:
    network_destroy(&restored);
    return ok;
}
