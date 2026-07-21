#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minisnn.h"
#include "agent_cycle_checkpoint_internal.h"
#include "neuron_model.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <errno.h>
#endif

#define AGENT_CYCLE_CHECKPOINT_VERSION "minisnn_agent_cycle_checkpoint_v1"
#define AGENT_CYCLE_CHECKPOINT_PROVENANCE "miniSNN_core_c7"
#define AGENT_CYCLE_CHECKPOINT_STATE_MAGIC UINT32_C(0x41374334)
#define AGENT_CYCLE_CHECKPOINT_FNV_OFFSET UINT64_C(14695981039346656037)
#define AGENT_CYCLE_CHECKPOINT_FNV_PRIME UINT64_C(1099511628211)
#define AGENT_CYCLE_CHECKPOINT_PATH_MAX 512U
#define AGENT_CYCLE_CHECKPOINT_MAX_FEEDBACK 1000000U

struct MiniSNNAgentCycle
{
    MiniSNN *network;
    MiniSNNAgentIOContext *agent_io;
    MiniSNNSensorEncoder *sensor_encoder;
    MiniSNNActionDecoder *action_decoder;

    MiniSNNNeuralInputFrame input_frame;
    MiniSNNNeuralActivityFrame activity_frame;
    MiniSNNActionFrame action_frame;
    MiniSNNActionDecodingDiagnostics decoder_diagnostics;

    MiniSNNAgentFeedback *feedback_queue;
    size_t feedback_count;
    size_t feedback_capacity;
    uint64_t terminal_feedback_source_tick;
    int terminal_feedback_applied;

    uint64_t episode_id;
    uint64_t episode_tick;
    uint64_t global_tick;
    uint64_t episode_start_tick;
    uint64_t total_ticks;
    uint64_t total_neural_steps;
    uint64_t total_actions;
    uint64_t total_spikes;
    uint64_t reset_count;
    double total_reward;

    MiniSNNAgentCycleState state;
    MiniSNNAgentCycleError last_error;
};

typedef struct
{
    uint32_t boundary;
    uint64_t episode_id;
    uint64_t episode_tick;
    uint64_t global_tick;
    uint64_t episode_start_tick;
    uint64_t total_ticks;
    uint64_t total_neural_steps;
    uint64_t total_actions;
    uint64_t total_spikes;
    uint64_t reset_count;
    double total_reward;
    uint64_t terminal_feedback_source_tick;
    int terminal_feedback_applied;
    MiniSNNAgentFeedback *feedback_queue;
    size_t feedback_count;
} AgentCycleCheckpointState;

static void set_error(MiniSNNAgentCycleError *out_error,
                      MiniSNNAgentCycleError error)
{
    if (out_error != NULL)
        *out_error = error;
}

static void cycle_error(MiniSNNAgentCycle *cycle,
                        MiniSNNAgentCycleError error)
{
    if (cycle != NULL)
        cycle->last_error = error;
}

static void fault_cycle(MiniSNNAgentCycle *cycle,
                        MiniSNNAgentCycleError error)
{
    cycle_error(cycle, error);
    if (cycle != NULL)
        cycle->state = MINISNN_AGENT_CYCLE_STATE_FAULTED;
}

static int contracts_match(const MiniSNNAgentCycle *cycle,
                           MiniSNNAgentCycleError *out_error)
{
    int neuron_count;

    if (cycle == NULL || cycle->network == NULL || cycle->agent_io == NULL ||
        cycle->sensor_encoder == NULL || cycle->action_decoder == NULL)
    {
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_INVALID_ARGUMENT);
        return 0;
    }

    neuron_count = minisnn_neuron_count(cycle->network);
    if (neuron_count <= 0 ||
        (uint32_t)neuron_count !=
            minisnn_sensor_encoder_neuron_count(cycle->sensor_encoder) ||
        (uint32_t)neuron_count !=
            minisnn_action_decoder_neuron_count(cycle->action_decoder) ||
        cycle->input_frame.neuron_count != (uint32_t)neuron_count ||
        cycle->activity_frame.neuron_count != (uint32_t)neuron_count)
    {
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_DIMENSION_MISMATCH);
        return 0;
    }

    if (minisnn_sensor_encoder_brain_steps_per_tick(cycle->sensor_encoder) == 0U ||
        minisnn_sensor_encoder_brain_steps_per_tick(cycle->sensor_encoder) !=
            minisnn_action_decoder_brain_steps_per_tick(cycle->action_decoder) ||
        cycle->input_frame.brain_step_count !=
            minisnn_sensor_encoder_brain_steps_per_tick(cycle->sensor_encoder) ||
        cycle->activity_frame.brain_step_count !=
            minisnn_sensor_encoder_brain_steps_per_tick(cycle->sensor_encoder) ||
        cycle->action_frame.value_count !=
            minisnn_action_decoder_action_count(cycle->action_decoder) ||
        cycle->decoder_diagnostics.action_count != cycle->action_frame.value_count)
    {
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_DIMENSION_MISMATCH);
        return 0;
    }

    if (minisnn_sensor_encoder_sensor_schema_signature(cycle->sensor_encoder) == 0U ||
        minisnn_sensor_encoder_sensor_schema_signature(cycle->sensor_encoder) !=
            minisnn_agent_io_sensor_schema_signature(cycle->agent_io) ||
        minisnn_action_decoder_action_schema_signature(cycle->action_decoder) == 0U ||
        minisnn_action_decoder_action_schema_signature(cycle->action_decoder) !=
            minisnn_agent_io_action_schema_signature(cycle->agent_io))
    {
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_SCHEMA_MISMATCH);
        return 0;
    }

    set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_NONE);
    return 1;
}

static int validate_feedback_for_submission(
    MiniSNNAgentCycle *cycle, const MiniSNNAgentFeedback *feedback)
{
    MiniSNNRewardConfig reward_config;
    uint64_t pending_tick;

    if (cycle == NULL || feedback == NULL)
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (feedback->episode_terminal > 1U)
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_FEEDBACK_INVALID);
        return 0;
    }
    if (cycle->state == MINISNN_AGENT_CYCLE_STATE_RUNNING ||
        cycle->state == MINISNN_AGENT_CYCLE_STATE_FAULTED)
    {
        cycle_error(cycle, cycle->state == MINISNN_AGENT_CYCLE_STATE_FAULTED ?
                    MINISNN_AGENT_CYCLE_ERROR_FAULTED :
                    MINISNN_AGENT_CYCLE_ERROR_INVALID_STATE);
        return 0;
    }
    if (!isfinite(feedback->reward))
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_NONFINITE_REWARD);
        return 0;
    }
    if (feedback->episode_terminal == 1U)
    {
        if (minisnn_agent_io_action_pending(cycle->agent_io) ||
            minisnn_agent_io_pending_sensor_tick(cycle->agent_io, &pending_tick) ||
            cycle->global_tick <= cycle->episode_start_tick ||
            feedback->source_tick != cycle->global_tick - 1U ||
            feedback->delivery_tick != cycle->global_tick ||
            (cycle->terminal_feedback_applied &&
             cycle->terminal_feedback_source_tick == feedback->source_tick))
        {
            cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_FEEDBACK_INVALID);
            return 0;
        }
    }
    else if (feedback->delivery_tick < feedback->source_tick)
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_FEEDBACK_INVALID);
        return 0;
    }
    if (feedback->source_tick < cycle->episode_start_tick ||
        feedback->source_tick >= cycle->global_tick)
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_UNKNOWN_SOURCE_TICK);
        return 0;
    }
    if (feedback->episode_terminal == 0U &&
        feedback->delivery_tick < cycle->global_tick)
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_FEEDBACK_TOO_LATE);
        return 0;
    }
    if (!minisnn_get_reward_config(cycle->network, &reward_config))
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_INVALID_STATE);
        return 0;
    }
    if (feedback->reward != 0.0 && !reward_config.enabled)
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_REWARD_UNAVAILABLE);
        return 0;
    }
    return 1;
}

static int reserve_feedback(MiniSNNAgentCycle *cycle)
{
    MiniSNNAgentFeedback *grown;
    size_t capacity;

    if (cycle->feedback_count < cycle->feedback_capacity)
        return 1;
    capacity = cycle->feedback_capacity == 0U ? 8U :
        cycle->feedback_capacity * 2U;
    if (capacity < cycle->feedback_capacity ||
        capacity > SIZE_MAX / sizeof(*cycle->feedback_queue))
        return 0;
    grown = realloc(cycle->feedback_queue, capacity * sizeof(*grown));
    if (grown == NULL)
        return 0;
    cycle->feedback_queue = grown;
    cycle->feedback_capacity = capacity;
    return 1;
}

static int feedback_events_are_deliverable(
    MiniSNNAgentCycle *cycle,
    const MiniSNNAgentFeedback *terminal_feedback,
    double *out_reward)
{
    MiniSNNRewardConfig reward_config;
    double pending_reward = 0.0;
    double total = 0.0;

    if (!minisnn_get_reward_config(cycle->network, &reward_config) ||
        !minisnn_get_pending_reward(cycle->network, &pending_reward))
        return 0;
    for (size_t index = 0U; index < cycle->feedback_count; index++)
    {
        const MiniSNNAgentFeedback *feedback = &cycle->feedback_queue[index];
        if (feedback->delivery_tick != cycle->global_tick)
            continue;
        if (!isfinite(feedback->reward) ||
            (feedback->reward != 0.0 && !reward_config.enabled) ||
            !isfinite(total + feedback->reward) ||
            !isfinite(pending_reward + feedback->reward) ||
            (!reward_config.clip_reward &&
             (pending_reward + feedback->reward < reward_config.reward_min ||
              pending_reward + feedback->reward > reward_config.reward_max)))
            return 0;
        total += feedback->reward;
        pending_reward += feedback->reward;
    }
    if (terminal_feedback != NULL)
    {
        if (!isfinite(terminal_feedback->reward) ||
            (terminal_feedback->reward != 0.0 && !reward_config.enabled) ||
            !isfinite(total + terminal_feedback->reward) ||
            !isfinite(pending_reward + terminal_feedback->reward) ||
            (!reward_config.clip_reward &&
             (pending_reward + terminal_feedback->reward < reward_config.reward_min ||
              pending_reward + terminal_feedback->reward > reward_config.reward_max)))
        {
            return 0;
        }
        total += terminal_feedback->reward;
    }
    if (out_reward != NULL)
        *out_reward = total;
    return 1;
}

static int deliver_due_feedback(MiniSNNAgentCycle *cycle,
                                uint32_t *out_count,
                                double *out_reward)
{
    size_t write_index = 0U;
    uint32_t count = 0U;
    double total = 0.0;

    for (size_t index = 0U; index < cycle->feedback_count; index++)
    {
        MiniSNNAgentFeedback feedback = cycle->feedback_queue[index];
        if (feedback.delivery_tick == cycle->global_tick)
        {
            if (feedback.reward != 0.0 &&
                !minisnn_queue_reward(cycle->network, feedback.reward))
                return 0;
            total += feedback.reward;
            count++;
        }
        else
        {
            cycle->feedback_queue[write_index++] = feedback;
        }
    }
    cycle->feedback_count = write_index;
    if (out_count != NULL)
        *out_count = count;
    if (out_reward != NULL)
        *out_reward = total;
    return 1;
}

static void remove_due_feedback(MiniSNNAgentCycle *cycle)
{
    size_t write_index = 0U;

    for (size_t index = 0U; index < cycle->feedback_count; index++)
    {
        if (cycle->feedback_queue[index].delivery_tick != cycle->global_tick)
            cycle->feedback_queue[write_index++] = cycle->feedback_queue[index];
    }
    cycle->feedback_count = write_index;
}

static int deliver_terminal_feedback(
    MiniSNNAgentCycle *cycle,
    const MiniSNNAgentFeedback *terminal_feedback,
    uint32_t *out_count,
    double *out_reward)
{
    uint32_t due_count = 0U;
    double due_reward = 0.0;

    if (cycle == NULL || terminal_feedback == NULL ||
        !feedback_events_are_deliverable(
            cycle, terminal_feedback, &due_reward))
    {
        return 0;
    }

    for (size_t index = 0U; index < cycle->feedback_count; index++)
    {
        const MiniSNNAgentFeedback *feedback = &cycle->feedback_queue[index];
        if (feedback->delivery_tick != cycle->global_tick)
            continue;
        due_count++;
        if (feedback->reward != 0.0 &&
            !minisnn_queue_reward(cycle->network, feedback->reward))
        {
            return 0;
        }
    }
    if ((terminal_feedback->reward != 0.0 &&
         !minisnn_queue_reward(cycle->network, terminal_feedback->reward)) ||
        !minisnn_apply_pending_reward_now(cycle->network))
    {
        return 0;
    }

    remove_due_feedback(cycle);
    cycle->terminal_feedback_source_tick = terminal_feedback->source_tick;
    cycle->terminal_feedback_applied = 1;
    if (out_count != NULL)
        *out_count = due_count + 1U;
    if (out_reward != NULL)
        *out_reward = due_reward;
    return 1;
}

static void write_diagnostics(const MiniSNNAgentCycle *cycle,
                              MiniSNNAgentCycleDiagnostics *diagnostics,
                              uint64_t tick,
                              uint32_t brain_steps,
                              uint64_t spikes,
                              uint32_t feedback_count,
                              double reward,
                              uint32_t failed_step)
{
    if (diagnostics == NULL)
        return;
    diagnostics->episode_id = cycle->episode_id;
    diagnostics->episode_tick = cycle->episode_tick;
    diagnostics->global_tick = tick;
    diagnostics->brain_steps_executed = brain_steps;
    diagnostics->total_spikes = spikes;
    diagnostics->feedback_events_delivered = feedback_count;
    diagnostics->reward_delivered = reward;
    diagnostics->state = cycle->state;
    diagnostics->failed_brain_step = failed_step;
}

MiniSNNAgentCycle *minisnn_agent_cycle_create(
    MiniSNN *network, MiniSNNAgentIOContext *agent_io,
    MiniSNNSensorEncoder *sensor_encoder, MiniSNNActionDecoder *action_decoder,
    MiniSNNAgentCycleError *out_error)
{
    MiniSNNAgentCycle *cycle;
    MiniSNNAgentCycleError error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    uint32_t neurons;
    uint32_t brain_steps;
    uint32_t action_count;

    if (network == NULL || agent_io == NULL || sensor_encoder == NULL ||
        action_decoder == NULL)
    {
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_INVALID_ARGUMENT);
        return NULL;
    }
    neurons = (uint32_t)minisnn_neuron_count(network);
    brain_steps = minisnn_sensor_encoder_brain_steps_per_tick(sensor_encoder);
    action_count = minisnn_action_decoder_action_count(action_decoder);
    if (neurons == 0U || brain_steps == 0U || action_count == 0U ||
        neurons != minisnn_sensor_encoder_neuron_count(sensor_encoder) ||
        neurons != minisnn_action_decoder_neuron_count(action_decoder) ||
        brain_steps != minisnn_action_decoder_brain_steps_per_tick(action_decoder))
    {
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_DIMENSION_MISMATCH);
        return NULL;
    }
    if (minisnn_sensor_encoder_sensor_schema_signature(sensor_encoder) !=
            minisnn_agent_io_sensor_schema_signature(agent_io) ||
        minisnn_action_decoder_action_schema_signature(action_decoder) !=
            minisnn_agent_io_action_schema_signature(agent_io))
    {
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_SCHEMA_MISMATCH);
        return NULL;
    }
    cycle = calloc(1U, sizeof(*cycle));
    if (cycle == NULL)
    {
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_ALLOCATION);
        return NULL;
    }
    cycle->network = network;
    cycle->agent_io = agent_io;
    cycle->sensor_encoder = sensor_encoder;
    cycle->action_decoder = action_decoder;
    if (!minisnn_neural_input_frame_init(&cycle->input_frame, neurons, brain_steps,
                                         NULL) ||
        !minisnn_neural_activity_frame_init(&cycle->activity_frame, neurons,
                                            brain_steps, NULL) ||
        !minisnn_action_frame_init(&cycle->action_frame, action_count) ||
        !minisnn_action_decoding_diagnostics_init(&cycle->decoder_diagnostics,
                                                  action_count))
    {
        minisnn_agent_cycle_destroy(&cycle);
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_ALLOCATION);
        return NULL;
    }
    cycle->state = MINISNN_AGENT_CYCLE_STATE_READY;
    cycle->last_error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    if (!contracts_match(cycle, &error))
    {
        minisnn_agent_cycle_destroy(&cycle);
        set_error(out_error, error);
        return NULL;
    }
    set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_NONE);
    return cycle;
}

void minisnn_agent_cycle_destroy(MiniSNNAgentCycle **cycle_ptr)
{
    MiniSNNAgentCycle *cycle;
    if (cycle_ptr == NULL || *cycle_ptr == NULL)
        return;
    cycle = *cycle_ptr;
    minisnn_neural_input_frame_destroy(&cycle->input_frame);
    minisnn_neural_activity_frame_destroy(&cycle->activity_frame);
    minisnn_action_frame_destroy(&cycle->action_frame);
    minisnn_action_decoding_diagnostics_destroy(&cycle->decoder_diagnostics);
    free(cycle->feedback_queue);
    free(cycle);
    *cycle_ptr = NULL;
}

int minisnn_agent_cycle_submit_feedback(MiniSNNAgentCycle *cycle,
                                        const MiniSNNAgentFeedback *feedback)
{
    if (!validate_feedback_for_submission(cycle, feedback))
        return 0;
    if (feedback->episode_terminal == 1U)
    {
        uint32_t delivered_count = 0U;
        double delivered_reward = 0.0;

        if (!deliver_terminal_feedback(
                cycle, feedback, &delivered_count, &delivered_reward))
        {
            fault_cycle(cycle, MINISNN_AGENT_CYCLE_ERROR_REWARD_UNAVAILABLE);
            return 0;
        }
        cycle->total_reward += delivered_reward;
        cycle->last_error = MINISNN_AGENT_CYCLE_ERROR_NONE;
        return 1;
    }
    if (!reserve_feedback(cycle))
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_ALLOCATION);
        return 0;
    }
    cycle->feedback_queue[cycle->feedback_count++] = *feedback;
    cycle->last_error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    return 1;
}

int minisnn_agent_cycle_run_tick(MiniSNNAgentCycle *cycle,
                                 MiniSNNAgentCycleDiagnostics *diagnostics)
{
    MiniSNNAgentCycleError error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    MiniSNNSensorEncoderError encoder_error = MINISNN_SENSOR_ENCODER_ERROR_NONE;
    MiniSNNActionDecoderError decoder_error = MINISNN_ACTION_DECODER_ERROR_NONE;
    uint64_t sensor_tick = 0U;
    uint64_t executed_episode_id;
    uint64_t executed_episode_tick;
    uint64_t spike_total = 0U;
    uint32_t feedback_count = 0U;
    uint32_t step;
    double feedback_reward = 0.0;

    if (cycle == NULL)
        return 0;
    executed_episode_id = cycle->episode_id;
    executed_episode_tick = cycle->episode_tick;
    if (cycle->state == MINISNN_AGENT_CYCLE_STATE_FAULTED)
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_FAULTED);
        return 0;
    }
    if (cycle->state == MINISNN_AGENT_CYCLE_STATE_RUNNING)
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_INVALID_STATE);
        return 0;
    }
    if (minisnn_agent_io_action_pending(cycle->agent_io))
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_ACTION_PENDING);
        return 0;
    }
    if (cycle->state == MINISNN_AGENT_CYCLE_STATE_ACTION_PENDING)
        cycle->state = MINISNN_AGENT_CYCLE_STATE_READY;
    if (!contracts_match(cycle, &error))
    {
        cycle_error(cycle, error);
        return 0;
    }
    if (!minisnn_agent_io_pending_sensor_tick(cycle->agent_io, &sensor_tick))
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_SENSOR_UNAVAILABLE);
        return 0;
    }
    if (sensor_tick != cycle->global_tick)
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_TICK_MISMATCH);
        return 0;
    }
    if (!feedback_events_are_deliverable(cycle, NULL, &feedback_reward))
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_REWARD_UNAVAILABLE);
        return 0;
    }

    cycle->state = MINISNN_AGENT_CYCLE_STATE_RUNNING;
    if (!deliver_due_feedback(cycle, &feedback_count, &feedback_reward))
    {
        cycle->state = MINISNN_AGENT_CYCLE_STATE_READY;
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_REWARD_UNAVAILABLE);
        return 0;
    }
    if (!minisnn_sensor_encoder_encode_from_agent_io(cycle->sensor_encoder,
                                                      cycle->agent_io,
                                                      &cycle->input_frame) ||
        cycle->input_frame.tick != sensor_tick ||
        !minisnn_neural_activity_frame_reset(&cycle->activity_frame, sensor_tick,
                                             &decoder_error))
    {
        fault_cycle(cycle, MINISNN_AGENT_CYCLE_ERROR_INVALID_STATE);
        write_diagnostics(cycle, diagnostics, sensor_tick, 0U, 0U,
                          feedback_count, feedback_reward, 0U);
        return 0;
    }

    for (step = 0U; step < cycle->input_frame.brain_step_count; step++)
    {
        int spikes;
        if (!minisnn_neural_input_frame_apply_step(&cycle->input_frame, step,
                                                    cycle->network,
                                                    &encoder_error))
        {
            fault_cycle(cycle, MINISNN_AGENT_CYCLE_ERROR_INVALID_STATE);
            write_diagnostics(cycle, diagnostics, sensor_tick, step, spike_total,
                              feedback_count, feedback_reward, step);
            return 0;
        }
        spikes = minisnn_step(cycle->network);
        if (spikes < 0)
        {
            fault_cycle(cycle, MINISNN_AGENT_CYCLE_ERROR_NETWORK_STEP);
            write_diagnostics(cycle, diagnostics, sensor_tick, step, spike_total,
                              feedback_count, feedback_reward, step);
            return 0;
        }
        if (!minisnn_neural_activity_frame_capture_step(&cycle->activity_frame,
                                                        step, cycle->network,
                                                        &decoder_error))
        {
            fault_cycle(cycle, MINISNN_AGENT_CYCLE_ERROR_ACTIVITY_CAPTURE);
            write_diagnostics(cycle, diagnostics, sensor_tick, step + 1U,
                              spike_total, feedback_count, feedback_reward, step);
            return 0;
        }
        spike_total += (uint64_t)spikes;
    }
    minisnn_clear_inputs(cycle->network);
    if (!minisnn_action_decoder_decode(cycle->action_decoder, &cycle->activity_frame,
                                       &cycle->action_frame,
                                       &cycle->decoder_diagnostics) ||
        cycle->action_frame.tick != sensor_tick ||
        !minisnn_agent_io_submit_action_and_finish_tick(cycle->agent_io,
                                                         &cycle->action_frame))
    {
        fault_cycle(cycle, MINISNN_AGENT_CYCLE_ERROR_ACTION_PUBLICATION);
        write_diagnostics(cycle, diagnostics, sensor_tick,
                          cycle->input_frame.brain_step_count, spike_total,
                          feedback_count, feedback_reward,
                          cycle->input_frame.brain_step_count);
        return 0;
    }

    cycle->total_ticks++;
    cycle->total_neural_steps += cycle->input_frame.brain_step_count;
    cycle->total_actions++;
    cycle->total_spikes += spike_total;
    cycle->total_reward += feedback_reward;
    cycle->state = MINISNN_AGENT_CYCLE_STATE_ACTION_PENDING;
    cycle->last_error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    if (diagnostics != NULL)
    {
        diagnostics->episode_id = executed_episode_id;
        diagnostics->episode_tick = executed_episode_tick;
        diagnostics->global_tick = sensor_tick;
        diagnostics->brain_steps_executed = cycle->input_frame.brain_step_count;
        diagnostics->total_spikes = spike_total;
        diagnostics->feedback_events_delivered = feedback_count;
        diagnostics->reward_delivered = feedback_reward;
        diagnostics->state = cycle->state;
        diagnostics->failed_brain_step = UINT32_MAX;
    }
    cycle->global_tick++;
    cycle->episode_tick++;
    return 1;
}

int minisnn_agent_cycle_reset_episode(MiniSNNAgentCycle *cycle)
{
    uint64_t pending_tick;

    if (cycle == NULL)
        return 0;
    if (cycle->state == MINISNN_AGENT_CYCLE_STATE_RUNNING ||
        minisnn_agent_io_action_pending(cycle->agent_io) ||
        minisnn_agent_io_pending_sensor_tick(cycle->agent_io, &pending_tick))
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_RESET_WHILE_BUSY);
        return 0;
    }
    minisnn_agent_io_reset(cycle->agent_io);
    if (!minisnn_reset_transient_state(cycle->network) ||
        !minisnn_neural_input_frame_reset(&cycle->input_frame, NULL) ||
        !minisnn_neural_activity_frame_reset(&cycle->activity_frame,
                                             cycle->global_tick, NULL) ||
        !minisnn_action_decoding_diagnostics_reset(&cycle->decoder_diagnostics))
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_INVALID_STATE);
        return 0;
    }
    memset(cycle->action_frame.values, 0,
           (size_t)cycle->action_frame.value_count *
               sizeof(*cycle->action_frame.values));
    cycle->action_frame.tick = cycle->global_tick;
    minisnn_sensor_encoder_reset(cycle->sensor_encoder);
    minisnn_action_decoder_reset(cycle->action_decoder);
    cycle->feedback_count = 0U;
    cycle->episode_id++;
    cycle->episode_tick = 0U;
    cycle->episode_start_tick = cycle->global_tick;
    cycle->reset_count++;
    cycle->state = MINISNN_AGENT_CYCLE_STATE_READY;
    cycle->last_error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    return 1;
}

MiniSNNAgentCycleState minisnn_agent_cycle_state(const MiniSNNAgentCycle *cycle)
{
    return cycle != NULL ? cycle->state : MINISNN_AGENT_CYCLE_STATE_FAULTED;
}

MiniSNNAgentCycleError minisnn_agent_cycle_last_error(
    const MiniSNNAgentCycle *cycle)
{
    return cycle != NULL ? cycle->last_error :
        MINISNN_AGENT_CYCLE_ERROR_INVALID_ARGUMENT;
}

const char *minisnn_agent_cycle_error_string(MiniSNNAgentCycleError error)
{
    static const char *const messages[] =
    {
        "ok", "argumento invalido", "estado invalido", "schemas incompativeis",
        "dimensoes incompativeis", "sensor indisponivel", "acao pendente",
        "tick incompativel", "feedback invalido", "feedback atrasado",
        "source tick desconhecido", "reward nao finito", "reward indisponivel",
        "falha no passo neural", "falha ao capturar atividade",
        "falha ao publicar acao", "reset durante ciclo ocupado", "falha de alocacao",
        "ciclo em falha", "fronteira de checkpoint instavel", "erro de E/S do checkpoint",
        "formato de checkpoint invalido", "assinatura de checkpoint incompativel",
        "checkpoint incompativel"
    };
    return (unsigned int)error < sizeof(messages) / sizeof(messages[0]) ?
        messages[error] : "erro de ciclo desconhecido";
}

uint64_t minisnn_agent_cycle_episode_id(const MiniSNNAgentCycle *cycle)
{
    return cycle != NULL ? cycle->episode_id : 0U;
}

uint64_t minisnn_agent_cycle_episode_tick(const MiniSNNAgentCycle *cycle)
{
    return cycle != NULL ? cycle->episode_tick : 0U;
}

uint64_t minisnn_agent_cycle_next_global_tick(const MiniSNNAgentCycle *cycle)
{
    return cycle != NULL ? cycle->global_tick : 0U;
}

uint64_t minisnn_agent_cycle_total_ticks(const MiniSNNAgentCycle *cycle)
{
    return cycle != NULL ? cycle->total_ticks : 0U;
}

uint64_t minisnn_agent_cycle_total_neural_steps(const MiniSNNAgentCycle *cycle)
{
    return cycle != NULL ? cycle->total_neural_steps : 0U;
}

uint64_t minisnn_agent_cycle_total_actions(const MiniSNNAgentCycle *cycle)
{
    return cycle != NULL ? cycle->total_actions : 0U;
}

double minisnn_agent_cycle_total_reward(const MiniSNNAgentCycle *cycle)
{
    return cycle != NULL ? cycle->total_reward : 0.0;
}

uint64_t minisnn_agent_cycle_reset_count(const MiniSNNAgentCycle *cycle)
{
    return cycle != NULL ? cycle->reset_count : 0U;
}

#ifdef MINISNN_TESTING
uint32_t minisnn_test_agent_cycle_feedback_count(
    const MiniSNNAgentCycle *cycle)
{
    return cycle != NULL && cycle->feedback_count <= UINT32_MAX ?
        (uint32_t)cycle->feedback_count : 0U;
}

int minisnn_test_agent_cycle_feedback_at(
    const MiniSNNAgentCycle *cycle,
    uint32_t index,
    MiniSNNAgentFeedback *out_feedback)
{
    if (cycle == NULL || out_feedback == NULL || index >= cycle->feedback_count)
        return 0;
    *out_feedback = cycle->feedback_queue[index];
    return 1;
}
#endif

static int checkpoint_write_u32(FILE *file, uint32_t value)
{
    unsigned char bytes[4];
    for (unsigned int shift = 0U; shift < 32U; shift += 8U)
        bytes[shift / 8U] = (unsigned char)((value >> shift) & 0xffU);
    return fwrite(bytes, 1U, sizeof(bytes), file) == sizeof(bytes);
}

static int checkpoint_write_u64(FILE *file, uint64_t value)
{
    unsigned char bytes[8];
    for (unsigned int shift = 0U; shift < 64U; shift += 8U)
        bytes[shift / 8U] = (unsigned char)((value >> shift) & 0xffU);
    return fwrite(bytes, 1U, sizeof(bytes), file) == sizeof(bytes);
}

static int checkpoint_read_u32(FILE *file, uint32_t *out_value)
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

static int checkpoint_read_u64(FILE *file, uint64_t *out_value)
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

static uint64_t checkpoint_double_bits(double value)
{
    uint64_t bits = 0U;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static double checkpoint_bits_double(uint64_t bits)
{
    double value = 0.0;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int checkpoint_write_double(FILE *file, double value)
{
    return isfinite(value) && checkpoint_write_u64(file, checkpoint_double_bits(value));
}

static int checkpoint_read_double(FILE *file, double *out_value)
{
    uint64_t bits;
    if (!checkpoint_read_u64(file, &bits))
        return 0;
    *out_value = checkpoint_bits_double(bits);
    return isfinite(*out_value);
}

static void checkpoint_hash_byte(uint64_t *hash, unsigned char value)
{
    *hash ^= (uint64_t)value;
    *hash *= AGENT_CYCLE_CHECKPOINT_FNV_PRIME;
}

static void checkpoint_hash_u32(uint64_t *hash, uint32_t value)
{
    for (unsigned int shift = 0U; shift < 32U; shift += 8U)
        checkpoint_hash_byte(hash, (unsigned char)((value >> shift) & 0xffU));
}

static void checkpoint_hash_u64(uint64_t *hash, uint64_t value)
{
    for (unsigned int shift = 0U; shift < 64U; shift += 8U)
        checkpoint_hash_byte(hash, (unsigned char)((value >> shift) & 0xffU));
}

static uint64_t checkpoint_file_signature(const char *path)
{
    FILE *file;
    unsigned char buffer[4096];
    size_t read_count;
    uint64_t hash = AGENT_CYCLE_CHECKPOINT_FNV_OFFSET;

    if (path == NULL || (file = fopen(path, "rb")) == NULL)
        return 0U;
    while ((read_count = fread(buffer, 1U, sizeof(buffer), file)) > 0U)
        for (size_t index = 0U; index < read_count; index++)
            checkpoint_hash_byte(&hash, buffer[index]);
    if (ferror(file))
        hash = 0U;
    fclose(file);
    return hash;
}

static int checkpoint_path(char *out_path, size_t out_size, const char *directory,
                           const char *filename)
{
    int written;
    if (out_path == NULL || out_size == 0U || directory == NULL || filename == NULL ||
        directory[0] == '\0' || filename[0] == '\0')
        return 0;
    written = snprintf(out_path, out_size, "%s/%s", directory, filename);
    return written >= 0 && (size_t)written < out_size;
}

static int checkpoint_directory_is_safe(const char *directory)
{
    const char *component;
    size_t length = 0U;
    if (directory == NULL || directory[0] == '\0' || directory[0] == '/' ||
        directory[0] == '\\')
        return 0;
    component = directory;
    for (const char *cursor = directory;; cursor++)
    {
        unsigned char value = (unsigned char)*cursor;
        if (value == '\0')
        {
            if (length == 0U || (length == 1U && component[0] == '.') ||
                (length == 2U && component[0] == '.' && component[1] == '.'))
                return 0;
            return 1;
        }
        if (value == ':' || value < 0x20U || value == 0x7fU)
            return 0;
        if (value == '/' || value == '\\')
        {
            if (length == 0U || (length == 1U && component[0] == '.') ||
                (length == 2U && component[0] == '.' && component[1] == '.'))
                return 0;
            component = cursor + 1U;
            length = 0U;
        }
        else
            length++;
    }
}

static int checkpoint_ensure_directory(const char *directory)
{
#ifdef _WIN32
    if (CreateDirectoryA(directory, NULL) != 0)
        return 1;
    return GetLastError() == ERROR_ALREADY_EXISTS;
#else
    if (mkdir(directory, 0777) == 0)
        return 1;
    return errno == EEXIST;
#endif
}

static int checkpoint_replace_file(const char *temporary, const char *final_path)
{
#ifdef _WIN32
    return MoveFileExA(temporary, final_path,
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary, final_path) == 0;
#endif
}

static uint64_t checkpoint_contract_signature(const MiniSNNAgentCycle *cycle)
{
    uint64_t hash = AGENT_CYCLE_CHECKPOINT_FNV_OFFSET;
    int neurons;
    if (cycle == NULL || cycle->network == NULL || cycle->agent_io == NULL ||
        cycle->sensor_encoder == NULL || cycle->action_decoder == NULL ||
        (neurons = minisnn_neuron_count(cycle->network)) <= 0)
        return 0U;
    checkpoint_hash_u32(&hash, UINT32_C(1));
    checkpoint_hash_u64(&hash, minisnn_agent_io_contract_signature(cycle->agent_io));
    checkpoint_hash_u64(&hash,
                        minisnn_sensor_encoder_contract_signature(cycle->sensor_encoder));
    checkpoint_hash_u64(&hash,
                        minisnn_action_decoder_contract_signature(cycle->action_decoder));
    checkpoint_hash_u64(&hash, minisnn_neuron_model_config_signature(cycle->network));
    checkpoint_hash_u32(&hash, (uint32_t)minisnn_neuron_model(cycle->network));
    checkpoint_hash_u32(&hash, (uint32_t)neurons);
    checkpoint_hash_u32(&hash,
        minisnn_sensor_encoder_brain_steps_per_tick(cycle->sensor_encoder));
    return hash;
}

static uint64_t checkpoint_state_signature(uint64_t contract,
                                           const uint64_t *components,
                                           size_t component_count)
{
    uint64_t hash = AGENT_CYCLE_CHECKPOINT_FNV_OFFSET;
    checkpoint_hash_u64(&hash, contract);
    for (size_t index = 0U; index < component_count; index++)
        checkpoint_hash_u64(&hash, components[index]);
    return hash;
}

static void checkpoint_state_destroy(AgentCycleCheckpointState *state)
{
    if (state == NULL)
        return;
    free(state->feedback_queue);
    memset(state, 0, sizeof(*state));
}

static int checkpoint_write_cycle_state(const MiniSNNAgentCycle *cycle,
                                        uint32_t boundary, FILE *file)
{
    if (cycle == NULL || file == NULL || !checkpoint_write_u32(
            file, AGENT_CYCLE_CHECKPOINT_STATE_MAGIC) ||
        !checkpoint_write_u32(file, boundary) ||
        !checkpoint_write_u64(file, cycle->episode_id) ||
        !checkpoint_write_u64(file, cycle->episode_tick) ||
        !checkpoint_write_u64(file, cycle->global_tick) ||
        !checkpoint_write_u64(file, cycle->episode_start_tick) ||
        !checkpoint_write_u64(file, cycle->total_ticks) ||
        !checkpoint_write_u64(file, cycle->total_neural_steps) ||
        !checkpoint_write_u64(file, cycle->total_actions) ||
        !checkpoint_write_u64(file, cycle->total_spikes) ||
        !checkpoint_write_u64(file, cycle->reset_count) ||
        !checkpoint_write_double(file, cycle->total_reward) ||
        !checkpoint_write_u64(file, cycle->terminal_feedback_source_tick) ||
        !checkpoint_write_u32(file, (uint32_t)cycle->terminal_feedback_applied) ||
        !checkpoint_write_u64(file, (uint64_t)cycle->feedback_count))
        return 0;
    for (size_t index = 0U; index < cycle->feedback_count; index++)
    {
        const MiniSNNAgentFeedback *feedback = &cycle->feedback_queue[index];
        if (!checkpoint_write_u64(file, feedback->source_tick) ||
            !checkpoint_write_u64(file, feedback->delivery_tick) ||
            !checkpoint_write_double(file, feedback->reward) ||
            !checkpoint_write_u32(file, (uint32_t)feedback->episode_terminal))
            return 0;
    }
    return 1;
}

static int checkpoint_read_cycle_state(FILE *file, AgentCycleCheckpointState *state)
{
    uint32_t magic, terminal_applied;
    uint64_t feedback_count;

    if (file == NULL || state == NULL || !checkpoint_read_u32(file, &magic) ||
        magic != AGENT_CYCLE_CHECKPOINT_STATE_MAGIC ||
        !checkpoint_read_u32(file, &state->boundary) || state->boundary > 1U ||
        !checkpoint_read_u64(file, &state->episode_id) ||
        !checkpoint_read_u64(file, &state->episode_tick) ||
        !checkpoint_read_u64(file, &state->global_tick) ||
        !checkpoint_read_u64(file, &state->episode_start_tick) ||
        state->episode_start_tick > state->global_tick ||
        state->episode_tick != state->global_tick - state->episode_start_tick ||
        !checkpoint_read_u64(file, &state->total_ticks) ||
        !checkpoint_read_u64(file, &state->total_neural_steps) ||
        !checkpoint_read_u64(file, &state->total_actions) ||
        !checkpoint_read_u64(file, &state->total_spikes) ||
        !checkpoint_read_u64(file, &state->reset_count) ||
        !checkpoint_read_double(file, &state->total_reward) ||
        !checkpoint_read_u64(file, &state->terminal_feedback_source_tick) ||
        !checkpoint_read_u32(file, &terminal_applied) || terminal_applied > 1U ||
        !checkpoint_read_u64(file, &feedback_count) ||
        feedback_count > AGENT_CYCLE_CHECKPOINT_MAX_FEEDBACK ||
        feedback_count > SIZE_MAX / sizeof(*state->feedback_queue))
        return 0;
    state->terminal_feedback_applied = (int)terminal_applied;
    if (feedback_count > 0U)
    {
        state->feedback_queue = calloc((size_t)feedback_count,
                                       sizeof(*state->feedback_queue));
        if (state->feedback_queue == NULL)
            return 0;
    }
    state->feedback_count = (size_t)feedback_count;
    for (size_t index = 0U; index < state->feedback_count; index++)
    {
        MiniSNNAgentFeedback *feedback = &state->feedback_queue[index];
        uint32_t terminal;
        if (!checkpoint_read_u64(file, &feedback->source_tick) ||
            !checkpoint_read_u64(file, &feedback->delivery_tick) ||
            !checkpoint_read_double(file, &feedback->reward) ||
            !checkpoint_read_u32(file, &terminal) || terminal != 0U ||
            feedback->source_tick < state->episode_start_tick ||
            feedback->source_tick >= state->global_tick ||
            feedback->delivery_tick < feedback->source_tick ||
            feedback->delivery_tick < state->global_tick)
        {
            checkpoint_state_destroy(state);
            return 0;
        }
        feedback->episode_terminal = 0U;
    }
    if (fgetc(file) != EOF)
    {
        checkpoint_state_destroy(state);
        return 0;
    }
    return 1;
}

static void checkpoint_apply_cycle_state(MiniSNNAgentCycle *cycle,
                                         AgentCycleCheckpointState *state)
{
    free(cycle->feedback_queue);
    cycle->feedback_queue = state->feedback_queue;
    cycle->feedback_count = state->feedback_count;
    cycle->feedback_capacity = state->feedback_count;
    state->feedback_queue = NULL;
    state->feedback_count = 0U;
    cycle->terminal_feedback_source_tick = state->terminal_feedback_source_tick;
    cycle->terminal_feedback_applied = state->terminal_feedback_applied;
    cycle->episode_id = state->episode_id;
    cycle->episode_tick = state->episode_tick;
    cycle->global_tick = state->global_tick;
    cycle->episode_start_tick = state->episode_start_tick;
    cycle->total_ticks = state->total_ticks;
    cycle->total_neural_steps = state->total_neural_steps;
    cycle->total_actions = state->total_actions;
    cycle->total_spikes = state->total_spikes;
    cycle->reset_count = state->reset_count;
    cycle->total_reward = state->total_reward;
    cycle->state = state->boundary == 0U ? MINISNN_AGENT_CYCLE_STATE_READY :
                                           MINISNN_AGENT_CYCLE_STATE_ACTION_PENDING;
    cycle->last_error = MINISNN_AGENT_CYCLE_ERROR_NONE;
}

static int checkpoint_write_component(const char *path,
                                      int (*writer)(FILE *, const void *),
                                      const void *object)
{
    FILE *file;
    int ok;
    if (path == NULL || writer == NULL || object == NULL || (file = fopen(path, "wb")) == NULL)
        return 0;
    ok = writer(file, object);
    if (fclose(file) != 0)
        ok = 0;
    if (!ok)
        remove(path);
    return ok;
}

static int checkpoint_write_agent_io_component(FILE *file, const void *object)
{
    return minisnn_agent_io_checkpoint_write((const MiniSNNAgentIOContext *)object, file);
}

static int checkpoint_write_encoder_component(FILE *file, const void *object)
{
    return minisnn_sensor_encoder_checkpoint_write((const MiniSNNSensorEncoder *)object, file);
}

static int checkpoint_write_decoder_component(FILE *file, const void *object)
{
    return minisnn_action_decoder_checkpoint_write((const MiniSNNActionDecoder *)object, file);
}

static int checkpoint_write_network_component(FILE *file, const void *object)
{
    return minisnn_network_checkpoint_write((const MiniSNN *)object, file);
}

static int checkpoint_write_manifest(const char *path, const MiniSNNAgentCycle *cycle,
                                     uint32_t boundary, uint64_t contract,
                                     uint64_t state_signature,
                                     const uint64_t hashes[5])
{
    FILE *file = fopen(path, "wb");
    uint64_t topology = 0U;
    int ok;
    if (file == NULL || !minisnn_get_topology_signature(cycle->network, &topology))
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    ok = fprintf(file,
                 "%s\nformat_version=1\ncheckpoint_boundary=%s\n"
                 "core_provenance=%s\n"
                 "neuron_model=%s\nneuron_count=%d\nbrain_steps_per_tick=%u\n"
                 "sensor_schema_signature=%016llx\naction_schema_signature=%016llx\n"
                 "encoder_contract_signature=%016llx\ndecoder_contract_signature=%016llx\n"
                 "network_signature=%016llx\nagent_cycle_checkpoint_contract_signature=%016llx\n"
                 "agent_cycle_checkpoint_state_signature=%016llx\nepisode_id=%llu\n"
                 "episode_tick=%llu\nglobal_tick=%llu\ncycle_state=%d\n"
                 "network_state_signature=%016llx\nagent_io_state_signature=%016llx\n"
                 "sensor_encoder_state_signature=%016llx\naction_decoder_state_signature=%016llx\n"
                 "agent_cycle_state_signature=%016llx\n",
                 AGENT_CYCLE_CHECKPOINT_VERSION, boundary == 0U ? "READY" : "ACTION_PENDING",
                 AGENT_CYCLE_CHECKPOINT_PROVENANCE,
                 minisnn_neuron_model_name(minisnn_neuron_model(cycle->network)),
                 minisnn_neuron_count(cycle->network),
                 minisnn_sensor_encoder_brain_steps_per_tick(cycle->sensor_encoder),
                 (unsigned long long)minisnn_agent_io_sensor_schema_signature(cycle->agent_io),
                 (unsigned long long)minisnn_agent_io_action_schema_signature(cycle->agent_io),
                 (unsigned long long)minisnn_sensor_encoder_contract_signature(cycle->sensor_encoder),
                 (unsigned long long)minisnn_action_decoder_contract_signature(cycle->action_decoder),
                 (unsigned long long)topology, (unsigned long long)contract,
                 (unsigned long long)state_signature, (unsigned long long)cycle->episode_id,
                 (unsigned long long)cycle->episode_tick, (unsigned long long)cycle->global_tick,
                 (int)(boundary == 0U ? MINISNN_AGENT_CYCLE_STATE_READY :
                                       MINISNN_AGENT_CYCLE_STATE_ACTION_PENDING),
                 (unsigned long long)hashes[0], (unsigned long long)hashes[1],
                 (unsigned long long)hashes[2], (unsigned long long)hashes[3],
                 (unsigned long long)hashes[4]) >= 0;
    if (fclose(file) != 0)
        ok = 0;
    if (!ok)
        remove(path);
    return ok;
}

int minisnn_agent_cycle_save_checkpoint(
    const MiniSNNAgentCycle *cycle, const char *directory,
    MiniSNNAgentCycleError *out_error)
{
    char paths[6][AGENT_CYCLE_CHECKPOINT_PATH_MAX];
    char temporaries[6][AGENT_CYCLE_CHECKPOINT_PATH_MAX];
    const char *names[6] = {"network_state.bin", "agent_io_state.bin",
                            "sensor_encoder_state.bin", "action_decoder_state.bin",
                            "agent_cycle_state.bin", "manifest.txt"};
    const void *objects[4];
    int (*writers[4])(FILE *, const void *);
    uint64_t hashes[5];
    uint64_t contract;
    uint32_t boundary;
    int io_boundary;

    if (cycle == NULL || !checkpoint_directory_is_safe(directory))
    {
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (cycle->state == MINISNN_AGENT_CYCLE_STATE_RUNNING ||
        cycle->state == MINISNN_AGENT_CYCLE_STATE_FAULTED ||
        !contracts_match(cycle, NULL) ||
        (io_boundary = minisnn_agent_io_checkpoint_boundary(cycle->agent_io,
                                                             cycle->global_tick)) < 0)
    {
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_UNSTABLE);
        return 0;
    }
    boundary = (uint32_t)io_boundary;
    contract = checkpoint_contract_signature(cycle);
    if (contract == 0U || !checkpoint_ensure_directory(directory))
    {
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_IO);
        return 0;
    }
    objects[0] = cycle->network;
    objects[1] = cycle->agent_io;
    objects[2] = cycle->sensor_encoder;
    objects[3] = cycle->action_decoder;
    writers[0] = checkpoint_write_network_component;
    writers[1] = checkpoint_write_agent_io_component;
    writers[2] = checkpoint_write_encoder_component;
    writers[3] = checkpoint_write_decoder_component;
    for (size_t index = 0U; index < 6U; index++)
    {
        int temporary_length;
        if (!checkpoint_path(paths[index], sizeof(paths[index]), directory, names[index]) ||
            (temporary_length = snprintf(temporaries[index], sizeof(temporaries[index]),
                                         "%s.tmp", paths[index])) < 0 ||
            (size_t)temporary_length >= sizeof(temporaries[index]))
        {
            set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_IO);
            return 0;
        }
    }
    for (size_t index = 0U; index < 4U; index++)
    {
        if (!checkpoint_write_component(temporaries[index], writers[index], objects[index]) ||
            !checkpoint_replace_file(temporaries[index], paths[index]))
        {
            for (size_t cleanup = 0U; cleanup < 6U; cleanup++)
                remove(temporaries[cleanup]);
            set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_IO);
            return 0;
        }
        hashes[index] = checkpoint_file_signature(paths[index]);
        if (hashes[index] == 0U)
        {
            set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_IO);
            return 0;
        }
    }
    {
        FILE *state_file = fopen(temporaries[4], "wb");
        int state_ok = state_file != NULL;
        if (state_ok)
            state_ok = checkpoint_write_cycle_state(cycle, boundary, state_file);
        if (state_file != NULL && fclose(state_file) != 0)
            state_ok = 0;
        if (!state_ok || !checkpoint_replace_file(temporaries[4], paths[4]))
        {
            remove(temporaries[4]);
            set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_IO);
            return 0;
        }
    }
    hashes[4] = checkpoint_file_signature(paths[4]);
    if (hashes[4] == 0U ||
        !checkpoint_write_manifest(temporaries[5], cycle, boundary, contract,
            checkpoint_state_signature(contract, hashes, 5U), hashes) ||
        !checkpoint_replace_file(temporaries[5], paths[5]))
    {
        remove(temporaries[5]);
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_IO);
        return 0;
    }
    set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_NONE);
    return 1;
}

typedef struct
{
    uint32_t boundary;
    MiniSNNNeuronModel model;
    uint32_t neurons;
    uint32_t brain_steps;
    uint64_t sensor_schema;
    uint64_t action_schema;
    uint64_t encoder_contract;
    uint64_t decoder_contract;
    uint64_t topology_signature;
    uint64_t contract_signature;
    uint64_t state_signature;
    uint64_t episode_id;
    uint64_t episode_tick;
    uint64_t global_tick;
    uint32_t cycle_state;
    uint64_t component_hashes[5];
} AgentCycleCheckpointManifest;

static int checkpoint_read_text_line(FILE *file, char *line, size_t size)
{
    size_t length;
    if (file == NULL || line == NULL || size < 2U || fgets(line, (int)size, file) == NULL)
        return 0;
    length = strlen(line);
    if (length == 0U || line[length - 1U] != '\n')
        return 0;
    line[length - 1U] = '\0';
    return 1;
}

static int checkpoint_read_key(FILE *file, const char *key, char *value,
                               size_t value_size)
{
    char line[256];
    size_t key_size;
    if (file == NULL || key == NULL || value == NULL || value_size == 0U ||
        !checkpoint_read_text_line(file, line, sizeof(line)))
        return 0;
    key_size = strlen(key);
    if (strncmp(line, key, key_size) != 0 || line[key_size] != '=')
        return 0;
    if (strlen(line + key_size + 1U) >= value_size)
        return 0;
    strcpy(value, line + key_size + 1U);
    return 1;
}

static int checkpoint_parse_u64(const char *text, uint64_t *out_value)
{
    char *end = NULL;
    unsigned long long value;
    if (text == NULL || out_value == NULL || text[0] == '\0')
        return 0;
    value = strtoull(text, &end, 10);
    if (end == text || *end != '\0')
        return 0;
    *out_value = (uint64_t)value;
    return 1;
}

static int checkpoint_parse_hex64(const char *text, uint64_t *out_value)
{
    char *end = NULL;
    unsigned long long value;
    if (text == NULL || out_value == NULL || strlen(text) != 16U)
        return 0;
    value = strtoull(text, &end, 16);
    if (end == text || *end != '\0')
        return 0;
    *out_value = (uint64_t)value;
    return 1;
}

static int checkpoint_read_manifest(const char *path,
                                    AgentCycleCheckpointManifest *manifest)
{
    FILE *file;
    char line[256];
    char value[128];
    uint64_t parsed;
    int ok = 0;
    if (path == NULL || manifest == NULL || (file = fopen(path, "rb")) == NULL)
        return 0;
    memset(manifest, 0, sizeof(*manifest));
    if (!checkpoint_read_text_line(file, line, sizeof(line)) ||
        strcmp(line, AGENT_CYCLE_CHECKPOINT_VERSION) != 0 ||
        !checkpoint_read_key(file, "format_version", value, sizeof(value)) ||
        strcmp(value, "1") != 0 ||
        !checkpoint_read_key(file, "checkpoint_boundary", value, sizeof(value)))
        goto done;
    if (strcmp(value, "READY") == 0)
        manifest->boundary = 0U;
    else if (strcmp(value, "ACTION_PENDING") == 0)
        manifest->boundary = 1U;
    else
        goto done;
    if (!checkpoint_read_key(file, "core_provenance", value, sizeof(value)) ||
        strcmp(value, AGENT_CYCLE_CHECKPOINT_PROVENANCE) != 0)
        goto done;
    if (!checkpoint_read_key(file, "neuron_model", value, sizeof(value)) ||
        !neuron_model_from_name(value, &manifest->model) ||
        !checkpoint_read_key(file, "neuron_count", value, sizeof(value)) ||
        !checkpoint_parse_u64(value, &parsed) || parsed == 0U || parsed > UINT32_MAX)
        goto done;
    manifest->neurons = (uint32_t)parsed;
    if (!checkpoint_read_key(file, "brain_steps_per_tick", value, sizeof(value)) ||
        !checkpoint_parse_u64(value, &parsed) || parsed == 0U || parsed > UINT32_MAX)
        goto done;
    manifest->brain_steps = (uint32_t)parsed;
    if (!checkpoint_read_key(file, "sensor_schema_signature", value, sizeof(value)) ||
        !checkpoint_parse_hex64(value, &manifest->sensor_schema) ||
        !checkpoint_read_key(file, "action_schema_signature", value, sizeof(value)) ||
        !checkpoint_parse_hex64(value, &manifest->action_schema) ||
        !checkpoint_read_key(file, "encoder_contract_signature", value, sizeof(value)) ||
        !checkpoint_parse_hex64(value, &manifest->encoder_contract) ||
        !checkpoint_read_key(file, "decoder_contract_signature", value, sizeof(value)) ||
        !checkpoint_parse_hex64(value, &manifest->decoder_contract) ||
        !checkpoint_read_key(file, "network_signature", value, sizeof(value)) ||
        !checkpoint_parse_hex64(value, &manifest->topology_signature) ||
        !checkpoint_read_key(file, "agent_cycle_checkpoint_contract_signature", value,
                             sizeof(value)) ||
        !checkpoint_parse_hex64(value, &manifest->contract_signature) ||
        !checkpoint_read_key(file, "agent_cycle_checkpoint_state_signature", value,
                             sizeof(value)) ||
        !checkpoint_parse_hex64(value, &manifest->state_signature) ||
        !checkpoint_read_key(file, "episode_id", value, sizeof(value)) ||
        !checkpoint_parse_u64(value, &manifest->episode_id) ||
        !checkpoint_read_key(file, "episode_tick", value, sizeof(value)) ||
        !checkpoint_parse_u64(value, &manifest->episode_tick) ||
        !checkpoint_read_key(file, "global_tick", value, sizeof(value)) ||
        !checkpoint_parse_u64(value, &manifest->global_tick) ||
        !checkpoint_read_key(file, "cycle_state", value, sizeof(value)) ||
        !checkpoint_parse_u64(value, &parsed) || parsed > UINT32_MAX)
        goto done;
    manifest->cycle_state = (uint32_t)parsed;
    for (size_t index = 0U; index < 5U; index++)
    {
        const char *key = index == 0U ? "network_state_signature" :
            index == 1U ? "agent_io_state_signature" :
            index == 2U ? "sensor_encoder_state_signature" :
            index == 3U ? "action_decoder_state_signature" :
                          "agent_cycle_state_signature";
        if (!checkpoint_read_key(file, key, value, sizeof(value)) ||
            !checkpoint_parse_hex64(value, &manifest->component_hashes[index]))
            goto done;
    }
    if (fgetc(file) != EOF)
        goto done;
    ok = 1;
done:
    fclose(file);
    return ok;
}

static int checkpoint_component_hashes_match(char paths[6][AGENT_CYCLE_CHECKPOINT_PATH_MAX],
                                             const AgentCycleCheckpointManifest *manifest)
{
    uint64_t hashes[5];
    if (manifest == NULL)
        return 0;
    for (size_t index = 0U; index < 5U; index++)
    {
        hashes[index] = checkpoint_file_signature(paths[index]);
        if (hashes[index] == 0U || hashes[index] != manifest->component_hashes[index])
            return 0;
    }
    return checkpoint_state_signature(manifest->contract_signature, hashes, 5U) ==
        manifest->state_signature;
}

static int checkpoint_load_component(const char *path,
                                     int (*loader)(void *, FILE *), void *object)
{
    FILE *file;
    int ok;
    if (path == NULL || loader == NULL || object == NULL || (file = fopen(path, "rb")) == NULL)
        return 0;
    ok = loader(object, file);
    if (fclose(file) != 0)
        ok = 0;
    return ok;
}

static int checkpoint_load_agent_io_component(void *object, FILE *file)
{
    return minisnn_agent_io_checkpoint_load((MiniSNNAgentIOContext *)object, file);
}

static int checkpoint_load_encoder_component(void *object, FILE *file)
{
    return minisnn_sensor_encoder_checkpoint_load((MiniSNNSensorEncoder *)object, file);
}

static int checkpoint_load_decoder_component(void *object, FILE *file)
{
    return minisnn_action_decoder_checkpoint_load((MiniSNNActionDecoder *)object, file);
}

static int checkpoint_load_network_component(void *object, FILE *file)
{
    return minisnn_network_checkpoint_load((MiniSNN *)object, file);
}

typedef struct
{
    FILE *agent_io;
    FILE *encoder;
    FILE *network;
} AgentCycleCheckpointRollback;

static void checkpoint_rollback_destroy(AgentCycleCheckpointRollback *rollback)
{
    if (rollback == NULL)
        return;
    if (rollback->agent_io != NULL)
        fclose(rollback->agent_io);
    if (rollback->encoder != NULL)
        fclose(rollback->encoder);
    if (rollback->network != NULL)
        fclose(rollback->network);
    memset(rollback, 0, sizeof(*rollback));
}

static int checkpoint_backup_component(FILE **out_file,
                                       int (*writer)(FILE *, const void *),
                                       const void *object)
{
    FILE *file;
    if (out_file == NULL || writer == NULL || object == NULL ||
        (file = tmpfile()) == NULL)
        return 0;
    if (!writer(file, object) || fflush(file) != 0 || fseek(file, 0L, SEEK_SET) != 0)
    {
        fclose(file);
        return 0;
    }
    *out_file = file;
    return 1;
}

static int checkpoint_restore_component(FILE *file,
                                        int (*loader)(void *, FILE *),
                                        void *object)
{
    if (file == NULL || loader == NULL || object == NULL ||
        fseek(file, 0L, SEEK_SET) != 0)
        return 0;
    return loader(object, file) && !ferror(file);
}

static int checkpoint_create_rollback(const MiniSNNAgentCycle *cycle,
                                      AgentCycleCheckpointRollback *rollback)
{
    if (cycle == NULL || rollback == NULL)
        return 0;
    memset(rollback, 0, sizeof(*rollback));
    if (!checkpoint_backup_component(&rollback->agent_io,
                                     checkpoint_write_agent_io_component,
                                     cycle->agent_io) ||
        !checkpoint_backup_component(&rollback->encoder,
                                     checkpoint_write_encoder_component,
                                     cycle->sensor_encoder) ||
        !checkpoint_backup_component(&rollback->network,
                                     checkpoint_write_network_component,
                                     cycle->network))
    {
        checkpoint_rollback_destroy(rollback);
        return 0;
    }
    return 1;
}

static int checkpoint_restore_rollback(MiniSNNAgentCycle *cycle,
                                       AgentCycleCheckpointRollback *rollback)
{
    int ok;
    if (cycle == NULL || rollback == NULL)
        return 0;
    /* Restore in the same component order used by loading. Every private
     * loader commits from a parsed temporary state, so no partial component
     * state escapes a failed checkpoint operation. */
    ok = checkpoint_restore_component(rollback->agent_io,
                                      checkpoint_load_agent_io_component,
                                      cycle->agent_io) &&
        checkpoint_restore_component(rollback->encoder,
                                     checkpoint_load_encoder_component,
                                     cycle->sensor_encoder) &&
        checkpoint_restore_component(rollback->network,
                                     checkpoint_load_network_component,
                                     cycle->network);
    checkpoint_rollback_destroy(rollback);
    return ok;
}

int minisnn_agent_cycle_load_checkpoint(
    MiniSNNAgentCycle *cycle, const char *directory,
    MiniSNNAgentCycleError *out_error)
{
    const char *names[6] = {"network_state.bin", "agent_io_state.bin",
                            "sensor_encoder_state.bin", "action_decoder_state.bin",
                            "agent_cycle_state.bin", "manifest.txt"};
    char paths[6][AGENT_CYCLE_CHECKPOINT_PATH_MAX];
    AgentCycleCheckpointManifest manifest;
    AgentCycleCheckpointState state = {0};
    AgentCycleCheckpointRollback rollback = {0};
    FILE *state_file = NULL;
    MiniSNNAgentCycleError contract_error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    uint64_t topology = 0U;
    int boundary;

    if (cycle == NULL || !checkpoint_directory_is_safe(directory))
    {
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (cycle->state == MINISNN_AGENT_CYCLE_STATE_RUNNING ||
        cycle->state == MINISNN_AGENT_CYCLE_STATE_FAULTED)
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_UNSTABLE);
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_UNSTABLE);
        return 0;
    }
    for (size_t index = 0U; index < 6U; index++)
    {
        if (!checkpoint_path(paths[index], sizeof(paths[index]), directory, names[index]))
        {
            cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_IO);
            set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_IO);
            return 0;
        }
    }
    if (!checkpoint_read_manifest(paths[5], &manifest))
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_FORMAT);
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_FORMAT);
        return 0;
    }
    if (manifest.contract_signature != checkpoint_contract_signature(cycle) ||
        manifest.model != minisnn_neuron_model(cycle->network) ||
        manifest.neurons != (uint32_t)minisnn_neuron_count(cycle->network) ||
        manifest.brain_steps !=
            minisnn_sensor_encoder_brain_steps_per_tick(cycle->sensor_encoder) ||
        manifest.sensor_schema != minisnn_agent_io_sensor_schema_signature(cycle->agent_io) ||
        manifest.action_schema != minisnn_agent_io_action_schema_signature(cycle->agent_io) ||
        manifest.encoder_contract !=
            minisnn_sensor_encoder_contract_signature(cycle->sensor_encoder) ||
        manifest.decoder_contract !=
            minisnn_action_decoder_contract_signature(cycle->action_decoder))
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_INCOMPATIBLE);
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_INCOMPATIBLE);
        return 0;
    }
    if (!checkpoint_component_hashes_match(paths, &manifest))
    {
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_SIGNATURE);
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_SIGNATURE);
        return 0;
    }
    state_file = fopen(paths[4], "rb");
    if (state_file == NULL)
    {
        checkpoint_state_destroy(&state);
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_FORMAT);
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_FORMAT);
        return 0;
    }
    {
        int state_ok = checkpoint_read_cycle_state(state_file, &state);
        if (fclose(state_file) != 0)
            state_ok = 0;
        state_file = NULL;
        if (!state_ok || state.boundary != manifest.boundary ||
            manifest.cycle_state != (uint32_t)(state.boundary == 0U ?
                MINISNN_AGENT_CYCLE_STATE_READY : MINISNN_AGENT_CYCLE_STATE_ACTION_PENDING) ||
            state.episode_id != manifest.episode_id || state.episode_tick != manifest.episode_tick ||
            state.global_tick != manifest.global_tick)
        {
            checkpoint_state_destroy(&state);
            cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_FORMAT);
            set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_FORMAT);
            return 0;
        }
    }
    /* Every component file is signature-verified before the first live
     * mutation. Backups keep the cross-component operation atomic even when
     * a structurally valid but semantically incompatible component is found
     * after an earlier component has been restored. */
    if (!checkpoint_create_rollback(cycle, &rollback))
    {
        checkpoint_state_destroy(&state);
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_IO);
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_IO);
        return 0;
    }
    if (!checkpoint_load_component(paths[1], checkpoint_load_agent_io_component,
                                   cycle->agent_io) ||
        !checkpoint_load_component(paths[2], checkpoint_load_encoder_component,
                                   cycle->sensor_encoder) ||
        !checkpoint_load_component(paths[3], checkpoint_load_decoder_component,
                                   cycle->action_decoder) ||
        !checkpoint_load_component(paths[0], checkpoint_load_network_component,
                                   cycle->network))
    {
        checkpoint_state_destroy(&state);
        if (!checkpoint_restore_rollback(cycle, &rollback))
        {
            cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_IO);
            set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_IO);
            return 0;
        }
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_FORMAT);
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_FORMAT);
        return 0;
    }
    boundary = minisnn_agent_io_checkpoint_boundary(cycle->agent_io, state.global_tick);
    if (boundary < 0 || (uint32_t)boundary != state.boundary ||
        !contracts_match(cycle, &contract_error) ||
        !minisnn_get_topology_signature(cycle->network, &topology) ||
        topology != manifest.topology_signature)
    {
        checkpoint_state_destroy(&state);
        if (!checkpoint_restore_rollback(cycle, &rollback))
        {
            cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_IO);
            set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_IO);
            return 0;
        }
        cycle_error(cycle, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_INCOMPATIBLE);
        set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_CHECKPOINT_INCOMPATIBLE);
        return 0;
    }
    checkpoint_apply_cycle_state(cycle, &state);
    checkpoint_state_destroy(&state);
    checkpoint_rollback_destroy(&rollback);
    set_error(out_error, MINISNN_AGENT_CYCLE_ERROR_NONE);
    return 1;
}
