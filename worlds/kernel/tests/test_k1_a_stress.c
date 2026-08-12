#include "minisnn_worlds_kernel.h"

#include <stdio.h>

#define ENTITY_COUNT 20000U
#define PLACE_COUNT 10000U

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "K1-A stress failed: %s at line %d\n", #condition, __LINE__); \
    return 1; } } while (0)

static MiniSNNWorldsKernelEntityId id(uint64_t value)
{
    MiniSNNWorldsKernelEntityId result = { value };
    return result;
}

int main(void)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelTransform transform;
    MiniSNNWorldsKernelEntityId entity;
    size_t index;
    uint64_t hash;

    config.space_bounds.min_x = -10000;
    config.space_bounds.min_y = -10000;
    config.space_bounds.max_x = 10000;
    config.space_bounds.max_y = 10000;
    kernel = minisnn_worlds_kernel_create(&config, &error);
    CHECK(kernel != NULL);
    for (index = 0U; index < ENTITY_COUNT; ++index)
    {
        CHECK(minisnn_worlds_kernel_queue_create_entity(
                  kernel, 1U, 0U, id(0U), &command) ==
              MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    transform.position.x = -10000;
    transform.position.y = 10000;
    transform.orientation = 0U;
    for (index = 0U; index < PLACE_COUNT; ++index)
    {
        transform.position.x = (MiniSNNWorldsKernelScalar)(-10000 + (index % 20001U));
        transform.position.y = (MiniSNNWorldsKernelScalar)(10000 - (index % 20001U));
        transform.orientation = (uint32_t)(index % 360000U);
        CHECK(minisnn_worlds_kernel_queue_place_entity(
                  kernel, 2U, 0U, id(0U), id((uint64_t)index + 1U), transform,
                  &command) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_placed_entity_count(kernel) == PLACE_COUNT);
    for (index = 0U; index < PLACE_COUNT; index += 2U)
    {
        CHECK(minisnn_worlds_kernel_queue_remove_entity_from_space(
                  kernel, 3U, 0U, id(0U), id((uint64_t)index + 1U), &command) ==
              MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_placed_entity_count(kernel) == PLACE_COUNT / 2U);
    for (index = 1U; index < PLACE_COUNT; index += 2U)
    {
        CHECK(minisnn_worlds_kernel_queue_destroy_entity(
                  kernel, 4U, 0U, id(0U), id((uint64_t)index + 1U), &command) ==
              MINISNN_WORLDS_KERNEL_ERROR_NONE);
    }
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_placed_entity_count(kernel) == 0U);
    CHECK(minisnn_worlds_kernel_entity_count(kernel) == ENTITY_COUNT - PLACE_COUNT / 2U);
    CHECK(minisnn_worlds_kernel_entity_at(kernel, 0U, &entity) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(entity.value == 1U);
    CHECK(minisnn_worlds_kernel_entity_at(
              kernel, minisnn_worlds_kernel_entity_count(kernel) - 1U, &entity) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(entity.value == ENTITY_COUNT);
    CHECK(minisnn_worlds_kernel_state_hash(kernel, &hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(hash != 0U);
    minisnn_worlds_kernel_destroy(kernel);
    puts("K1-A moderate spatial stress validation OK");
    return 0;
}
