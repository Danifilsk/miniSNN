#include "minisnn_worlds_kernel.h"
#include "minisnn_worlds_kernel_snapshot.h"

#include <stdint.h>
#include <stdio.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "command batch test failed: %s line %d\n", #c, __LINE__); return 1; } } while (0)

static int snapshots_equal(const MiniSNNWorldsKernelSnapshot *a,
                           const MiniSNNWorldsKernelSnapshot *b)
{
    size_t i;
    if (minisnn_worlds_kernel_snapshot_size(a) != minisnn_worlds_kernel_snapshot_size(b)) return 0;
    for (i = 0; i < minisnn_worlds_kernel_snapshot_size(a); ++i)
    {
        if (minisnn_worlds_kernel_snapshot_data(a)[i] != minisnn_worlds_kernel_snapshot_data(b)[i]) return 0;
    }
    return 1;
}

int main(void)
{
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel = minisnn_worlds_kernel_create(NULL, &error);
    MiniSNNWorldsKernelSnapshot *before = NULL;
    MiniSNNWorldsKernelSnapshot *after = NULL;
    MiniSNNWorldsKernelCommandId old_id = {0}, a = {0}, b = {0};
    MiniSNNWorldsKernelEntityId none = {0};
    uint64_t hash_before = 0, hash_after = 0;
    size_t old_pending;

    CHECK(kernel != NULL && error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_queue_create_entity(kernel, 5, 0, none, &old_id) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    old_pending = minisnn_worlds_kernel_pending_command_count(kernel);
    CHECK(minisnn_worlds_kernel_snapshot_capture(kernel, &before) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_state_hash(kernel, &hash_before) == MINISNN_WORLDS_KERNEL_ERROR_NONE);

    CHECK(minisnn_worlds_kernel_command_batch_begin(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_command_batch_active(kernel));
    CHECK(minisnn_worlds_kernel_command_batch_begin(kernel) == MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE);
    CHECK(minisnn_worlds_kernel_snapshot_capture(kernel, &after) == MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE);
    CHECK(after == NULL);
    CHECK(minisnn_worlds_kernel_queue_create_entity(kernel, 1, 0, none, &a) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_queue_create_entity(kernel, 2, 0, none, &b) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_pending_command_count(kernel) == old_pending + 2U);
    CHECK(minisnn_worlds_kernel_command_batch_rollback(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(!minisnn_worlds_kernel_command_batch_active(kernel));
    CHECK(minisnn_worlds_kernel_pending_command_count(kernel) == old_pending);
    CHECK(minisnn_worlds_kernel_snapshot_capture(kernel, &after) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(snapshots_equal(before, after));
    CHECK(minisnn_worlds_kernel_state_hash(kernel, &hash_after) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(hash_before == hash_after);
    minisnn_worlds_kernel_snapshot_destroy(after); after = NULL;

    CHECK(minisnn_worlds_kernel_command_batch_begin(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_queue_create_entity(kernel, 1, 0, none, &a) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    minisnn_worlds_kernel_testing_fail_next_allocation();
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);
    CHECK(minisnn_worlds_kernel_command_batch_rollback(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_snapshot_capture(kernel, &after) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(snapshots_equal(before, after));
    minisnn_worlds_kernel_snapshot_destroy(after); after = NULL;

    CHECK(minisnn_worlds_kernel_command_batch_begin(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_queue_create_entity(kernel, 1, 0, none, &a) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_command_batch_rollback(kernel) == MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE);
    CHECK(minisnn_worlds_kernel_command_batch_active(kernel));
    CHECK(minisnn_worlds_kernel_command_batch_commit(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(!minisnn_worlds_kernel_command_batch_active(kernel));
    CHECK(minisnn_worlds_kernel_command_batch_commit(kernel) == MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE);
    CHECK(minisnn_worlds_kernel_command_batch_rollback(kernel) == MINISNN_WORLDS_KERNEL_ERROR_INVALID_STATE);

    minisnn_worlds_kernel_snapshot_destroy(before);
    minisnn_worlds_kernel_destroy(kernel);
    puts("Kernel provisional command batch OK");
    return 0;
}
