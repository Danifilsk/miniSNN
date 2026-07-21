#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "minisnn.h"

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
        "ciclo em falha"
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
