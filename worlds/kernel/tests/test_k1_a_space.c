#include "minisnn_worlds_kernel.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "K1-A space test failed: %s at line %d\n", #condition, __LINE__); \
    return 1; } } while (0)

static MiniSNNWorldsKernelEntityId entity_id(uint64_t value)
{
    MiniSNNWorldsKernelEntityId id = { value };
    return id;
}

static MiniSNNWorldsKernelTransform transform(
    MiniSNNWorldsKernelScalar x,
    MiniSNNWorldsKernelScalar y,
    MiniSNNWorldsKernelOrientation orientation)
{
    MiniSNNWorldsKernelTransform value;

    value.position.x = x;
    value.position.y = y;
    value.orientation = orientation;
    return value;
}

static int create_entities(MiniSNNWorldsKernel *kernel, size_t count)
{
    MiniSNNWorldsKernelCommandId command;
    size_t index;

    for (index = 0U; index < count; ++index)
    {
        if (minisnn_worlds_kernel_queue_create_entity(
                kernel, 1U, 0U, entity_id(0U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return 0;
        }
    }
    return minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static void copy_config_prefix(
    void *buffer,
    size_t buffer_size,
    const MiniSNNWorldsKernelConfig *config,
    uint32_t declared_size)
{
    unsigned char *bytes = (unsigned char *)buffer;
    size_t copy_size = buffer_size < sizeof(*config) ? buffer_size : sizeof(*config);

    memset(bytes, 0, buffer_size);
    memcpy(bytes, config, copy_size);
    memcpy(bytes + offsetof(MiniSNNWorldsKernelConfig, struct_size),
           &declared_size, sizeof(declared_size));
}

static int expect_bounds(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelScalar min_x,
    MiniSNNWorldsKernelScalar min_y,
    MiniSNNWorldsKernelScalar max_x,
    MiniSNNWorldsKernelScalar max_y)
{
    MiniSNNWorldsKernelSpaceBounds bounds;

    CHECK(minisnn_worlds_kernel_space_bounds(kernel, &bounds) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(bounds.min_x == min_x);
    CHECK(bounds.min_y == min_y);
    CHECK(bounds.max_x == max_x);
    CHECK(bounds.max_y == max_y);
    return 0;
}

static int test_scalar_overflow(void)
{
    MiniSNNWorldsKernelScalar result = INT64_C(77);

    CHECK(minisnn_worlds_kernel_testing_scalar_add(0, 0, &result));
    CHECK(result == 0);
    CHECK(minisnn_worlds_kernel_testing_scalar_add(INT64_MAX, 0, &result));
    CHECK(result == INT64_MAX);
    CHECK(minisnn_worlds_kernel_testing_scalar_add(INT64_MIN, 0, &result));
    CHECK(result == INT64_MIN);
    result = INT64_C(77);
    CHECK(!minisnn_worlds_kernel_testing_scalar_add(INT64_MAX, 1, &result));
    CHECK(result == INT64_C(77));
    result = INT64_C(88);
    CHECK(!minisnn_worlds_kernel_testing_scalar_add(INT64_MIN, -1, &result));
    CHECK(result == INT64_C(88));
    result = INT64_C(99);
    CHECK(!minisnn_worlds_kernel_testing_scalar_subtract(0, INT64_MIN, &result));
    CHECK(result == INT64_C(99));
    CHECK(minisnn_worlds_kernel_testing_scalar_subtract(
              INT64_MIN, INT64_MIN, &result));
    CHECK(result == 0);
    CHECK(minisnn_worlds_kernel_testing_scalar_subtract(-1, INT64_MIN, &result));
    CHECK(result == INT64_MAX);
    return 0;
}

static int test_config_prefixes(void)
{
    const size_t legacy_size = offsetof(MiniSNNWorldsKernelConfig, master_seed) +
                               sizeof(((MiniSNNWorldsKernelConfig *)0)->master_seed);
    const size_t partial_size = offsetof(MiniSNNWorldsKernelConfig, space_bounds) +
                                sizeof(((MiniSNNWorldsKernelConfig *)0)->space_bounds) - 1U;
    const size_t exact_size = sizeof(MiniSNNWorldsKernelConfig);
    const size_t larger_size = sizeof(MiniSNNWorldsKernelConfig) + sizeof(uint64_t);
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernel *first;
    MiniSNNWorldsKernel *second;
    MiniSNNWorldsKernelDiagnostics first_diagnostics;
    MiniSNNWorldsKernelDiagnostics second_diagnostics;
    unsigned char *buffer;
    unsigned char *first_buffer;
    unsigned char *second_buffer;
    uint64_t unknown_tail;
    uint64_t first_hash;
    uint64_t second_hash;

    buffer = (unsigned char *)malloc(legacy_size);
    CHECK(buffer != NULL);
    config.master_seed = UINT64_C(1234567);
    copy_config_prefix(buffer, legacy_size, &config, (uint32_t)legacy_size);
    kernel = minisnn_worlds_kernel_create(
        (const MiniSNNWorldsKernelConfig *)buffer, &error);
    CHECK(kernel != NULL && error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_master_seed(kernel) == UINT64_C(1234567));
    CHECK(expect_bounds(kernel, -1000000, -1000000, 1000000, 1000000) == 0);
    minisnn_worlds_kernel_destroy(kernel);
    free(buffer);

    buffer = (unsigned char *)malloc(partial_size);
    CHECK(buffer != NULL);
    config.space_bounds.min_x = -10;
    config.space_bounds.min_y = -20;
    config.space_bounds.max_x = 30;
    config.space_bounds.max_y = 40;
    copy_config_prefix(buffer, partial_size, &config, (uint32_t)partial_size);
    kernel = minisnn_worlds_kernel_create(
        (const MiniSNNWorldsKernelConfig *)buffer, &error);
    CHECK(kernel != NULL && error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(expect_bounds(kernel, -1000000, -1000000, 1000000, 1000000) == 0);
    minisnn_worlds_kernel_destroy(kernel);
    free(buffer);

    buffer = (unsigned char *)malloc(exact_size);
    CHECK(buffer != NULL);
    copy_config_prefix(buffer, exact_size, &config, (uint32_t)exact_size);
    kernel = minisnn_worlds_kernel_create(
        (const MiniSNNWorldsKernelConfig *)buffer, &error);
    CHECK(kernel != NULL && error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(expect_bounds(kernel, -10, -20, 30, 40) == 0);
    minisnn_worlds_kernel_destroy(kernel);
    free(buffer);

    first_buffer = (unsigned char *)malloc(larger_size);
    second_buffer = (unsigned char *)malloc(larger_size);
    CHECK(first_buffer != NULL && second_buffer != NULL);
    copy_config_prefix(first_buffer, larger_size, &config, (uint32_t)larger_size);
    copy_config_prefix(second_buffer, larger_size, &config, (uint32_t)larger_size);
    unknown_tail = UINT64_C(0x1111111111111111);
    memcpy(first_buffer + exact_size, &unknown_tail, sizeof(unknown_tail));
    unknown_tail = UINT64_C(0xEEEEEEEEEEEEEEEE);
    memcpy(second_buffer + exact_size, &unknown_tail, sizeof(unknown_tail));
    first = minisnn_worlds_kernel_create(
        (const MiniSNNWorldsKernelConfig *)first_buffer, &error);
    CHECK(first != NULL && error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    second = minisnn_worlds_kernel_create(
        (const MiniSNNWorldsKernelConfig *)second_buffer, &error);
    CHECK(second != NULL && error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(expect_bounds(first, -10, -20, 30, 40) == 0);
    CHECK(expect_bounds(second, -10, -20, 30, 40) == 0);
    CHECK(minisnn_worlds_kernel_master_seed(first) == config.master_seed);
    CHECK(minisnn_worlds_kernel_master_seed(second) == config.master_seed);
    CHECK(minisnn_worlds_kernel_get_diagnostics(first, &first_diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_get_diagnostics(second, &second_diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(first_diagnostics.current_state_hash == second_diagnostics.current_state_hash);
    CHECK(first_diagnostics.space_min_x == second_diagnostics.space_min_x);
    CHECK(minisnn_worlds_kernel_state_hash(first, &first_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_state_hash(second, &second_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(first_hash == second_hash);
    CHECK(minisnn_worlds_kernel_step(first) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(second) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_state_hash(first, &first_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_state_hash(second, &second_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(first_hash == second_hash);
    minisnn_worlds_kernel_destroy(first);
    minisnn_worlds_kernel_destroy(second);
    free(first_buffer);
    free(second_buffer);

    buffer = (unsigned char *)malloc(larger_size);
    CHECK(buffer != NULL);
    config.space_bounds.min_x = 10;
    config.space_bounds.max_x = 0;
    copy_config_prefix(buffer, larger_size, &config, (uint32_t)larger_size);
    kernel = minisnn_worlds_kernel_create(
        (const MiniSNNWorldsKernelConfig *)buffer, &error);
    CHECK(kernel == NULL);
    CHECK(error == MINISNN_WORLDS_KERNEL_ERROR_INVALID_CONFIG);
    free(buffer);
    return 0;
}
int main(void)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelConfig default_config;
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernel *legacy_kernel;
    MiniSNNWorldsKernelEntityId first = entity_id(1U);
    MiniSNNWorldsKernelEntityId second = entity_id(2U);
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelCommandId before_error;
    MiniSNNWorldsKernelCommandInfo pending;
    MiniSNNWorldsKernelEvent event;
    MiniSNNWorldsKernelTransform actual;
    MiniSNNWorldsKernelTransform edge = transform(10000, -10000, 359999U);
    MiniSNNWorldsKernelDiagnostics diagnostics;
    uint64_t before_hash;
    uint64_t after_hash;
    uint64_t v1_hash;
    uint64_t v2_hash;

    CHECK(MINISNN_WORLDS_KERNEL_SCALAR_SCALE == INT64_C(1000));
    CHECK(MINISNN_WORLDS_KERNEL_SCALAR_ZERO == INT64_C(0));
    CHECK(MINISNN_WORLDS_KERNEL_ORIENTATION_FULL_TURN == UINT32_C(360000));
    CHECK(config.space_bounds.min_x == INT64_C(-1000000));
    CHECK(config.space_bounds.max_y == INT64_C(1000000));

    CHECK(test_scalar_overflow() == 0);
    CHECK(test_config_prefixes() == 0);

    config.space_bounds.min_x = -10000;
    config.space_bounds.min_y = -10000;
    config.space_bounds.max_x = 10000;
    config.space_bounds.max_y = 10000;
    kernel = minisnn_worlds_kernel_create(&config, &error);
    CHECK(kernel != NULL);
    CHECK(create_entities(kernel, 3U));
    CHECK(minisnn_worlds_kernel_entity_count(kernel) == 3U);
    CHECK(!minisnn_worlds_kernel_entity_is_placed(kernel, first));
    CHECK(minisnn_worlds_kernel_entity_transform(kernel, first, &actual) ==
          MINISNN_WORLDS_KERNEL_ERROR_ENTITY_NOT_PLACED);

    CHECK(minisnn_worlds_kernel_queue_place_entity(
              kernel, 2U, 0U, entity_id(0U), first, edge, &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(command.value == 4U);
    CHECK(minisnn_worlds_kernel_queue_place_entity(
              kernel, 2U, 0U, entity_id(0U), second, edge, &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_queue_place_entity(
              kernel, 2U, 0U, entity_id(0U), first, edge, &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_pending_command_at(kernel, 0U, &pending) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(pending.has_transform);
    CHECK(pending.transform.orientation == 359999U);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_placed_entity_count(kernel) == 2U);
    CHECK(minisnn_worlds_kernel_entity_is_placed(kernel, first));
    CHECK(minisnn_worlds_kernel_entity_transform(kernel, first, &actual) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(actual.position.x == edge.position.x && actual.position.y == edge.position.y);
    CHECK(minisnn_worlds_kernel_last_tick_event_count(kernel) == 3U);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 2U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED);
    CHECK(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_ALREADY_PLACED);
    CHECK(!event.has_transform);

    CHECK(minisnn_worlds_kernel_queue_remove_entity_from_space(
              kernel, 3U, 0U, entity_id(0U), first, &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_queue_remove_entity_from_space(
              kernel, 3U, 0U, entity_id(0U), first, &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_entity_count(kernel) == 3U);
    CHECK(minisnn_worlds_kernel_placed_entity_count(kernel) == 1U);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_REMOVED_FROM_SPACE);
    CHECK(event.has_transform && event.transform.orientation == 359999U);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 1U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_NOT_PLACED);

    CHECK(minisnn_worlds_kernel_queue_destroy_entity(
              kernel, 4U, 0U, entity_id(0U), second, &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_placed_entity_count(kernel) == 0U);
    CHECK(minisnn_worlds_kernel_last_tick_event_count(kernel) == 1U);
    CHECK(minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &event) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(event.type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_DESTROYED);
    CHECK(event.has_transform);

    before_error.value = command.value;
    CHECK(minisnn_worlds_kernel_queue_place_entity(
              kernel, 5U, 0U, entity_id(0U), first,
              transform(10001, 0, 0U), &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_INVALID_BOUND);
    CHECK(command.value == before_error.value);
    CHECK(minisnn_worlds_kernel_queue_place_entity(
              kernel, 5U, 0U, entity_id(0U), first,
              transform(0, 0, 360000U), &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_INVALID_TRANSFORM);
    CHECK(command.value == before_error.value);

    CHECK(minisnn_worlds_kernel_state_hash_versioned(
              kernel, MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V1, &v1_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE);
    CHECK(minisnn_worlds_kernel_state_hash_versioned(
              kernel, MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V2, &v2_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(v2_hash != 0U);
    CHECK(minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(diagnostics.state_hash_version == MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V2);
    CHECK(diagnostics.total_entities_placed == 2U);
    CHECK(diagnostics.total_entities_removed_from_space == 1U);

    CHECK(minisnn_worlds_kernel_queue_place_entity(
              kernel, 5U, 0U, entity_id(0U), first, transform(0, 0, 0U), &command) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_state_hash(kernel, &before_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    minisnn_worlds_kernel_testing_fail_allocation_after(1U);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
    CHECK(minisnn_worlds_kernel_state_hash(kernel, &after_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(before_hash == after_hash);
    CHECK(minisnn_worlds_kernel_pending_command_count(kernel) == 1U);
    CHECK(!minisnn_worlds_kernel_entity_is_placed(kernel, first));
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_entity_is_placed(kernel, first));
    CHECK(minisnn_worlds_kernel_placed_entity_at(kernel, 0U, &first, &actual) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(first.value == 1U && actual.position.x == 0);

    default_config = minisnn_worlds_kernel_config_default();
    legacy_kernel = minisnn_worlds_kernel_create(&default_config, &error);
    CHECK(legacy_kernel != NULL);
    CHECK(minisnn_worlds_kernel_state_hash_versioned(
              legacy_kernel, MINISNN_WORLDS_KERNEL_STATE_HASH_VERSION_V1, &v1_hash) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    minisnn_worlds_kernel_destroy(legacy_kernel);
    minisnn_worlds_kernel_destroy(kernel);
    puts("K1-A scalar, space, transform and hash validation OK");
    return 0;
}
