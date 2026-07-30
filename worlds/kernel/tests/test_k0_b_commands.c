#include "minisnn_worlds_kernel.h"

#include <stdint.h>
#include <stdio.h>

#define CHECK(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            fprintf(stderr, "K0-B command test failed: %s at line %d\n", #condition, __LINE__); \
            return 1; \
        } \
    } while (0)

int main(void)
{
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel = minisnn_worlds_kernel_create(NULL, &error);
    MiniSNNWorldsKernelEntityId external = { 0U };
    MiniSNNWorldsKernelEntityId issuer_five = { UINT64_C(5) };
    MiniSNNWorldsKernelEntityId invalid_target = { 0U };
    MiniSNNWorldsKernelCommandId command_id = { UINT64_C(77) };
    MiniSNNWorldsKernelCommandInfo command;

    CHECK(kernel != NULL);
    CHECK(minisnn_worlds_kernel_queue_create_entity(kernel, 0U, 0U,
                                                     external, &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_INVALID_TICK);
    CHECK(command_id.value == UINT64_C(77));
    CHECK(minisnn_worlds_kernel_queue_destroy_entity(kernel, 1U, 0U,
                                                      external, invalid_target,
                                                      &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_INVALID_ENTITY_ID);
    CHECK(command_id.value == UINT64_C(77));
    CHECK(minisnn_worlds_kernel_pending_command_count(kernel) == 0U);
    CHECK(minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 0U,
                                                     external, NULL) ==
          MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT);
    CHECK(minisnn_worlds_kernel_last_error(kernel) ==
          MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT);
    CHECK(minisnn_worlds_kernel_pending_command_count(kernel) == 0U);

    minisnn_worlds_kernel_testing_fail_next_allocation();
    CHECK(minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 0U,
                                                     external, &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
    CHECK(command_id.value == UINT64_C(77));
    CHECK(minisnn_worlds_kernel_pending_command_count(kernel) == 0U);
    CHECK(minisnn_worlds_kernel_queue_create_entity(kernel, 3U, 10U,
                                                     external, &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(command_id.value == UINT64_C(1));
    CHECK(minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 5U,
                                                     external, &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(command_id.value == UINT64_C(2));
    CHECK(minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 2U,
                                                     issuer_five, &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(command_id.value == UINT64_C(3));
    CHECK(minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 2U,
                                                     external, &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(command_id.value == UINT64_C(4));
    CHECK(minisnn_worlds_kernel_pending_command_at(kernel, 0U, &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(command.command_id.value == UINT64_C(4));
    CHECK(minisnn_worlds_kernel_pending_command_at(kernel, 1U, &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(command.command_id.value == UINT64_C(3));
    CHECK(minisnn_worlds_kernel_pending_command_at(kernel, 2U, &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(command.command_id.value == UINT64_C(2));
    CHECK(minisnn_worlds_kernel_pending_command_at(kernel, 3U, &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(command.command_id.value == UINT64_C(1));
    command.command_id.value = UINT64_C(88);
    CHECK(minisnn_worlds_kernel_pending_command_at(kernel, 4U, &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_INDEX_OUT_OF_RANGE);
    CHECK(command.command_id.value == UINT64_C(88));

    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_pending_command_count(kernel) == 1U);
    CHECK(minisnn_worlds_kernel_last_tick_event_count(kernel) == 3U);
    command_id.value = UINT64_C(66);
    CHECK(minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 0U,
                                                     external, &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_INVALID_TICK);
    CHECK(command_id.value == UINT64_C(66));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_last_tick_event_count(kernel) == 0U);
    CHECK(minisnn_worlds_kernel_pending_command_count(kernel) == 1U);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_pending_command_count(kernel) == 0U);

    CHECK(minisnn_worlds_kernel_testing_set_next_command_id(
              kernel, (MiniSNNWorldsKernelCommandId){ UINT64_MAX }) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    command_id.value = UINT64_C(55);
    CHECK(minisnn_worlds_kernel_queue_create_entity(kernel, 4U, 0U,
                                                     external, &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_IDENTIFIER_OVERFLOW);
    CHECK(command_id.value == UINT64_C(55));
    CHECK(minisnn_worlds_kernel_pending_command_count(kernel) == 0U);

    minisnn_worlds_kernel_destroy(kernel);
    printf("K0-B command validation OK\n");
    return 0;
}
