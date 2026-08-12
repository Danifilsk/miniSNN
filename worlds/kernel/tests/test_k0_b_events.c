#include "minisnn_worlds_kernel.h"

#include <stdint.h>
#include <stdio.h>

#define CHECK(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            fprintf(stderr, "K0-B event test failed: %s at line %d\n", #condition, __LINE__); \
            return 1; \
        } \
    } while (0)

static int queue_two_entities(MiniSNNWorldsKernel *kernel)
{
    MiniSNNWorldsKernelEntityId external = { 0U };
    MiniSNNWorldsKernelCommandId command_id;

    return minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 0U,
                                                      external, &command_id) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 0U,
                                                      external, &command_id) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

int main(void)
{
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel = minisnn_worlds_kernel_create(NULL, &error);
    MiniSNNWorldsKernel *overflow_kernel;
    MiniSNNWorldsKernel *window_kernel;
    MiniSNNWorldsKernel *issuer_kernel;
    MiniSNNWorldsKernel *rejected_create_kernel;
    MiniSNNWorldsKernelEntityId external = { 0U };
    MiniSNNWorldsKernelEntityId first = { UINT64_C(1) };
    MiniSNNWorldsKernelEntityId second = { UINT64_C(2) };
    MiniSNNWorldsKernelEntityId missing_issuer = { UINT64_C(9) };
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelEvent event;
    MiniSNNWorldsKernelDiagnostics before_failure;
    MiniSNNWorldsKernelDiagnostics after_failure;
    size_t allocation_index;

    CHECK(kernel != NULL);
    CHECK(queue_two_entities(kernel));
    CHECK(minisnn_worlds_kernel_last_tick_event_count(kernel) == 2U);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.event_id.value == UINT64_C(1));
    CHECK(event.tick == UINT64_C(1));
    CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_CREATED);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 1U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.event_id.value == UINT64_C(2));

    CHECK(minisnn_worlds_kernel_queue_destroy_entity(kernel, 2U, 0U,
                                                      external, first,
                                                      &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_queue_destroy_entity(kernel, 2U, 1U,
                                                      first, second,
                                                      &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_queue_destroy_entity(kernel, 2U, 2U,
                                                      external, second,
                                                      &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_queue_destroy_entity(kernel, 2U, 3U,
                                                      external, second,
                                                      &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_last_tick_event_count(kernel) == 4U);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_DESTROYED);
    CHECK(event.subject.value == first.value);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 1U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED);
    CHECK(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_ISSUER_NOT_ALIVE);
    CHECK(event.subject.value == second.value);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 2U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_DESTROYED);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 3U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED);
    CHECK(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_NOT_ALIVE);
    CHECK(event.event_id.value == UINT64_C(6));
    CHECK(minisnn_worlds_kernel_get_diagnostics(kernel, &after_failure) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(after_failure.total_commands_applied == UINT64_C(4));
    CHECK(after_failure.total_commands_rejected == UINT64_C(2));
    CHECK(after_failure.total_events_emitted == UINT64_C(6));
    CHECK(after_failure.alive_entities == 0U);
    event.event_id.value = UINT64_C(99);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 4U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_INDEX_OUT_OF_RANGE);
    CHECK(event.event_id.value == UINT64_C(99));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_last_tick_event_count(kernel) == 0U);

    overflow_kernel = minisnn_worlds_kernel_create(NULL, &error);
    CHECK(overflow_kernel != NULL);
    CHECK(minisnn_worlds_kernel_queue_create_entity(overflow_kernel, 1U, 0U,
                                                     external, &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_testing_set_next_event_id(
              overflow_kernel, (MiniSNNWorldsKernelEventId){ UINT64_MAX }) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(overflow_kernel) ==
          MINISNN_WORLDS_KERNEL_ERROR_IDENTIFIER_OVERFLOW);
    CHECK(minisnn_worlds_kernel_tick(overflow_kernel) == 0U);
    CHECK(minisnn_worlds_kernel_pending_command_count(overflow_kernel) == 1U);
    CHECK(minisnn_worlds_kernel_last_tick_event_count(overflow_kernel) == 0U);
    CHECK(minisnn_worlds_kernel_testing_set_next_event_id(
              overflow_kernel, (MiniSNNWorldsKernelEventId){ UINT64_C(1) }) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(overflow_kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);

    window_kernel = minisnn_worlds_kernel_create(NULL, &error);
    CHECK(window_kernel != NULL);
    CHECK(minisnn_worlds_kernel_queue_create_entity(window_kernel, 1U, 0U,
                                                     external, &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(window_kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);

    issuer_kernel = minisnn_worlds_kernel_create(NULL, &error);
    CHECK(issuer_kernel != NULL);
    CHECK(queue_two_entities(issuer_kernel));
    CHECK(minisnn_worlds_kernel_queue_destroy_entity(issuer_kernel, 2U, 0U,
                                                      first, second,
                                                      &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(issuer_kernel) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(issuer_kernel, 0U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_DESTROYED);
    CHECK(event.issuer.value == first.value);
    CHECK(event.subject.value == second.value);

    rejected_create_kernel = minisnn_worlds_kernel_create(NULL, &error);
    CHECK(rejected_create_kernel != NULL);
    CHECK(minisnn_worlds_kernel_queue_create_entity(rejected_create_kernel, 1U, 0U,
                                                     missing_issuer, &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_testing_set_next_entity_id(
              rejected_create_kernel,
              (MiniSNNWorldsKernelEntityId){ UINT64_MAX }) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(rejected_create_kernel) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_entity_count(rejected_create_kernel) == 0U);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(rejected_create_kernel, 0U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED);
    CHECK(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_ISSUER_NOT_ALIVE);
    CHECK(minisnn_worlds_kernel_last_tick_event_count(window_kernel) == 1U);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(window_kernel, 0U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.event_id.value == UINT64_C(1));
    CHECK(minisnn_worlds_kernel_queue_create_entity(window_kernel, 2U, 0U,
                                                     external, &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    minisnn_worlds_kernel_testing_fail_next_allocation();
    CHECK(minisnn_worlds_kernel_step(window_kernel) ==
          MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
    CHECK(minisnn_worlds_kernel_tick(window_kernel) == UINT64_C(1));
    CHECK(minisnn_worlds_kernel_last_tick_event_count(window_kernel) == 1U);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(window_kernel, 0U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.event_id.value == UINT64_C(1));
    CHECK(minisnn_worlds_kernel_step(window_kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);

    for (allocation_index = 0U; allocation_index < 3U; ++allocation_index)
    {
        MiniSNNWorldsKernel *allocation_kernel = minisnn_worlds_kernel_create(NULL, &error);

        CHECK(allocation_kernel != NULL);
        CHECK(minisnn_worlds_kernel_queue_create_entity(allocation_kernel, 1U, 0U,
                                                         external, &command_id) ==
              MINISNN_WORLDS_KERNEL_ERROR_NONE);
        CHECK(minisnn_worlds_kernel_get_diagnostics(allocation_kernel, &before_failure) ==
              MINISNN_WORLDS_KERNEL_ERROR_NONE);
        minisnn_worlds_kernel_testing_fail_allocation_after(allocation_index);
        CHECK(minisnn_worlds_kernel_step(allocation_kernel) ==
              MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
        CHECK(minisnn_worlds_kernel_get_diagnostics(allocation_kernel, &after_failure) ==
              MINISNN_WORLDS_KERNEL_ERROR_NONE);
        CHECK(after_failure.completed_ticks == before_failure.completed_ticks);
        CHECK(after_failure.pending_commands == before_failure.pending_commands);
        CHECK(after_failure.total_events_emitted == before_failure.total_events_emitted);
        CHECK(minisnn_worlds_kernel_last_tick_event_count(allocation_kernel) == 0U);
        CHECK(minisnn_worlds_kernel_step(allocation_kernel) ==
              MINISNN_WORLDS_KERNEL_ERROR_NONE);
        minisnn_worlds_kernel_destroy(allocation_kernel);
    }

    minisnn_worlds_kernel_destroy(kernel);
    minisnn_worlds_kernel_destroy(overflow_kernel);
    minisnn_worlds_kernel_destroy(window_kernel);
    minisnn_worlds_kernel_destroy(issuer_kernel);
    minisnn_worlds_kernel_destroy(rejected_create_kernel);
    printf("K0-B event and atomicity validation OK\n");
    return 0;
}
