#include "studio_product_launcher.h"

#include <windows.h>

#include <stdio.h>
#include <string.h>

static void set_error(char *error_message, size_t error_message_size,
                      const char *message)
{
    if (error_message != NULL && error_message_size > 0U)
    {
        (void)snprintf(error_message, error_message_size, "%s", message);
    }
}

static int executable_directory(
    const char *executable_path,
    char *out_directory,
    size_t out_directory_size)
{
    char *separator;
    size_t length;

    if (executable_path == NULL || out_directory == NULL || out_directory_size == 0U)
    {
        return 0;
    }
    length = strlen(executable_path);
    if (length == 0U || length >= out_directory_size)
    {
        return 0;
    }
    memcpy(out_directory, executable_path, length + 1U);
    separator = strrchr(out_directory, '\\');
    if (separator == NULL)
    {
        separator = strrchr(out_directory, '/');
    }
    if (separator == NULL || separator == out_directory)
    {
        return 0;
    }
    *separator = '\0';
    return 1;
}

int studio_product_launcher_resolve(
    const char *studio_executable_path,
    char *out_worlds_executable_path,
    size_t out_worlds_executable_path_size,
    char *error_message,
    size_t error_message_size)
{
    char directory[MAX_PATH];
    DWORD attributes;

    set_error(error_message, error_message_size, "");
    if (out_worlds_executable_path != NULL && out_worlds_executable_path_size > 0U)
    {
        out_worlds_executable_path[0] = '\0';
    }
    if (!executable_directory(studio_executable_path, directory, sizeof(directory)) ||
        out_worlds_executable_path == NULL || out_worlds_executable_path_size == 0U ||
        snprintf(out_worlds_executable_path, out_worlds_executable_path_size,
                 "%s\\minisnn_worlds.exe", directory) >=
            (int)out_worlds_executable_path_size)
    {
        set_error(error_message, error_message_size,
                  "Nao foi possivel resolver o diretorio do miniSNN Studio.");
        return 0;
    }
    attributes = GetFileAttributesA(out_worlds_executable_path);
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U)
    {
        out_worlds_executable_path[0] = '\0';
        set_error(error_message, error_message_size,
                  "miniSNN Worlds nao foi encontrado.\n"
                  "Recompile usando o target \"worlds\".");
        return 0;
    }
    return 1;
}

int studio_product_launcher_launch(
    const char *worlds_executable_path,
    char *error_message,
    size_t error_message_size)
{
    STARTUPINFOA startup_info;
    PROCESS_INFORMATION process_info;
    char command_line[MAX_PATH + 4U];

    set_error(error_message, error_message_size, "");
    if (worlds_executable_path == NULL || worlds_executable_path[0] == '\0' ||
        snprintf(command_line, sizeof(command_line), "\"%s\"",
                 worlds_executable_path) >= (int)sizeof(command_line))
    {
        set_error(error_message, error_message_size,
                  "Nao foi possivel preparar a abertura do miniSNN Worlds.");
        return 0;
    }
    ZeroMemory(&startup_info, sizeof(startup_info));
    ZeroMemory(&process_info, sizeof(process_info));
    startup_info.cb = sizeof(startup_info);
    if (!CreateProcessA(worlds_executable_path, command_line, NULL, NULL, FALSE,
                        0U, NULL, NULL, &startup_info, &process_info))
    {
        set_error(error_message, error_message_size,
                  "Nao foi possivel iniciar o miniSNN Worlds.");
        return 0;
    }
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return 1;
}
