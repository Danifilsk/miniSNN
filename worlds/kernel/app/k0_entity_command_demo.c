#include "minisnn_worlds_kernel.h"

#include <stdio.h>

static const char *event_name(MiniSNNWorldsKernelEventType type)
{
    switch (type)
    {
        case MINISNN_WORLDS_KERNEL_EVENT_ENTITY_CREATED:
            return "ENTITY_CREATED";
        case MINISNN_WORLDS_KERNEL_EVENT_ENTITY_DESTROYED:
            return "ENTITY_DESTROYED";
        case MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED:
            return "COMMAND_REJECTED";
        default:
            return "INVALID";
    }
}

static const char *rejection_name(MiniSNNWorldsKernelCommandRejection rejection)
{
    switch (rejection)
    {
        case MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_NOT_ALIVE:
            return "TARGET_NOT_ALIVE";
        case MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_ISSUER_NOT_ALIVE:
            return "ISSUER_NOT_ALIVE";
        case MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_INVALID_COMMAND:
            return "INVALID_COMMAND";
        default:
            return "NONE";
    }
}

static int print_event(
    const MiniSNNWorldsKernel *kernel,
    size_t index)
{
    MiniSNNWorldsKernelEvent event;

    if (minisnn_worlds_kernel_last_tick_event_at(kernel, index, &event) !=
        MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    printf("tick=%llu event=%llu type=%s subject=%llu",
           (unsigned long long)event.tick,
           (unsigned long long)event.event_id.value,
           event_name(event.type),
           (unsigned long long)event.subject.value);
    if (event.type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED)
    {
        printf(" reason=%s", rejection_name(event.rejection));
    }
    printf("\n");
    return 1;
}

int main(void)
{
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelEntityId external_issuer = { 0U };
    MiniSNNWorldsKernelEntityId second_entity = { 2U };
    MiniSNNWorldsKernelDiagnostics diagnostics;
    size_t index;

    kernel = minisnn_worlds_kernel_create(NULL, &error);
    if (kernel == NULL || error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 1;
    }
    if (minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 10U,
                                                   external_issuer, &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 1U,
                                                   external_issuer, &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 1U,
                                                   external_issuer, &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 1;
    }

    printf("miniSNN Worlds Kernel K0-B entity/command demo\n");
    for (index = 0U; index < minisnn_worlds_kernel_last_tick_event_count(kernel); ++index)
    {
        if (!print_event(kernel, index))
        {
            minisnn_worlds_kernel_destroy(kernel);
            return 1;
        }
    }
    if (minisnn_worlds_kernel_queue_destroy_entity(kernel, 2U, 0U,
                                                    external_issuer, second_entity,
                                                    &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_destroy_entity(kernel, 2U, 0U,
                                                    external_issuer, second_entity,
                                                    &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_create_entity(kernel, 4U, 0U,
                                                   external_issuer, &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 1;
    }
    for (index = 0U; index < minisnn_worlds_kernel_last_tick_event_count(kernel); ++index)
    {
        if (!print_event(kernel, index))
        {
            minisnn_worlds_kernel_destroy(kernel);
            return 1;
        }
    }
    if (minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_last_tick_event_count(kernel) != 0U)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 1;
    }
    printf("tick=3 events=0\n");
    if (minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !print_event(kernel, 0U) ||
        minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 1;
    }
    printf("alive_entities=%llu\n", (unsigned long long)diagnostics.alive_entities);
    printf("pending_commands=%llu\n", (unsigned long long)diagnostics.pending_commands);
    printf("status=OK\n");
    minisnn_worlds_kernel_destroy(kernel);
    return 0;
}
