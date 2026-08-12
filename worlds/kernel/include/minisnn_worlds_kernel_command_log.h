#ifndef MINISNN_WORLDS_KERNEL_COMMAND_LOG_H
#define MINISNN_WORLDS_KERNEL_COMMAND_LOG_H

#include <stddef.h>
#include <stdint.h>

#include "minisnn_worlds_kernel_command.h"

#define MINISNN_WORLDS_KERNEL_COMMAND_LOG_FORMAT_VERSION_V1 UINT32_C(1)

typedef struct MiniSNNWorldsKernelCommandLog MiniSNNWorldsKernelCommandLog;
typedef struct MiniSNNWorldsKernelReplaySession MiniSNNWorldsKernelReplaySession;

typedef struct
{
    MiniSNNWorldsTick submission_tick;
    MiniSNNWorldsKernelCommandInfo command;
} MiniSNNWorldsKernelCommandLogRecord;

MiniSNNWorldsKernelError minisnn_worlds_kernel_command_log_create(
    MiniSNNWorldsKernelCommandLog **out_log);

void minisnn_worlds_kernel_command_log_destroy(
    MiniSNNWorldsKernelCommandLog *log);

/* Appends one accepted external submission in its original submission order. */
MiniSNNWorldsKernelError minisnn_worlds_kernel_command_log_append(
    MiniSNNWorldsKernelCommandLog *log,
    const MiniSNNWorldsKernelCommandLogRecord *record);

/* Captures one pending command by its assigned CommandId without exposing storage. */
MiniSNNWorldsKernelError minisnn_worlds_kernel_command_log_capture_submission(
    MiniSNNWorldsKernelCommandLog *log,
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsTick submission_tick,
    MiniSNNWorldsKernelCommandId command_id);

size_t minisnn_worlds_kernel_command_log_count(
    const MiniSNNWorldsKernelCommandLog *log);

MiniSNNWorldsKernelError minisnn_worlds_kernel_command_log_record_at(
    const MiniSNNWorldsKernelCommandLog *log,
    size_t index,
    MiniSNNWorldsKernelCommandLogRecord *out_record);

const uint8_t *minisnn_worlds_kernel_command_log_data(
    const MiniSNNWorldsKernelCommandLog *log);

size_t minisnn_worlds_kernel_command_log_size(
    const MiniSNNWorldsKernelCommandLog *log);

uint64_t minisnn_worlds_kernel_command_log_digest(
    const MiniSNNWorldsKernelCommandLog *log);

MiniSNNWorldsKernelError minisnn_worlds_kernel_command_log_from_bytes(
    const uint8_t *data,
    size_t size,
    MiniSNNWorldsKernelCommandLog **out_log);

/* Replays one record only when the Kernel is at that record's submission tick. */
MiniSNNWorldsKernelError minisnn_worlds_kernel_command_log_replay_next(
    const MiniSNNWorldsKernelCommandLog *log,
    size_t index,
    MiniSNNWorldsKernel *kernel);
/* A caller-owned session borrows log, which must outlive the session.
 * Calls are not synchronized; callers serialize access to one session.
 * initial_cursor is the number of records already reflected in the Kernel state.
 */
MiniSNNWorldsKernelError minisnn_worlds_kernel_replay_session_create(
    const MiniSNNWorldsKernelCommandLog *log,
    size_t initial_cursor,
    uint64_t expected_state_hash,
    MiniSNNWorldsKernelReplaySession **out_session);

void minisnn_worlds_kernel_replay_session_destroy(
    MiniSNNWorldsKernelReplaySession *session);

/* Validates the exact restored/fresh state before a record is submitted. */
MiniSNNWorldsKernelError minisnn_worlds_kernel_replay_session_validate(
    MiniSNNWorldsKernelReplaySession *session,
    MiniSNNWorldsKernel *kernel);

/* Replays one next record only after state binding has been validated. */
MiniSNNWorldsKernelError minisnn_worlds_kernel_replay_session_replay_next(
    MiniSNNWorldsKernelReplaySession *session,
    MiniSNNWorldsKernel *kernel);

size_t minisnn_worlds_kernel_replay_session_cursor(
    const MiniSNNWorldsKernelReplaySession *session);

#endif