#include "g0_visualizer_assets.h"

#include <stdio.h>
#include <string.h>

static const char *const asset_relative_paths[G0_VISUALIZER_ASSET_COUNT] =
{
    "assets/sprites/terrain/terra.png",
    "assets/sprites/terrain/agua.png",
    "assets/sprites/terrain/gramado.png",
    "assets/sprites/objects/pedra.png",
    "assets/sprites/objects/comida.png",
    "assets/sprites/creatures/peixe.png"
};

static const char *const asset_names[G0_VISUALIZER_ASSET_COUNT] =
{
    "terra", "agua", "gramado", "pedra", "comida", "peixe"
};

static int set_error(char *message, size_t size, const char *prefix, const char *value)
{
    if (message != NULL && size > 0U)
    {
        (void)snprintf(message, size, "%s%s", prefix, value);
    }
    return 0;
}

static int png_signature_is_valid(const char *path)
{
    static const unsigned char expected[] =
    {
        0x89U, 0x50U, 0x4EU, 0x47U, 0x0DU, 0x0AU, 0x1AU, 0x0AU
    };
    unsigned char signature[sizeof(expected)];
    FILE *file = fopen(path, "rb");
    int valid = file != NULL && fread(signature, 1U, sizeof(signature), file) ==
        sizeof(signature) && memcmp(signature, expected, sizeof(expected)) == 0;

    if (file != NULL)
    {
        (void)fclose(file);
    }
    return valid;
}

int g0_visualizer_assets_load(
    G0VisualizerAssets *out_assets,
    const char *repository_root,
    char *error_message,
    size_t error_message_size)
{
    size_t index;
    int length;

    if (out_assets == NULL || repository_root == NULL || repository_root[0] == '\0')
    {
        return set_error(error_message, error_message_size,
                         "G0 assets require a repository root: ", "invalid root");
    }
    memset(out_assets, 0, sizeof(*out_assets));
    for (index = 0U; index < G0_VISUALIZER_ASSET_COUNT; ++index)
    {
        length = snprintf(out_assets->path[index], G0_VISUALIZER_ASSET_PATH_MAX,
                          "%s/%s", repository_root, asset_relative_paths[index]);
        if (length < 0 || (size_t)length >= G0_VISUALIZER_ASSET_PATH_MAX)
        {
            return set_error(error_message, error_message_size,
                             "G0 asset path is too long: ", asset_relative_paths[index]);
        }
        if (!png_signature_is_valid(out_assets->path[index]))
        {
            return set_error(error_message, error_message_size,
                             "G0 required PNG is missing or invalid: ",
                             out_assets->path[index]);
        }
    }
    if (error_message != NULL && error_message_size > 0U)
    {
        error_message[0] = '\0';
    }
    return 1;
}

const char *g0_visualizer_asset_name(G0VisualizerAsset asset)
{
    return asset >= 0 && asset < G0_VISUALIZER_ASSET_COUNT ?
        asset_names[asset] : "unknown";
}

const char *g0_visualizer_asset_path(
    const G0VisualizerAssets *assets,
    G0VisualizerAsset asset)
{
    return assets != NULL && asset >= 0 && asset < G0_VISUALIZER_ASSET_COUNT ?
        assets->path[asset] : NULL;
}
