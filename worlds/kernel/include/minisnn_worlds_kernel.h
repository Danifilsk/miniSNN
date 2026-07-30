#ifndef MINISNN_WORLDS_KERNEL_H
#define MINISNN_WORLDS_KERNEL_H

#include "minisnn_worlds_kernel_config.h"
#include "minisnn_worlds_kernel_entity.h"
#include "minisnn_worlds_kernel_command.h"
#include "minisnn_worlds_kernel_event.h"
#include "minisnn_worlds_kernel_random.h"
#include "minisnn_worlds_kernel_hash.h"
#include "minisnn_worlds_kernel_diagnostics.h"

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

MiniSNNWorldsKernelError minisnn_worlds_kernel_step(
    MiniSNNWorldsKernel *kernel);

MiniSNNWorldsKernelError minisnn_worlds_kernel_get_diagnostics(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelDiagnostics *out_diagnostics);

MiniSNNWorldsKernelError minisnn_worlds_kernel_capture_trace_point(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelTracePoint *out_trace);

#ifdef MINISNN_WORLDS_KERNEL_TESTING
void minisnn_worlds_kernel_testing_fail_next_allocation(void);

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

void minisnn_worlds_kernel_testing_fail_allocation_after(size_t successful_allocations);

MiniSNNWorldsKernelError minisnn_worlds_kernel_testing_set_random_counts(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelRandomStreamKey key,
    uint64_t generated_u32_count,
    uint64_t total_random_u32_generated);
#endif

#endif
