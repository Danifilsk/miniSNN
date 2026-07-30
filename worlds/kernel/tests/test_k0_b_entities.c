#include "minisnn_worlds_kernel.h"

#include <stdint.h>
#include <stdio.h>

#define CHECK(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            fprintf(stderr, "K0-B entity test failed: %s at line %d\n", #condition, __LINE__); \
            return 1; \
        } \
    } while (0)

static MiniSNNWorldsKernel *create_kernel(void)
{
    MiniSNNWorldsKernelError error;

    return minisnn_worlds_kernel_create(NULL, &error);
}

int main(void)
{
    MiniSNNWorldsKernel *kernel = create_kernel();
    MiniSNNWorldsKernel *other_kernel = create_kernel();
    MiniSNNWorldsKernelEntityId external = { 0U };
    MiniSNNWorldsKernelEntityId entity_id = { UINT64_C(99) };
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelEvent event;
    size_t index;

    CHECK(kernel != NULL);
    CHECK(other_kernel != NULL);
    CHECK(!minisnn_worlds_kernel_entity_exists(kernel, external));
    CHECK(minisnn_worlds_kernel_entity_count(kernel) == 0U);
    CHECK(minisnn_worlds_kernel_entity_at(kernel, 0U, &entity_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_INDEX_OUT_OF_RANGE);
    CHECK(entity_id.value == UINT64_C(99));

    CHECK(minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 10U,
                                                     external, &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(command_id.value == UINT64_C(1));
    CHECK(minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 1U,
                                                     external, &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(command_id.value == UINT64_C(2));
    CHECK(minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 1U,
                                                     external, &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(command_id.value == UINT64_C(3));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_entity_count(kernel) == 3U);
    for (index = 0U; index < 3U; ++index)
    {
        CHECK(minisnn_worlds_kernel_entity_at(kernel, index, &entity_id) ==
              MINISNN_WORLDS_KERNEL_ERROR_NONE);
        CHECK(entity_id.value == (uint64_t)(index + 1U));
        CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, index, &event) ==
              MINISNN_WORLDS_KERNEL_ERROR_NONE);
        CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_CREATED);
        CHECK(event.subject.value == (uint64_t)(index + 1U));
    }

    entity_id.value = UINT64_C(2);
    CHECK(minisnn_worlds_kernel_queue_destroy_entity(kernel, 2U, 0U,
                                                      external, entity_id,
                                                      &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(!minisnn_worlds_kernel_entity_exists(kernel, entity_id));
    CHECK(minisnn_worlds_kernel_entity_count(kernel) == 2U);
    CHECK(minisnn_worlds_kernel_entity_at(kernel, 0U, &entity_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(entity_id.value == UINT64_C(1));
    CHECK(minisnn_worlds_kernel_entity_at(kernel, 1U, &entity_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(entity_id.value == UINT64_C(3));

    CHECK(minisnn_worlds_kernel_queue_create_entity(kernel, 3U, 0U,
                                                     external, &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.subject.value == UINT64_C(4));
    CHECK(minisnn_worlds_kernel_entity_exists(kernel, event.subject));

    for (index = 0U; index < 6U; ++index)
    {
        CHECK(minisnn_worlds_kernel_queue_create_entity(kernel, 4U, 0U,
                                                         external, &command_id) ==
              MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_entity_count(kernel) == 9U);
    CHECK(minisnn_worlds_kernel_entity_at(kernel, 8U, &entity_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(entity_id.value == UINT64_C(10));

    CHECK(minisnn_worlds_kernel_queue_create_entity(other_kernel, 1U, 0U,
                                                     external, &command_id) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(other_kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(other_kernel, 0U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.subject.value == UINT64_C(1));

    minisnn_worlds_kernel_destroy(kernel);
    minisnn_worlds_kernel_destroy(other_kernel);
    printf("K0-B entity validation OK\n");
    return 0;
}
