#ifndef HOMEOSTASIS_H
#define HOMEOSTASIS_H

#include "connection.h"
#include "minisnn_types.h"
#include "neuron.h"
#include "plasticity.h"

typedef struct HomeostasisState
{
    MiniSNNHomeostasisConfig config;
    MiniSNNHomeostasisStats stats;
    double *rate_trace;
    double *effective_threshold;
    double *initial_threshold;
    double *initial_incoming_exc_sum;
    double *current_incoming_exc_sum;
    unsigned char *threshold_modified;
    double inhibitory_gain;
    int neuron_count;
    int targets_valid;
} HomeostasisState;

MiniSNNHomeostasisConfig homeostasis_default_config(void);

int homeostasis_config_is_valid(
    const MiniSNNHomeostasisConfig *config,
    double base_threshold);

int homeostasis_state_init(HomeostasisState *state, int neuron_count);
void homeostasis_state_destroy(HomeostasisState *state);

int homeostasis_state_configure(
    HomeostasisState *state,
    const MiniSNNHomeostasisConfig *config,
    double base_threshold,
    const LIFNeuron *neurons,
    const ConnectionList *connections,
    PlasticityState *incoming_index);

int homeostasis_state_reset(
    HomeostasisState *state,
    double base_threshold,
    const LIFNeuron *neurons,
    const ConnectionList *connections,
    PlasticityState *incoming_index);

void homeostasis_state_invalidate_targets(HomeostasisState *state);

int homeostasis_apply_step(
    HomeostasisState *state,
    double base_threshold,
    double dt,
    unsigned long long completed_steps,
    const int *spikes,
    const LIFNeuron *neurons,
    ConnectionList *connections,
    PlasticityState *incoming_index);

double homeostasis_effective_threshold(
    const HomeostasisState *state,
    int neuron_id,
    double base_threshold);

double homeostasis_transmission_gain(
    const HomeostasisState *state,
    NeuronType source_type);

int homeostasis_current_incoming_sum(
    HomeostasisState *state,
    int neuron_id,
    const LIFNeuron *neurons,
    const ConnectionList *connections,
    PlasticityState *incoming_index,
    double *out_sum);

double homeostasis_rate_trace_next(
    double previous_rate,
    int spike,
    double dt,
    double rate_tau);

double homeostasis_threshold_next(
    double threshold,
    double rate,
    const MiniSNNHomeostasisConfig *config);

double homeostasis_scaling_factor(
    double target_sum,
    double current_sum,
    const MiniSNNHomeostasisConfig *config);

double homeostasis_inhibitory_gain_next(
    double gain,
    double population_rate,
    const MiniSNNHomeostasisConfig *config);

#endif
