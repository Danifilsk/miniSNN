#ifndef APP_FILESYSTEM_H
#define APP_FILESYSTEM_H

#include <stddef.h>

/* Small portable filesystem boundary for headless applications and tests. */
int app_filesystem_file_exists(const char *path);
int app_filesystem_directory_exists(const char *path);
int app_filesystem_ensure_directory(const char *path);
int app_filesystem_ensure_directory_tree(const char *path);
int app_filesystem_copy_file(const char *source_path, const char *destination_path);
int app_filesystem_replace_file(const char *temporary_path, const char *final_path);
int app_filesystem_remove_tree(const char *path);
unsigned long long app_filesystem_file_size(const char *path);
double app_filesystem_monotonic_seconds(void);
void app_filesystem_timestamp(char *out_timestamp, size_t out_size,
                              int filename_style);

#endif
