#include "k2_snapshot_file.h"

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

MiniSNNWorldsKernelError minisnn_worlds_kernel_snapshot_save_file(
    const char *path,
    const MiniSNNWorldsKernelSnapshot *snapshot)
{
    const uint8_t *data;
    size_t size;
    char *temp_path;
    FILE *file;
    int write_succeeded;
    int close_succeeded;

    if (path == NULL || snapshot == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    data = minisnn_worlds_kernel_snapshot_data(snapshot);
    size = minisnn_worlds_kernel_snapshot_size(snapshot);
    if (data == NULL || size == 0U)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT;
    }
    temp_path = temporary_path(path);
    if (temp_path == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    file = fopen(temp_path, "wb");
    if (file == NULL)
    {
        free(temp_path);
        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }
    write_succeeded = write_all(file, data, size);
    close_succeeded = fclose(file) == 0;
    if (!write_succeeded || !close_succeeded || !replace_file(temp_path, path))
    {
        (void)remove(temp_path);
        free(temp_path);
        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }
    free(temp_path);
    return MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsKernelError minisnn_worlds_kernel_snapshot_load_file(
    const char *path,
    MiniSNNWorldsKernelSnapshot **out_snapshot)
{
    FILE *file;
    long file_size;
    uint8_t *data;
    MiniSNNWorldsKernelError error;

    if (out_snapshot == NULL)
    {
        return MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT;
    }
    *out_snapshot = NULL;
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
        return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT;
    }
    if ((uintmax_t)file_size > (uintmax_t)SIZE_MAX)
    {
        fclose(file);
        return MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_SIZE_OVERFLOW;
    }
    data = malloc((size_t)file_size);
    if (data == NULL)
    {
        fclose(file);
        return MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION;
    }
    if (fread(data, 1U, (size_t)file_size, file) != (size_t)file_size ||
        fclose(file) != 0)
    {
        free(data);
        return MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    }
    error = minisnn_worlds_kernel_snapshot_from_bytes(data, (size_t)file_size,
                                                       out_snapshot);
    free(data);
    return error;
}