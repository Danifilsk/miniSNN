#ifndef MINISNN_WORLDS_BRAIN_BRIDGE_H
#define MINISNN_WORLDS_BRAIN_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#include "minisnn.h"
#include "minisnn_worlds_domain.h"

#define MINISNN_WORLDS_BRAIN_BRIDGE_SENSOR_COUNT_V1 UINT32_C(6)
#define MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1 UINT32_C(6)
#define MINISNN_WORLDS_BRAIN_BRIDGE_MAX_CORE_STEPS UINT32_C(10000)

typedef struct MiniSNNWorldsBrainBridge MiniSNNWorldsBrainBridge;

/*
 * The Bridge owns only its binding table and same-tick decision cache. A
 * Bridge may bind actors from exactly one Domain at a time. The first
 * successful binding selects that Domain; unbinding the final actor releases
 * it, allowing the Bridge to be reused with another Domain. The Domain and
 * every MiniSNN brain supplied to minisnn_worlds_brain_bridge_bind remain
 * owned by the caller and must outlive the binding.
 */

typedef enum
{
    MINISNN_WORLDS_BRAIN_SENSOR_SELF_ENERGY = 0,
    MINISNN_WORLDS_BRAIN_SENSOR_SELF_HUNGER,
    MINISNN_WORLDS_BRAIN_SENSOR_NEAREST_FOOD_PRESENT,
    MINISNN_WORLDS_BRAIN_SENSOR_NEAREST_FOOD_DELTA_X,
    MINISNN_WORLDS_BRAIN_SENSOR_NEAREST_FOOD_DELTA_Y,
    MINISNN_WORLDS_BRAIN_SENSOR_NEAREST_FOOD_DISTANCE
} MiniSNNWorldsBrainSensorV1;

typedef enum
{
    MINISNN_WORLDS_BRAIN_ACTION_WAIT = 0,
    MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_X,
    MINISNN_WORLDS_BRAIN_ACTION_MOVE_NEG_X,
    MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_Y,
    MINISNN_WORLDS_BRAIN_ACTION_MOVE_NEG_Y,
    MINISNN_WORLDS_BRAIN_ACTION_EAT
} MiniSNNWorldsBrainActionChannel;

typedef enum
{
    MINISNN_WORLDS_BRAIN_BRIDGE_FALLBACK_NONE = 0,
    MINISNN_WORLDS_BRAIN_BRIDGE_FALLBACK_NO_OUTPUT,
    MINISNN_WORLDS_BRAIN_BRIDGE_FALLBACK_EAT_WITHOUT_FOOD
} MiniSNNWorldsBrainBridgeFallback;

typedef enum
{
    MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE = 0,
    MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NULL_ARGUMENT,
    MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_INVALID_ARGUMENT,
    MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_ALLOCATION,
    MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_DUPLICATE_BINDING,
    MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_UNKNOWN_ACTOR,
    MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NOT_ORGANISM,
    MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NOT_BOUND,
    MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_DOMAIN_MISMATCH,
    MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_DOMAIN_FAILURE,
    MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_CORE_FAILURE,
    MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_TICK_REGRESSION,
    MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_INVARIANT_VIOLATION,
    MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_COUNTER_OVERFLOW
} MiniSNNWorldsBrainBridgeError;

typedef struct
{
    double values[MINISNN_WORLDS_BRAIN_BRIDGE_SENSOR_COUNT_V1];
} MiniSNNWorldsBrainSensorFrameV1;

typedef struct
{
    double perception_scale;
    double input_gain;
    MiniSNNWorldsKernelScalar move_step;
    uint32_t core_steps_per_decision;
    int sensor_neuron[MINISNN_WORLDS_BRAIN_BRIDGE_SENSOR_COUNT_V1];
    int action_neuron[MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1];
} MiniSNNWorldsBrainBridgeConfig;

typedef struct
{
    uint64_t total_decisions_computed;
    uint64_t total_cache_hits;
    uint64_t total_waits;
    uint64_t total_moves;
    uint64_t total_eats;
    uint64_t total_ties;
    uint64_t total_fallbacks;
} MiniSNNWorldsBrainBridgeDiagnostics;

typedef struct
{
    MiniSNNWorldsTick domain_tick;
    MiniSNNWorldsKernelEntityId actor;
    MiniSNNWorldsBrainSensorFrameV1 sensor_frame;
    uint32_t output_scores[MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1];
    MiniSNNWorldsBrainActionChannel selected_channel;
    MiniSNNWorldsDomainAction action;
    uint32_t core_steps_executed;
    uint8_t tie_break_used;
    uint8_t cache_hit;
    MiniSNNWorldsBrainBridgeFallback fallback;
} MiniSNNWorldsBrainDecisionReport;

MiniSNNWorldsBrainBridgeConfig minisnn_worlds_brain_bridge_config_default(void);
int minisnn_worlds_brain_bridge_config_is_valid(
    const MiniSNNWorldsBrainBridgeConfig *config);

MiniSNNWorldsBrainBridge *minisnn_worlds_brain_bridge_create(
    const MiniSNNWorldsBrainBridgeConfig *config,
    MiniSNNWorldsBrainBridgeError *out_error);
void minisnn_worlds_brain_bridge_destroy(MiniSNNWorldsBrainBridge **bridge_ptr);
MiniSNNWorldsBrainBridgeError minisnn_worlds_brain_bridge_last_error(
    const MiniSNNWorldsBrainBridge *bridge);
const char *minisnn_worlds_brain_bridge_error_string(
    MiniSNNWorldsBrainBridgeError error);

MiniSNNWorldsBrainBridgeError minisnn_worlds_brain_bridge_bind(
    MiniSNNWorldsBrainBridge *bridge,
    const MiniSNNWorldsDomain *domain,
    MiniSNNWorldsKernelEntityId actor,
    MiniSNN *brain);
MiniSNNWorldsBrainBridgeError minisnn_worlds_brain_bridge_unbind(
    MiniSNNWorldsBrainBridge *bridge,
    MiniSNNWorldsKernelEntityId actor);
size_t minisnn_worlds_brain_bridge_binding_count(
    const MiniSNNWorldsBrainBridge *bridge);

MiniSNNWorldsBrainBridgeError minisnn_worlds_brain_bridge_encode_perception_v1(
    const MiniSNNWorldsDomainPerception *perception,
    MiniSNNWorldsDomainEnergy max_energy,
    double perception_scale,
    MiniSNNWorldsBrainSensorFrameV1 *out_frame);
MiniSNNWorldsBrainBridgeError minisnn_worlds_brain_bridge_decode_scores_v1(
    const MiniSNNWorldsDomainPerception *perception,
    const uint32_t scores[MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1],
    MiniSNNWorldsKernelEntityId actor,
    MiniSNNWorldsKernelScalar move_step,
    MiniSNNWorldsDomainAction *out_action,
    MiniSNNWorldsBrainActionChannel *out_selected,
    uint8_t *out_tie_break_used,
    MiniSNNWorldsBrainBridgeFallback *out_fallback);

MiniSNNWorldsBrainBridgeError minisnn_worlds_brain_bridge_decide(
    MiniSNNWorldsBrainBridge *bridge,
    const MiniSNNWorldsDomain *domain,
    MiniSNNWorldsKernelEntityId actor,
    MiniSNNWorldsDomainAction *out_action,
    MiniSNNWorldsBrainDecisionReport *out_report);
MiniSNNWorldsBrainBridgeError minisnn_worlds_brain_bridge_get_diagnostics(
    const MiniSNNWorldsBrainBridge *bridge,
    MiniSNNWorldsBrainBridgeDiagnostics *out_diagnostics);

#endif