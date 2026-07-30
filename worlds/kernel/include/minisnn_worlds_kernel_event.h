#ifndef MINISNN_WORLDS_KERNEL_EVENT_H
#define MINISNN_WORLDS_KERNEL_EVENT_H

#include <stdbool.h>
#include <stddef.h>

#include "minisnn_worlds_kernel_types.h"
#include "minisnn_worlds_kernel_space.h"

typedef enum
{
    MINISNN_WORLDS_KERNEL_EVENT_ENTITY_CREATED = 1,
    MINISNN_WORLDS_KERNEL_EVENT_ENTITY_DESTROYED = 2,
    MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED = 3,
    MINISNN_WORLDS_KERNEL_EVENT_ENTITY_PLACED = 4,
    MINISNN_WORLDS_KERNEL_EVENT_ENTITY_REMOVED_FROM_SPACE = 5
} MiniSNNWorldsKernelEventType;

typedef enum
{
    MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_NONE = 0,
    MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_ISSUER_NOT_ALIVE,
    MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_NOT_ALIVE,
    MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_INVALID_COMMAND,
    MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_ALREADY_PLACED,
    MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_NOT_PLACED,
    MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_POSITION_OUT_OF_BOUNDS,
    MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_INVALID_ORIENTATION
} MiniSNNWorldsKernelCommandRejection;

typedef struct
{
    MiniSNNWorldsKernelEventId event_id;
    MiniSNNWorldsTick tick;
    MiniSNNWorldsKernelEventType type;
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelEntityId issuer;
    MiniSNNWorldsKernelEntityId subject;
    MiniSNNWorldsKernelCommandRejection rejection;
    bool has_transform;
    MiniSNNWorldsKernelTransform transform;
} MiniSNNWorldsKernelEvent;

size_t minisnn_worlds_kernel_last_tick_event_count(
    const MiniSNNWorldsKernel *kernel);

MiniSNNWorldsKernelError minisnn_worlds_kernel_last_tick_event_at(
    const MiniSNNWorldsKernel *kernel,
    size_t canonical_index,
    MiniSNNWorldsKernelEvent *out_event);

#endif
