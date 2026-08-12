#ifndef MINISNN_WORLDS_KERNEL_H
#define MINISNN_WORLDS_KERNEL_H

#include <stdbool.h>

#include "minisnn_worlds_kernel_config.h"
#include "minisnn_worlds_kernel_space.h"
#include "minisnn_worlds_kernel_occupancy.h"
#include "minisnn_worlds_kernel_spatial_link.h"
#include "minisnn_worlds_kernel_entity.h"
#include "minisnn_worlds_kernel_command.h"
#include "minisnn_worlds_kernel_command_log.h"
#include "minisnn_worlds_kernel_event.h"
#include "minisnn_worlds_kernel_random.h"
#include "minisnn_worlds_kernel_hash.h"
#include "minisnn_worlds_kernel_diagnostics.h"
#include "minisnn_worlds_kernel_snapshot.h"

MiniSNNWorldsKernel *minisnn_worlds_kernel_create(
    const MiniSNNWorldsKernelConfig *config,
    MiniSNNWorldsKernelError *out_error);

void minisnn_worlds_kernel_destroy(MiniSNNWorldsKernel *kernel);

MiniSNNWorldsTick minisnn_worlds_kernel_tick(
    const MiniSNNWorldsKernel *kernel);

MiniSNNWorldsKernelState minisnn_worlds_kernel_state(
    const MiniSNNWorldsKernel *kernel);

MiniSNNWorldsKernelError minisnn_worlds_kernel_last_error(
    const MiniSNNWorldsKernel *kernel);

uint64_t minisnn_worlds_kernel_master_seed(
    const MiniSNNWorldsKernel *kernel);

MiniSNNWorldsKernelError minisnn_worlds_kernel_space_bounds(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelSpaceBounds *out_bounds);

MiniSNNWorldsKernelError minisnn_worlds_kernel_step(
    MiniSNNWorldsKernel *kernel);

/* A provisional batch groups submissions until the caller commits a successful tick.
 * Rolled-back CommandIds were never committed and may be reused. */
MiniSNNWorldsKernelError minisnn_worlds_kernel_command_batch_begin(
    MiniSNNWorldsKernel *kernel);

MiniSNNWorldsKernelError minisnn_worlds_kernel_command_batch_commit(
    MiniSNNWorldsKernel *kernel);

MiniSNNWorldsKernelError minisnn_worlds_kernel_command_batch_rollback(
    MiniSNNWorldsKernel *kernel);

bool minisnn_worlds_kernel_command_batch_active(
    const MiniSNNWorldsKernel *kernel);

MiniSNNWorldsKernelError minisnn_worlds_kernel_get_diagnostics(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelDiagnostics *out_diagnostics);

MiniSNNWorldsKernelError minisnn_worlds_kernel_capture_trace_point(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelTracePoint *out_trace);

#ifdef MINISNN_WORLDS_KERNEL_TESTING
typedef enum
{
    MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_SPATIAL_LINK_CYCLE = 1,
    MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_CHILD_HAS_TWO_PARENTS,
    MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_DUPLICATE_SPATIAL_LINK,
    MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_UNSORTED_SPATIAL_LINKS,
    MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_DEAD_LINK_ENDPOINT,
    MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_UNPLACED_LINK_ENDPOINT,
    MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_LINK_OFFSET,
    MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_UNKNOWN_LINK_ENDPOINT,
    MINISNN_WORLDS_KERNEL_TESTING_CORRUPTION_COUNTERS
} MiniSNNWorldsKernelTestingCorruption;

int minisnn_worlds_kernel_testing_scalar_add(
    MiniSNNWorldsKernelScalar left,
    MiniSNNWorldsKernelScalar right,
    MiniSNNWorldsKernelScalar *out_result);

int minisnn_worlds_kernel_testing_scalar_subtract(
    MiniSNNWorldsKernelScalar left,
    MiniSNNWorldsKernelScalar right,
    MiniSNNWorldsKernelScalar *out_result);

void minisnn_worlds_kernel_testing_fail_next_allocation(void);


MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_reserve_entity_capacity(
    MiniSNNWorldsKernel *kernel,
    size_t capacity);
MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_set_tick(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick tick);

MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_set_next_entity_id(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId entity_id);

MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_set_next_command_id(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelCommandId command_id);

MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_set_next_event_id(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEventId event_id);

MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_set_observability_counters(
    MiniSNNWorldsKernel *kernel,
    uint64_t total_events_emitted,
    uint64_t total_entities_moved);
MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_set_all_counters(
    MiniSNNWorldsKernel *kernel,
    uint64_t value);

MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_validate_invariants(
    const MiniSNNWorldsKernel *kernel);

MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_inject_corruption(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelTestingCorruption corruption);

void minisnn_worlds_kernel_testing_fail_allocation_after(size_t successful_allocations);

MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_set_random_counts(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelRandomStreamKey key,
    uint64_t generated_u32_count,
    uint64_t total_random_u32_generated);
#endif

#endif
