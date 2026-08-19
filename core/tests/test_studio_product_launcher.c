#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "studio_product_launcher.h"

static int assert_true(int condition, const char *message)
{
    if (condition)
    {
        return 1;
    }
    fprintf(stderr, "FAILED: %s\n", message);
    return 0;
}

static int create_empty_file(const char *path)
{
    FILE *file = fopen(path, "wb");

    if (file == NULL)
    {
        return 0;
    }
    return fclose(file) == 0;
}

int main(void)
{
    char temporary_directory[MAX_PATH];
    char fixture_directory[MAX_PATH];
    char studio_path[MAX_PATH];
    char worlds_path[MAX_PATH];
    char resolved_path[MAX_PATH];
    char error_message[STUDIO_PRODUCT_LAUNCHER_ERROR_SIZE];
    int ok = 1;

    if (GetTempPathA(sizeof(temporary_directory), temporary_directory) == 0 ||
        GetTempFileNameA(temporary_directory, "mwl", 0U, fixture_directory) == 0)
    {
        fprintf(stderr, "FAILED: temporary fixture unavailable\n");
        return 1;
    }
    DeleteFileA(fixture_directory);
    if (!CreateDirectoryA(fixture_directory, NULL) ||
        snprintf(studio_path, sizeof(studio_path), "%s\\minisnn_studio.exe",
                 fixture_directory) >= (int)sizeof(studio_path) ||
        snprintf(worlds_path, sizeof(worlds_path), "%s\\minisnn_worlds.exe",
                 fixture_directory) >= (int)sizeof(worlds_path) ||
        !create_empty_file(studio_path) || !create_empty_file(worlds_path))
    {
        fprintf(stderr, "FAILED: fixture creation failed\n");
        return 1;
    }

    ok &= assert_true(
        studio_product_launcher_resolve(studio_path, resolved_path,
                                       sizeof(resolved_path), error_message,
                                       sizeof(error_message)),
        "Worlds executable beside Studio resolves");
    ok &= assert_true(strcmp(resolved_path, worlds_path) == 0,
                      "resolved Worlds executable path is exact");

    DeleteFileA(worlds_path);
    ok &= assert_true(
        !studio_product_launcher_resolve(studio_path, resolved_path,
                                        sizeof(resolved_path), error_message,
                                        sizeof(error_message)),
        "missing Worlds executable is rejected");
    ok &= assert_true(resolved_path[0] == '\0',
                      "missing Worlds executable does not leave a launch path");
    ok &= assert_true(strstr(error_message, "nao foi encontrado") != NULL,
                      "missing Worlds executable reports a controlled error");

    DeleteFileA(studio_path);
    RemoveDirectoryA(fixture_directory);
    if (!ok)
    {
        return 1;
    }
    printf("Studio Worlds launcher validation OK\n");
    return 0;
}
