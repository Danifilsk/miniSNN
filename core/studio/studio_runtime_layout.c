#include "studio_runtime_layout.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define STUDIO_RUNTIME_ERROR_SIZE 512

static void set_error(
    char *error_message,
    size_t error_message_size,
    const char *message)
{
    if (error_message != NULL && error_message_size > 0)
        snprintf(error_message, error_message_size, "%s", message);
}

static void set_path_error(
    char *error_message,
    size_t error_message_size,
    const char *prefix,
    const char *path)
{
    if (error_message != NULL && error_message_size > 0)
    {
        snprintf(error_message, error_message_size, "%s: %s", prefix,
                 path != NULL && path[0] != '\0' ? path : "(caminho invalido)");
    }
}

static int directory_exists(const char *path)
{
    DWORD attributes;

    if (path == NULL || path[0] == '\0')
        return 0;
    attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static int file_exists(const char *path)
{
    DWORD attributes;

    if (path == NULL || path[0] == '\0')
        return 0;
    attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static int copy_path(char *destination, size_t destination_size, const char *source)
{
    size_t length;

    if (destination == NULL || destination_size == 0 || source == NULL)
        return 0;
    length = strlen(source);
    if (length >= destination_size)
        return 0;
    memcpy(destination, source, length + 1);
    return 1;
}

static int normalise_existing_path(const char *source, char *destination, size_t destination_size)
{
    DWORD length;

    if (source == NULL || source[0] == '\0' || destination == NULL ||
        destination_size == 0)
    {
        return 0;
    }
    length = GetFullPathNameA(source, (DWORD)destination_size, destination, NULL);
    if (length == 0 || length >= destination_size)
        return 0;
    while (length > 3 && (destination[length - 1] == '\\' || destination[length - 1] == '/'))
        destination[--length] = '\0';
    return 1;
}

static int relative_path_is_safe(const char *relative_path)
{
    const char *cursor;

    if (relative_path == NULL || relative_path[0] == '\0' ||
        relative_path[0] == '\\' || relative_path[0] == '/' ||
        (isalpha((unsigned char)relative_path[0]) && relative_path[1] == ':'))
    {
        return 0;
    }
    cursor = relative_path;
    while (*cursor != '\0')
    {
        const char *component = cursor;
        size_t length = 0;

        while (*cursor != '\0' && *cursor != '\\' && *cursor != '/')
        {
            cursor++;
            length++;
        }
        if (length == 0 || (length == 1 && component[0] == '.') ||
            (length == 2 && component[0] == '.' && component[1] == '.'))
        {
            return 0;
        }
        if (*cursor != '\0')
            cursor++;
    }
    return 1;
}

static int join_path(
    const char *root,
    const char *relative_path,
    char *out_path,
    size_t out_path_size)
{
    size_t root_length;

    if (root == NULL || root[0] == '\0' || !relative_path_is_safe(relative_path) ||
        out_path == NULL || out_path_size == 0)
    {
        return 0;
    }
    root_length = strlen(root);
    if (snprintf(out_path, out_path_size, "%s%s%s", root,
                 root_length > 0 && root[root_length - 1] == '\\' ? "" : "\\",
                 relative_path) >= (int)out_path_size)
    {
        return 0;
    }
    return 1;
}

static int parent_path(char *path)
{
    char *slash;

    if (path == NULL)
        return 0;
    slash = strrchr(path, '\\');
    if (slash == NULL || slash == path)
        return 0;
    *slash = '\0';
    return 1;
}

static int repository_sentinels_present(const char *root)
{
    char core_configs[MAX_PATH];
    char core_scripts[MAX_PATH];
    char build[MAX_PATH];

    return join_path(root, "core\\configs", core_configs, sizeof(core_configs)) &&
           join_path(root, "core\\scripts", core_scripts, sizeof(core_scripts)) &&
           join_path(root, "build", build, sizeof(build)) &&
           directory_exists(core_configs) && directory_exists(core_scripts) &&
           directory_exists(build);
}

static int package_sentinels_present(const char *root)
{
    char configs[MAX_PATH];
    char bin[MAX_PATH];
    char studio[MAX_PATH];

    return join_path(root, "configs", configs, sizeof(configs)) &&
           join_path(root, "bin", bin, sizeof(bin)) &&
           join_path(root, "bin\\minisnn_studio.exe", studio, sizeof(studio)) &&
           directory_exists(configs) && directory_exists(bin) && file_exists(studio);
}

static int path_equals_case_insensitive(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0')
    {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right))
            return 0;
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

static int executable_is_release_studio(const char *repository_root, const char *executable_path)
{
    char expected[MAX_PATH];

    return join_path(repository_root, "build\\release\\studio\\bin\\minisnn_studio.exe",
                     expected, sizeof(expected)) &&
           path_equals_case_insensitive(expected, executable_path);
}

static int set_repository_layout(
    StudioRuntimeLayout *layout,
    const char *repository_root,
    const char *executable_path)
{
    const char *tools_relative;

    if (!repository_sentinels_present(repository_root) ||
        !copy_path(layout->repository_root, sizeof(layout->repository_root), repository_root) ||
        !join_path(repository_root, "core", layout->core_resource_root,
                   sizeof(layout->core_resource_root)) ||
        !join_path(repository_root, "build", layout->build_root,
                   sizeof(layout->build_root)) ||
        !join_path(repository_root, "core\\results", layout->results_root,
                   sizeof(layout->results_root)) ||
        !copy_path(layout->runtime_root, sizeof(layout->runtime_root), repository_root))
    {
        return 0;
    }
    tools_relative = executable_is_release_studio(repository_root, executable_path) ?
        "build\\release\\tools\\bin" : "build\\tools\\bin";
    if (!join_path(repository_root, tools_relative, layout->tools_bin_root,
                   sizeof(layout->tools_bin_root)))
    {
        return 0;
    }
    layout->packaged_mode = 0;
    return 1;
}

static int set_package_layout(StudioRuntimeLayout *layout, const char *runtime_root)
{
    if (!package_sentinels_present(runtime_root) ||
        !copy_path(layout->runtime_root, sizeof(layout->runtime_root), runtime_root) ||
        !copy_path(layout->core_resource_root, sizeof(layout->core_resource_root), runtime_root) ||
        !join_path(runtime_root, "bin", layout->tools_bin_root,
                   sizeof(layout->tools_bin_root)) ||
        !join_path(runtime_root, "results", layout->results_root,
                   sizeof(layout->results_root)))
    {
        return 0;
    }
    layout->repository_root[0] = '\0';
    layout->build_root[0] = '\0';
    layout->packaged_mode = 1;
    return 1;
}

static int environment_value(const char *name, char *out_path, size_t out_path_size)
{
    DWORD length;

    length = GetEnvironmentVariableA(name, out_path, (DWORD)out_path_size);
    return length > 0 && length < out_path_size;
}

static int discover_layout(
    StudioRuntimeLayout *layout,
    const char *executable_path)
{
    char ancestor[MAX_PATH];

    if (!copy_path(ancestor, sizeof(ancestor), executable_path) || !parent_path(ancestor))
        return 0;
    do
    {
        if (repository_sentinels_present(ancestor) &&
            set_repository_layout(layout, ancestor, executable_path))
        {
            return 1;
        }
        if (package_sentinels_present(ancestor) && set_package_layout(layout, ancestor))
            return 1;
    }
    while (parent_path(ancestor));
    return 0;
}

int studio_runtime_layout_resolve(
    StudioRuntimeLayout *layout,
    const char *executable_path,
    const char *repository_root_override,
    const char *runtime_root_override,
    char *error_message,
    size_t error_message_size)
{
    char executable[MAX_PATH];
    char override[MAX_PATH];
    const char *selected_repository = repository_root_override;
    const char *selected_runtime = runtime_root_override;

    if (layout == NULL || !normalise_existing_path(executable_path, executable,
                                                   sizeof(executable)))
    {
        set_error(error_message, error_message_size, "caminho do executavel invalido");
        return 0;
    }
    if (selected_repository != NULL && selected_repository[0] != '\0' &&
        selected_runtime != NULL && selected_runtime[0] != '\0')
    {
        set_error(error_message, error_message_size,
                  "--repository-root e --runtime-root nao podem ser usados juntos");
        return 0;
    }
    memset(layout, 0, sizeof(*layout));
    if ((selected_repository == NULL || selected_repository[0] == '\0') &&
        (selected_runtime == NULL || selected_runtime[0] == '\0'))
    {
        if (environment_value("MINISNN_REPOSITORY_ROOT", override, sizeof(override)))
            selected_repository = override;
        else if (environment_value("MINISNN_RUNTIME_ROOT", override, sizeof(override)))
            selected_runtime = override;
    }
    if (selected_repository != NULL && selected_repository[0] != '\0')
    {
        if (!normalise_existing_path(selected_repository, override, sizeof(override)) ||
            !set_repository_layout(layout, override, executable))
        {
            set_error(error_message, error_message_size,
                      "repository root nao possui core/configs, core/scripts e build");
            return 0;
        }
        return 1;
    }
    if (selected_runtime != NULL && selected_runtime[0] != '\0')
    {
        if (!normalise_existing_path(selected_runtime, override, sizeof(override)) ||
            !set_package_layout(layout, override))
        {
            set_error(error_message, error_message_size,
                      "runtime root nao possui bin/minisnn_studio.exe e configs");
            return 0;
        }
        return 1;
    }
    if (!discover_layout(layout, executable))
    {
        set_error(error_message, error_message_size,
                  "nenhum layout repository/package reconhecido foi encontrado nos ancestrais");
        return 0;
    }
    return 1;
}

int studio_runtime_layout_resource_path(
    const StudioRuntimeLayout *layout,
    const char *relative_path,
    char *out_path,
    size_t out_path_size)
{
    return layout != NULL && join_path(layout->core_resource_root, relative_path,
                                       out_path, out_path_size);
}

int studio_runtime_layout_results_path(
    const StudioRuntimeLayout *layout,
    const char *relative_path,
    char *out_path,
    size_t out_path_size)
{
    return layout != NULL && join_path(layout->results_root, relative_path,
                                       out_path, out_path_size);
}

int studio_runtime_layout_tool_path(
    const StudioRuntimeLayout *layout,
    const char *filename,
    char *out_path,
    size_t out_path_size)
{
    return layout != NULL && join_path(layout->tools_bin_root, filename,
                                       out_path, out_path_size);
}

static int ensure_results_directory(const char *path)
{
    if (directory_exists(path))
        return 1;
    return CreateDirectoryA(path, NULL) ||
           (GetLastError() == ERROR_ALREADY_EXISTS && directory_exists(path));
}

int studio_runtime_layout_preflight(
    const StudioRuntimeLayout *layout,
    char *error_message,
    size_t error_message_size)
{
    static const char *const required_scripts[] = {
        "analyze_run.py", "compare_neuron_models.py", "compare_runs.py",
        "generate_evolution_report.py", "generate_history_report.py",
        "generate_run_reports.py", "plot_evolution.py", "plot_homeostasis.py",
        "plot_neuron.py", "plot_plasticity.py", "plot_reward.py", "plot_scenario.py"
    };
    char path[MAX_PATH];
    char script_root[MAX_PATH];
    char writable_probe[MAX_PATH];

    path[0] = '\0';

    if (layout == NULL || layout->core_resource_root[0] == '\0')
    {
        set_error(error_message, error_message_size, "layout de runtime invalido");
        return 0;
    }
    if (!studio_runtime_layout_resource_path(layout, "configs\\evolution_weight_target_demo.ini",
                                              path, sizeof(path)) || !file_exists(path))
    {
        set_path_error(error_message, error_message_size,
                       "config evolutiva padrao ausente", path);
        return 0;
    }
    if (!studio_runtime_layout_resource_path(layout, "configs\\evolution_weight_target_base.ini",
                                              path, sizeof(path)) || !file_exists(path))
    {
        set_path_error(error_message, error_message_size,
                       "cenario-base evolutivo ausente", path);
        return 0;
    }
    if (!studio_runtime_layout_tool_path(layout, "minisnn_runner.exe", path, sizeof(path)) ||
        !file_exists(path))
    {
        set_path_error(error_message, error_message_size,
                       "runner de cenarios ausente", path);
        return 0;
    }
    if (!studio_runtime_layout_tool_path(layout, "evolution_runner.exe", path, sizeof(path)) ||
        !file_exists(path))
    {
        set_path_error(error_message, error_message_size,
                       "runner evolutivo ausente", path);
        return 0;
    }
    for (size_t index = 0; index < sizeof(required_scripts) / sizeof(required_scripts[0]); index++)
    {
        if (!studio_runtime_layout_resource_path(layout, "scripts", script_root,
                                                  sizeof(script_root)) ||
            !join_path(script_root, required_scripts[index], path, sizeof(path)) ||
            !file_exists(path))
        {
            set_path_error(error_message, error_message_size,
                           "script obrigatorio ausente", path);
            return 0;
        }
    }
    if (!ensure_results_directory(layout->results_root) ||
        GetTempFileNameA(layout->results_root, "mss", 0, writable_probe) == 0)
    {
        set_error(error_message, error_message_size, "diretorio results nao e gravavel");
        return 0;
    }
    DeleteFileA(writable_probe);
    return 1;
}

const char *studio_runtime_layout_mode_name(const StudioRuntimeLayout *layout)
{
    return layout != NULL && layout->packaged_mode ? "package" : "repository";
}
