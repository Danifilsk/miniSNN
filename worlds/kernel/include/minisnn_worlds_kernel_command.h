#ifndef MINISNN_WORLDS_KERNEL_COMMAND_H
#define MINISNN_WORLDS_KERNEL_COMMAND_H

#include <stddef.h>

#include "minisnn_worlds_kernel_types.h"

#define MINISNN_WORLDS_KERNEL_COMMAND_PRIORITY_DEFAULT UINT32_C(0)

typedef enum
{
    MINISNN_WORLDS_KERNEL_COMMAND_CREATE_ENTITY = 1,
    MINISNN_WORLDS_KERNEL_COMMAND_DESTROY_ENTITY = 2
} MiniSNNWorldsKernelCommandType;

typedef struct
{
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsTick target_tick;
    uint32_t priority;
    MiniSNNWorldsKernelEntityId issuer;
    MiniSNNWorldsKernelCommandType type;
    MiniSNNWorldsKernelEntityId target_entity;
} MiniSNNWorldsKernelCommandInfo;

MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_create_entity(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelCommandId *out_command_id);

MiniSNNWorldsKernelError minisnn_worlds_kernel_queue_destroy_entity(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick target_tick,
    uint32_t priority,
    MiniSNNWorldsKernelEntityId issuer,
    MiniSNNWorldsKernelEntityId target_entity,
    MiniSNNWorldsKernelCommandId *out_command_id);

size_t minisnn_worlds_kernel_pending_command_count(
    const MiniSNNWorldsKernel *kernel);

MiniSNNWorldsKernelError minisnn_worlds_kernel_pending_command_at(
    const MiniSNNWorldsKernel *kernel,
    size_t canonical_index,
    MiniSNNWorldsKernelCommandInfo *out_command);

#endif
