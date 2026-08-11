#include "minisnn_worlds_kernel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(condition) \
    do { if (!(condition)) { \
        fprintf(stderr, "requirement failed: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
        exit(1); \
    } } while (0)

#define SNAPSHOT_POINT_COUNT 4U

typedef struct
{
    uint8_t *data;
    size_t size;
} SnapshotCopy;

static MiniSNNWorldsKernelEntityId id(uint64_t value)
{
    MiniSNNWorldsKernelEntityId result;
    result.value = value;
    return result;
}

static MiniSNNWorldsKernelTransform transform(int64_t x, int64_t y)
{
    MiniSNNWorldsKernelTransform result;
    result.position.x = x;
    result.position.y = y;
    result.orientation = UINT32_C(0);
    return result;
}

static void capture_copy(MiniSNNWorldsKernel *kernel, SnapshotCopy *out_copy)
{
    MiniSNNWorldsKernelSnapshot *snapshot = NULL;

    REQUIRE(minisnn_worlds_kernel_snapshot_capture(kernel, &snapshot) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    out_copy->size = minisnn_worlds_kernel_snapshot_size(snapshot);
    out_copy->data = malloc(out_copy->size);
    REQUIRE(out_copy->data != NULL);
    memcpy(out_copy->data, minisnn_worlds_kernel_snapshot_data(snapshot), out_copy->size);
    minisnn_worlds_kernel_snapshot_destroy(snapshot);
}

static void destroy_copies(SnapshotCopy *copies)
{
    size_t index;

    for (index = 0U; index < SNAPSHOT_POINT_COUNT; ++index)
    {
        free(copies[index].data);
    }
}

static void run_workload(SnapshotCopy *out_copies)
{
    static const uint64_t snapshot_ticks[SNAPSHOT_POINT_COUNT] = {
        UINT64_C(0), UINT64_C(100), UINT64_C(500), UINT64_C(1000)
    };
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernel *kernel;
    size_t snapshot_index = 0U;

    memset(out_copies, 0, sizeof(SnapshotCopy) * SNAPSHOT_POINT_COUNT);
    config.master_seed = UINT64_C(0x4B32415F4C4F4E47);
    kernel = minisnn_worlds_kernel_create(&config, &error);
    REQUIRE(kernel != NULL);
    REQUIRE(error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    capture_copy(kernel, &out_copies[snapshot_index++]);

    REQUIRE(minisnn_worlds_kernel_queue_create_entity(
                kernel, UINT64_C(1), UINT32_C(0), id(UINT64_C(0)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_entity(
                kernel, UINT64_C(1), UINT32_C(1), id(UINT64_C(0)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_place_entity(
                kernel, UINT64_C(2), UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)),
                transform(INT64_C(0), INT64_C(0)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_place_entity(
                kernel, UINT64_C(2), UINT32_C(1), id(UINT64_C(0)), id(UINT64_C(2)),
                transform(INT64_C(10), INT64_C(0)), &command_id) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_queue_create_spatial_link(
                kernel, UINT64_C(3), UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)),
                id(UINT64_C(2)), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);

    while (minisnn_worlds_kernel_tick(kernel) < UINT64_C(1000))
    {
        MiniSNNWorldsTick target_tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
        int64_t delta = (target_tick & UINT64_C(1)) == 0U ? INT64_C(1) : INT64_C(-1);

        REQUIRE(minisnn_worlds_kernel_queue_move_entity(
                    kernel, target_tick, UINT32_C(0), id(UINT64_C(0)), id(UINT64_C(1)),
                    delta, INT64_C(0), &command_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
        REQUIRE(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
        if (snapshot_index < SNAPSHOT_POINT_COUNT &&
            minisnn_worlds_kernel_tick(kernel) == snapshot_ticks[snapshot_index])
        {
            capture_copy(kernel, &out_copies[snapshot_index]);
            ++snapshot_index;
        }
    }
    REQUIRE(snapshot_index == SNAPSHOT_POINT_COUNT);
    minisnn_worlds_kernel_destroy(kernel);
}

int main(void)
{
    SnapshotCopy first[SNAPSHOT_POINT_COUNT];
    SnapshotCopy second[SNAPSHOT_POINT_COUNT];
    size_t index;

    run_workload(first);
    run_workload(second);
    for (index = 0U; index < SNAPSHOT_POINT_COUNT; ++index)
    {
        REQUIRE(first[index].size == second[index].size);
        REQUIRE(memcmp(first[index].data, second[index].data, first[index].size) == 0);
    }
    destroy_copies(first);
    destroy_copies(second);
    printf("K2-A long-run snapshot determinism OK: ticks=0,100,500,1000\n");
    return 0;
}