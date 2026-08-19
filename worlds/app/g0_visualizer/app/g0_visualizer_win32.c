#define COBJMACROS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <wincodec.h>
#include <stdio.h>
#include <string.h>

#include "g0_visualizer_runtime.h"

#define G0_WINDOW_CLASS "MiniSNNG0Visualizer"
#define G0_TIMER_ID ((UINT_PTR)1)
#define G0_TIMER_MS ((UINT)16)
#define G0_HEADER_HEIGHT 62
#define G0_FOOTER_HEIGHT 38
#define G0_PANEL_MARGIN 18
#define G0_DEBUG_PANEL_WIDTH 280

typedef struct
{
    HBITMAP bitmap;
    BYTE *pixels;
    int width;
    int height;
} G0WinBitmap;

typedef struct
{
    G0VisualizerRuntime *runtime;
    G0VisualizerAssets assets;
    G0WinBitmap bitmaps[G0_VISUALIZER_ASSET_COUNT];
    G0WinBitmap fish_facing[4];
    LARGE_INTEGER performance_frequency;
    LARGE_INTEGER previous_counter;
    int counter_ready;
} G0WindowApp;

static G0WindowApp g_app;

static int utf8_to_wide(const char *text, WCHAR *out, int out_size)
{
    return text != NULL && out != NULL && out_size > 0 &&
           MultiByteToWideChar(CP_UTF8, 0, text, -1, out, out_size) > 0;
}

static void destroy_bitmap(G0WinBitmap *bitmap)
{
    if (bitmap != NULL && bitmap->bitmap != NULL)
    {
        DeleteObject(bitmap->bitmap);
        bitmap->bitmap = NULL;
        bitmap->pixels = NULL;
        bitmap->width = 0;
        bitmap->height = 0;
    }
}

static int load_png_bitmap(const char *path, G0WinBitmap *out_bitmap)
{
    IWICImagingFactory *factory = NULL;
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICFormatConverter *converter = NULL;
    HDC screen = NULL;
    HBITMAP bitmap = NULL;
    void *pixels = NULL;
    BITMAPINFO info;
    WCHAR wide_path[MAX_PATH];
    UINT width;
    UINT height;
    HRESULT result;
    int success = 0;

    if (path == NULL || out_bitmap == NULL || !utf8_to_wide(path, wide_path, MAX_PATH))
    {
        return 0;
    }
    destroy_bitmap(out_bitmap);
    result = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                              &IID_IWICImagingFactory, (void **)&factory);
    if (FAILED(result))
    {
        goto done;
    }
    result = IWICImagingFactory_CreateDecoderFromFilename(
        factory, wide_path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(result) ||
        FAILED(IWICBitmapDecoder_GetFrame(decoder, 0U, &frame)) ||
        FAILED(IWICImagingFactory_CreateFormatConverter(factory, &converter)) ||
        FAILED(IWICFormatConverter_Initialize(
            converter, (IWICBitmapSource *)frame, &GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom)) ||
        FAILED(IWICBitmapSource_GetSize((IWICBitmapSource *)converter, &width, &height)) ||
        width == 0U || height == 0U)
    {
        goto done;
    }
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = (LONG)width;
    info.bmiHeader.biHeight = -(LONG)height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    screen = GetDC(NULL);
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, NULL, 0U);
    if (bitmap == NULL || pixels == NULL ||
        FAILED(IWICBitmapSource_CopyPixels(
            (IWICBitmapSource *)converter, NULL, width * 4U,
            width * height * 4U, (BYTE *)pixels)))
    {
        goto done;
    }
    out_bitmap->bitmap = bitmap;
    out_bitmap->pixels = (BYTE *)pixels;
    out_bitmap->width = (int)width;
    out_bitmap->height = (int)height;
    bitmap = NULL;
    success = 1;

done:
    if (screen != NULL)
    {
        ReleaseDC(NULL, screen);
    }
    if (bitmap != NULL)
    {
        DeleteObject(bitmap);
    }
    if (converter != NULL)
    {
        IWICFormatConverter_Release(converter);
    }
    if (frame != NULL)
    {
        IWICBitmapFrameDecode_Release(frame);
    }
    if (decoder != NULL)
    {
        IWICBitmapDecoder_Release(decoder);
    }
    if (factory != NULL)
    {
        IWICImagingFactory_Release(factory);
    }
    return success;
}

static int create_rotated_fish_bitmap(
    const G0WinBitmap *source,
    unsigned int quarter_turns,
    G0WinBitmap *out_bitmap)
{
    BITMAPINFO info;
    HDC screen;
    void *pixels = NULL;
    int x;
    int y;

    if (source == NULL || out_bitmap == NULL || source->pixels == NULL ||
        source->width != source->height || source->width <= 0)
    {
        return 0;
    }
    destroy_bitmap(out_bitmap);
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = source->width;
    info.bmiHeader.biHeight = -source->height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    screen = GetDC(NULL);
    if (screen == NULL)
    {
        return 0;
    }
    out_bitmap->bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, NULL, 0U);
    ReleaseDC(NULL, screen);
    if (out_bitmap->bitmap == NULL || pixels == NULL)
    {
        destroy_bitmap(out_bitmap);
        return 0;
    }
    out_bitmap->pixels = (BYTE *)pixels;
    out_bitmap->width = source->width;
    out_bitmap->height = source->height;
    quarter_turns %= 4U;
    for (y = 0; y < source->height; ++y)
    {
        for (x = 0; x < source->width; ++x)
        {
            int destination_x = x;
            int destination_y = y;
            size_t source_offset = ((size_t)y * (size_t)source->width + (size_t)x) * 4U;
            size_t destination_offset;

            if (quarter_turns == 1U)
            {
                destination_x = source->height - 1 - y;
                destination_y = x;
            }
            else if (quarter_turns == 2U)
            {
                destination_x = source->width - 1 - x;
                destination_y = source->height - 1 - y;
            }
            else if (quarter_turns == 3U)
            {
                destination_x = y;
                destination_y = source->width - 1 - x;
            }
            destination_offset = ((size_t)destination_y * (size_t)source->width +
                                  (size_t)destination_x) * 4U;
            memcpy(out_bitmap->pixels + destination_offset,
                   source->pixels + source_offset, 4U);
        }
    }
    return 1;
}

static void destroy_fish_facings(void)
{
    int index;

    for (index = 0; index < 4; ++index)
    {
        destroy_bitmap(&g_app.fish_facing[index]);
    }
}
static int load_all_assets(void)
{
    static const unsigned int fish_turns[4] = { 2U, 3U, 0U, 1U };
    G0VisualizerAsset asset;
    int index;

    for (asset = G0_VISUALIZER_ASSET_DIRT; asset < G0_VISUALIZER_ASSET_COUNT; ++asset)
    {
        if (!load_png_bitmap(g0_visualizer_asset_path(&g_app.assets, asset),
                             &g_app.bitmaps[asset]))
        {
            return 0;
        }
    }
    for (index = 0; index < 4; ++index)
    {
        if (!create_rotated_fish_bitmap(&g_app.bitmaps[G0_VISUALIZER_ASSET_FISH],
                                        fish_turns[index], &g_app.fish_facing[index]))
        {
            destroy_fish_facings();
            return 0;
        }
    }
    return 1;
}

static int directory_parent(char *path)
{
    char *backslash;
    char *slash;
    char *separator;

    if (path == NULL)
    {
        return 0;
    }
    backslash = strrchr(path, '\\');
    slash = strrchr(path, '/');
    separator = backslash;
    if (slash != NULL && (separator == NULL || slash > separator))
    {
        separator = slash;
    }
    if (separator == NULL)
    {
        return 0;
    }
    *separator = '\0';
    return 1;
}

static int repository_root_has_assets(const char *repository_root)
{
    G0VisualizerAssets assets;
    char error_message[G0_VISUALIZER_ASSET_ERROR_MAX];

    return repository_root != NULL &&
           g0_visualizer_assets_load(&assets, repository_root, error_message,
                                     sizeof(error_message));
}

static int resolve_repository_root_from_executable(char *out_root, size_t out_size)
{
    static const char *const relative_candidates[] =
    {
        "..\\..\\..",
        "..\\..\\..\\..\\.."
    };
    char executable[MAX_PATH];
    char executable_directory[MAX_PATH];
    char candidate[MAX_PATH];
    DWORD length;
    size_t index;
    size_t directory_length;
    size_t relative_length;

    if (out_root == NULL || out_size == 0U)
    {
        return 0;
    }
    length = GetModuleFileNameA(NULL, executable, MAX_PATH);
    if (length == 0U || length >= MAX_PATH)
    {
        return 0;
    }
    executable_directory[0] = '\0';
    (void)snprintf(executable_directory, sizeof(executable_directory), "%s", executable);
    if (!directory_parent(executable_directory))
    {
        return 0;
    }
    for (index = 0U; index < sizeof(relative_candidates) / sizeof(relative_candidates[0]);
         ++index)
    {
        directory_length = strlen(executable_directory);
        relative_length = strlen(relative_candidates[index]);
        if (directory_length + 1U + relative_length + 1U > sizeof(candidate))
        {
            continue;
        }
        memcpy(candidate, executable_directory, directory_length);
        candidate[directory_length] = '\\';
        memcpy(candidate + directory_length + 1U, relative_candidates[index],
               relative_length + 1U);
        if (GetFullPathNameA(candidate, (DWORD)out_size, out_root, NULL) != 0U &&
            repository_root_has_assets(out_root))
        {
            return 1;
        }
    }
    out_root[0] = '\0';
    return 0;
}

static void draw_bitmap(HDC target, const G0WinBitmap *bitmap,
                        int x, int y, int width, int height, BYTE alpha)
{
    HDC source;
    HGDIOBJ previous;
    BLENDFUNCTION blend;

    if (target == NULL || bitmap == NULL || bitmap->bitmap == NULL)
    {
        return;
    }
    source = CreateCompatibleDC(target);
    if (source == NULL)
    {
        return;
    }
    previous = SelectObject(source, bitmap->bitmap);
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0U;
    blend.SourceConstantAlpha = alpha;
    blend.AlphaFormat = AC_SRC_ALPHA;
    (void)AlphaBlend(target, x, y, width, height, source, 0, 0,
                     bitmap->width, bitmap->height, blend);
    SelectObject(source, previous);
    DeleteDC(source);
}

static int tile_screen(uint32_t tile_x, uint32_t tile_y, int origin_x, int origin_y,
                       G0VisualizerScreenPoint *out_point)
{
    return g0_visualizer_tile_to_screen(tile_x, tile_y,
                                        origin_x, origin_y, out_point);
}

static void draw_base_terrain(HDC hdc, int origin_x, int origin_y)
{
    const MiniSNNWorldsTerrain *terrain = g0_visualizer_runtime_terrain(g_app.runtime);
    uint32_t x;
    uint32_t y;

    for (y = 0U; terrain != NULL && y < minisnn_worlds_terrain_height(terrain); ++y)
    {
        for (x = 0U; x < minisnn_worlds_terrain_width(terrain); ++x)
        {
            MiniSNNWorldsTerrainTile tile;
            G0VisualizerScreenPoint point;

            if (minisnn_worlds_terrain_get_tile(terrain, x, y, &tile) !=
                    MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
                !tile_screen(x, y, origin_x, origin_y, &point))
            {
                continue;
            }
            draw_bitmap(hdc, &g_app.bitmaps[
                tile == MINISNN_WORLDS_TERRAIN_TILE_LAND ?
                G0_VISUALIZER_ASSET_GRASS : G0_VISUALIZER_ASSET_DIRT],
                point.screen_x, point.screen_y, G0_VISUALIZER_TILE_PIXELS,
                G0_VISUALIZER_TILE_PIXELS, 255U);
        }
    }
}

static void draw_rocks(HDC hdc, int origin_x, int origin_y)
{
    const MiniSNNWorldsTerrain *terrain = g0_visualizer_runtime_terrain(g_app.runtime);
    uint32_t x;
    uint32_t y;

    for (y = 0U; terrain != NULL && y < minisnn_worlds_terrain_height(terrain); ++y)
    {
        for (x = 0U; x < minisnn_worlds_terrain_width(terrain); ++x)
        {
            bool rock = false;
            G0VisualizerScreenPoint point;

            if (minisnn_worlds_terrain_has_rock(terrain, x, y, &rock) !=
                    MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
                !rock || !tile_screen(x, y, origin_x, origin_y, &point))
            {
                continue;
            }
            draw_bitmap(hdc, &g_app.bitmaps[G0_VISUALIZER_ASSET_ROCK],
                        point.screen_x + 16, point.screen_y + 16,
                        G0_VISUALIZER_ENTITY_PIXELS, G0_VISUALIZER_ENTITY_PIXELS, 255U);
        }
    }
}

static const G0WinBitmap *fish_bitmap_for_facing(G0VisualizerFacing facing)
{
    if (facing < G0_VISUALIZER_FACING_NORTH ||
        facing > G0_VISUALIZER_FACING_WEST ||
        g_app.fish_facing[facing].bitmap == NULL)
    {
        return &g_app.bitmaps[G0_VISUALIZER_ASSET_FISH];
    }
    return &g_app.fish_facing[facing];
}
static void draw_entities(HDC hdc, int origin_x, int origin_y)
{
    const WF0FishWorldState *state = g0_visualizer_runtime_state(g_app.runtime);
    G0VisualizerScreenPoint fish;
    G0VisualizerScreenPoint food;
    MiniSNNWorldsKernelPosition food_position;

    if (state == NULL)
    {
        return;
    }
    food_position.x = 1000;
    food_position.y = 0;
    if (g0_visualizer_runtime_food_visible(g_app.runtime) &&
        g0_visualizer_world_to_screen(g_app.runtime, food_position,
                                      origin_x, origin_y, &food))
    {
        draw_bitmap(hdc, &g_app.bitmaps[G0_VISUALIZER_ASSET_FOOD],
                    food.screen_x + 16, food.screen_y + 16,
                    G0_VISUALIZER_ENTITY_PIXELS, G0_VISUALIZER_ENTITY_PIXELS, 255U);
    }
    if (g0_visualizer_world_to_screen(g_app.runtime, state->actor_transform.position,
                                      origin_x, origin_y, &fish))
    {
        draw_bitmap(hdc, fish_bitmap_for_facing(g0_visualizer_runtime_facing(g_app.runtime)),
                    fish.screen_x + 16, fish.screen_y + 16,
                    G0_VISUALIZER_ENTITY_PIXELS, G0_VISUALIZER_ENTITY_PIXELS, 255U);
    }
}

static void draw_water_overlay(HDC hdc, int origin_x, int origin_y)
{
    const MiniSNNWorldsTerrain *terrain = g0_visualizer_runtime_terrain(g_app.runtime);
    uint32_t x;
    uint32_t y;

    for (y = 0U; terrain != NULL && y < minisnn_worlds_terrain_height(terrain); ++y)
    {
        for (x = 0U; x < minisnn_worlds_terrain_width(terrain); ++x)
        {
            MiniSNNWorldsTerrainTile tile;
            G0VisualizerScreenPoint point;

            if (minisnn_worlds_terrain_get_tile(terrain, x, y, &tile) !=
                    MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
                tile != MINISNN_WORLDS_TERRAIN_TILE_WATER ||
                !tile_screen(x, y, origin_x, origin_y, &point))
            {
                continue;
            }
            draw_bitmap(hdc, &g_app.bitmaps[G0_VISUALIZER_ASSET_WATER],
                        point.screen_x, point.screen_y, G0_VISUALIZER_TILE_PIXELS,
                        G0_VISUALIZER_TILE_PIXELS, 128U);
        }
    }
}

static void fill_color(HDC hdc, const RECT *rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);

    if (brush != NULL)
    {
        FillRect(hdc, rect, brush);
        DeleteObject(brush);
    }
}

static void stroke_panel(HDC hdc, const RECT *rect)
{
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(78, 92, 106));
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;

    if (pen == NULL)
    {
        return;
    }
    old_pen = SelectObject(hdc, pen);
    old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rect->left, rect->top, rect->right, rect->bottom);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static void draw_grid(HDC hdc, int origin_x, int origin_y)
{
    const MiniSNNWorldsTerrain *terrain = g0_visualizer_runtime_terrain(g_app.runtime);
    HPEN pen;
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;
    uint32_t x;
    uint32_t y;

    if (!g0_visualizer_runtime_grid_enabled(g_app.runtime) || terrain == NULL)
    {
        return;
    }
    pen = CreatePen(PS_SOLID, 1, RGB(65, 90, 108));
    if (pen == NULL)
    {
        return;
    }
    old_pen = SelectObject(hdc, pen);
    old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    for (y = 0U; y < minisnn_worlds_terrain_height(terrain); ++y)
    {
        for (x = 0U; x < minisnn_worlds_terrain_width(terrain); ++x)
        {
            G0VisualizerScreenPoint point;
            if (tile_screen(x, y, origin_x, origin_y, &point))
            {
                Rectangle(hdc, point.screen_x, point.screen_y,
                          point.screen_x + G0_VISUALIZER_TILE_PIXELS,
                          point.screen_y + G0_VISUALIZER_TILE_PIXELS);
            }
        }
    }
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static void draw_text(HDC hdc, COLORREF color, int x, int y, const char *text)
{
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    (void)TextOutA(hdc, x, y, text, (int)strlen(text));
}

static void draw_ui(HDC hdc, const RECT *client, const RECT *debug_panel,
                    int origin_x, int origin_y)
{
    const WF0FishWorldState *state = g0_visualizer_runtime_state(g_app.runtime);
    const WF0FishTickRecord *record = g0_visualizer_runtime_last_record(g_app.runtime);
    const MiniSNNWorldsTerrain *terrain = g0_visualizer_runtime_terrain(g_app.runtime);
    RECT header = { 0, 0, client->right, G0_HEADER_HEIGHT };
    RECT footer = { 0, client->bottom - G0_FOOTER_HEIGHT, client->right, client->bottom };
    char text[256];
    int y;
    uint32_t tile_x = 0U;
    uint32_t tile_y = 0U;
    uint64_t terrain_hash = 0U;

    fill_color(hdc, &header, RGB(24, 31, 38));
    stroke_panel(hdc, &header);
    draw_text(hdc, RGB(242, 246, 250), G0_PANEL_MARGIN, 18, "miniSNN Worlds");
    (void)snprintf(text, sizeof(text), "%s   tick: %llu",
                   g0_visualizer_runtime_is_paused(g_app.runtime) ? "PAUSED" : "RUNNING",
                   state == NULL ? 0ULL : (unsigned long long)state->tick);
    draw_text(hdc,
              g0_visualizer_runtime_is_paused(g_app.runtime) ? RGB(243, 188, 92) : RGB(119, 225, 164),
              client->right - 210, 18, text);
    fill_color(hdc, &footer, RGB(24, 31, 38));
    stroke_panel(hdc, &footer);
    draw_text(hdc, RGB(184, 197, 208), G0_PANEL_MARGIN, footer.top + 10,
              "SPACE pause | N single step | G grid | D debug | R reset | ESC close");
    draw_grid(hdc, origin_x, origin_y);

    if (!g0_visualizer_runtime_debug_enabled(g_app.runtime) || debug_panel == NULL)
    {
        return;
    }
    fill_color(hdc, debug_panel, RGB(24, 31, 38));
    stroke_panel(hdc, debug_panel);
    y = debug_panel->top + 16;
    draw_text(hdc, RGB(242, 246, 250), debug_panel->left + 14, y, "DEBUG PANEL");
    y += 32;
    draw_text(hdc, RGB(184, 197, 208), debug_panel->left + 14, y, "SIMULATION");
    y += 22;
    (void)snprintf(text, sizeof(text), "Tick: %llu",
                   state == NULL ? 0ULL : (unsigned long long)state->tick);
    draw_text(hdc, RGB(242, 246, 250), debug_panel->left + 14, y, text);
    y += 20;
    draw_text(hdc,
              g0_visualizer_runtime_is_paused(g_app.runtime) ? RGB(243, 188, 92) : RGB(119, 225, 164),
              debug_panel->left + 14, y,
              g0_visualizer_runtime_is_paused(g_app.runtime) ? "State: PAUSED" : "State: RUNNING");
    y += 20;
    (void)snprintf(text, sizeof(text), "ticks/s: %u",
                   g0_visualizer_runtime_ticks_per_second(g_app.runtime));
    draw_text(hdc, RGB(184, 197, 208), debug_panel->left + 14, y, text);
    y += 28;
    draw_text(hdc, RGB(184, 197, 208), debug_panel->left + 14, y, "FISH");
    y += 22;
    if (state != NULL && terrain != NULL)
    {
        (void)minisnn_worlds_terrain_world_to_tile(terrain, state->actor_transform.position,
                                                    &tile_x, &tile_y);
        (void)snprintf(text, sizeof(text), "Tile: (%u, %u)", tile_x, tile_y);
        draw_text(hdc, RGB(242, 246, 250), debug_panel->left + 14, y, text);
        y += 20;
        (void)snprintf(text, sizeof(text), "World: (%lld, %lld)",
                       (long long)state->actor_transform.position.x,
                       (long long)state->actor_transform.position.y);
        draw_text(hdc, RGB(242, 246, 250), debug_panel->left + 14, y, text);
        y += 20;
        (void)snprintf(text, sizeof(text), "Energy: %llu", (unsigned long long)state->actor_energy);
        draw_text(hdc, RGB(242, 246, 250), debug_panel->left + 14, y, text);
        y += 20;
        (void)snprintf(text, sizeof(text), "Facing: %s",
                       g0_visualizer_facing_name(g0_visualizer_runtime_facing(g_app.runtime)));
        draw_text(hdc, RGB(242, 246, 250), debug_panel->left + 14, y, text);
        y += 20;
        (void)snprintf(text, sizeof(text), "Core step: %d", state->core_step);
        draw_text(hdc, RGB(242, 246, 250), debug_panel->left + 14, y, text);
        y += 30;
        draw_text(hdc, RGB(184, 197, 208), debug_panel->left + 14, y, "WORLD");
        y += 22;
        (void)snprintf(text, sizeof(text), "Food remaining: %llu",
                       (unsigned long long)state->remaining_food);
        draw_text(hdc, RGB(242, 246, 250), debug_panel->left + 14, y, text);
        y += 20;
        (void)snprintf(text, sizeof(text), "Kernel: %llu", (unsigned long long)state->kernel_hash);
        draw_text(hdc, RGB(184, 197, 208), debug_panel->left + 14, y, text);
        y += 20;
        (void)snprintf(text, sizeof(text), "Domain: %llu", (unsigned long long)state->domain_hash);
        draw_text(hdc, RGB(184, 197, 208), debug_panel->left + 14, y, text);
        y += 20;
        if (minisnn_worlds_terrain_hash(terrain, &terrain_hash) ==
            MINISNN_WORLDS_TERRAIN_ERROR_NONE)
        {
            (void)snprintf(text, sizeof(text), "Terrain: %llu",
                           (unsigned long long)terrain_hash);
            draw_text(hdc, RGB(184, 197, 208), debug_panel->left + 14, y, text);
            y += 20;
        }
        y += 10;
    }
    if (record != NULL)
    {
        if (record->decision.action.type == MINISNN_WORLDS_DOMAIN_ACTION_MOVE)
        {
            (void)snprintf(text, sizeof(text), "Action: MOVE %s%s",
                           g0_visualizer_facing_name(
                               g0_visualizer_facing_from_move_delta(
                                   G0_VISUALIZER_FACING_SOUTH,
                                   record->decision.action.move_delta)),
                           record->action_result.status ==
                               MINISNN_WORLDS_DOMAIN_ACTION_REJECTED ?
                               " (BLOCKED)" : "");
        }
        else
        {
            (void)snprintf(text, sizeof(text), "Action: %s%s",
                           g0_visualizer_action_name(record->decision.action.type),
                           record->action_result.status ==
                               MINISNN_WORLDS_DOMAIN_ACTION_REJECTED ?
                               " (BLOCKED)" : "");
        }
        draw_text(hdc, RGB(242, 246, 250), debug_panel->left + 14, y, text);
    }
}

static void render_scene(HWND window, HDC hdc)
{
    RECT client;
    RECT world_panel;
    RECT debug_panel;
    const int map_width = G0_VISUALIZER_TILE_PIXELS * 5;
    const int map_height = G0_VISUALIZER_TILE_PIXELS * 5;
    int origin_x;
    int origin_y;
    int debug_visible;

    GetClientRect(window, &client);
    fill_color(hdc, &client, RGB(14, 18, 23));
    debug_visible = g0_visualizer_runtime_debug_enabled(g_app.runtime);
    world_panel.left = G0_PANEL_MARGIN;
    world_panel.top = G0_HEADER_HEIGHT + G0_PANEL_MARGIN;
    world_panel.right = client.right - G0_PANEL_MARGIN;
    world_panel.bottom = client.bottom - G0_FOOTER_HEIGHT - G0_PANEL_MARGIN;
    if (debug_visible)
    {
        debug_panel.right = client.right - G0_PANEL_MARGIN;
        debug_panel.left = debug_panel.right - G0_DEBUG_PANEL_WIDTH;
        debug_panel.top = world_panel.top;
        debug_panel.bottom = world_panel.bottom;
        world_panel.right = debug_panel.left - G0_PANEL_MARGIN;
    }
    fill_color(hdc, &world_panel, RGB(20, 27, 34));
    stroke_panel(hdc, &world_panel);
    origin_x = world_panel.left + (world_panel.right - world_panel.left - map_width) / 2;
    origin_y = world_panel.top + (world_panel.bottom - world_panel.top - map_height) / 2;
    draw_base_terrain(hdc, origin_x, origin_y);
    draw_rocks(hdc, origin_x, origin_y);
    draw_entities(hdc, origin_x, origin_y);
    draw_water_overlay(hdc, origin_x, origin_y);
    draw_ui(hdc, &client, debug_visible ? &debug_panel : NULL, origin_x, origin_y);
}

static uint32_t elapsed_since_last_timer(void)
{
    LARGE_INTEGER counter;
    LONGLONG delta;

    if (!QueryPerformanceCounter(&counter) || g_app.performance_frequency.QuadPart <= 0)
    {
        return 0U;
    }
    if (!g_app.counter_ready)
    {
        g_app.previous_counter = counter;
        g_app.counter_ready = 1;
        return 0U;
    }
    delta = counter.QuadPart - g_app.previous_counter.QuadPart;
    g_app.previous_counter = counter;
    return delta <= 0 ? 0U : (uint32_t)(((uint64_t)delta * UINT64_C(1000)) /
                                         (uint64_t)g_app.performance_frequency.QuadPart);
}
static LRESULT CALLBACK g0_window_proc(HWND window, UINT message,
                                        WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
        case WM_TIMER:
            if (w_param == G0_TIMER_ID &&
                !g0_visualizer_runtime_advance_elapsed(g_app.runtime,
                                                        elapsed_since_last_timer()))
            {
                MessageBoxA(window, "The G0 simulation tick failed.", "miniSNN Worlds",
                            MB_OK | MB_ICONERROR);
                KillTimer(window, G0_TIMER_ID);
            }
            InvalidateRect(window, NULL, FALSE);
            return 0;
        case WM_KEYDOWN:
            if (w_param == VK_ESCAPE)
            {
                DestroyWindow(window);
            }
            else if (w_param == VK_SPACE)
            {
                g0_visualizer_runtime_toggle_paused(g_app.runtime);
            }
            else if (w_param == 'N' && g0_visualizer_runtime_is_paused(g_app.runtime))
            {
                (void)g0_visualizer_runtime_step_once(g_app.runtime);
            }
            else if (w_param == 'G')
            {
                g0_visualizer_runtime_toggle_grid(g_app.runtime);
            }
            else if (w_param == 'D')
            {
                g0_visualizer_runtime_toggle_debug(g_app.runtime);
            }
            else if (w_param == 'R')
            {
                if (g0_visualizer_runtime_reset(g_app.runtime))
                {
                    g_app.counter_ready = 0;
                }
            }
            InvalidateRect(window, NULL, FALSE);
            return 0;
        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC hdc = BeginPaint(window, &paint);
            render_scene(window, hdc);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_DESTROY:
            KillTimer(window, G0_TIMER_ID);
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcA(window, message, w_param, l_param);
    }
}

static void shutdown_visualizer(void)
{
    int index;

    destroy_fish_facings();

    for (index = 0; index < G0_VISUALIZER_ASSET_COUNT; ++index)
    {
        destroy_bitmap(&g_app.bitmaps[index]);
    }
    g0_visualizer_runtime_destroy(&g_app.runtime);
    OleUninitialize();
}

static int initialize_visualizer(const char *repository_root, char *error_message,
                                 size_t error_message_size)
{
    HRESULT result;

    if (repository_root == NULL || repository_root[0] == '\0')
    {
        (void)snprintf(error_message, error_message_size,
                       "Unable to resolve repository root relative to this executable.");
        return 0;
    }
    result = OleInitialize(NULL);
    if (FAILED(result))
    {
        (void)snprintf(error_message, error_message_size,
                       "Unable to initialize COM for PNG decoding.");
        return 0;
    }
    if (!g0_visualizer_assets_load(&g_app.assets, repository_root, error_message,
                                   error_message_size) ||
        !load_all_assets() ||
        (g_app.runtime = g0_visualizer_runtime_create(0)) == NULL ||
        !QueryPerformanceFrequency(&g_app.performance_frequency))
    {
        if (error_message[0] == '\0')
        {
            (void)snprintf(error_message, error_message_size,
                           "Unable to initialize G0 visualizer runtime or PNG assets.");
        }
        shutdown_visualizer();
        return 0;
    }
    return 1;
}

static int resolve_requested_repository_root(const char *command_line,
                                             char *out_root, size_t out_size,
                                             char *error_message,
                                             size_t error_message_size)
{
    char requested_root[MAX_PATH];
    DWORD length;

    if (command_line == NULL || out_root == NULL || out_size == 0U)
    {
        return 0;
    }
    requested_root[0] = '\0';
    if (sscanf(command_line, "--repository-root \"%259[^\"]\"", requested_root) != 1)
    {
        (void)sscanf(command_line, "--repository-root %259s", requested_root);
    }
    if (requested_root[0] == '\0')
    {
        if (resolve_repository_root_from_executable(out_root, out_size))
        {
            return 1;
        }
        (void)snprintf(error_message, error_message_size,
                       "Unable to locate assets relative to the Worlds executable. "
                       "Use --repository-root with the repository path.");
        return 0;
    }
    length = GetFullPathNameA(requested_root, (DWORD)out_size, out_root, NULL);
    if (length == 0U || length >= out_size || !repository_root_has_assets(out_root))
    {
        (void)snprintf(error_message, error_message_size,
                       "Unable to locate G0 assets below --repository-root: %s",
                       requested_root);
        out_root[0] = '\0';
        return 0;
    }
    return 1;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show)
{
    WNDCLASSA window_class;
    MSG message;
    HWND window;
    char repository_root[MAX_PATH];
    char asset_error[G0_VISUALIZER_ASSET_ERROR_MAX];
    int runtime_smoke;

    (void)previous;
    ZeroMemory(&g_app, sizeof(g_app));
    repository_root[0] = '\0';
    asset_error[0] = '\0';
    runtime_smoke = command_line != NULL && strstr(command_line, "--runtime-smoke") != NULL;
    if (!resolve_requested_repository_root(command_line, repository_root, sizeof(repository_root),
                                           asset_error, sizeof(asset_error)) ||
        !initialize_visualizer(repository_root, asset_error, sizeof(asset_error)))
    {
        if (!runtime_smoke)
        {
            MessageBoxA(NULL, asset_error[0] == '\0' ?
                        "Unable to initialize G0 visualizer runtime or PNG assets." : asset_error,
                        "miniSNN Worlds", MB_OK | MB_ICONERROR);
        }
        return 1;
    }
    if (runtime_smoke)
    {
        shutdown_visualizer();
        return 0;
    }
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.lpfnWndProc = g0_window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = NULL;
    window_class.lpszClassName = G0_WINDOW_CLASS;
    if (RegisterClassA(&window_class) == 0U)
    {
        MessageBoxA(NULL, "Unable to register G0 window class.", "miniSNN Worlds",
                    MB_OK | MB_ICONERROR);
        shutdown_visualizer();
        return 1;
    }
    window = CreateWindowExA(0U, G0_WINDOW_CLASS, "miniSNN Worlds - G0-B",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             760, 560, NULL, NULL, instance, NULL);
    if (window == NULL)
    {
        MessageBoxA(NULL, "Unable to create G0 visualizer window.", "miniSNN Worlds",
                    MB_OK | MB_ICONERROR);
        shutdown_visualizer();
        return 1;
    }
    ShowWindow(window, show);
    UpdateWindow(window);
    (void)SetTimer(window, G0_TIMER_ID, G0_TIMER_MS, NULL);
    while (GetMessageA(&message, NULL, 0U, 0U) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    shutdown_visualizer();
    return (int)message.wParam;
}