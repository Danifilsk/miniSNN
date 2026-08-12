#ifndef K2_COMMAND_LOG_FILE_H
#define K2_COMMAND_LOG_FILE_H

#include "minisnn_worlds_kernel_command_log.h"

MiniSNNWorldsKernelError minisnn_worlds_kernel_command_log_save_file(
    const char *path,
    const MiniSNNWorldsKernelCommandLog *log);

MiniSNNWorldsKernelError minisnn_worlds_kernel_command_log_load_file(
    const char *path,
    MiniSNNWorldsKernelCommandLog **out_log);

#endif