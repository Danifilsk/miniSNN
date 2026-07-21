#ifndef MINISNN_AGENT_CYCLE_H
#define MINISNN_AGENT_CYCLE_H

#include <stdint.h>

#include "minisnn_action_decoder.h"
#include "minisnn_sensor_encoder.h"

typedef struct MiniSNNAgentCycle MiniSNNAgentCycle;

typedef enum
{
    MINISNN_AGENT_CYCLE_STATE_READY = 0,
    MINISNN_AGENT_CYCLE_STATE_RUNNING,
    MINISNN_AGENT_CYCLE_STATE_ACTION_PENDING,
    MINISNN_AGENT_CYCLE_STATE_FAULTED
} MiniSNNAgentCycleState;

typedef enum
{
    MINISNN_AGENT_CYCLE_ERROR_NONE = 0,
    MINISNN_AGENT_CYCLE_ERROR_INVALID_ARGUMENT,
    MINISNN_AGENT_CYCLE_ERROR_INVALID_STATE,
    MINISNN_AGENT_CYCLE_ERROR_SCHEMA_MISMATCH,
    MINISNN_AGENT_CYCLE_ERROR_DIMENSION_MISMATCH,
    MINISNN_AGENT_CYCLE_ERROR_SENSOR_UNAVAILABLE,
    MINISNN_AGENT_CYCLE_ERROR_ACTION_PENDING,
    MINISNN_AGENT_CYCLE_ERROR_TICK_MISMATCH,
    MINISNN_AGENT_CYCLE_ERROR_FEEDBACK_INVALID,
    MINISNN_AGENT_CYCLE_ERROR_FEEDBACK_TOO_LATE,
    MINISNN_AGENT_CYCLE_ERROR_UNKNOWN_SOURCE_TICK,
    MINISNN_AGENT_CYCLE_ERROR_NONFINITE_REWARD,
    MINISNN_AGENT_CYCLE_ERROR_REWARD_UNAVAILABLE,
    MINISNN_AGENT_CYCLE_ERROR_NETWORK_STEP,
    MINISNN_AGENT_CYCLE_ERROR_ACTIVITY_CAPTURE,
    MINISNN_AGENT_CYCLE_ERROR_ACTION_PUBLICATION,
    MINISNN_AGENT_CYCLE_ERROR_RESET_WHILE_BUSY,
    MINISNN_AGENT_CYCLE_ERROR_ALLOCATION,
    MINISNN_AGENT_CYCLE_ERROR_FAULTED
} MiniSNNAgentCycleError;

typedef struct
{
    uint64_t source_tick;
    uint64_t delivery_tick;
    double reward;
    uint8_t episode_terminal;
} MiniSNNAgentFeedback;

typedef struct
{
    uint64_t episode_id;
    uint64_t episode_tick;
    uint64_t global_tick;
    uint32_t brain_steps_executed;
    uint64_t total_spikes;
    uint32_t feedback_events_delivered;
    double reward_delivered;
    MiniSNNAgentCycleState state;
    uint32_t failed_brain_step;
} MiniSNNAgentCycleDiagnostics;

/* The cycle does not own the network, AgentIO, encoder, or decoder. They must
 * outlive it. The cycle owns only its transient input/activity/action frames. */
MiniSNNAgentCycle *minisnn_agent_cycle_create(
    MiniSNN *network,
    MiniSNNAgentIOContext *agent_io,
    MiniSNNSensorEncoder *sensor_encoder,
    MiniSNNActionDecoder *action_decoder,
    MiniSNNAgentCycleError *out_error);
void minisnn_agent_cycle_destroy(MiniSNNAgentCycle **cycle_ptr);

/* Runs exactly one externally submitted sensor tick. The produced action stays
 * in AgentIO for the external caller to consume. */
int minisnn_agent_cycle_run_tick(
    MiniSNNAgentCycle *cycle,
    MiniSNNAgentCycleDiagnostics *diagnostics);

/* Adds generic external feedback. Common feedback (episode_terminal == 0) is
 * submitted to C2 before neural step zero of delivery_tick. Terminal feedback
 * (episode_terminal == 1) is applied at the completed episode boundary after
 * its external action is consumed; it never advances a neural tick or resets
 * the episode automatically. */
int minisnn_agent_cycle_submit_feedback(
    MiniSNNAgentCycle *cycle,
    const MiniSNNAgentFeedback *feedback);

/* Reset transient episode state while preserving topology, learned weights, and
 * configured model parameters. A pending external action must be consumed first. */
int minisnn_agent_cycle_reset_episode(MiniSNNAgentCycle *cycle);

MiniSNNAgentCycleState minisnn_agent_cycle_state(
    const MiniSNNAgentCycle *cycle);
MiniSNNAgentCycleError minisnn_agent_cycle_last_error(
    const MiniSNNAgentCycle *cycle);
const char *minisnn_agent_cycle_error_string(MiniSNNAgentCycleError error);
uint64_t minisnn_agent_cycle_episode_id(const MiniSNNAgentCycle *cycle);
uint64_t minisnn_agent_cycle_episode_tick(const MiniSNNAgentCycle *cycle);
uint64_t minisnn_agent_cycle_next_global_tick(const MiniSNNAgentCycle *cycle);
uint64_t minisnn_agent_cycle_total_ticks(const MiniSNNAgentCycle *cycle);
uint64_t minisnn_agent_cycle_total_neural_steps(const MiniSNNAgentCycle *cycle);
uint64_t minisnn_agent_cycle_total_actions(const MiniSNNAgentCycle *cycle);
double minisnn_agent_cycle_total_reward(const MiniSNNAgentCycle *cycle);
uint64_t minisnn_agent_cycle_reset_count(const MiniSNNAgentCycle *cycle);

#endif
