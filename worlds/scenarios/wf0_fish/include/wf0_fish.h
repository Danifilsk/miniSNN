#ifndef WF0_FISH_H
#define WF0_FISH_H

#include <stdint.h>

#include "minisnn.h"
#include "minisnn_worlds_brain_bridge.h"
#include "minisnn_worlds_domain.h"

#define WF0_FISH_NEURON_COUNT_V1 12
#define WF0_FISH_WORLD_SEED_V1 UINT64_C(0x5746305F46495348)

typedef struct WF0FishWorld WF0FishWorld;

typedef struct
{
    MiniSNNWorldsKernelTransform actor_transform;
    MiniSNNWorldsDomainEnergy actor_energy;
    MiniSNNWorldsDomainLifeState actor_life_state;
    MiniSNNWorldsDomainDeathCause actor_death_cause;
    MiniSNNWorldsTick actor_death_tick;
    uint64_t remaining_food;
    MiniSNNWorldsTick tick;
    uint64_t kernel_hash;
    uint64_t domain_hash;
    int core_step;
} WF0FishWorldState;
typedef struct
{
    MiniSNNWorldsTick tick_before;
    int decision_performed;
    MiniSNNWorldsBrainDecisionReport decision;
    MiniSNNWorldsDomainActionResult action_result;
    MiniSNNWorldsKernelTransform transform_after;
    MiniSNNWorldsDomainEnergy energy_after_tick;
    MiniSNNWorldsDomainEnergy metabolism_cost;
    MiniSNNWorldsDomainEnergy move_cost;
    MiniSNNWorldsDomainEnergy food_nutrition;
    uint64_t remaining_food;
    uint64_t kernel_hash;
    uint64_t domain_hash;
    int core_step_start;
    int core_step_end;
} WF0FishTickRecord;

/* WF0 Fish Species V1 is a generic Domain species with frozen semantic values. */
MiniSNNWorldsDomainSpeciesConfig wf0_fish_species_v1(void);

/* Fish Brain V1 is a fixed, non-plastic LIF network using the six WB0 channels. */
MiniSNN *wf0_fish_brain_v1_create(void);
MiniSNNWorldsBrainBridgeConfig wf0_fish_brain_bridge_config_v1(void);

/* Creates one Fish at the origin and one Food at the supplied fixed-point position. */
WF0FishWorld *wf0_fish_world_create(
    MiniSNNWorldsKernelScalar food_x,
    MiniSNNWorldsKernelScalar food_y);
/* Adopts a preconfigured Kernel. On return the caller pointer is NULL; the
 * world owns the Kernel on success and destroys it on initialization failure. */
WF0FishWorld *wf0_fish_world_create_on_kernel(
    MiniSNNWorldsKernel **in_out_kernel,
    MiniSNNWorldsKernelScalar food_x,
    MiniSNNWorldsKernelScalar food_y,
    int actor_has_occupancy);
void wf0_fish_world_destroy(WF0FishWorld **world_ptr);

MiniSNNWorldsKernelEntityId wf0_fish_world_actor(const WF0FishWorld *world);
MiniSNNWorldsKernelEntityId wf0_fish_world_food(const WF0FishWorld *world);
int wf0_fish_world_core_step(const WF0FishWorld *world);
MiniSNNWorldsTick wf0_fish_world_tick(const WF0FishWorld *world);
uint64_t wf0_fish_world_food_count(const WF0FishWorld *world);
/* Reads scenario state without exposing mutable Domain or Kernel handles. */
int wf0_fish_world_state(
    const WF0FishWorld *world,
    WF0FishWorldState *out_state);

/* Observation is separate from the action loop; actions are always bridge-decoded. */
int wf0_fish_world_perceive(
    const WF0FishWorld *world,
    MiniSNNWorldsDomainPerception *out_perception);

/* Explicit scenario reset only; normal world ticks preserve all transient Core state. */
int wf0_fish_world_reset_brain(WF0FishWorld *world);

/* Executes one real Core decision, then applies that decoded action through Domain. */
int wf0_fish_world_tick_once(
    WF0FishWorld *world,
    WF0FishTickRecord *out_record);

#endif