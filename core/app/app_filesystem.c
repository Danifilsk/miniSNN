#include "app_filesystem.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

static int app_filesystem_make_directory(const char *path)
{
#ifdef _WIN32
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0777) == 0 || errno == EEXIST;
#endif
}

int app_filesystem_file_exists(const char *path)
{
    struct stat state;

    return path != NULL && stat(path, &state) == 0 && S_ISREG(state.st_mode);
}

int app_filesystem_directory_exists(const char *path)
{
    struct stat state;

    return path != NULL && stat(path, &state) == 0 && S_ISDIR(state.st_mode);
}

int app_filesystem_ensure_directory(const char *path)
{
    if (path == NULL || path[0] == '\0')
        return 0;

    return app_filesystem_directory_exists(path) ||
           app_filesystem_make_directory(path);
}

int app_filesystem_ensure_directory_tree(const char *path)
{
    char buffer[1024];
    size_t length;
    size_t index;

    if (path == NULL || path[0] == '\0' ||
        snprintf(buffer, sizeof(buffer), "%s", path) >= (int)sizeof(buffer))
        return 0;

    length = strlen(buffer);
    for (index = 0U; index < length; index++)
    {
        if ((buffer[index] == '/' || buffer[index] == '\\') && index > 0U &&
            !(index == 2U && buffer[1] == ':'))
        {
            char separator = buffer[index];
            buffer[index] = '\0';
            if (!app_filesystem_ensure_directory(buffer))
                return 0;
            buffer[index] = separator;
        }
    }

    return app_filesystem_ensure_directory(buffer);
}

int app_filesystem_copy_file(const char *source_path, const char *destination_path)
{
    FILE *source;
    FILE *destination;
    unsigned char buffer[4096];
    size_t count;
    int ok = 1;

    if (source_path == NULL || destination_path == NULL)
        return 0;

    source = fopen(source_path, "rb");
    if (source == NULL)
        return 0;
    destination = fopen(destination_path, "wb");
    if (destination == NULL)
    {
        fclose(source);
        return 0;
    }

    while ((count = fread(buffer, 1U, sizeof(buffer), source)) > 0U)
    {
        if (fwrite(buffer, 1U, count, destination) != count)
        {
            ok = 0;
            break;
        }
    }
    if (ferror(source) != 0)
        ok = 0;
    if (fclose(source) != 0)
        ok = 0;
    if (fclose(destination) != 0)
        ok = 0;
    return ok;
}

int app_filesystem_replace_file(const char *temporary_path, const char *final_path)
{
    if (temporary_path == NULL || final_path == NULL)
        return 0;

    /* POSIX replace is atomic. Some Windows C runtimes reject an existing
       destination, so retain a portable fallback for generated artifacts. */
    if (rename(temporary_path, final_path) == 0)
        return 1;
    if (remove(final_path) != 0)
        return 0;
    return rename(temporary_path, final_path) == 0;
}

int app_filesystem_remove_tree(const char *path)
{
    DIR *directory;
    struct dirent *entry;

    if (path == NULL || path[0] == '\0')
        return 0;

    directory = opendir(path);
    if (directory == NULL)
        return errno == ENOENT;

    while ((entry = readdir(directory)) != NULL)
    {
        char child[2048];

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) >=
            (int)sizeof(child))
        {
            closedir(directory);
            return 0;
        }
        if (app_filesystem_directory_exists(child))
        {
            if (!app_filesystem_remove_tree(child))
            {
                closedir(directory);
                return 0;
            }
        }
        else if (remove(child) != 0)
        {
            closedir(directory);
            return 0;
        }
    }

    if (closedir(directory) != 0)
        return 0;
#ifdef _WIN32
    return _rmdir(path) == 0;
#else
    return rmdir(path) == 0;
#endif
}

unsigned long long app_filesystem_file_size(const char *path)
{
    struct stat state;

    if (path == NULL || stat(path, &state) != 0 || state.st_size < 0)
        return 0ULL;
    return (unsigned long long)state.st_size;
}

double app_filesystem_monotonic_seconds(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

void app_filesystem_timestamp(char *out_timestamp, size_t out_size,
                              int filename_style)
{
    time_t now;
    struct tm local_time;
    struct tm *time_value;

    if (out_timestamp == NULL || out_size == 0U)
        return;

    now = time(NULL);
#ifdef _WIN32
    if (localtime_s(&local_time, &now) != 0)
    {
        out_timestamp[0] = '\0';
        return;
    }
    time_value = &local_time;
#else
    time_value = localtime_r(&now, &local_time);
    if (time_value == NULL)
    {
        out_timestamp[0] = '\0';
        return;
    }
#endif
    if (filename_style)
        snprintf(out_timestamp, out_size, "%04d%02d%02d_%02d%02d%02d",
                 time_value->tm_year + 1900, time_value->tm_mon + 1,
                 time_value->tm_mday, time_value->tm_hour, time_value->tm_min,
                 time_value->tm_sec);
    else
        snprintf(out_timestamp, out_size, "%04d-%02d-%02dT%02d:%02d:%02d",
                 time_value->tm_year + 1900, time_value->tm_mon + 1,
                 time_value->tm_mday, time_value->tm_hour, time_value->tm_min,
                 time_value->tm_sec);
}
