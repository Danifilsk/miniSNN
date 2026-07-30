#include "minisnn_worlds_kernel.h"

#include <stdint.h>
#include <stdio.h>

#define CHECK(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            fprintf(stderr, "K0-B determinism test failed: %s at line %d\n", #condition, __LINE__); \
            return 1; \
        } \
    } while (0)

typedef struct
{
    MiniSNNWorldsKernelEvent first_tick_events[3];
    MiniSNNWorldsKernelEvent final_event;
    MiniSNNWorldsKernelEntityId entities[4];
    MiniSNNWorldsKernelDiagnostics diagnostics;
    size_t entity_count;
    size_t first_tick_event_count;
    size_t final_event_count;
} Snapshot;

static int event_matches(
    const MiniSNNWorldsKernelEvent *left,
    const MiniSNNWorldsKernelEvent *right)
{
    return left->event_id.value == right->event_id.value &&
           left->tick == right->tick &&
           left->type == right->type &&
           left->command_id.value == right->command_id.value &&
           left->issuer.value == right->issuer.value &&
           left->subject.value == right->subject.value &&
           left->rejection == right->rejection;
}

static int snapshot_matches(const Snapshot *left, const Snapshot *right)
{
    size_t index;

    if (left->entity_count != right->entity_count ||
        left->first_tick_event_count != right->first_tick_event_count ||
        left->final_event_count != right->final_event_count ||
        left->diagnostics.completed_ticks != right->diagnostics.completed_ticks ||
        left->diagnostics.alive_entities != right->diagnostics.alive_entities ||
        left->diagnostics.total_commands_submitted != right->diagnostics.total_commands_submitted ||
        left->diagnostics.total_commands_applied != right->diagnostics.total_commands_applied ||
        left->diagnostics.total_commands_rejected != right->diagnostics.total_commands_rejected ||
        left->diagnostics.total_events_emitted != right->diagnostics.total_events_emitted)
    {
        return 0;
    }
    for (index = 0U; index < left->entity_count; ++index)
    {
        if (left->entities[index].value != right->entities[index].value)
        {
            return 0;
        }
    }
    for (index = 0U; index < left->first_tick_event_count; ++index)
    {
        if (!event_matches(&left->first_tick_events[index], &right->first_tick_events[index]))
        {
            return 0;
        }
    }
    return left->final_event_count == 0U ||
           event_matches(&left->final_event, &right->final_event);
}

static int execute_sequence(int swapped_priorities, Snapshot *out_snapshot)
{
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel = minisnn_worlds_kernel_create(NULL, &error);
    MiniSNNWorldsKernelEntityId external = { 0U };
    MiniSNNWorldsKernelCommandId command_id;
    uint32_t first_priority = swapped_priorities != 0 ? 1U : 9U;
    uint32_t second_priority = swapped_priorities != 0 ? 9U : 1U;
    size_t index;

    if (kernel == NULL ||
        minisnn_worlds_kernel_queue_create_entity(kernel, 1U, first_priority,
                                                   external, &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_create_entity(kernel, 1U, second_priority,
                                                   external, &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 1U,
                                                   external, &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_create_entity(kernel, 3U, 0U,
                                                   external, &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    out_snapshot->first_tick_event_count =
        minisnn_worlds_kernel_last_tick_event_count(kernel);
    if (out_snapshot->first_tick_event_count != 3U)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    for (index = 0U; index < out_snapshot->first_tick_event_count; ++index)
    {
        if (minisnn_worlds_kernel_last_tick_event_at(kernel, index,
                                                      &out_snapshot->first_tick_events[index]) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            minisnn_worlds_kernel_destroy(kernel);
            return 0;
        }
    }
    if (minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    out_snapshot->final_event_count = minisnn_worlds_kernel_last_tick_event_count(kernel);
    if (out_snapshot->final_event_count != 1U ||
        minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &out_snapshot->final_event) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_get_diagnostics(kernel, &out_snapshot->diagnostics) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    out_snapshot->entity_count = minisnn_worlds_kernel_entity_count(kernel);
    for (index = 0U; index < out_snapshot->entity_count; ++index)
    {
        if (minisnn_worlds_kernel_entity_at(kernel, index, &out_snapshot->entities[index]) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            minisnn_worlds_kernel_destroy(kernel);
            return 0;
        }
    }
    minisnn_worlds_kernel_destroy(kernel);
    return 1;
}

int main(void)
{
    Snapshot first = { 0 };
    Snapshot second = { 0 };
    Snapshot third = { 0 };
    Snapshot changed = { 0 };
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *overflow_kernel = minisnn_worlds_kernel_create(NULL, &error);
    MiniSNNWorldsKernelEntityId external = { 0U };
    MiniSNNWorldsKernelCommandId command_id;

    CHECK(execute_sequence(0, &first));
    CHECK(execute_sequence(0, &second));
    CHECK(execute_sequence(0, &third));
    CHECK(snapshot_matches(&first, &second));
    CHECK(snapshot_matches(&first, &third));
    CHECK(execute_sequence(1, &changed));
    CHECK(first.first_tick_events[0].command_id.value !=
          changed.first_tick_events[0].command_id.value);

    CHECK(overflow_kernel != NULL);
    CHECK(minisnn_worlds_kernel_queue_create_entity(overflow_kernel, 1U, 0U,
                                                     external, &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_testing_set_next_entity_id(
              overflow_kernel, (MiniSNNWorldsKernelEntityId){ UINT64_MAX }) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(overflow_kernel) ==
          MINISNN_WORLDS_KERNEL_ERROR_IDENTIFIER_OVERFLOW);
    CHECK(minisnn_worlds_kernel_tick(overflow_kernel) == 0U);
    CHECK(minisnn_worlds_kernel_pending_command_count(overflow_kernel) == 1U);
    CHECK(minisnn_worlds_kernel_testing_set_next_entity_id(
              overflow_kernel, (MiniSNNWorldsKernelEntityId){ UINT64_C(1) }) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(overflow_kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    minisnn_worlds_kernel_destroy(overflow_kernel);
    printf("K0-B deterministic resolution validation OK\n");
    return 0;
}
