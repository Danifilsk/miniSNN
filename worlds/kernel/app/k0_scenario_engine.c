#include "k0_scenario_engine.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minisnn_worlds_kernel.h"

static void set_error(char *message, size_t message_size, const char *format, ...)
{
    va_list arguments;

    if (message == NULL || message_size == 0U)
    {
        return;
    }
    va_start(arguments, format);
    (void)vsnprintf(message, message_size, format, arguments);
    va_end(arguments);
}

static int grow_array(void **items, size_t *capacity, size_t item_size)
{
    size_t next_capacity;
    void *next_items;

    if (*capacity > SIZE_MAX / 2U)
    {
        return 0;
    }
    next_capacity = *capacity == 0U ? 32U : *capacity * 2U;
    if (next_capacity > SIZE_MAX / item_size)
    {
        return 0;
    }
    next_items = realloc(*items, next_capacity * item_size);
    if (next_items == NULL)
    {
        return 0;
    }
    *items = next_items;
    *capacity = next_capacity;
    return 1;
}

static int append_trace(
    K0ScenarioRunResult *result,
    size_t *capacity,
    const MiniSNNWorldsKernelDiagnostics *diagnostics)
{
    K0ScenarioTraceRow *row;

    if (result->trace_row_count == *capacity &&
        !grow_array((void **)&result->trace_rows, capacity, sizeof(*result->trace_rows)))
    {
        return 0;
    }
    row = &result->trace_rows[result->trace_row_count++];
    row->tick = diagnostics->completed_ticks;
    row->state_hash = diagnostics->current_state_hash;
    row->alive_entities = diagnostics->alive_entities;
    row->pending_commands = diagnostics->pending_commands;
    row->last_tick_events = diagnostics->last_tick_events;
    row->total_entities_created = diagnostics->total_entities_created;
    row->total_entities_destroyed = diagnostics->total_entities_destroyed;
    row->total_commands_submitted = diagnostics->total_commands_submitted;
    row->total_commands_applied = diagnostics->total_commands_applied;
    row->total_commands_rejected = diagnostics->total_commands_rejected;
    row->total_events_emitted = diagnostics->total_events_emitted;
    row->random_streams = diagnostics->random_streams;
    row->total_random_u32_generated = diagnostics->total_random_u32_generated;
    row->state_hash_version = diagnostics->state_hash_version;
    row->prng_version = diagnostics->prng_version;
    return 1;
}

static int append_events(
    K0ScenarioRunResult *result,
    size_t *capacity,
    const MiniSNNWorldsKernel *kernel)
{
    size_t index;
    size_t count = minisnn_worlds_kernel_last_tick_event_count(kernel);

    for (index = 0U; index < count; ++index)
    {
        MiniSNNWorldsKernelEvent event;
        K0ScenarioEventRow *row;

        if (minisnn_worlds_kernel_last_tick_event_at(kernel, index, &event) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return 0;
        }
        if (result->event_row_count == *capacity &&
            !grow_array((void **)&result->event_rows, capacity, sizeof(*result->event_rows)))
        {
            return 0;
        }
        row = &result->event_rows[result->event_row_count++];
        row->event_id = event.event_id.value;
        row->tick = event.tick;
        row->event_type = (uint32_t)event.type;
        row->command_id = event.command_id.value;
        row->issuer_entity_id = event.issuer.value;
        row->subject_entity_id = event.subject.value;
        row->rejection_reason = (uint32_t)event.rejection;
    }
    return 1;
}

static uint64_t pending_create_count(const MiniSNNWorldsKernel *kernel)
{
    size_t index;
    uint64_t count = 0U;

    for (index = 0U; index < minisnn_worlds_kernel_pending_command_count(kernel); ++index)
    {
        MiniSNNWorldsKernelCommandInfo command;

        if (minisnn_worlds_kernel_pending_command_at(kernel, index, &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return UINT64_MAX;
        }
        if (command.type == MINISNN_WORLDS_KERNEL_COMMAND_CREATE_ENTITY)
        {
            ++count;
        }
    }
    return count;
}

static uint64_t pending_destroy_count(const MiniSNNWorldsKernel *kernel)
{
    size_t index;
    uint64_t count = 0U;

    for (index = 0U; index < minisnn_worlds_kernel_pending_command_count(kernel); ++index)
    {
        MiniSNNWorldsKernelCommandInfo command;

        if (minisnn_worlds_kernel_pending_command_at(kernel, index, &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return UINT64_MAX;
        }
        if (command.type == MINISNN_WORLDS_KERNEL_COMMAND_DESTROY_ENTITY)
        {
            ++count;
        }
    }
    return count;
}

static int queue_initial_entities(
    MiniSNNWorldsKernel *kernel,
    const K0ScenarioConfig *config)
{
    MiniSNNWorldsKernelEntityId external = { 0U };
    MiniSNNWorldsKernelCommandId command_id;
    uint64_t index;

    for (index = 0U; index < config->initial_entities; ++index)
    {
        if (minisnn_worlds_kernel_queue_create_entity(kernel, 1U, config->priority_base,
                                                       external, &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return 0;
        }
    }
    return 1;
}

static int queue_periodic_creates(
    MiniSNNWorldsKernel *kernel,
    const K0ScenarioConfig *config,
    const MiniSNNWorldsKernelDiagnostics *diagnostics,
    MiniSNNWorldsTick target_tick)
{
    MiniSNNWorldsKernelEntityId external = { 0U };
    MiniSNNWorldsKernelCommandId command_id;
    uint64_t pending = pending_create_count(kernel);
    uint64_t available;
    uint64_t count;
    uint64_t index;

    if (pending == UINT64_MAX || diagnostics->alive_entities > config->maximum_alive_entities ||
        pending > config->maximum_alive_entities - diagnostics->alive_entities)
    {
        return 0;
    }
    available = config->maximum_alive_entities - diagnostics->alive_entities - pending;
    count = config->create_batch < available ? config->create_batch : available;
    for (index = 0U; index < count; ++index)
    {
        if (minisnn_worlds_kernel_queue_create_entity(kernel, target_tick,
                                                       config->priority_base,
                                                       external, &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return 0;
        }
    }
    return 1;
}

static int selected_contains(
    const MiniSNNWorldsKernelEntityId *selected,
    size_t count,
    MiniSNNWorldsKernelEntityId candidate)
{
    size_t index;

    for (index = 0U; index < count; ++index)
    {
        if (selected[index].value == candidate.value)
        {
            return 1;
        }
    }
    return 0;
}

static int queue_periodic_destroys(
    MiniSNNWorldsKernel *kernel,
    const K0ScenarioConfig *config,
    const MiniSNNWorldsKernelDiagnostics *diagnostics,
    MiniSNNWorldsTick target_tick,
    MiniSNNWorldsKernelRandomStreamKey selection_key)
{
    MiniSNNWorldsKernelEntityId external = { 0U };
    MiniSNNWorldsKernelEntityId selected[K0_SCENARIO_BATCH_MAX];
    MiniSNNWorldsKernelCommandId command_id;
    uint64_t pending = pending_destroy_count(kernel);
    uint64_t available;
    uint64_t requested;
    size_t alive_count;
    size_t selected_count = 0U;
    uint64_t attempt_limit;
    uint64_t attempts = 0U;

    if (pending == UINT64_MAX || diagnostics->alive_entities <= config->minimum_alive_entities ||
        pending >= diagnostics->alive_entities - config->minimum_alive_entities)
    {
        return pending == UINT64_MAX ? 0 : 1;
    }
    available = diagnostics->alive_entities - config->minimum_alive_entities - pending;
    requested = config->destroy_batch < available ? config->destroy_batch : available;
    alive_count = minisnn_worlds_kernel_entity_count(kernel);
    if (alive_count == 0U || alive_count > UINT32_MAX)
    {
        return 0;
    }
    attempt_limit = requested * UINT64_C(16) + UINT64_C(16);
    while (selected_count < (size_t)requested && attempts < attempt_limit)
    {
        uint32_t alive_index;
        MiniSNNWorldsKernelEntityId target;

        if (minisnn_worlds_kernel_random_bounded_u32(kernel, selection_key,
                                                      (uint32_t)alive_count,
                                                      &alive_index) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
            minisnn_worlds_kernel_entity_at(kernel, (size_t)alive_index, &target) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return 0;
        }
        if (!selected_contains(selected, selected_count, target))
        {
            selected[selected_count++] = target;
        }
        ++attempts;
    }
    while (selected_count < (size_t)requested)
    {
        size_t index;
        int found = 0;

        for (index = 0U; index < alive_count; ++index)
        {
            MiniSNNWorldsKernelEntityId target;

            if (minisnn_worlds_kernel_entity_at(kernel, index, &target) !=
                MINISNN_WORLDS_KERNEL_ERROR_NONE)
            {
                return 0;
            }
            if (!selected_contains(selected, selected_count, target))
            {
                selected[selected_count++] = target;
                found = 1;
                break;
            }
        }
        if (!found)
        {
            return 0;
        }
    }
    for (attempts = 0U; attempts < selected_count; ++attempts)
    {
        if (minisnn_worlds_kernel_queue_destroy_entity(kernel, target_tick,
                                                        config->priority_base,
                                                        external, selected[attempts],
                                                        &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return 0;
        }
    }
    /* A duplicate target is a generic deterministic conflict, not domain behavior. */
    if (selected_count != 0U &&
        minisnn_worlds_kernel_queue_destroy_entity(kernel, target_tick,
                                                    config->priority_base,
                                                    external, selected[0], &command_id) !=
        MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    return 1;
}

void k0_scenario_run_result_destroy(K0ScenarioRunResult *result)
{
    if (result == NULL)
    {
        return;
    }
    free(result->trace_rows);
    free(result->event_rows);
    memset(result, 0, sizeof(*result));
}

int k0_scenario_execute(
    const K0ScenarioConfig *config,
    K0ScenarioRunResult *out_result,
    char *error_message,
    size_t error_message_size)
{
    MiniSNNWorldsKernelConfig kernel_config;
    MiniSNNWorldsKernelError kernel_error;
    MiniSNNWorldsKernel *kernel = NULL;
    MiniSNNWorldsKernelDiagnostics diagnostics;
    MiniSNNWorldsKernelRandomStreamKey technical_key;
    MiniSNNWorldsKernelRandomStreamKey selection_key;
    K0ScenarioRunResult candidate;
    size_t trace_capacity = 0U;
    size_t event_capacity = 0U;
    uint64_t step_index;

    if (out_result == NULL || !k0_scenario_config_validate(config, error_message, error_message_size))
    {
        return 0;
    }
    memset(&candidate, 0, sizeof(candidate));
    candidate.config = *config;
    candidate.scenario_config_signature = k0_scenario_config_signature(config);
    kernel_config = minisnn_worlds_kernel_config_default();
    kernel_config.master_seed = config->master_seed;
    kernel = minisnn_worlds_kernel_create(&kernel_config, &kernel_error);
    if (kernel == NULL || kernel_error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        set_error(error_message, error_message_size, "kernel create failed: %d", (int)kernel_error);
        return 0;
    }
    technical_key.namespace_id = config->random_namespace;
    technical_key.stream_id = config->random_stream;
    selection_key.namespace_id = config->random_namespace;
    selection_key.stream_id = config->random_stream + UINT64_C(1);
    if (!queue_initial_entities(kernel, config) ||
        minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !append_trace(&candidate, &trace_capacity, &diagnostics))
    {
        set_error(error_message, error_message_size, "cannot prepare initial scenario state");
        minisnn_worlds_kernel_destroy(kernel);
        k0_scenario_run_result_destroy(&candidate);
        return 0;
    }
    candidate.initial_state_hash = diagnostics.current_state_hash;
    for (step_index = 0U; step_index < config->ticks; ++step_index)
    {
        MiniSNNWorldsTick current_tick = step_index;
        MiniSNNWorldsKernelDiagnostics scheduling_diagnostics;
        uint64_t draw_index;
        uint32_t ignored_value;

        memset(&scheduling_diagnostics, 0, sizeof(scheduling_diagnostics));
        scheduling_diagnostics.alive_entities =
            (uint64_t)minisnn_worlds_kernel_entity_count(kernel);

        for (draw_index = 0U; draw_index < config->random_draws_per_tick; ++draw_index)
        {
            if (minisnn_worlds_kernel_random_u32(kernel, technical_key, &ignored_value) !=
                MINISNN_WORLDS_KERNEL_ERROR_NONE)
            {
                set_error(error_message, error_message_size, "technical random draw failed");
                minisnn_worlds_kernel_destroy(kernel);
                k0_scenario_run_result_destroy(&candidate);
                return 0;
            }
        }
        if (current_tick <= config->ticks - config->command_delay)
        {
            MiniSNNWorldsTick target_tick = current_tick + config->command_delay;
            MiniSNNWorldsTick next_tick = current_tick + UINT64_C(1);

            if (next_tick % config->create_interval == 0U &&
                !queue_periodic_creates(kernel, config, &scheduling_diagnostics, target_tick))
            {
                set_error(error_message, error_message_size, "periodic create queue failed");
                minisnn_worlds_kernel_destroy(kernel);
                k0_scenario_run_result_destroy(&candidate);
                return 0;
            }
            if (next_tick % config->destroy_interval == 0U &&
                !queue_periodic_destroys(kernel, config, &scheduling_diagnostics, target_tick,
                                         selection_key))
            {
                set_error(error_message, error_message_size, "periodic destroy queue failed");
                minisnn_worlds_kernel_destroy(kernel);
                k0_scenario_run_result_destroy(&candidate);
                return 0;
            }
        }
        if (minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
            !append_events(&candidate, &event_capacity, kernel))
        {
            set_error(error_message, error_message_size, "kernel step or event capture failed");
            minisnn_worlds_kernel_destroy(kernel);
            k0_scenario_run_result_destroy(&candidate);
            return 0;
        }
        if ((step_index + UINT64_C(1)) % config->trace_interval == 0U ||
            step_index + UINT64_C(1) == config->ticks)
        {
            if (minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) !=
                    MINISNN_WORLDS_KERNEL_ERROR_NONE ||
                !append_trace(&candidate, &trace_capacity, &diagnostics))
            {
                set_error(error_message, error_message_size, "trace allocation failed");
                minisnn_worlds_kernel_destroy(kernel);
                k0_scenario_run_result_destroy(&candidate);
                return 0;
            }
        }
    }
    if (minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) !=
        MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        set_error(error_message, error_message_size, "final diagnostics failed");
        minisnn_worlds_kernel_destroy(kernel);
        k0_scenario_run_result_destroy(&candidate);
        return 0;
    }
    candidate.final_state_hash = diagnostics.current_state_hash;
    candidate.ticks_completed = diagnostics.completed_ticks;
    candidate.final_alive_entities = diagnostics.alive_entities;
    candidate.final_pending_commands = diagnostics.pending_commands;
    candidate.total_entities_created = diagnostics.total_entities_created;
    candidate.total_entities_destroyed = diagnostics.total_entities_destroyed;
    candidate.total_commands_submitted = diagnostics.total_commands_submitted;
    candidate.total_commands_applied = diagnostics.total_commands_applied;
    candidate.total_commands_rejected = diagnostics.total_commands_rejected;
    candidate.total_events_emitted = diagnostics.total_events_emitted;
    candidate.random_streams = diagnostics.random_streams;
    candidate.total_random_u32_generated = diagnostics.total_random_u32_generated;
    candidate.state_hash_version = diagnostics.state_hash_version;
    candidate.prng_version = diagnostics.prng_version;
    minisnn_worlds_kernel_destroy(kernel);
    *out_result = candidate;
    set_error(error_message, error_message_size, "");
    return 1;
}
