#include "../src/minisnn_worlds_kernel_internal.h"

#define main k2_a_snapshot_fixture_main
#include "test_k2_a_snapshot.c"
#undef main

#define SNAPSHOT_HEADER_SIZE ((size_t)40U)
#define PAYLOAD_ENTITY_COUNT_OFFSET ((size_t)124U)
#define FIRST_ENTITY_ALIVE_OFFSET ((size_t)372U)

static void write_u32_le(uint8_t *data, uint32_t value)
{
    size_t index;

    for (index = 0U; index < 4U; ++index)
    {
        data[index] = (uint8_t)(value >> (index * 8U));
    }
}

static void assert_snapshot_equal(
    const MiniSNNWorldsKernelSnapshot *left,
    const MiniSNNWorldsKernelSnapshot *right)
{
    REQUIRE(left != NULL);
    REQUIRE(right != NULL);
    REQUIRE(minisnn_worlds_kernel_snapshot_size(left) ==
            minisnn_worlds_kernel_snapshot_size(right));
    REQUIRE(memcmp(minisnn_worlds_kernel_snapshot_data(left),
                   minisnn_worlds_kernel_snapshot_data(right),
                   minisnn_worlds_kernel_snapshot_size(left)) == 0);
}

static MiniSNNWorldsKernelSnapshot *capture(MiniSNNWorldsKernel *kernel)
{
    MiniSNNWorldsKernelSnapshot *snapshot = NULL;

    REQUIRE(minisnn_worlds_kernel_snapshot_capture(kernel, &snapshot) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(snapshot != NULL);
    return snapshot;
}

static void assert_same_state(
    MiniSNNWorldsKernel *left,
    MiniSNNWorldsKernel *right)
{
    MiniSNNWorldsKernelSnapshot *left_snapshot = capture(left);
    MiniSNNWorldsKernelSnapshot *right_snapshot = capture(right);
    uint64_t left_hash;
    uint64_t right_hash;

    REQUIRE(minisnn_worlds_kernel_state_hash(left, &left_hash) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_state_hash(right, &right_hash) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(left_hash == right_hash);
    assert_snapshot_equal(left_snapshot, right_snapshot);
    minisnn_worlds_kernel_snapshot_destroy(right_snapshot);
    minisnn_worlds_kernel_snapshot_destroy(left_snapshot);
}

static uint8_t *copy_snapshot_data(
    const MiniSNNWorldsKernelSnapshot *snapshot)
{
    size_t size = minisnn_worlds_kernel_snapshot_size(snapshot);
    uint8_t *copy = malloc(size);

    REQUIRE(copy != NULL);
    memcpy(copy, minisnn_worlds_kernel_snapshot_data(snapshot), size);
    return copy;
}

static void require_rejected(
    const uint8_t *data,
    size_t size,
    MiniSNNWorldsKernelError expected_error)
{
    MiniSNNWorldsKernelSnapshot *imported =
        (MiniSNNWorldsKernelSnapshot *)(uintptr_t)UINTPTR_MAX;

    REQUIRE(minisnn_worlds_kernel_snapshot_from_bytes(data, size, &imported) ==
            expected_error);
    REQUIRE(imported == NULL);
}

static void test_import_restore_and_continuation(void)
{
    MiniSNNWorldsKernel *original = build_full_state();
    MiniSNNWorldsKernel *restored = NULL;
    MiniSNNWorldsKernelSnapshot *captured = capture(original);
    MiniSNNWorldsKernelSnapshot *imported = NULL;
    uint8_t *external_bytes = copy_snapshot_data(captured);
    MiniSNNWorldsKernelRandomStreamKey key = { UINT64_C(31), UINT64_C(7) };
    size_t index;

    REQUIRE(minisnn_worlds_kernel_snapshot_from_bytes(
                external_bytes,
                minisnn_worlds_kernel_snapshot_size(captured),
                &imported) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(imported != NULL);
    external_bytes[0] ^= UINT8_C(0xFF);
    REQUIRE(minisnn_worlds_kernel_snapshot_data(imported)[0] == (uint8_t)'M');
    REQUIRE(minisnn_worlds_kernel_create_from_snapshot(imported, &restored) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(restored != NULL);
    assert_same_state(original, restored);

    for (index = 0U; index < 12U; ++index)
    {
        uint32_t original_value;
        uint32_t restored_value;

        REQUIRE(minisnn_worlds_kernel_random_u32(original, key, &original_value) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
        REQUIRE(minisnn_worlds_kernel_random_u32(restored, key, &restored_value) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
        REQUIRE(original_value == restored_value);
    }
    assert_same_state(original, restored);
    for (index = 0U; index < 10U; ++index)
    {
        step_ok(original);
        step_ok(restored);
        assert_same_state(original, restored);
    }

    free(external_bytes);
    minisnn_worlds_kernel_destroy(restored);
    minisnn_worlds_kernel_snapshot_destroy(imported);
    minisnn_worlds_kernel_snapshot_destroy(captured);
    minisnn_worlds_kernel_destroy(original);
}

static void test_invalid_bytes_and_failure_atomicity(void)
{
    MiniSNNWorldsKernel *kernel = build_full_state();
    MiniSNNWorldsKernelSnapshot *snapshot = capture(kernel);
    const size_t size = minisnn_worlds_kernel_snapshot_size(snapshot);
    uint8_t *bytes = copy_snapshot_data(snapshot);
    MiniSNNWorldsKernelSnapshot *imported = NULL;
    MiniSNNWorldsKernel *restored = NULL;
    size_t index;
    MiniSNNWorldsKernelError error;

    REQUIRE(minisnn_worlds_kernel_snapshot_from_bytes(NULL, size, &imported) ==
            MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT);
    REQUIRE(imported == NULL);
    REQUIRE(minisnn_worlds_kernel_snapshot_from_bytes(bytes, size, NULL) ==
            MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT);
    require_rejected(bytes, 0U, MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT);
    require_rejected(bytes, SNAPSHOT_HEADER_SIZE - 1U,
                     MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT);
    for (index = 1U; index < size; ++index)
    {
        require_rejected(bytes, index,
                         MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT);
    }

    bytes[0] ^= UINT8_C(1);
    require_rejected(bytes, size, MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT);
    bytes[0] ^= UINT8_C(1);
    write_u32_le(bytes + 8U, UINT32_C(2));
    require_rejected(bytes, size, MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_UNSUPPORTED_VERSION);
    write_u32_le(bytes + 8U, MINISNN_WORLDS_KERNEL_SNAPSHOT_FORMAT_VERSION_V1);
    bytes[FIRST_ENTITY_ALIVE_OFFSET] = UINT8_C(2);
    require_rejected(bytes, size, MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT);
    bytes[FIRST_ENTITY_ALIVE_OFFSET] = UINT8_C(1);
    memset(bytes + PAYLOAD_ENTITY_COUNT_OFFSET, 0xFF, sizeof(uint64_t));
    require_rejected(bytes, size, MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT);
    memcpy(bytes, minisnn_worlds_kernel_snapshot_data(snapshot), size);
    bytes[32U] ^= UINT8_C(1);
    require_rejected(bytes, size,
                     MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_STATE_HASH_MISMATCH);
    memcpy(bytes, minisnn_worlds_kernel_snapshot_data(snapshot), size);

    for (index = 0U; index < 64U; ++index)
    {
        imported = NULL;
        minisnn_worlds_kernel_testing_fail_allocation_after(index);
        error = minisnn_worlds_kernel_snapshot_from_bytes(bytes, size, &imported);
        if (error == MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            break;
        }
        REQUIRE(error == MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
        REQUIRE(imported == NULL);
    }
    REQUIRE(imported != NULL);
    minisnn_worlds_kernel_testing_fail_allocation_after(SIZE_MAX);
    for (index = 0U; index < 64U; ++index)
    {
        restored = NULL;
        minisnn_worlds_kernel_testing_fail_allocation_after(index);
        error = minisnn_worlds_kernel_create_from_snapshot(imported, &restored);
        if (error == MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            break;
        }
        REQUIRE(error == MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
        REQUIRE(restored == NULL);
    }
    REQUIRE(restored != NULL);
    minisnn_worlds_kernel_testing_fail_allocation_after(SIZE_MAX);
    assert_same_state(kernel, restored);

    minisnn_worlds_kernel_destroy(restored);
    minisnn_worlds_kernel_snapshot_destroy(imported);
    free(bytes);
    minisnn_worlds_kernel_snapshot_destroy(snapshot);
    minisnn_worlds_kernel_destroy(kernel);
}

static void test_duplicate_command_id_and_counter_invariants(void)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelSnapshot *snapshot = NULL;
    MiniSNNWorldsKernelSnapshot *imported = NULL;
    MiniSNNWorldsKernelCommandId command;

    kernel = minisnn_worlds_kernel_create(&config, &error);
    REQUIRE(kernel != NULL);
    REQUIRE(minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 0U, id(0U), &command) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_entity(kernel, 2U, 0U, id(0U), &command) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(kernel->pending_command_count == 2U);
    kernel->pending_commands[1U].command_id = kernel->pending_commands[0U].command_id;
    REQUIRE(minisnn_worlds_kernel_snapshot_capture(kernel, &snapshot) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_snapshot_from_bytes(
                minisnn_worlds_kernel_snapshot_data(snapshot),
                minisnn_worlds_kernel_snapshot_size(snapshot), &imported) ==
            MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT);
    REQUIRE(imported == NULL);
    minisnn_worlds_kernel_snapshot_destroy(snapshot);
    minisnn_worlds_kernel_destroy(kernel);

    kernel = build_full_state();
    REQUIRE(minisnn_worlds_kernel_testing_set_all_counters(kernel, UINT64_C(0)) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_snapshot_capture(kernel, &snapshot) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_snapshot_from_bytes(
                minisnn_worlds_kernel_snapshot_data(snapshot),
                minisnn_worlds_kernel_snapshot_size(snapshot), &imported) ==
            MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT);
    REQUIRE(imported == NULL);
    minisnn_worlds_kernel_snapshot_destroy(snapshot);
    minisnn_worlds_kernel_destroy(kernel);
}
int main(void)
{
    test_import_restore_and_continuation();
    test_invalid_bytes_and_failure_atomicity();
    test_duplicate_command_id_and_counter_invariants();
    puts("K2-B import, restore and deterministic continuation OK");
    return 0;
}
