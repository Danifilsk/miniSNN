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
typedef struct MiniSNNWorldsTrainableBrain MiniSNNWorldsTrainableBrain;

/* WB1 retains the WB0 sensor/action contract while making the neural
 * implementation configurable and persistent. */
#define MINISNN_WORLDS_BRAIN_CONFIG_VERSION_V1 UINT32_C(1)
#define MINISNN_WORLDS_BRAIN_NAME_MAX 63U

typedef enum
{
    MINISNN_WORLDS_BRAIN_TOPOLOGY_RANDOM = 0,
    MINISNN_WORLDS_BRAIN_TOPOLOGY_SMALL_WORLD,
    MINISNN_WORLDS_BRAIN_TOPOLOGY_FULLY_CONNECTED
} MiniSNNWorldsBrainTopology;

typedef enum
{
    MINISNN_WORLDS_BRAIN_MODE_TRAINING = 0,
    MINISNN_WORLDS_BRAIN_MODE_EVALUATION
} MiniSNNWorldsBrainMode;

typedef struct
{
    double eat_applied_reward;
    double rejected_action_penalty;
} MiniSNNWorldsBrainRewardProfile;

typedef struct
{
    uint32_t version;
    char brain_name[MINISNN_WORLDS_BRAIN_NAME_MAX + 1U];
    MiniSNNWorldsBrainTopology topology;
    uint32_t neuron_count;
    uint32_t inhibitory_count;
    double connection_probability;
    uint32_t small_world_neighbors;
    double small_world_rewire_probability;
    double excitatory_weight;
    double inhibitory_weight;
    uint32_t connection_delay;
    int allow_self_connections;
    int allow_inhibitory_to_inhibitory;
    /* Seed controls the deterministic Core Topology Factory. WB1 enables no
     * other stochastic Core subsystem, so all initialization randomness is
     * derived from this one value. */
    uint64_t seed;
    uint32_t decision_steps_per_tick;
    /* Uniform, schema-neutral output population size for every action. */
    uint32_t action_population_size;
    MiniSNNWorldsBrainMode mode;
    int plasticity_enabled;
    MiniSNNWorldsBrainRewardProfile reward_profile;
    MiniSNNConfig neural_config;
} MiniSNNWorldsBrainConfig;

typedef enum
{
    MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE = 0,
    MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_INVALID_ARGUMENT,
    MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_ALLOCATION,
    MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_DOMAIN_MISMATCH,
    MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_UNKNOWN_ACTOR,
    MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NOT_ORGANISM,
    MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_CORE_FAILURE,
    MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_IO,
    MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_FORMAT,
    MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_INCOMPATIBLE,
    MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_FEEDBACK_NOT_PENDING,
    MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_FEEDBACK_MISMATCH,
    MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_INVALID_MODE,
    MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_PENDING_ACTION_RESULT
} MiniSNNWorldsTrainableBrainError;

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

typedef struct
{
    uint64_t domain_tick;
    uint64_t core_tick;
    uint64_t total_spikes;
    MiniSNNWorldsBrainSensorFrameV1 sensor_frame;
    uint32_t output_scores[MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1];
    MiniSNNWorldsDomainAction action;
    MiniSNNWorldsBrainActionChannel selected_channel;
    uint32_t core_steps_executed;
    uint8_t tie_break_used;
    MiniSNNWorldsBrainBridgeFallback fallback;
    double last_reward;
    uint8_t cache_hit;
    uint8_t plasticity_active;
} MiniSNNWorldsTrainableBrainReport;

/* WB1 trainable brain API. The object owns all public Core AgentCycle pieces.
 * Bind it to one Domain/actor before deciding; a second Domain is rejected. */
MiniSNNWorldsBrainConfig minisnn_worlds_trainable_brain_config_default(void);
int minisnn_worlds_trainable_brain_config_is_valid(
    const MiniSNNWorldsBrainConfig *config);
const char *minisnn_worlds_brain_topology_name(MiniSNNWorldsBrainTopology topology);
const char *minisnn_worlds_brain_mode_name(MiniSNNWorldsBrainMode mode);

MiniSNNWorldsTrainableBrain *minisnn_worlds_trainable_brain_create(
    const MiniSNNWorldsBrainConfig *config,
    MiniSNNWorldsTrainableBrainError *out_error);
void minisnn_worlds_trainable_brain_destroy(
    MiniSNNWorldsTrainableBrain **brain_ptr);
MiniSNNWorldsTrainableBrainError minisnn_worlds_trainable_brain_last_error(
    const MiniSNNWorldsTrainableBrain *brain);
const char *minisnn_worlds_trainable_brain_error_string(
    MiniSNNWorldsTrainableBrainError error);
int minisnn_worlds_trainable_brain_get_config(
    const MiniSNNWorldsTrainableBrain *brain,
    MiniSNNWorldsBrainConfig *out_config);
int minisnn_worlds_trainable_brain_bind(
    MiniSNNWorldsTrainableBrain *brain,
    const MiniSNNWorldsDomain *domain,
    MiniSNNWorldsKernelEntityId actor);
/* Changes learning in place. Evaluation disables both plasticity and reward
 * delivery while preserving the current network and learned weights. */
int minisnn_worlds_trainable_brain_set_mode(
    MiniSNNWorldsTrainableBrain *brain,
    MiniSNNWorldsBrainMode mode);
/* Changes only the existing R-STDP update magnitude. It is rejected while a
 * decision consequence or feedback remains pending. */
int minisnn_worlds_trainable_brain_set_reward_learning_rate(
    MiniSNNWorldsTrainableBrain *brain,
    double learning_rate);
/* Changes only the temporal decay horizon for existing R-STDP eligibility.
 * It is rejected while a decision consequence or feedback remains pending. */
int minisnn_worlds_trainable_brain_set_reward_eligibility_tau(
    MiniSNNWorldsTrainableBrain *brain,
    double eligibility_tau);
int minisnn_worlds_trainable_brain_decide(
    MiniSNNWorldsTrainableBrain *brain,
    const MiniSNNWorldsDomain *domain,
    MiniSNNWorldsKernelEntityId actor,
    MiniSNNWorldsDomainAction *out_action,
    MiniSNNWorldsTrainableBrainReport *out_report);
/* External action feedback is accepted exactly once for the pending decision.
 * The caller supplies the original Domain, actor, pre-step domain tick, and
 * action so stale or cross-episode feedback is rejected atomically. */
int minisnn_worlds_trainable_brain_apply_action_result(
    MiniSNNWorldsTrainableBrain *brain,
    const MiniSNNWorldsDomain *domain,
    MiniSNNWorldsKernelEntityId actor,
    MiniSNNWorldsTick decision_domain_tick,
    const MiniSNNWorldsDomainAction *action,
    const MiniSNNWorldsDomainActionResult *result);
/* Delivers one generic terminal reward for the already-consumed external
 * consequence. The caller defines the terminal semantics; WB1 never knows
 * whether it was starvation, a time limit, or another scenario boundary.
 * The AgentCycle rejects a second terminal delivery for the same decision. */
int minisnn_worlds_trainable_brain_apply_terminal_feedback(
    MiniSNNWorldsTrainableBrain *brain,
    double reward);
int minisnn_worlds_trainable_brain_reset_episode(
    MiniSNNWorldsTrainableBrain *brain);
MiniSNNWorldsTrainableBrain *minisnn_worlds_trainable_brain_full_reset(
    const MiniSNNWorldsTrainableBrain *brain,
    MiniSNNWorldsTrainableBrainError *out_error);
int minisnn_worlds_trainable_brain_save(
    const MiniSNNWorldsTrainableBrain *brain,
    const char *filename,
    MiniSNNWorldsTrainableBrainError *out_error);
MiniSNNWorldsTrainableBrain *minisnn_worlds_trainable_brain_load(
    const char *filename,
    MiniSNNWorldsTrainableBrainError *out_error);
int minisnn_worlds_trainable_brain_core_step(
    const MiniSNNWorldsTrainableBrain *brain);
uint64_t minisnn_worlds_trainable_brain_weight_signature(
    const MiniSNNWorldsTrainableBrain *brain);
uint64_t minisnn_worlds_trainable_brain_topology_signature(
    const MiniSNNWorldsTrainableBrain *brain);
uint64_t minisnn_worlds_trainable_brain_config_signature(
    const MiniSNNWorldsTrainableBrain *brain);
double minisnn_worlds_trainable_brain_last_reward(
    const MiniSNNWorldsTrainableBrain *brain);
/* Delegates the existing public Core R-STDP aggregate statistics. It exposes
 * no mutable Core object and is intended for diagnostics such as WF1-A.3. */
int minisnn_worlds_trainable_brain_get_reward_stats(
    const MiniSNNWorldsTrainableBrain *brain,
    MiniSNNRewardStats *out_stats);
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