#ifndef MINISNN_STUDIO_RUNTIME_LAYOUT_H
#define MINISNN_STUDIO_RUNTIME_LAYOUT_H

#include <stddef.h>
#include <windows.h>

typedef struct
{
    char repository_root[MAX_PATH];
    char core_resource_root[MAX_PATH];
    char build_root[MAX_PATH];
    char tools_bin_root[MAX_PATH];
    char results_root[MAX_PATH];
    char runtime_root[MAX_PATH];
    int packaged_mode;
} StudioRuntimeLayout;

int studio_runtime_layout_resolve(
    StudioRuntimeLayout *layout,
    const char *executable_path,
    const char *repository_root_override,
    const char *runtime_root_override,
    char *error_message,
    size_t error_message_size);

int studio_runtime_layout_resource_path(
    const StudioRuntimeLayout *layout,
    const char *relative_path,
    char *out_path,
    size_t out_path_size);

int studio_runtime_layout_results_path(
    const StudioRuntimeLayout *layout,
    const char *relative_path,
    char *out_path,
    size_t out_path_size);

int studio_runtime_layout_tool_path(
    const StudioRuntimeLayout *layout,
    const char *filename,
    char *out_path,
    size_t out_path_size);

int studio_runtime_layout_preflight(
    const StudioRuntimeLayout *layout,
    char *error_message,
    size_t error_message_size);

const char *studio_runtime_layout_mode_name(const StudioRuntimeLayout *layout);

#endif
