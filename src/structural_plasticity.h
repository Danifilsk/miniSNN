#ifndef STRUCTURAL_PLASTICITY_H
#define STRUCTURAL_PLASTICITY_H

#include <stddef.h>
#include <stdint.h>

#include "minisnn_types.h"
#include "structure.h"

typedef struct
{
    uint64_t state;
    uint64_t increment;
} StructuralPrng;

typedef struct StructuralPlasticityState
{
    MiniSNNStructuralPlasticityConfig config;
    MiniSNNStructuralStats stats;
    StructureGenome initial_topology;
    MiniSNNStructuralConnectionState *connection_states;
    size_t connection_state_count;
    double *rate_traces;
    int neuron_count;
    StructuralPrng prng;
    MiniSNNStructuralEvent *events;
    size_t event_count;
    size_t event_capacity;
} StructuralPlasticityState;

struct Network;

MiniSNNStructuralPlasticityConfig structural_plasticity_default_config(void);

int structural_plasticity_config_is_valid(
    const MiniSNNStructuralPlasticityConfig *config,
    int neuron_count,
    int max_synaptic_delay,
    size_t current_connection_count);

int structural_plasticity_state_create(
    StructuralPlasticityState **out_state,
    struct Network *net,
    const MiniSNNStructuralPlasticityConfig *config);

void structural_plasticity_state_destroy(
    StructuralPlasticityState *state);

int structural_plasticity_apply_step(
    StructuralPlasticityState *state,
    struct Network *net,
    unsigned long long completed_step);

int structural_plasticity_reset(
    StructuralPlasticityState *state,
    struct Network *net,
    MiniSNNStructuralResetMode mode);

int structural_plasticity_rebuild_connection_states(
    StructuralPlasticityState *state,
    const StructureGenome *before,
    const StructureGenome *after,
    unsigned long long birth_step);

int structural_plasticity_append_event(
    StructuralPlasticityState *state,
    const MiniSNNStructuralEvent *event);

uint32_t structural_prng_next(StructuralPrng *prng);
double structural_prng_unit(StructuralPrng *prng);
void structural_prng_seed(StructuralPrng *prng, uint64_t seed);

#endif
