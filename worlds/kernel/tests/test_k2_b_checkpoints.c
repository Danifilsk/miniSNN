#define main k2_a_long_run_fixture_main
#include "test_k2_a_long_run.c"
#undef main

static void test_restore_at_all_checkpoints(void)
{
    SnapshotCopy copies[SNAPSHOT_POINT_COUNT];
    size_t index;

    run_workload(copies);
    for (index = 0U; index < SNAPSHOT_POINT_COUNT; ++index)
    {
        MiniSNNWorldsKernelSnapshot *imported = NULL;
        MiniSNNWorldsKernelSnapshot *captured = NULL;
        MiniSNNWorldsKernel *restored = NULL;

        REQUIRE(minisnn_worlds_kernel_snapshot_from_bytes(copies[index].data,
                                                          copies[index].size,
                                                          &imported) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
        REQUIRE(minisnn_worlds_kernel_create_from_snapshot(imported, &restored) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
        REQUIRE(minisnn_worlds_kernel_snapshot_capture(restored, &captured) ==
                MINISNN_WORLDS_KERNEL_ERROR_NONE);
        REQUIRE(minisnn_worlds_kernel_snapshot_size(captured) == copies[index].size);
        REQUIRE(memcmp(minisnn_worlds_kernel_snapshot_data(captured),
                       copies[index].data, copies[index].size) == 0);
        minisnn_worlds_kernel_snapshot_destroy(captured);
        minisnn_worlds_kernel_destroy(restored);
        minisnn_worlds_kernel_snapshot_destroy(imported);
    }
    destroy_copies(copies);
}

int main(void)
{
    test_restore_at_all_checkpoints();
    puts("K2-B restore checkpoints OK: ticks=0,100,500,1000");
    return 0;
}