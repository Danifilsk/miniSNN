#ifndef PLASTICITY_H
#define PLASTICITY_H

#include <stddef.h>

#include "connection.h"
#include "minisnn_types.h"
#include "neuron.h"

typedef struct
{
    int source;
    int outgoing_index;
} PlasticityIncomingRef;

typedef struct
{
    PlasticityIncomingRef *refs;
    size_t count;
} PlasticityIncomingList;

typedef struct PlasticityState
{
    MiniSNNPlasticityConfig config;
    MiniSNNPlasticityStats stats;
    double *pre_trace;
    double *post_trace;
    PlasticityIncomingList *incoming;
    size_t *source_offsets;
    double *deltas;
    size_t *candidate_ids;
    unsigned char *candidate_active;
    size_t candidate_count;
    unsigned char *modified;
    size_t connection_count;
    int neuron_count;
    int index_valid;
} PlasticityState;

MiniSNNPlasticityConfig plasticity_default_config(void);

int plasticity_config_is_valid(
    const MiniSNNPlasticityConfig *config);

int plasticity_state_init(
    PlasticityState *state,
    int neuron_count);

void plasticity_state_destroy(PlasticityState *state);

int plasticity_state_configure(
    PlasticityState *state,
    const MiniSNNPlasticityConfig *config,
    const LIFNeuron *neurons,
    const ConnectionList *connections);

void plasticity_state_invalidate_index(PlasticityState *state);

int plasticity_state_rebuild_index(
    PlasticityState *state,
    const LIFNeuron *neurons,
    const ConnectionList *connections);

int plasticity_apply_step(
    PlasticityState *state,
    const LIFNeuron *neurons,
    ConnectionList *connections,
    const int *spikes,
    double dt);

int plasticity_connection_is_eligible(
    const PlasticityState *state,
    const LIFNeuron *neurons,
    int source,
    const Connection *connection);

#endif
