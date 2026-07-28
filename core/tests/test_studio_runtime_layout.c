#include <stdio.h>
#include <string.h>

#include "studio_runtime_layout.h"

#define TEST_ERROR_SIZE 512

static const char *const required_scripts[] = {
    "analyze_run.py", "compare_neuron_models.py", "compare_runs.py",
    "generate_evolution_report.py", "generate_history_report.py",
    "generate_run_reports.py", "plot_evolution.py", "plot_homeostasis.py",
    "plot_neuron.py", "plot_plasticity.py", "plot_reward.py", "plot_scenario.py"
};

static int join_path(
    const char *root,
    const char *tail,
    char *out_path,
    size_t out_path_size)
{
    return snprintf(out_path, out_path_size, "%s\\%s", root, tail) <
           (int)out_path_size;
}

static int make_directory(const char *path)
{
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static int make_tree(const char *root, const char *relative_path)
{
    char path[MAX_PATH];
    char *cursor;

    if (!join_path(root, relative_path, path, sizeof(path)))
        return 0;
    for (cursor = path + strlen(root) + 1; *cursor != '\0'; cursor++)
    {
        if (*cursor == '\\')
        {
            *cursor = '\0';
            if (!make_directory(path))
                return 0;
            *cursor = '\\';
        }
    }
    return make_directory(path);
}

static int write_file(const char *path)
{
    FILE *file = fopen(path, "wb");

    if (file == NULL)
        return 0;
    fputs("runtime layout test\n", file);
    return fclose(file) == 0;
}

static int make_file(const char *root, const char *relative_path)
{
    char parent[MAX_PATH];
    char path[MAX_PATH];
    char *slash;

    if (!join_path(root, relative_path, path, sizeof(path)))
        return 0;
    snprintf(parent, sizeof(parent), "%s", path);
    slash = strrchr(parent, '\\');
    if (slash == NULL)
        return 0;
    *slash = '\0';
    return make_tree(root, parent + strlen(root) + 1) && write_file(path);
}

static int make_runtime_resources(const char *root, int repository_mode)
{
    const char *prefix = repository_mode ? "core\\" : "";
    char relative[MAX_PATH];
    size_t index;

    if (snprintf(relative, sizeof(relative), "%sconfigs\\evolution_weight_target_demo.ini",
                 prefix) >= (int)sizeof(relative) || !make_file(root, relative) ||
        snprintf(relative, sizeof(relative), "%sconfigs\\evolution_weight_target_base.ini",
                 prefix) >= (int)sizeof(relative) || !make_file(root, relative))
    {
        return 0;
    }
    for (index = 0; index < sizeof(required_scripts) / sizeof(required_scripts[0]); index++)
    {
        if (snprintf(relative, sizeof(relative), "%sscripts\\%s", prefix,
                     required_scripts[index]) >= (int)sizeof(relative) ||
            !make_file(root, relative))
        {
            return 0;
        }
    }
    return make_tree(root, repository_mode ? "core\\results" : "results");
}

static int assert_true(int condition, const char *message)
{
    if (condition)
        return 1;
    fprintf(stderr, "FAILED: %s\n", message);
    return 0;
}

static int resolve_layout(
    StudioRuntimeLayout *layout,
    const char *executable,
    const char *repository_root,
    const char *runtime_root)
{
    char error[TEST_ERROR_SIZE];

    if (studio_runtime_layout_resolve(layout, executable, repository_root,
                                      runtime_root, error, sizeof(error)))
    {
        return 1;
    }
    fprintf(stderr, "resolve failed: %s\n", error);
    return 0;
}

static int preflight_layout(const StudioRuntimeLayout *layout)
{
    char error[TEST_ERROR_SIZE];

    return studio_runtime_layout_preflight(layout, error, sizeof(error));
}

int main(void)
{
    char temporary_directory[MAX_PATH];
    char temporary_name[MAX_PATH];
    char repository[MAX_PATH];
    char package[MAX_PATH];
    char debug_executable[MAX_PATH];
    char release_executable[MAX_PATH];
    char package_executable[MAX_PATH];
    char expected[MAX_PATH];
    char output[MAX_PATH];
    char previous_directory[MAX_PATH];
    char original_repository_environment[MAX_PATH];
    StudioRuntimeLayout layout;
    DWORD original_repository_environment_length;
    int ok = 1;

    if (GetTempPathA(sizeof(temporary_directory), temporary_directory) == 0 ||
        GetTempFileNameA(temporary_directory, "mss", 0, temporary_name) == 0)
    {
        fprintf(stderr, "FAILED: temporary directory unavailable\n");
        return 1;
    }
    DeleteFileA(temporary_name);
    if (!make_directory(temporary_name) ||
        !join_path(temporary_name, "repository with spaces", repository,
                   sizeof(repository)) ||
        !join_path(temporary_name, "package with spaces", package, sizeof(package)) ||
        !make_directory(repository) || !make_directory(package) ||
        !make_tree(repository, "build") ||
        !make_runtime_resources(repository, 1) ||
        !make_file(repository, "build\\studio\\bin\\minisnn_studio.exe") ||
        !make_file(repository, "build\\release\\studio\\bin\\minisnn_studio.exe") ||
        !make_file(repository, "build\\tools\\bin\\minisnn_runner.exe") ||
        !make_file(repository, "build\\tools\\bin\\evolution_runner.exe") ||
        !make_file(repository, "build\\release\\tools\\bin\\minisnn_runner.exe") ||
        !make_file(repository, "build\\release\\tools\\bin\\evolution_runner.exe") ||
        !make_runtime_resources(package, 0) ||
        !make_file(package, "bin\\minisnn_studio.exe") ||
        !make_file(package, "bin\\minisnn_runner.exe") ||
        !make_file(package, "bin\\evolution_runner.exe"))
    {
        fprintf(stderr, "FAILED: could not create runtime fixtures\n");
        return 1;
    }

    join_path(repository, "build\\studio\\bin\\minisnn_studio.exe",
              debug_executable, sizeof(debug_executable));
    join_path(repository, "build\\release\\studio\\bin\\minisnn_studio.exe",
              release_executable, sizeof(release_executable));
    join_path(package, "bin\\minisnn_studio.exe", package_executable,
              sizeof(package_executable));

    ok &= assert_true(resolve_layout(&layout, debug_executable, repository, NULL),
                      "explicit repository debug layout");
    join_path(repository, "core", expected, sizeof(expected));
    ok &= assert_true(strcmp(layout.core_resource_root, expected) == 0,
                      "repository core resource root");
    join_path(repository, "build\\tools\\bin", expected, sizeof(expected));
    ok &= assert_true(strcmp(layout.tools_bin_root, expected) == 0,
                      "debug tools path");
    ok &= assert_true(preflight_layout(&layout), "debug preflight");

    ok &= assert_true(resolve_layout(&layout, release_executable, repository, NULL),
                      "explicit repository release layout");
    join_path(repository, "build\\release\\tools\\bin", expected, sizeof(expected));
    ok &= assert_true(strcmp(layout.tools_bin_root, expected) == 0,
                      "release tools path");
    ok &= assert_true(preflight_layout(&layout), "release preflight");

    ok &= assert_true(resolve_layout(&layout, package_executable, NULL, package),
                      "explicit package layout");
    ok &= assert_true(layout.packaged_mode && strcmp(layout.runtime_root, package) == 0,
                      "package mode and runtime root");
    join_path(package, "bin", expected, sizeof(expected));
    ok &= assert_true(strcmp(layout.tools_bin_root, expected) == 0,
                      "package tools path");
    ok &= assert_true(preflight_layout(&layout), "package preflight");

    GetCurrentDirectoryA(sizeof(previous_directory), previous_directory);
    SetCurrentDirectoryA(temporary_directory);
    ok &= assert_true(resolve_layout(&layout, release_executable, NULL, NULL),
                      "repository autodetection from unrelated cwd");
    ok &= assert_true(resolve_layout(&layout, package_executable, NULL, NULL),
                      "package autodetection from unrelated cwd");
    SetCurrentDirectoryA(previous_directory);

    original_repository_environment_length = GetEnvironmentVariableA(
        "MINISNN_REPOSITORY_ROOT",
        original_repository_environment,
        sizeof(original_repository_environment));
    SetEnvironmentVariableA("MINISNN_REPOSITORY_ROOT", repository);
    ok &= assert_true(resolve_layout(&layout, debug_executable, NULL, NULL),
                      "repository environment override");
    if (original_repository_environment_length > 0 &&
        original_repository_environment_length < sizeof(original_repository_environment))
    {
        SetEnvironmentVariableA("MINISNN_REPOSITORY_ROOT",
                                original_repository_environment);
    }
    else
    {
        SetEnvironmentVariableA("MINISNN_REPOSITORY_ROOT", NULL);
    }

    ok &= assert_true(resolve_layout(&layout, package_executable, NULL, package),
                      "restore package layout for preflight failures");

    ok &= assert_true(!studio_runtime_layout_resource_path(
                          &layout, "configs\\..\\outside.ini", output, sizeof(output)),
                      "resource traversal rejected");
    ok &= assert_true(!studio_runtime_layout_results_path(
                          &layout, "..\\outside.csv", output, sizeof(output)),
                      "results traversal rejected");

    join_path(package, "configs\\evolution_weight_target_demo.ini", expected,
              sizeof(expected));
    DeleteFileA(expected);
    ok &= assert_true(!preflight_layout(&layout), "missing default config rejected");
    ok &= assert_true(make_file(package, "configs\\evolution_weight_target_demo.ini"),
                      "restore default config");
    join_path(package, "bin\\evolution_runner.exe", expected, sizeof(expected));
    DeleteFileA(expected);
    ok &= assert_true(!preflight_layout(&layout), "missing runner rejected");
    ok &= assert_true(make_file(package, "bin\\evolution_runner.exe"),
                      "restore evolution runner");
    join_path(package, "scripts\\plot_scenario.py", expected, sizeof(expected));
    DeleteFileA(expected);
    ok &= assert_true(!preflight_layout(&layout), "missing script rejected");
    ok &= assert_true(make_file(package, "scripts\\plot_scenario.py"),
                      "restore script");
    ok &= assert_true(preflight_layout(&layout), "restored package preflight");

    if (!ok)
        return 1;
    printf("Studio runtime layout validation OK\n");
    return 0;
}
