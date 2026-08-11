#include "k2_command_log_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

static int write_all(FILE *file, const uint8_t *data, size_t size)
{
    return file != NULL && data != NULL && size != 0U &&
           fwrite(data, 1U, size, file) == size && fflush(file) == 0;
}

static char *temporary_path(const char *path)
{
    const size_t suffix_size = sizeof(".tmp");
    const size_t path_size = path == NULL ? 0U : strlen(path);
    char *result;

    if (path == NULL || path_size == 0U || path_size > SIZE_MAX - suffix_size)
    {
        return NULL;
    }
    result = malloc(path_size + suffix_size);
    if (result == NULL)
    {
        return NULL;
    }
    memcpy(result, path, path_size);
    memcpy(result + path_size, ".tmp", suffix_size);
    return result;
}

static int replace_file(const char *temporary, const char *destination)
{
#ifdef _WIN32
    return MoveFileExA(temporary, destination,
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary, destination) == 0;
#endif
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_command_log_save_file(
    const char *path,
    const MiniSNNWorldsKernelCommandLog *log)
{
    const uint8_t *data;
    size_t size;
    char *temporary;
    FILE *file;
    int wrote;
    int closed;

    if (path == NULL || log == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    data = minisnn_worlds_kernel_command_log_data(log);
    size = minisnn_worlds_kernel_command_log_size(log);
    if (data == NULL || size == 0U)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_COMMAND_LOG_INVALID_FORMAT;
    }
    temporary = temporary_path(path);
    if (temporary == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    file = fopen(temporary, "wb");
    if (file == NULL)
    {
        free(temporary);
        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }
    wrote = write_all(file, data, size);
    closed = fclose(file) == 0;
    if (!wrote || !closed || !replace_file(temporary, path))
    {
        (void)remove(temporary);
        free(temporary);
        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }
    free(temporary);
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_command_log_load_file(
    const char *path,
    MiniSNNWorldsKernelCommandLog **out_log)
{
    FILE *file;
    long file_size;
    uint8_t *data;
    MiniSNNWorldsKernelError error;

    if (out_log == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    *out_log = NULL;
    if (path == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    file = fopen(path, "rb");
    if (file == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }
    if (fseek(file, 0L, SEEK_END) != 0 || (file_size = ftell(file)) < 0 ||
        fseek(file, 0L, SEEK_SET) != 0)
    {
        fclose(file);
        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }
    if (file_size == 0L)
    {
        fclose(file);
        return MINISNN_WORLDS_KERNEL_ERROR_COMMAND_LOG_INVALID_FORMAT;
    }
    if ((uintmax_t)file_size > (uintmax_t)SIZE_MAX)
    {
        fclose(file);
        return MINISNN_WORLDS_KERNEL_ERROR_COMMAND_LOG_INVALID_FORMAT;
    }
    data = malloc((size_t)file_size);
    if (data == NULL)
    {
        fclose(file);
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    if (fread(data, 1U, (size_t)file_size, file) != (size_t)file_size || fclose(file) != 0)
    {
        free(data);
        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }
    error = minisnn_worlds_kernel_command_log_from_bytes(data, (size_t)file_size, out_log);
    free(data);
    return error;
}