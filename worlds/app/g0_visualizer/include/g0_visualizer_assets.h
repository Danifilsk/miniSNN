#ifndef G0_VISUALIZER_ASSETS_H
#define G0_VISUALIZER_ASSETS_H

#include <stddef.h>

typedef enum
{
    G0_VISUALIZER_ASSET_DIRT = 0,
    G0_VISUALIZER_ASSET_WATER,
    G0_VISUALIZER_ASSET_GRASS,
    G0_VISUALIZER_ASSET_ROCK,
    G0_VISUALIZER_ASSET_FOOD,
    G0_VISUALIZER_ASSET_FISH,
    G0_VISUALIZER_ASSET_COUNT
} G0VisualizerAsset;

#define G0_VISUALIZER_ASSET_PATH_MAX 512U
#define G0_VISUALIZER_ASSET_ERROR_MAX 256U

typedef struct
{
    char path[G0_VISUALIZER_ASSET_COUNT][G0_VISUALIZER_ASSET_PATH_MAX];
} G0VisualizerAssets;

/* Resolves and validates every required PNG below repository_root/assets. */
int g0_visualizer_assets_load(
    G0VisualizerAssets *out_assets,
    const char *repository_root,
    char *error_message,
    size_t error_message_size);

const char *g0_visualizer_asset_name(G0VisualizerAsset asset);
const char *g0_visualizer_asset_path(
    const G0VisualizerAssets *assets,
    G0VisualizerAsset asset);

#endif
