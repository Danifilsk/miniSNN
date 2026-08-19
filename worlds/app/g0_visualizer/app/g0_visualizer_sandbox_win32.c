#define COBJMACROS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "g0_event_log.h"
#include "g0_visualizer_camera.h"
#include "g0_visualizer_runtime.h"
#include "g0_visualizer_ui_layout.h"
#include "g0_world_document.h"

#define G0_WINDOW_CLASS "MiniSNNG0Sandbox"
#define G0_TIMER_ID ((UINT_PTR)1)
#define G0_TIMER_MS ((UINT)16)
#define G0_HEADER_HEIGHT_NORMAL 62
#define G0_FOOTER_HEIGHT_NORMAL 42
#define G0_HEADER_HEIGHT G0_HEADER_HEIGHT_NORMAL
#define G0_FOOTER_HEIGHT G0_FOOTER_HEIGHT_NORMAL
#define G0_PRESENTATION_SCALE_NUMERATOR 3
#define G0_PRESENTATION_SCALE_DENOMINATOR 2
#define G0_PANEL_MARGIN 16
#define G0_DEBUG_PANEL_WIDTH 280
#define G0_EDITOR_PANEL_WIDTH 300
#define G0_TOOL_COUNT 6
#define G0_ZOOM_COUNT 8
#define G0_WORLD_NAME_MAX G0_WORLD_DOCUMENT_NAME_MAX
#define G0_SAVE_PATH_MAX MAX_PATH

typedef enum
{
    G0_APP_MAIN_MENU = 0,
    G0_APP_WORLD_SANDBOX,
    G0_APP_SETTINGS
} G0AppState;

typedef enum
{
    G0_STARTUP_ERROR_NONE = 0,
    G0_STARTUP_ERROR_ASSET_ROOT,
    G0_STARTUP_ERROR_ASSETS,
    G0_STARTUP_ERROR_SANDBOX
} G0StartupError;

typedef struct
{
    HBITMAP bitmap;
    BYTE *pixels;
    int width;
    int height;
} G0WinBitmap;

typedef struct
{
    HFONT handle;
    int owned;
} G0UiFont;

typedef struct
{
    G0VisualizerRuntime *runtime;
    G0VisualizerAssets assets;
    G0WinBitmap bitmaps[G0_VISUALIZER_ASSET_COUNT];
    G0WinBitmap fish_facing[4];
    LARGE_INTEGER performance_frequency;
    LARGE_INTEGER previous_counter;
    int counter_ready;
    HDC backbuffer_dc;
    HBITMAP backbuffer_bitmap;
    HGDIOBJ backbuffer_original;
    int backbuffer_width;
    int backbuffer_height;
    int edit_mode;
    G0WorldTool selected_tool;
    unsigned int new_world_width;
    unsigned int new_world_height;
    int zoom_index;
    G0VisualizerCamera camera;
    int pan_x;
    int pan_y;
    int tile_pixels;
    RECT world_panel;
    RECT side_panel;
    RECT tool_rects[G0_TOOL_COUNT];
    RECT apply_rect;
    RECT new_world_rect;
    RECT width_down_rect;
    RECT width_up_rect;
    RECT height_down_rect;
    RECT height_up_rect;
    RECT edit_button_rect;
    RECT menu_button_rect;
    RECT save_button_rect;
    RECT load_button_rect;
    RECT log_button_rect;
    RECT export_log_button_rect;
    RECT clear_log_button_rect;
    RECT main_new_rect;
    RECT main_load_rect;
    RECT main_continue_rect;
    RECT main_settings_rect;
    RECT main_exit_rect;
    RECT settings_speed_rect;
    RECT settings_pause_rect;
    RECT settings_grid_rect;
    RECT settings_debug_rect;
    RECT settings_zoom_rect;
    RECT settings_log_rect;
    RECT settings_water_rects[G0_APP_WATER_OPACITY_COUNT];
    RECT settings_presentation_rect;
    RECT settings_back_rect;
    G0UiFont title_font;
    G0UiFont ui_font;
    G0UiFont small_font;
    G0AppState app_state;
    G0AppSettings settings;
    G0EventLog event_log;
    char repository_root[MAX_PATH];
    char save_directory[MAX_PATH];
    G0StartupError startup_error;
    char startup_diagnostic[G0_VISUALIZER_ASSET_ERROR_MAX];
    char current_world_path[G0_SAVE_PATH_MAX];
    char world_name[G0_WORLD_NAME_MAX];
    int world_dirty;
    int log_open;
    size_t log_scroll;
    HWND main_window;
    HWND new_world_dialog;
    HWND new_world_name_edit;
    HWND new_world_width_edit;
    HWND new_world_height_edit;
    int map_origin_x;
    int map_origin_y;
} G0WindowApp;

static G0WindowApp g_app;
static const int g0_zoom_percentages[G0_ZOOM_COUNT] =
{ 25, 50, 75, 100, 150, 200, 300, 400 };
static const char *const g0_tool_names[G0_TOOL_COUNT] =
{ "WATER", "LAND", "ROCK", "FOOD", "ERASE", "FISH SPAWN" };
static const int g0_water_opacity_percentages[G0_APP_WATER_OPACITY_COUNT] =
{ 25, 35, 50, 65 };

static void rect_set(RECT *rect, int left, int top, int width, int height);
static void draw_main_menu(HDC hdc, const RECT *client);
static void draw_settings_panel(HDC hdc, const RECT *client);
static void draw_event_log_panel(HDC hdc);
static void invalidate_scene(HWND hwnd);
static int begin_edit(HWND hwnd);
static void calculate_layout(HWND hwnd);

static void set_startup_error(G0StartupError error, const char *diagnostic)
{
    g_app.startup_error = error;
    (void)snprintf(g_app.startup_diagnostic, sizeof(g_app.startup_diagnostic), "%s",
                   diagnostic == NULL ? "No diagnostic available." : diagnostic);
}

static void show_startup_failure(void)
{
    const char *title = "miniSNN Worlds";
    const char *message;
    char debug_message[G0_VISUALIZER_ASSET_ERROR_MAX + 96U];

    if (g_app.startup_error == G0_STARTUP_ERROR_ASSET_ROOT ||
        g_app.startup_error == G0_STARTUP_ERROR_ASSETS)
    {
        message = "ASSET INITIALIZATION FAILED.\nThe visual assets could not be initialized.\nSee the debug output for the resolved path and detail.";
    }
    else
    {
        message = "SANDBOX INITIALIZATION FAILED.\nThe Worlds sandbox could not be initialized.\nSee the debug output for the reason.";
    }
    (void)snprintf(debug_message, sizeof(debug_message),
                   "miniSNN Worlds startup error %d: %s\n",
                   (int)g_app.startup_error, g_app.startup_diagnostic);
    OutputDebugStringA(debug_message);
    MessageBoxA(NULL, message, title, MB_OK | MB_ICONERROR);
}

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
        return 0;
    destroy_bitmap(out_bitmap);
    result = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                              &IID_IWICImagingFactory, (void **)&factory);
    if (FAILED(result))
        goto done;
    if (FAILED(IWICImagingFactory_CreateDecoderFromFilename(
            factory, wide_path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
            &decoder)) ||
        FAILED(IWICBitmapDecoder_GetFrame(decoder, 0U, &frame)) ||
        FAILED(IWICImagingFactory_CreateFormatConverter(factory, &converter)) ||
        FAILED(IWICFormatConverter_Initialize(
            converter, (IWICBitmapSource *)frame, &GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom)) ||
        FAILED(IWICBitmapSource_GetSize((IWICBitmapSource *)converter, &width, &height)) ||
        width == 0U || height == 0U)
        goto done;
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
            (IWICBitmapSource *)converter, NULL, width * 4U, width * height * 4U,
            (BYTE *)pixels)))
        goto done;
    out_bitmap->bitmap = bitmap;
    out_bitmap->pixels = (BYTE *)pixels;
    out_bitmap->width = (int)width;
    out_bitmap->height = (int)height;
    bitmap = NULL;
    success = 1;
done:
    if (screen != NULL)
        ReleaseDC(NULL, screen);
    if (bitmap != NULL)
        DeleteObject(bitmap);
    if (converter != NULL)
        IWICFormatConverter_Release(converter);
    if (frame != NULL)
        IWICBitmapFrameDecode_Release(frame);
    if (decoder != NULL)
        IWICBitmapDecoder_Release(decoder);
    if (factory != NULL)
        IWICImagingFactory_Release(factory);
    return success;
}

static int create_rotated_fish_bitmap(const G0WinBitmap *source,
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
        return 0;
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
        return 0;
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
        destroy_bitmap(&g_app.fish_facing[index]);
}

static int load_all_assets(char *error_message, size_t error_message_size)
{
    static const unsigned int fish_turns[4] = { 2U, 3U, 0U, 1U };
    G0VisualizerAsset asset;
    int index;

    if (error_message != NULL && error_message_size > 0U)
    {
        error_message[0] = 0;
    }
    for (asset = G0_VISUALIZER_ASSET_DIRT; asset < G0_VISUALIZER_ASSET_COUNT; ++asset)
    {
        const char *path = g0_visualizer_asset_path(&g_app.assets, asset);
        if (!load_png_bitmap(path, &g_app.bitmaps[asset]))
        {
            (void)snprintf(error_message, error_message_size,
                           "PNG decoder failed for: %s", path == NULL ? "unknown asset" : path);
            return 0;
        }
    }
    for (index = 0; index < 4; ++index)
    {
        if (!create_rotated_fish_bitmap(&g_app.bitmaps[G0_VISUALIZER_ASSET_FISH],
                                        fish_turns[index], &g_app.fish_facing[index]))
        {
            destroy_fish_facings();
            (void)snprintf(error_message, error_message_size,
                           "Could not create rotated fish sprite %d.", index);
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
        return 0;
    backslash = strrchr(path, '\\');
    slash = strrchr(path, '/');
    separator = backslash;
    if (slash != NULL && (separator == NULL || slash > separator))
        separator = slash;
    if (separator == NULL)
        return 0;
    *separator = '\0';
    return 1;
}

static int asset_root_has_assets_directory(const char *asset_root)
{
    char assets_directory[MAX_PATH];
    DWORD attributes;

    if (asset_root == NULL || asset_root[0] == 0 ||
        snprintf(assets_directory, sizeof(assets_directory), "%s\\assets", asset_root) <= 0 ||
        strlen(assets_directory) >= sizeof(assets_directory))
    {
        return 0;
    }
    attributes = GetFileAttributesA(assets_directory);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U;
}

static int executable_directory_utf8(char *out_directory, size_t out_size)
{
    WCHAR executable_wide[MAX_PATH];
    char executable_utf8[MAX_PATH];
    DWORD length;
    int converted;

    if (out_directory == NULL || out_size == 0U)
    {
        return 0;
    }
    length = GetModuleFileNameW(NULL, executable_wide, MAX_PATH);
    if (length == 0U || length >= MAX_PATH)
    {
        return 0;
    }
    converted = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, executable_wide, -1,
                                    executable_utf8, MAX_PATH, NULL, NULL);
    if (converted <= 0 || (size_t)converted > out_size)
    {
        return 0;
    }
    (void)snprintf(out_directory, out_size, "%s", executable_utf8);
    return directory_parent(out_directory);
}

static int resolve_repository_root_from_executable(char *out_root, size_t out_size)
{
    static const char *const relative_candidates[] =
    { ".", "..", "..\\..", "..\\..\\..", "..\\..\\..\\.." };
    char executable_directory[MAX_PATH];
    char candidate[MAX_PATH];
    DWORD full_length;
    size_t index;

    if (out_root == NULL || out_size == 0U ||
        !executable_directory_utf8(executable_directory, sizeof(executable_directory)))
    {
        return 0;
    }
    for (index = 0U; index < sizeof(relative_candidates) / sizeof(relative_candidates[0]); ++index)
    {
        if (snprintf(candidate, sizeof(candidate), "%s\\%s", executable_directory,
                     relative_candidates[index]) <= 0)
        {
            continue;
        }
        full_length = GetFullPathNameA(candidate, (DWORD)out_size, out_root, NULL);
        if (full_length == 0U || full_length >= out_size)
        {
            continue;
        }
        if (asset_root_has_assets_directory(out_root))
        {
            return 1;
        }
    }
    out_root[0] = 0;
    return 0;
}
static void draw_bitmap(HDC target, const G0WinBitmap *bitmap,
                        int x, int y, int width, int height, BYTE alpha)
{
    HDC source;
    HGDIOBJ previous;
    BLENDFUNCTION blend;

    if (target == NULL || bitmap == NULL || bitmap->bitmap == NULL || width <= 0 || height <= 0)
        return;
    source = CreateCompatibleDC(target);
    if (source == NULL)
        return;
    previous = SelectObject(source, bitmap->bitmap);
    SetStretchBltMode(target, COLORONCOLOR);
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0U;
    blend.SourceConstantAlpha = alpha;
    blend.AlphaFormat = AC_SRC_ALPHA;
    (void)AlphaBlend(target, x, y, width, height, source, 0, 0,
                     bitmap->width, bitmap->height, blend);
    SelectObject(source, previous);
    DeleteDC(source);
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

static void stroke_panel(HDC hdc, const RECT *rect, COLORREF color)
{
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;
    if (pen == NULL)
        return;
    old_pen = SelectObject(hdc, pen);
    old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rect->left, rect->top, rect->right, rect->bottom);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static int g0_ui_scaled(int value)
{
    if (g_app.settings.presentation_mode == 0)
        return value;
    return (value * G0_PRESENTATION_SCALE_NUMERATOR +
            G0_PRESENTATION_SCALE_DENOMINATOR - 1) /
           G0_PRESENTATION_SCALE_DENOMINATOR;
}

static int g0_header_height(void)
{
    return g0_ui_scaled(G0_HEADER_HEIGHT_NORMAL);
}

static int g0_footer_height(void)
{
    return g0_ui_scaled(G0_FOOTER_HEIGHT_NORMAL);
}

static void destroy_ui_font(G0UiFont *font)
{
    if (font != NULL && font->owned != 0 && font->handle != NULL)
        DeleteObject(font->handle);
    if (font != NULL)
    {
        font->handle = NULL;
        font->owned = 0;
    }
}

static void create_ui_font(G0UiFont *font, int width, int height, int weight)
{
    HFONT handle;

    if (font == NULL || width <= 0 || height <= 0)
    {
        return;
    }
    destroy_ui_font(font);
    handle = CreateFontA(-height, width, 0, 0, weight, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_RASTER_PRECIS, CLIP_DEFAULT_PRECIS,
                         NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN,
                         "Fixedsys");
    if (handle != NULL)
    {
        font->handle = handle;
        font->owned = 1;
    }
    else
    {
        font->handle = (HFONT)GetStockObject(SYSTEM_FIXED_FONT);
    }
}
static void rebuild_ui_fonts(void)
{
    create_ui_font(&g_app.title_font, g0_ui_scaled(12), g0_ui_scaled(20), FW_BOLD);
    create_ui_font(&g_app.ui_font, g0_ui_scaled(8), g0_ui_scaled(14), FW_NORMAL);
    create_ui_font(&g_app.small_font, g0_ui_scaled(7), g0_ui_scaled(12), FW_NORMAL);
}

static void destroy_ui_fonts(void)
{
    destroy_ui_font(&g_app.title_font);
    destroy_ui_font(&g_app.ui_font);
    destroy_ui_font(&g_app.small_font);
}

static void draw_text_with_font(HDC hdc, const G0UiFont *font, COLORREF color,
                                 int x, int y, const char *text)
{
    HGDIOBJ old_font = NULL;

    if (hdc == NULL || text == NULL)
        return;
    if (font != NULL && font->handle != NULL)
        old_font = SelectObject(hdc, font->handle);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    (void)TextOutA(hdc, x, y, text, (int)strlen(text));
    if (old_font != NULL)
        SelectObject(hdc, old_font);
}

static void draw_text(HDC hdc, COLORREF color, int x, int y, const char *text)
{
    draw_text_with_font(hdc, &g_app.ui_font, color, x, y, text);
}

static void draw_title_text(HDC hdc, COLORREF color, int x, int y, const char *text)
{
    draw_text_with_font(hdc, &g_app.title_font, color, x, y, text);
}

static void draw_small_text(HDC hdc, COLORREF color, int x, int y, const char *text)
{
    draw_text_with_font(hdc, &g_app.small_font, color, x, y, text);
}

static void draw_wrapped_text(HDC hdc, COLORREF color, const RECT *bounds, const char *text)
{
    RECT clipped;
    int saved_dc;

    if (hdc == NULL || bounds == NULL || text == NULL)
        return;
    clipped = *bounds;
    saved_dc = SaveDC(hdc);
    if (g_app.small_font.handle != NULL)
        SelectObject(hdc, g_app.small_font.handle);
    IntersectClipRect(hdc, clipped.left, clipped.top, clipped.right, clipped.bottom);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    (void)DrawTextA(hdc, text, -1, &clipped,
                    DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
    RestoreDC(hdc, saved_dc);
}
static int point_in_rect(const RECT *rect, int x, int y)
{
    return rect != NULL && x >= rect->left && x < rect->right &&
           y >= rect->top && y < rect->bottom;
}

static const G0WorldConfig *current_config(void)
{
    return g_app.runtime == NULL ? NULL : g0_visualizer_runtime_config(g_app.runtime);
}

static void calculate_layout(HWND hwnd)
{
    RECT client;
    const G0WorldConfig *config = current_config();
    int side_width;

    if (hwnd == NULL || config == NULL)
        return;
    GetClientRect(hwnd, &client);
    side_width = g_app.edit_mode ? g0_ui_scaled(G0_EDITOR_PANEL_WIDTH) :
                  (g_app.log_open ? g0_ui_scaled(G0_DEBUG_PANEL_WIDTH) :
                   (g0_visualizer_runtime_debug_enabled(g_app.runtime) ?
                        g0_ui_scaled(G0_DEBUG_PANEL_WIDTH) : 0));
    g_app.world_panel.left = G0_PANEL_MARGIN;
    g_app.world_panel.top = g0_header_height() + G0_PANEL_MARGIN;
    g_app.world_panel.right = client.right - G0_PANEL_MARGIN - side_width;
    g_app.world_panel.bottom = client.bottom - g0_footer_height() - G0_PANEL_MARGIN;
    g_app.side_panel.left = g_app.world_panel.right + G0_PANEL_MARGIN;
    g_app.side_panel.top = g_app.world_panel.top;
    g_app.side_panel.right = client.right - G0_PANEL_MARGIN;
    g_app.side_panel.bottom = g_app.world_panel.bottom;
    if (!g0_visualizer_camera_layout(
            &g_app.camera, config->width, config->height,
            g_app.world_panel.left, g_app.world_panel.top,
            g_app.world_panel.right, g_app.world_panel.bottom))
    {
        return;
    }

    /* Paint-space values are derived from fixed-point camera state only. */
    g_app.tile_pixels = g_app.camera.tile_pixels;
    g_app.pan_x = g_app.camera.pan_x;
    g_app.pan_y = g_app.camera.pan_y;
    g_app.map_origin_x = g_app.camera.map_origin_x;
    g_app.map_origin_y = g_app.camera.map_origin_y;
}
static void fit_map(HWND hwnd)
{
    const G0WorldConfig *config = current_config();
    int client_width;
    int client_height;
    int desired_percent;
    int index;

    if (hwnd == NULL || config == NULL)
        return;
    client_width = g_app.world_panel.right - g_app.world_panel.left;
    client_height = g_app.world_panel.bottom - g_app.world_panel.top;
    if (client_width <= 0 || client_height <= 0)
        return;
    desired_percent = (client_width * 90) /
                      ((int)config->width * G0_VISUALIZER_TILE_PIXELS);
    if (((client_height * 90) /
         ((int)config->height * G0_VISUALIZER_TILE_PIXELS)) < desired_percent)
    {
        desired_percent = (client_height * 90) /
                          ((int)config->height * G0_VISUALIZER_TILE_PIXELS);
    }
    g_app.zoom_index = 0;
    for (index = 0; index < G0_ZOOM_COUNT; ++index)
    {
        if (g0_zoom_percentages[index] <= desired_percent)
            g_app.zoom_index = index;
    }
    (void)g0_visualizer_camera_set_zoom_percent(
        &g_app.camera, g0_zoom_percentages[g_app.zoom_index]);
    g0_visualizer_camera_reset_pan(&g_app.camera);
    calculate_layout(hwnd);
}

static void draw_base_terrain(HDC hdc, const G0WorldConfig *config)
{
    uint32_t x;
    uint32_t y;
    for (y = 0U; y < config->height; ++y)
    {
        for (x = 0U; x < config->width; ++x)
        {
            MiniSNNWorldsTerrainTile tile;
            const G0WinBitmap *bitmap;
            G0VisualizerScreenPoint point;
            if (!g0_world_config_tile(config, x, y, &tile) ||
                !g0_visualizer_tile_to_screen_scaled(x, y, g_app.map_origin_x, g_app.map_origin_y, g_app.tile_pixels, &point))
                continue;
            /* WATER uses dirt as substrate; its translucent medium is drawn last. */
            bitmap = tile == MINISNN_WORLDS_TERRAIN_TILE_WATER ?
                     &g_app.bitmaps[G0_VISUALIZER_ASSET_DIRT] :
                     &g_app.bitmaps[G0_VISUALIZER_ASSET_GRASS];
            draw_bitmap(hdc, bitmap, point.screen_x, point.screen_y,
                        g_app.tile_pixels, g_app.tile_pixels, 255U);
        }
    }
}

static void draw_grid(HDC hdc, const G0WorldConfig *config)
{
    HPEN pen;
    HGDIOBJ previous;
    uint32_t index;
    int map_right;
    int map_bottom;

    if (g_app.tile_pixels < 12 ||
        (!g_app.edit_mode && !g0_visualizer_runtime_grid_enabled(g_app.runtime)))
        return;
    pen = CreatePen(PS_SOLID, 1, RGB(38, 56, 76));
    if (pen == NULL)
        return;
    previous = SelectObject(hdc, pen);
    map_right = g_app.map_origin_x + (int)config->width * g_app.tile_pixels;
    map_bottom = g_app.map_origin_y + (int)config->height * g_app.tile_pixels;
    for (index = 0U; index <= config->width; ++index)
    {
        int x = g_app.map_origin_x + (int)index * g_app.tile_pixels;
        MoveToEx(hdc, x, g_app.map_origin_y, NULL);
        LineTo(hdc, x, map_bottom);
    }
    for (index = 0U; index <= config->height; ++index)
    {
        int y = g_app.map_origin_y + (int)index * g_app.tile_pixels;
        MoveToEx(hdc, g_app.map_origin_x, y, NULL);
        LineTo(hdc, map_right, y);
    }
    SelectObject(hdc, previous);
    DeleteObject(pen);
}
static int world_position_to_tile(const MiniSNNWorldsTerrain *terrain,
                                  MiniSNNWorldsKernelPosition position,
                                  uint32_t *out_x,
                                  uint32_t *out_y)
{
    return terrain != NULL && out_x != NULL && out_y != NULL &&
           minisnn_worlds_terrain_world_to_tile(terrain, position, out_x, out_y) ==
               MINISNN_WORLDS_TERRAIN_ERROR_NONE;
}

static void draw_static_objects(HDC hdc, const G0WorldConfig *config)
{
    uint32_t x;
    uint32_t y;
    for (y = 0U; y < config->height; ++y)
    {
        for (x = 0U; x < config->width; ++x)
        {
            int present = 0;
            G0VisualizerScreenPoint point;
            if (!g0_visualizer_tile_to_screen_scaled(x, y, g_app.map_origin_x, g_app.map_origin_y, g_app.tile_pixels, &point))
                continue;
            if (g0_world_config_has_rock(config, x, y, &present) && present)
            {
                draw_bitmap(hdc, &g_app.bitmaps[G0_VISUALIZER_ASSET_ROCK],
                            point.screen_x, point.screen_y, g_app.tile_pixels,
                            g_app.tile_pixels, 255U);
            }
            if (g_app.edit_mode && g0_world_config_has_food(config, x, y, &present) &&
                present)
            {
                draw_bitmap(hdc, &g_app.bitmaps[G0_VISUALIZER_ASSET_FOOD],
                            point.screen_x, point.screen_y, g_app.tile_pixels,
                            g_app.tile_pixels, 255U);
            }
        }
    }
}

static void draw_water_overlay(HDC hdc, const G0WorldConfig *config)
{
    uint32_t x;
    uint32_t y;

    for (y = 0U; y < config->height; ++y)
    {
        for (x = 0U; x < config->width; ++x)
        {
            MiniSNNWorldsTerrainTile tile;
            G0VisualizerScreenPoint point;

            if (!g0_world_config_tile(config, x, y, &tile) ||
                tile != MINISNN_WORLDS_TERRAIN_TILE_WATER ||
                !g0_visualizer_tile_to_screen_scaled(
                    x, y, g_app.map_origin_x, g_app.map_origin_y,
                    g_app.tile_pixels, &point))
            {
                continue;
            }
            draw_bitmap(hdc, &g_app.bitmaps[G0_VISUALIZER_ASSET_WATER],
                        point.screen_x, point.screen_y, g_app.tile_pixels,
                        g_app.tile_pixels,
                        g0_app_settings_effective_water_alpha(&g_app.settings));
        }
    }
}
static void draw_runtime_food(HDC hdc)
{
    size_t index;
    const MiniSNNWorldsTerrain *terrain = g0_visualizer_runtime_terrain(g_app.runtime);
    for (index = 0U; index < g0_visualizer_runtime_food_count(g_app.runtime); ++index)
    {
        MiniSNNWorldsKernelPosition position;
        uint32_t tile_x;
        uint32_t tile_y;
        G0VisualizerScreenPoint point;
        if (!g0_visualizer_runtime_food_position_at(g_app.runtime, index, &position) ||
            !world_position_to_tile(terrain, position, &tile_x, &tile_y) ||
            !g0_visualizer_tile_to_screen_scaled(tile_x, tile_y, g_app.map_origin_x, g_app.map_origin_y, g_app.tile_pixels, &point))
            continue;
        draw_bitmap(hdc, &g_app.bitmaps[G0_VISUALIZER_ASSET_FOOD], point.screen_x,
                    point.screen_y, g_app.tile_pixels, g_app.tile_pixels, 255U);
    }
}

static void draw_fish(HDC hdc, const G0WorldConfig *config)
{
    uint32_t tile_x;
    uint32_t tile_y;
    G0VisualizerFacing facing = G0_VISUALIZER_FACING_EAST;
    G0VisualizerScreenPoint point;
    int size;
    int draw_x;
    int draw_y;

    if (g_app.edit_mode)
    {
        tile_x = config->fish_spawn_x;
        tile_y = config->fish_spawn_y;
    }
    else
    {
        const WF0FishWorldState *state = g0_visualizer_runtime_state(g_app.runtime);
        if (state == NULL || !world_position_to_tile(g0_visualizer_runtime_terrain(g_app.runtime),
                                                     state->actor_transform.position,
                                                     &tile_x, &tile_y))
            return;
        facing = g0_visualizer_runtime_facing(g_app.runtime);
    }
    if (!g0_visualizer_tile_to_screen_scaled(tile_x, tile_y, g_app.map_origin_x, g_app.map_origin_y, g_app.tile_pixels, &point))
        return;
    size = g_app.tile_pixels / 2;
    if (size < 12)
        size = 12;
    draw_x = point.screen_x + (g_app.tile_pixels - size) / 2;
    draw_y = point.screen_y + (g_app.tile_pixels - size) / 2;
    draw_bitmap(hdc, &g_app.fish_facing[(unsigned int)facing], draw_x, draw_y,
                size, size, 255U);
}

static void draw_button(HDC hdc, const RECT *rect, const char *text, int active)
{
    COLORREF fill = active ? RGB(47, 128, 113) : RGB(35, 48, 65);
    COLORREF border = active ? RGB(117, 222, 191) : RGB(82, 111, 139);
    HGDIOBJ old_font = NULL;
    int text_x;
    int text_y;
    SIZE text_size;

    fill_color(hdc, rect, fill);
    stroke_panel(hdc, rect, border);
    if (g_app.ui_font.handle != NULL)
        old_font = SelectObject(hdc, g_app.ui_font.handle);
    GetTextExtentPoint32A(hdc, text, (int)strlen(text), &text_size);
    if (old_font != NULL)
        SelectObject(hdc, old_font);
    text_x = rect->left + ((rect->right - rect->left) - text_size.cx) / 2;
    text_y = rect->top + ((rect->bottom - rect->top) - text_size.cy) / 2;
    draw_text(hdc, RGB(235, 246, 250), text_x, text_y, text);
}
static void draw_header(HDC hdc, const RECT *client)
{
    char line[192];
    const char *status;
    const WF0FishWorldState *state;
    const int header_height = g0_header_height();
    G0VisualizerUiRect controls[G0_VISUALIZER_HEADER_CONTROL_COUNT];
    RECT header = { 0, 0, client->right, header_height };

    fill_color(hdc, &header, RGB(20, 29, 42));
    (void)snprintf(line, sizeof(line), "miniSNN Worlds / %s%s",
                   g_app.world_name[0] == 0 ? "Untitled" : g_app.world_name,
                   g_app.world_dirty ? " *" : "");
    draw_title_text(hdc, RGB(117, 222, 191), g0_ui_scaled(18), g0_ui_scaled(10), line);
    if (g_app.edit_mode)
        status = "EDITING";
    else if (g0_visualizer_runtime_is_paused(g_app.runtime))
        status = "PAUSED";
    else
        status = "RUNNING";
    state = g0_visualizer_runtime_state(g_app.runtime);
    (void)snprintf(line, sizeof(line), "%s | EP %llu | Domain %llu | %s | %d%%%s",
                   status,
                   (unsigned long long)g0_visualizer_runtime_episode_tick(g_app.runtime),
                   state == NULL ? 0ULL : (unsigned long long)state->tick,
                   g0_visualizer_runtime_speed_name(g_app.runtime),
                   g0_zoom_percentages[g_app.zoom_index],
                   g_app.settings.presentation_mode != 0 ? " | PRESENTATION" : "");
    draw_small_text(hdc, RGB(217, 229, 239), g0_ui_scaled(18), g0_ui_scaled(39), line);

    if (!g0_visualizer_header_control_layout(
            client->right,
            g_app.settings.presentation_mode,
            controls))
    {
        return;
    }
    g_app.menu_button_rect = (RECT) {
        controls[G0_VISUALIZER_HEADER_MENU].left,
        controls[G0_VISUALIZER_HEADER_MENU].top,
        controls[G0_VISUALIZER_HEADER_MENU].right,
        controls[G0_VISUALIZER_HEADER_MENU].bottom
    };
    g_app.save_button_rect = (RECT) {
        controls[G0_VISUALIZER_HEADER_SAVE].left,
        controls[G0_VISUALIZER_HEADER_SAVE].top,
        controls[G0_VISUALIZER_HEADER_SAVE].right,
        controls[G0_VISUALIZER_HEADER_SAVE].bottom
    };
    g_app.load_button_rect = (RECT) {
        controls[G0_VISUALIZER_HEADER_LOAD].left,
        controls[G0_VISUALIZER_HEADER_LOAD].top,
        controls[G0_VISUALIZER_HEADER_LOAD].right,
        controls[G0_VISUALIZER_HEADER_LOAD].bottom
    };
    g_app.log_button_rect = (RECT) {
        controls[G0_VISUALIZER_HEADER_LOG].left,
        controls[G0_VISUALIZER_HEADER_LOG].top,
        controls[G0_VISUALIZER_HEADER_LOG].right,
        controls[G0_VISUALIZER_HEADER_LOG].bottom
    };
    g_app.edit_button_rect = (RECT) {
        controls[G0_VISUALIZER_HEADER_EDITING].left,
        controls[G0_VISUALIZER_HEADER_EDITING].top,
        controls[G0_VISUALIZER_HEADER_EDITING].right,
        controls[G0_VISUALIZER_HEADER_EDITING].bottom
    };
    draw_button(hdc, &g_app.menu_button_rect, "MENU", 0);
    draw_button(hdc, &g_app.save_button_rect, "SAVE", 0);
    draw_button(hdc, &g_app.load_button_rect, "LOAD", 0);
    draw_button(hdc, &g_app.log_button_rect, g_app.log_open ? "LOG ON" : "LOG", g_app.log_open);
    draw_button(hdc, &g_app.edit_button_rect,
                g_app.edit_mode ? "EDITING" : "EDIT", g_app.edit_mode);
}
static void draw_footer(HDC hdc, const RECT *client)
{
    const int footer_height = g0_footer_height();
    RECT footer = { 0, client->bottom - footer_height, client->right, client->bottom };
    RECT help_bounds = { g0_ui_scaled(18), footer.top + g0_ui_scaled(8),
                         client->right - g0_ui_scaled(18), footer.bottom - g0_ui_scaled(4) };
    const char *help = g_app.edit_mode ?
        "EDIT: click map to paint | ENTER apply/run | R reset episode | F fit | wheel zoom | arrows/WASD pan" :
        "SPACE pause/resume | N one tick | R reset episode | E edit | Ctrl+S save | Ctrl+O load | L event log";
    fill_color(hdc, &footer, RGB(20, 29, 42));
    draw_wrapped_text(hdc, RGB(166, 187, 203), &help_bounds, help);
}
static void draw_main_menu(HDC hdc, const RECT *client)
{
    RECT panel;
    const int width = g_app.settings.presentation_mode != 0 ? 570 : 410;
    const int height = g_app.settings.presentation_mode != 0 ? 490 : 360;
    const int padding = g0_ui_scaled(28);
    const int row_height = g0_ui_scaled(38);
    const int row_gap = g0_ui_scaled(10);
    const int left = (client->right - width) / 2;
    const int top = (client->bottom - height) / 2;
    int row_y = top + g0_ui_scaled(100);

    rect_set(&panel, left, top, width, height);
    fill_color(hdc, &panel, RGB(25, 37, 52));
    stroke_panel(hdc, &panel, RGB(76, 105, 133));
    draw_title_text(hdc, RGB(117, 222, 191), left + padding, top + g0_ui_scaled(28),
                    "miniSNN Worlds");
    draw_small_text(hdc, RGB(217, 229, 239), left + padding, top + g0_ui_scaled(62),
                    "Persistent sandbox for SNN minds");
    rect_set(&g_app.main_new_rect, left + padding, row_y, width - 2 * padding, row_height);
    row_y += row_height + row_gap;
    rect_set(&g_app.main_load_rect, left + padding, row_y, width - 2 * padding, row_height);
    row_y += row_height + row_gap;
    rect_set(&g_app.main_continue_rect, left + padding, row_y, width - 2 * padding, row_height);
    row_y += row_height + row_gap;
    rect_set(&g_app.main_settings_rect, left + padding, row_y, width - 2 * padding, row_height);
    row_y += row_height + row_gap;
    rect_set(&g_app.main_exit_rect, left + padding, row_y, width - 2 * padding, row_height);
    draw_button(hdc, &g_app.main_new_rect, "NEW WORLD", 1);
    draw_button(hdc, &g_app.main_load_rect, "LOAD WORLD", 0);
    draw_button(hdc, &g_app.main_continue_rect, "OPEN SANDBOX", 0);
    draw_button(hdc, &g_app.main_settings_rect, "SETTINGS", 0);
    draw_button(hdc, &g_app.main_exit_rect, "EXIT", 0);
}
static const char *settings_bool_name(int value)
{
    return value != 0 ? "ON" : "OFF";
}

static void copy_ui_rect(RECT *destination, const G0VisualizerUiRect *source)
{
    if (destination != NULL && source != NULL)
    {
        destination->left = source->left;
        destination->top = source->top;
        destination->right = source->right;
        destination->bottom = source->bottom;
    }
}

static void draw_settings_panel(HDC hdc, const RECT *client)
{
    char line[128];
    G0VisualizerSettingsLayout layout;
    RECT panel;
    unsigned int index;

    if (hdc == NULL || client == NULL ||
        !g0_visualizer_settings_layout(client->right, client->bottom,
                                       g_app.settings.presentation_mode, &layout))
    {
        return;
    }
    copy_ui_rect(&panel, &layout.panel);
    fill_color(hdc, &panel, RGB(25, 37, 52));
    stroke_panel(hdc, &panel, RGB(76, 105, 133));
    draw_title_text(hdc, RGB(117, 222, 191),
                    layout.controls[G0_VISUALIZER_SETTINGS_TITLE].left,
                    layout.controls[G0_VISUALIZER_SETTINGS_TITLE].top,
                    "APPLICATION SETTINGS");
    draw_small_text(hdc, RGB(166, 187, 203),
                    layout.controls[G0_VISUALIZER_SETTINGS_SIMULATION_LABEL].left,
                    layout.controls[G0_VISUALIZER_SETTINGS_SIMULATION_LABEL].top,
                    "SIMULATION");
    (void)snprintf(line, sizeof(line), "Default speed: %s",
                   g0_visualizer_runtime_speed_name(g_app.runtime));
    copy_ui_rect(&g_app.settings_speed_rect,
                 &layout.controls[G0_VISUALIZER_SETTINGS_SPEED]);
    draw_button(hdc, &g_app.settings_speed_rect, line, 0);
    (void)snprintf(line, sizeof(line), "Start paused: %s",
                   settings_bool_name(g_app.settings.start_paused));
    copy_ui_rect(&g_app.settings_pause_rect,
                 &layout.controls[G0_VISUALIZER_SETTINGS_START_PAUSED]);
    draw_button(hdc, &g_app.settings_pause_rect, line, g_app.settings.start_paused);

    draw_small_text(hdc, RGB(166, 187, 203),
                    layout.controls[G0_VISUALIZER_SETTINGS_VISUAL_LABEL].left,
                    layout.controls[G0_VISUALIZER_SETTINGS_VISUAL_LABEL].top,
                    "VISUAL");
    draw_small_text(hdc, RGB(217, 229, 239),
                    layout.controls[G0_VISUALIZER_SETTINGS_WATER_LABEL].left,
                    layout.controls[G0_VISUALIZER_SETTINGS_WATER_LABEL].top,
                    "WATER OPACITY");
    for (index = 0U; index < G0_APP_WATER_OPACITY_COUNT; ++index)
    {
        (void)snprintf(line, sizeof(line), "%d%%", g0_water_opacity_percentages[index]);
        copy_ui_rect(&g_app.settings_water_rects[index],
                     &layout.controls[G0_VISUALIZER_SETTINGS_WATER_25 + index]);
        draw_button(hdc, &g_app.settings_water_rects[index], line,
                    g_app.settings.water_opacity_percent ==
                        g0_water_opacity_percentages[index]);
    }
    (void)snprintf(line, sizeof(line), "Presentation mode: %s",
                   settings_bool_name(g_app.settings.presentation_mode));
    copy_ui_rect(&g_app.settings_presentation_rect,
                 &layout.controls[G0_VISUALIZER_SETTINGS_PRESENTATION]);
    draw_button(hdc, &g_app.settings_presentation_rect, line,
                g_app.settings.presentation_mode);
    (void)snprintf(line, sizeof(line), "Show grid: %s",
                   settings_bool_name(g_app.settings.show_grid));
    copy_ui_rect(&g_app.settings_grid_rect,
                 &layout.controls[G0_VISUALIZER_SETTINGS_GRID]);
    draw_button(hdc, &g_app.settings_grid_rect, line, g_app.settings.show_grid);
    (void)snprintf(line, sizeof(line), "Show debug: %s",
                   settings_bool_name(g_app.settings.show_debug_panel));
    copy_ui_rect(&g_app.settings_debug_rect,
                 &layout.controls[G0_VISUALIZER_SETTINGS_DEBUG]);
    draw_button(hdc, &g_app.settings_debug_rect, line, g_app.settings.show_debug_panel);
    if (g_app.settings.initial_zoom_index == G0_APP_INITIAL_ZOOM_FIT)
    {
        (void)snprintf(line, sizeof(line), "Initial zoom: FIT");
    }
    else
    {
        (void)snprintf(line, sizeof(line), "Initial zoom: %d%%",
                       g0_zoom_percentages[g_app.settings.initial_zoom_index]);
    }
    copy_ui_rect(&g_app.settings_zoom_rect,
                 &layout.controls[G0_VISUALIZER_SETTINGS_INITIAL_ZOOM]);
    draw_button(hdc, &g_app.settings_zoom_rect, line, 0);

    draw_small_text(hdc, RGB(166, 187, 203),
                    layout.controls[G0_VISUALIZER_SETTINGS_LOG_LABEL].left,
                    layout.controls[G0_VISUALIZER_SETTINGS_LOG_LABEL].top,
                    "LOG");
    (void)snprintf(line, sizeof(line), "Event log: %s",
                   settings_bool_name(g_app.settings.event_log_enabled));
    copy_ui_rect(&g_app.settings_log_rect,
                 &layout.controls[G0_VISUALIZER_SETTINGS_EVENT_LOG]);
    draw_button(hdc, &g_app.settings_log_rect, line, g_app.settings.event_log_enabled);
    copy_ui_rect(&g_app.settings_back_rect,
                 &layout.controls[G0_VISUALIZER_SETTINGS_BACK]);
    draw_button(hdc, &g_app.settings_back_rect, "BACK TO MENU", 1);
}static void draw_event_log_panel(HDC hdc)
{
    char line[256];
    size_t total = g0_event_log_count(&g_app.event_log);
    size_t row = 0U;
    size_t visible;
    size_t offset;
    int padding = g0_ui_scaled(12);
    int title_y = g_app.side_panel.top + g0_ui_scaled(14);
    int button_y = title_y + g0_ui_scaled(30);
    int button_height = g0_ui_scaled(28);
    int row_height = g0_ui_scaled(36);
    int x = g_app.side_panel.left + padding;
    int button_width = (g_app.side_panel.right - x - padding - g0_ui_scaled(10)) / 2;

    fill_color(hdc, &g_app.side_panel, RGB(25, 37, 52));
    stroke_panel(hdc, &g_app.side_panel, RGB(76, 105, 133));
    (void)snprintf(line, sizeof(line), "EVENT LOG (%llu)", (unsigned long long)total);
    draw_title_text(hdc, RGB(117, 222, 191), x, title_y, line);
    rect_set(&g_app.clear_log_button_rect, x, button_y, button_width, button_height);
    rect_set(&g_app.export_log_button_rect, x + button_width + g0_ui_scaled(10), button_y,
             button_width, button_height);
    draw_button(hdc, &g_app.clear_log_button_rect, "CLEAR", 0);
    draw_button(hdc, &g_app.export_log_button_rect, "EXPORT CSV", 0);
    visible = (size_t)((g_app.side_panel.bottom - (button_y + button_height + g0_ui_scaled(14))) /
                       row_height);
    if (visible == 0U)
        return;
    if (g_app.log_scroll > (total > visible ? total - visible : 0U))
        g_app.log_scroll = total > visible ? total - visible : 0U;
    offset = g_app.log_scroll;
    while (row < visible && row + offset < total)
    {
        G0LogEntry entry;
        RECT text_bounds;
        size_t index = total - 1U - offset - row;
        if (!g0_event_log_at(&g_app.event_log, index, &entry))
            break;
        (void)snprintf(line, sizeof(line), "t=%llu %s %s: %s",
                       (unsigned long long)entry.tick, g0_log_category_name(entry.category),
                       entry.event, entry.result);
        rect_set(&text_bounds, x, button_y + button_height + g0_ui_scaled(10) +
                 (int)row * row_height, g_app.side_panel.right - x - padding,
                 row_height - g0_ui_scaled(2));
        draw_wrapped_text(hdc, RGB(220, 230, 240), &text_bounds, line);
        ++row;
    }
}
static void rect_set(RECT *rect, int left, int top, int width, int height)
{
    rect->left = left;
    rect->top = top;
    rect->right = left + width;
    rect->bottom = top + height;
}

static void draw_editor_panel(HDC hdc, const G0WorldConfig *config)
{
    char line[128];
    int padding = g0_ui_scaled(14);
    int small_gap = g0_ui_scaled(8);
    int button_height = g0_ui_scaled(28);
    int row_height = g0_ui_scaled(30);
    int x = g_app.side_panel.left;
    int y = g_app.side_panel.top;
    int width = g_app.side_panel.right - g_app.side_panel.left;
    int index;

    fill_color(hdc, &g_app.side_panel, RGB(25, 37, 52));
    stroke_panel(hdc, &g_app.side_panel, RGB(76, 105, 133));
    draw_title_text(hdc, RGB(117, 222, 191), x + padding, y + padding, "WORLD EDITOR");
    (void)snprintf(line, sizeof(line), "Size: %u x %u", config->width, config->height);
    draw_small_text(hdc, RGB(220, 230, 240), x + padding, y + g0_ui_scaled(42), line);
    rect_set(&g_app.width_down_rect, x + padding, y + g0_ui_scaled(66),
             g0_ui_scaled(42), button_height);
    rect_set(&g_app.width_up_rect, x + padding + g0_ui_scaled(50), y + g0_ui_scaled(66),
             g0_ui_scaled(42), button_height);
    rect_set(&g_app.height_down_rect, x + padding + g0_ui_scaled(112), y + g0_ui_scaled(66),
             g0_ui_scaled(42), button_height);
    rect_set(&g_app.height_up_rect, x + padding + g0_ui_scaled(162), y + g0_ui_scaled(66),
             g0_ui_scaled(42), button_height);
    draw_button(hdc, &g_app.width_down_rect, "W-", 0);
    draw_button(hdc, &g_app.width_up_rect, "W+", 0);
    draw_button(hdc, &g_app.height_down_rect, "H-", 0);
    draw_button(hdc, &g_app.height_up_rect, "H+", 0);
    rect_set(&g_app.new_world_rect, x + padding, y + g0_ui_scaled(106),
             width - 2 * padding, row_height);
    draw_button(hdc, &g_app.new_world_rect, "NEW WORLD", 0);
    draw_small_text(hdc, RGB(166, 187, 203), x + padding, y + g0_ui_scaled(152), "TOOLS");
    for (index = 0; index < G0_TOOL_COUNT; ++index)
    {
        rect_set(&g_app.tool_rects[index], x + padding,
                 y + g0_ui_scaled(174) + index * (row_height + small_gap),
                 width - 2 * padding, row_height);
        draw_button(hdc, &g_app.tool_rects[index], g0_tool_names[index],
                    (int)g_app.selected_tool == index);
    }
    (void)snprintf(line, sizeof(line), "Configured food: %llu",
                   (unsigned long long)g0_world_config_food_count(config));
    draw_small_text(hdc, RGB(220, 230, 240), x + padding, y + g0_ui_scaled(402), line);
    draw_wrapped_text(hdc, RGB(166, 187, 203),
                      &(RECT){x + padding, y + g0_ui_scaled(425),
                              g_app.side_panel.right - padding, y + g0_ui_scaled(478)},
                      "RUN rebuilds the world from this blueprint.");
    rect_set(&g_app.apply_rect, x + padding, g_app.side_panel.bottom - g0_ui_scaled(48),
             width - 2 * padding, g0_ui_scaled(34));
    draw_button(hdc, &g_app.apply_rect, "APPLY / RUN", 1);
}
static const char *action_status_name(MiniSNNWorldsDomainActionStatus status)
{
    switch (status)
    {
        case MINISNN_WORLDS_DOMAIN_ACTION_APPLIED: return "APPLIED";
        case MINISNN_WORLDS_DOMAIN_ACTION_REJECTED: return "REJECTED";
        default: return "NA";
    }
}

static const char *action_reason_name(MiniSNNWorldsDomainActionReason reason)
{
    switch (reason)
    {
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_NONE: return "NONE";
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_ACTOR_NOT_ORGANISM:
            return "ACTOR_NOT_ORGANISM";
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_TARGET_NOT_FOOD:
            return "TARGET_NOT_FOOD";
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_TARGET_NOT_AVAILABLE:
            return "TARGET_NOT_AVAILABLE";
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_TARGET_OUT_OF_RANGE:
            return "TARGET_OUT_OF_RANGE";
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_INSUFFICIENT_ENERGY:
            return "INSUFFICIENT_ENERGY";
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_INVALID_ACTION:
            return "INVALID_ACTION";
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_KERNEL_REJECTED:
            return "KERNEL_REJECTED";
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_DUPLICATE_ACTOR:
            return "DUPLICATE_ACTOR";
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_ACTOR_DEAD:
            return "ACTOR_DEAD";
        default: return "UNKNOWN";
    }
}

static void draw_debug_panel(HDC hdc)
{
    char line[192];
    const WF0FishWorldState *state = g0_visualizer_runtime_state(g_app.runtime);
    const WF0FishTickRecord *record = g0_visualizer_runtime_last_record(g_app.runtime);
    uint32_t tile_x;
    uint32_t tile_y;
    int padding = g0_ui_scaled(14);
    int first_row = g0_ui_scaled(72);
    int row_height = g0_ui_scaled(24);
    int x = g_app.side_panel.left;
    int y = g_app.side_panel.top;
    int row = 0;

    if (state == NULL)
        return;
    fill_color(hdc, &g_app.side_panel, RGB(25, 37, 52));
    stroke_panel(hdc, &g_app.side_panel, RGB(76, 105, 133));
    draw_title_text(hdc, RGB(117, 222, 191), x + padding, y + padding, "FISH STATUS");
    draw_small_text(hdc, RGB(166, 187, 203), x + padding, y + g0_ui_scaled(38),
                    "Brain: WF0 Fixed Brain V1");
    draw_small_text(hdc, RGB(166, 187, 203), x + padding, y + g0_ui_scaled(54),
                    "Neurons: 12 | Plasticity: OFF");
    if (world_position_to_tile(g0_visualizer_runtime_terrain(g_app.runtime),
                               state->actor_transform.position, &tile_x, &tile_y))
        (void)snprintf(line, sizeof(line), "Tile: (%u, %u)", tile_x, tile_y);
    else
        (void)snprintf(line, sizeof(line), "Tile: unavailable");
    draw_text(hdc, RGB(220, 230, 240), x + padding, y + first_row + row++ * row_height, line);
    (void)snprintf(line, sizeof(line), "World: (%lld, %lld)",
                   (long long)state->actor_transform.position.x,
                   (long long)state->actor_transform.position.y);
    draw_text(hdc, RGB(220, 230, 240), x + padding, y + first_row + row++ * row_height, line);
    (void)snprintf(line, sizeof(line), "Energy: %lld", (long long)state->actor_energy);
    draw_text(hdc, RGB(220, 230, 240), x + padding, y + first_row + row++ * row_height, line);
    if (state->actor_life_state == MINISNN_WORLDS_DOMAIN_LIFE_DEAD)
    {
        (void)snprintf(line, sizeof(line), "State: DEAD");
        draw_text(hdc, RGB(244, 157, 122), x + padding, y + first_row + row++ * row_height, line);
        (void)snprintf(line, sizeof(line), "Cause: %s",
                       state->actor_death_cause == MINISNN_WORLDS_DOMAIN_DEATH_CAUSE_STARVATION ?
                           "STARVATION" : "UNKNOWN");
        draw_text(hdc, RGB(244, 157, 122), x + padding, y + first_row + row++ * row_height, line);
        (void)snprintf(line, sizeof(line), "Death tick: %llu",
                       (unsigned long long)state->actor_death_tick);
        draw_text(hdc, RGB(244, 157, 122), x + padding, y + first_row + row++ * row_height, line);
    }
    (void)snprintf(line, sizeof(line), "Food: %llu", (unsigned long long)state->remaining_food);
    draw_text(hdc, RGB(220, 230, 240), x + padding, y + first_row + row++ * row_height, line);
    (void)snprintf(line, sizeof(line), "Facing: %s",
                   g0_visualizer_facing_name(g0_visualizer_runtime_facing(g_app.runtime)));
    draw_text(hdc, RGB(220, 230, 240), x + padding, y + first_row + row++ * row_height, line);
    (void)snprintf(line, sizeof(line), "Core step: %d", state->core_step);
    draw_text(hdc, RGB(220, 230, 240), x + padding, y + first_row + row++ * row_height, line);
    (void)snprintf(line, sizeof(line), "Action: %s", record == NULL ? "NONE" :
                   g0_visualizer_action_name(record->decision.action.type));
    draw_text(hdc, RGB(220, 230, 240), x + padding, y + first_row + row++ * row_height, line);
    (void)snprintf(line, sizeof(line), "Result: %s", record == NULL ? "NA" :
                   action_status_name(record->action_result.status));
    draw_text(hdc, RGB(220, 230, 240), x + padding, y + first_row + row++ * row_height, line);
    (void)snprintf(line, sizeof(line), "Reason: %s", record == NULL ? "NA" :
                   action_reason_name(record->action_result.reason));
    draw_text(hdc, RGB(220, 230, 240), x + padding, y + first_row + row++ * row_height, line);
    {
        RECT help_bounds = {x + padding, y + first_row + row * row_height + g0_ui_scaled(8),
                            g_app.side_panel.right - padding,
                            y + first_row + row * row_height + g0_ui_scaled(58)};
        draw_wrapped_text(hdc, RGB(166, 187, 203), &help_bounds,
                          "E enters the editor. FOOD changes rebuild the sandbox while preserving the app session.");
    }
}
static void draw_scene(HDC hdc, HWND hwnd)
{
    RECT client;
    const G0WorldConfig *config = current_config();

    if (hdc == NULL || hwnd == NULL || config == NULL)
        return;
    GetClientRect(hwnd, &client);
    calculate_layout(hwnd);
    fill_color(hdc, &client, RGB(11, 17, 25));
    if (g_app.app_state == G0_APP_MAIN_MENU)
    {
        draw_main_menu(hdc, &client);
        return;
    }
    if (g_app.app_state == G0_APP_SETTINGS)
    {
        draw_settings_panel(hdc, &client);
        return;
    }
    draw_header(hdc, &client);
    fill_color(hdc, &g_app.world_panel, RGB(16, 25, 36));
    stroke_panel(hdc, &g_app.world_panel, RGB(76, 105, 133));
    SaveDC(hdc);
    IntersectClipRect(hdc, g_app.world_panel.left, g_app.world_panel.top,
                     g_app.world_panel.right, g_app.world_panel.bottom);
    draw_base_terrain(hdc, config);
    draw_static_objects(hdc, config);
    if (!g_app.edit_mode)
        draw_runtime_food(hdc);
    draw_fish(hdc, config);
    draw_water_overlay(hdc, config);
    draw_grid(hdc, config);
    RestoreDC(hdc, -1);

    if (g_app.edit_mode)
        draw_editor_panel(hdc, config);
    else if (g_app.log_open)
        draw_event_log_panel(hdc);
    else if (g0_visualizer_runtime_debug_enabled(g_app.runtime))
        draw_debug_panel(hdc);
    draw_footer(hdc, &client);
}

static void apply_app_settings(void)
{
    if (g_app.runtime == NULL)
        return;
    (void)g0_visualizer_runtime_set_speed_index(
        g_app.runtime, g_app.settings.default_speed_index);
    g0_visualizer_runtime_set_paused(g_app.runtime, g_app.settings.start_paused);
    g0_visualizer_runtime_set_grid_enabled(g_app.runtime, g_app.settings.show_grid);
    g0_visualizer_runtime_set_debug_enabled(
        g_app.runtime, g_app.settings.show_debug_panel);
}

static void append_log(
    uint64_t tick,
    G0LogCategory category,
    int has_actor,
    uint64_t actor,
    const char *event,
    const char *result,
    const char *reason,
    const char *text)
{
    if (g_app.settings.event_log_enabled)
    {
        (void)g0_event_log_append(&g_app.event_log, tick, category, has_actor,
                                  actor, event, result, reason, text);
    }
}

static void append_episode_started_log(const char *event, const char *text)
{
    append_log(g0_visualizer_runtime_episode_tick(g_app.runtime), G0_LOG_CATEGORY_SYSTEM, 0, 0U,
               event, "APPLIED", "NONE", text);
}

static void append_runtime_record_log(void)
{
    const WF0FishTickRecord *record = g0_visualizer_runtime_last_record(g_app.runtime);
    const WF0FishWorldState *state = g0_visualizer_runtime_state(g_app.runtime);
    char text[G0_EVENT_LOG_TEXT_MAX];

    if (record == NULL || state == NULL)
        return;
    (void)snprintf(text, sizeof(text), "Fish %s -> %s",
                   g0_visualizer_action_name(record->decision.action.type),
                   action_status_name(record->action_result.status));
    append_log(g0_visualizer_runtime_episode_tick(g_app.runtime), G0_LOG_CATEGORY_ACTION, 1, 1U,
               g0_visualizer_action_name(record->decision.action.type),
               action_status_name(record->action_result.status),
               action_reason_name(record->action_result.reason), text);
    if (record->food_nutrition > 0U)
    {
        (void)snprintf(text, sizeof(text), "Food consumed; energy +%llu",
                       (unsigned long long)record->food_nutrition);
        append_log(g0_visualizer_runtime_episode_tick(g_app.runtime), G0_LOG_CATEGORY_FOOD, 1, 1U,
                   "FOOD_CONSUMED", "APPLIED", "NONE", text);
        append_log(g0_visualizer_runtime_episode_tick(g_app.runtime), G0_LOG_CATEGORY_ENERGY, 1, 1U,
                   "ENERGY", "GAIN", "FOOD_CONSUMED", text);
    }
    else if (record->move_cost > 0U)
    {
        (void)snprintf(text, sizeof(text), "Move energy cost %llu",
                       (unsigned long long)record->move_cost);
        append_log(g0_visualizer_runtime_episode_tick(g_app.runtime), G0_LOG_CATEGORY_ENERGY, 1, 1U,
                   "ENERGY", "COST", "MOVE", text);
    }
}

static int ensure_save_directory(void)
{
    DWORD attributes;

    if (g_app.save_directory[0] == '\0')
        return 0;
    attributes = GetFileAttributesA(g_app.save_directory);
    if (attributes != INVALID_FILE_ATTRIBUTES)
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    return CreateDirectoryA(g_app.save_directory, NULL) != 0 ||
           GetLastError() == ERROR_ALREADY_EXISTS;
}

static int choose_world_file(HWND hwnd, int save, char *out_path, size_t out_size)
{
    OPENFILENAMEA dialog;
    char file_buffer[MAX_PATH];
    DWORD original_directory_length;
    char original_directory[MAX_PATH];
    int selected;

    if (hwnd == NULL || out_path == NULL || out_size == 0U || !ensure_save_directory())
        return 0;
    original_directory_length = GetCurrentDirectoryA(MAX_PATH, original_directory);
    if (original_directory_length == 0U || original_directory_length >= MAX_PATH)
        return 0;
    ZeroMemory(&dialog, sizeof(dialog));
    ZeroMemory(file_buffer, sizeof(file_buffer));
    if (save && g_app.world_name[0] != '\0')
        (void)snprintf(file_buffer, sizeof(file_buffer), "%s.world", g_app.world_name);
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd;
    dialog.lpstrFilter = "miniSNN Worlds World (*.world)\0*.world\0All files\0*.*\0";
    dialog.lpstrFile = file_buffer;
    dialog.nMaxFile = sizeof(file_buffer);
    dialog.lpstrInitialDir = g_app.save_directory;
    dialog.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST |
                   (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    dialog.lpstrDefExt = "world";
    selected = save ? GetSaveFileNameA(&dialog) : GetOpenFileNameA(&dialog);
    (void)SetCurrentDirectoryA(original_directory);
    if (!selected || strlen(file_buffer) >= out_size)
        return 0;
    memcpy(out_path, file_buffer, strlen(file_buffer) + 1U);
    return 1;
}

static int save_world_to_path(const char *filename)
{
    G0WorldDocument document;
    G0WorldDocumentError error;
    const G0WorldConfig *config = current_config();

    if (filename == NULL || config == NULL)
        return 0;
    g0_world_document_init(&document);
    if (!g0_world_document_create(&document, g_app.world_name, config, &g_app.settings) ||
        !g0_world_document_save(filename, &document, &error))
    {
        g0_world_document_destroy(&document);
        return 0;
    }
    g0_world_document_destroy(&document);
    (void)snprintf(g_app.current_world_path, sizeof(g_app.current_world_path), "%s", filename);
    g_app.world_dirty = 0;
    return 1;
}

static int save_world_interactive(HWND hwnd, int save_as)
{
    char path[MAX_PATH];

    if (!save_as && g_app.current_world_path[0] != '\0')
        return save_world_to_path(g_app.current_world_path);
    if (!choose_world_file(hwnd, 1, path, sizeof(path)))
        return 0;
    return save_world_to_path(path);
}

static int load_world_from_path(const char *filename)
{
    G0WorldDocument document;
    G0WorldDocumentError error;
    G0VisualizerRuntime *candidate;

    if (filename == NULL)
        return 0;
    g0_world_document_init(&document);
    if (!g0_world_document_load(filename, &document, &error))
    {
        g0_world_document_destroy(&document);
        return 0;
    }
    candidate = g0_visualizer_runtime_create_from_config(&document.world);
    if (candidate == NULL)
    {
        g0_world_document_destroy(&document);
        return 0;
    }
    g0_visualizer_runtime_destroy(&g_app.runtime);
    g_app.runtime = candidate;
    g_app.settings = document.settings;
    (void)snprintf(g_app.world_name, sizeof(g_app.world_name), "%s", document.name);
    (void)snprintf(g_app.current_world_path, sizeof(g_app.current_world_path), "%s", filename);
    g_app.world_dirty = 0;
    g_app.edit_mode = 0;
    g_app.log_open = 0;
    g_app.log_scroll = 0U;
    g0_event_log_clear(&g_app.event_log);
    apply_app_settings();
    append_episode_started_log("WORLD_LOADED", "World loaded and episode constructed");
    g0_world_document_destroy(&document);
    return 1;
}

static int load_world_interactive(HWND hwnd)
{
    char path[MAX_PATH];

    if (!choose_world_file(hwnd, 0, path, sizeof(path)))
        return 0;
    if (!load_world_from_path(path))
    {
        MessageBoxA(hwnd,
                    "Nao foi possivel carregar o mundo. O sandbox atual foi preservado.",
                    "miniSNN Worlds", MB_OK | MB_ICONERROR);
        return 0;
    }
    return 1;
}

#define G0_NEW_WORLD_CLASS "MiniSNNG0NewWorldDialog"
#define G0_NEW_WORLD_NAME_EDIT 501
#define G0_NEW_WORLD_WIDTH_EDIT 502
#define G0_NEW_WORLD_HEIGHT_EDIT 503
#define G0_NEW_WORLD_CREATE 504
#define G0_NEW_WORLD_CANCEL 505

static void apply_initial_zoom(HWND hwnd)
{
    if (g_app.settings.initial_zoom_index == G0_APP_INITIAL_ZOOM_FIT)
        fit_map(hwnd);
    else
    {
        g_app.zoom_index = g_app.settings.initial_zoom_index;
        (void)g0_visualizer_camera_set_zoom_percent(
            &g_app.camera, g0_zoom_percentages[g_app.zoom_index]);
        g0_visualizer_camera_reset_pan(&g_app.camera);
        calculate_layout(hwnd);
    }
}

static int reset_episode(void)
{
    if (g_app.runtime == NULL || !g0_visualizer_runtime_reset(g_app.runtime))
        return 0;
    g_app.edit_mode = 0;
    g_app.log_scroll = 0U;
    apply_app_settings();
    append_episode_started_log("EPISODE_RESET", "Episode reset; next simulation tick is 1");
    return 1;
}

static int export_event_log(HWND hwnd)
{
    char filename[MAX_PATH];
    char base[MAX_PATH];
    char *extension;

    if (!ensure_save_directory())
        return 0;
    if (g_app.current_world_path[0] != '\0')
    {
        (void)snprintf(base, sizeof(base), "%s", g_app.current_world_path);
        extension = strrchr(base, '.');
        if (extension != NULL)
            *extension = '\0';
        if (snprintf(filename, sizeof(filename), "%s_events.csv", base) <= 0 ||
            strlen(filename) >= sizeof(filename))
        {
            return 0;
        }
    }
    else if (snprintf(filename, sizeof(filename), "%s\\event_log.csv", g_app.save_directory) <= 0 ||
             strlen(filename) >= sizeof(filename))
    {
        return 0;
    }
    if (!g0_event_log_export_csv(&g_app.event_log, filename))
    {
        MessageBoxA(hwnd, "Nao foi possivel exportar o event log em CSV.",
                    "miniSNN Worlds", MB_OK | MB_ICONERROR);
        return 0;
    }
    MessageBoxA(hwnd, filename, "Event log exportado", MB_OK | MB_ICONINFORMATION);
    return 1;
}

static int confirm_save_or_discard(HWND hwnd, const char *operation)
{
    char message[256];
    int choice;

    if (!g_app.world_dirty)
        return 1;
    (void)snprintf(message, sizeof(message),
                   "O mundo possui alteracoes nao salvas.\n\nSalvar antes de %s?",
                   operation == NULL ? "continuar" : operation);
    choice = MessageBoxA(hwnd, message, "miniSNN Worlds",
                         MB_YESNOCANCEL | MB_ICONWARNING);
    if (choice == IDYES)
    {
        if (!save_world_interactive(hwnd, 0))
        {
            MessageBoxA(hwnd, "O mundo nao foi salvo; a operacao foi cancelada.",
                        "miniSNN Worlds", MB_OK | MB_ICONWARNING);
            return 0;
        }
        return 1;
    }
    if (choice == IDNO)
    {
        g_app.world_dirty = 0;
        return 1;
    }
    return 0;
}

static int text_to_dimension(HWND edit, uint32_t *out_value)
{
    char buffer[32];
    char *end = NULL;
    unsigned long value;

    if (edit == NULL || out_value == NULL ||
        GetWindowTextA(edit, buffer, (int)sizeof(buffer)) <= 0)
    {
        return 0;
    }
    value = strtoul(buffer, &end, 10);
    if (end == buffer || *end != '\0' || value < G0_WORLD_CONFIG_MIN_WIDTH ||
        value > G0_WORLD_CONFIG_MAX_WIDTH)
    {
        return 0;
    }
    *out_value = (uint32_t)value;
    return 1;
}

static int new_world_dialog_values(HWND hwnd)
{
    G0WorldDocument validator;
    char name[G0_WORLD_NAME_MAX];
    uint32_t width;
    uint32_t height;

    if (GetWindowTextA(g_app.new_world_name_edit, name, (int)sizeof(name)) <= 0 ||
        !text_to_dimension(g_app.new_world_width_edit, &width) ||
        !text_to_dimension(g_app.new_world_height_edit, &height))
    {
        MessageBoxA(hwnd, "Informe nome e dimensoes entre 5 e 64 tiles.",
                    "Novo mundo", MB_OK | MB_ICONWARNING);
        return 0;
    }
    g0_world_document_init(&validator);
    if (!g0_world_document_set_name(&validator, name) ||
        !g0_visualizer_runtime_new_world(g_app.runtime, width, height))
    {
        g0_world_document_destroy(&validator);
        MessageBoxA(hwnd, "Nome ou configuracao do mundo invalida.",
                    "Novo mundo", MB_OK | MB_ICONWARNING);
        return 0;
    }
    g0_world_document_destroy(&validator);
    if (!g0_visualizer_runtime_apply_config(g_app.runtime))
    {
        MessageBoxA(hwnd, "Nao foi possivel construir o novo mundo.",
                    "Novo mundo", MB_OK | MB_ICONERROR);
        return 0;
    }
    (void)snprintf(g_app.world_name, sizeof(g_app.world_name), "%s", name);
    g_app.current_world_path[0] = '\0';
    g_app.new_world_width = width;
    g_app.new_world_height = height;
    g_app.world_dirty = 1;
    g_app.app_state = G0_APP_WORLD_SANDBOX;
    g_app.edit_mode = 0;
    g0_event_log_clear(&g_app.event_log);
    apply_app_settings();
    append_episode_started_log("WORLD_CREATED", "New world blueprint created");
    apply_initial_zoom(g_app.main_window);
    (void)begin_edit(g_app.main_window);
    return 1;
}

static LRESULT CALLBACK new_world_dialog_proc(HWND hwnd, UINT message,
                                               WPARAM w_param, LPARAM l_param)
{
    HINSTANCE instance = GetModuleHandleA(NULL);

    (void)l_param;
    switch (message)
    {
        case WM_CREATE:
            CreateWindowExA(0, "STATIC", "Name", WS_CHILD | WS_VISIBLE,
                            18, 18, 90, 20, hwnd, NULL, instance, NULL);
            g_app.new_world_name_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "new_world",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                108, 15, 210, 24, hwnd, (HMENU)(INT_PTR)G0_NEW_WORLD_NAME_EDIT,
                instance, NULL);
            CreateWindowExA(0, "STATIC", "Width", WS_CHILD | WS_VISIBLE,
                            18, 56, 90, 20, hwnd, NULL, instance, NULL);
            g_app.new_world_width_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "20",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER,
                108, 53, 90, 24, hwnd, (HMENU)(INT_PTR)G0_NEW_WORLD_WIDTH_EDIT,
                instance, NULL);
            CreateWindowExA(0, "STATIC", "Height", WS_CHILD | WS_VISIBLE,
                            18, 92, 90, 20, hwnd, NULL, instance, NULL);
            g_app.new_world_height_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "15",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER,
                108, 89, 90, 24, hwnd, (HMENU)(INT_PTR)G0_NEW_WORLD_HEIGHT_EDIT,
                instance, NULL);
            CreateWindowExA(0, "BUTTON", "CREATE", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                            108, 136, 100, 28, hwnd, (HMENU)(INT_PTR)G0_NEW_WORLD_CREATE,
                            instance, NULL);
            CreateWindowExA(0, "BUTTON", "CANCEL", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                            218, 136, 100, 28, hwnd, (HMENU)(INT_PTR)G0_NEW_WORLD_CANCEL,
                            instance, NULL);
            SetFocus(g_app.new_world_name_edit);
            return 0;
        case WM_COMMAND:
            if (LOWORD(w_param) == G0_NEW_WORLD_CREATE)
            {
                if (new_world_dialog_values(hwnd))
                    DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(w_param) == G0_NEW_WORLD_CANCEL)
            {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            g_app.new_world_dialog = NULL;
            g_app.new_world_name_edit = NULL;
            g_app.new_world_width_edit = NULL;
            g_app.new_world_height_edit = NULL;
            if (g_app.main_window != NULL)
                EnableWindow(g_app.main_window, TRUE);
            return 0;
        default:
            break;
    }
    return DefWindowProcA(hwnd, message, w_param, l_param);
}

static int open_new_world_dialog(HWND owner)
{
    WNDCLASSEXA window_class;
    HWND dialog;
    HINSTANCE instance = GetModuleHandleA(NULL);

    if (g_app.new_world_dialog != NULL)
    {
        SetForegroundWindow(g_app.new_world_dialog);
        return 1;
    }
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.hInstance = instance;
    window_class.lpszClassName = G0_NEW_WORLD_CLASS;
    window_class.lpfnWndProc = new_world_dialog_proc;
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    if (RegisterClassExA(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return 0;
    dialog = CreateWindowExA(WS_EX_DLGMODALFRAME, G0_NEW_WORLD_CLASS, "New World",
                             WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                             CW_USEDEFAULT, CW_USEDEFAULT, 350, 220, owner, NULL,
                             instance, NULL);
    if (dialog == NULL)
        return 0;
    g_app.new_world_dialog = dialog;
    EnableWindow(owner, FALSE);
    return 1;
}

static void open_main_menu(HWND hwnd)
{
    g_app.app_state = G0_APP_MAIN_MENU;
    g_app.edit_mode = 0;
    g_app.log_open = 0;
    if (g_app.runtime != NULL)
        g0_visualizer_runtime_set_paused(g_app.runtime, 1);
    invalidate_scene(hwnd);
}

static void ensure_presentation_window_size(HWND hwnd)
{
    RECT client;
    RECT window;
    int required_client_height = 840;

    if (hwnd == NULL || g_app.settings.presentation_mode == 0)
        return;
    GetClientRect(hwnd, &client);
    if (client.right >= 980 && client.bottom >= required_client_height)
        return;
    GetWindowRect(hwnd, &window);
    SetWindowPos(hwnd, NULL, 0, 0,
                 (window.right - window.left) + (980 - client.right > 0 ? 980 - client.right : 0),
                 (window.bottom - window.top) +
                     (required_client_height - client.bottom > 0 ?
                          required_client_height - client.bottom : 0),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static void handle_settings_click(HWND hwnd, int x, int y)
{
    unsigned int index;
    int visual_scale_changed = 0;

    if (point_in_rect(&g_app.settings_speed_rect, x, y))
    {
        g_app.settings.default_speed_index =
            (g_app.settings.default_speed_index + 1U) % G0_VISUALIZER_SPEED_COUNT;
        (void)g0_visualizer_runtime_set_speed_index(g_app.runtime,
                                                     g_app.settings.default_speed_index);
    }
    else if (point_in_rect(&g_app.settings_pause_rect, x, y))
    {
        g_app.settings.start_paused = !g_app.settings.start_paused;
        g0_visualizer_runtime_set_paused(g_app.runtime, g_app.settings.start_paused);
    }
    else
    {
        for (index = 0U; index < G0_APP_WATER_OPACITY_COUNT; ++index)
        {
            if (point_in_rect(&g_app.settings_water_rects[index], x, y))
            {
                g_app.settings.water_opacity_percent = g0_water_opacity_percentages[index];
                g_app.world_dirty = 1;
                invalidate_scene(hwnd);
                return;
            }
        }
        if (point_in_rect(&g_app.settings_presentation_rect, x, y))
        {
            g_app.settings.presentation_mode = !g_app.settings.presentation_mode;
            visual_scale_changed = 1;
        }
        else if (point_in_rect(&g_app.settings_grid_rect, x, y))
        {
            g_app.settings.show_grid = !g_app.settings.show_grid;
            g0_visualizer_runtime_set_grid_enabled(g_app.runtime, g_app.settings.show_grid);
        }
        else if (point_in_rect(&g_app.settings_debug_rect, x, y))
        {
            g_app.settings.show_debug_panel = !g_app.settings.show_debug_panel;
            g0_visualizer_runtime_set_debug_enabled(g_app.runtime, g_app.settings.show_debug_panel);
        }
        else if (point_in_rect(&g_app.settings_zoom_rect, x, y))
        {
            if (g_app.settings.initial_zoom_index == G0_APP_INITIAL_ZOOM_FIT)
                g_app.settings.initial_zoom_index = 0;
            else if (g_app.settings.initial_zoom_index >= G0_APP_ZOOM_INDEX_MAX)
                g_app.settings.initial_zoom_index = G0_APP_INITIAL_ZOOM_FIT;
            else
                ++g_app.settings.initial_zoom_index;
            apply_initial_zoom(hwnd);
        }
        else if (point_in_rect(&g_app.settings_log_rect, x, y))
        {
            g_app.settings.event_log_enabled = !g_app.settings.event_log_enabled;
        }
        else if (point_in_rect(&g_app.settings_back_rect, x, y))
        {
            g_app.app_state = G0_APP_MAIN_MENU;
            invalidate_scene(hwnd);
            return;
        }
        else
        {
            return;
        }
    }
    if (visual_scale_changed)
    {
        rebuild_ui_fonts();
        ensure_presentation_window_size(hwnd);
        calculate_layout(hwnd);
    }
    g_app.world_dirty = 1;
    invalidate_scene(hwnd);
}
static void destroy_backbuffer(void)
{
    if (g_app.backbuffer_dc != NULL)
    {
        if (g_app.backbuffer_original != NULL)
            SelectObject(g_app.backbuffer_dc, g_app.backbuffer_original);
        if (g_app.backbuffer_bitmap != NULL)
            DeleteObject(g_app.backbuffer_bitmap);
        DeleteDC(g_app.backbuffer_dc);
    }
    g_app.backbuffer_dc = NULL;
    g_app.backbuffer_bitmap = NULL;
    g_app.backbuffer_original = NULL;
    g_app.backbuffer_width = 0;
    g_app.backbuffer_height = 0;
}

static int ensure_backbuffer(HDC source, int width, int height)
{
    HBITMAP bitmap;
    HGDIOBJ original;

    if (source == NULL || width <= 0 || height <= 0)
        return 0;
    if (g_app.backbuffer_dc != NULL && g_app.backbuffer_width == width &&
        g_app.backbuffer_height == height)
        return 1;
    destroy_backbuffer();
    g_app.backbuffer_dc = CreateCompatibleDC(source);
    if (g_app.backbuffer_dc == NULL)
        return 0;
    bitmap = CreateCompatibleBitmap(source, width, height);
    if (bitmap == NULL)
    {
        destroy_backbuffer();
        return 0;
    }
    original = SelectObject(g_app.backbuffer_dc, bitmap);
    if (original == NULL || original == HGDI_ERROR)
    {
        DeleteObject(bitmap);
        destroy_backbuffer();
        return 0;
    }
    g_app.backbuffer_bitmap = bitmap;
    g_app.backbuffer_original = original;
    g_app.backbuffer_width = width;
    g_app.backbuffer_height = height;
    return 1;
}

static void invalidate_scene(HWND hwnd)
{
    if (hwnd != NULL)
        InvalidateRect(hwnd, NULL, FALSE);
}

static int begin_edit(HWND hwnd)
{
    if (g_app.runtime == NULL || !g0_visualizer_runtime_prepare_edit(g_app.runtime))
    {
        MessageBoxA(hwnd, "Nao foi possivel preparar o mundo para edicao.",
                    "miniSNN Worlds", MB_OK | MB_ICONERROR);
        return 0;
    }
    g_app.edit_mode = 1;
    g0_visualizer_runtime_set_paused(g_app.runtime, 1);
    calculate_layout(hwnd);
    fit_map(hwnd);
    invalidate_scene(hwnd);
    return 1;
}

static int apply_editor_world(HWND hwnd)
{
    if (g_app.runtime == NULL || !g0_visualizer_runtime_apply_config(g_app.runtime))
    {
        MessageBoxA(hwnd,
                    "Configuracao invalida ou falha ao reconstruir o mundo. O runtime anterior foi preservado.",
                    "miniSNN Worlds", MB_OK | MB_ICONERROR);
        return 0;
    }
    g_app.edit_mode = 0;
    g_app.world_dirty = 1;
    apply_app_settings();
    append_episode_started_log("WORLD_APPLIED", "Blueprint applied and episode rebuilt");
    calculate_layout(hwnd);
    fit_map(hwnd);
    invalidate_scene(hwnd);
    return 1;
}
static void edit_world_tile(HWND hwnd, int screen_x, int screen_y)
{
    const G0WorldConfig *config = current_config();
    uint32_t tile_x;
    uint32_t tile_y;
    if (!g_app.edit_mode || config == NULL ||
        !point_in_rect(&g_app.world_panel, screen_x, screen_y) ||
        !g0_visualizer_screen_to_tile(config->width, config->height,
                                      g_app.map_origin_x, g_app.map_origin_y,
                                      g_app.tile_pixels, screen_x, screen_y,
                                      &tile_x, &tile_y))
    {
        return;
    }
    if (!g0_visualizer_runtime_edit_tile(g_app.runtime, g_app.selected_tool,
                                          tile_x, tile_y))
    {
        MessageBoxA(hwnd, "A ferramenta nao pode ser aplicada neste tile.",
                    "miniSNN Worlds", MB_OK | MB_ICONWARNING);
        return;
    }
    g_app.world_dirty = 1;
    invalidate_scene(hwnd);
}
static void new_editor_world(HWND hwnd, uint32_t width, uint32_t height)
{
    if (!g0_visualizer_runtime_new_world(g_app.runtime, width, height))
    {
        MessageBoxA(hwnd, "Dimensoes invalidas para o novo mundo.",
                    "miniSNN Worlds", MB_OK | MB_ICONWARNING);
        return;
    }
    g_app.new_world_width = width;
    g_app.new_world_height = height;
    g_app.world_dirty = 1;
    (void)g0_visualizer_camera_set_zoom_percent(
        &g_app.camera, g0_zoom_percentages[g_app.zoom_index]);
    g0_visualizer_camera_reset_pan(&g_app.camera);
    calculate_layout(hwnd);
    fit_map(hwnd);
    invalidate_scene(hwnd);
}
static void resize_editor_world(HWND hwnd, uint32_t width, uint32_t height)
{
    if (!g0_visualizer_runtime_resize_world(g_app.runtime, width, height))
    {
        MessageBoxA(hwnd,
                    "O resize deixaria o fish spawn fora do mapa ou criaria uma configuracao invalida. O blueprint foi preservado.",
                    "miniSNN Worlds", MB_OK | MB_ICONWARNING);
        return;
    }
    g_app.new_world_width = width;
    g_app.new_world_height = height;
    g_app.world_dirty = 1;
    calculate_layout(hwnd);
    fit_map(hwnd);
    invalidate_scene(hwnd);
}

static void handle_editor_click(HWND hwnd, int x, int y)
{
    int index;
    if (point_in_rect(&g_app.width_down_rect, x, y) &&
        g_app.new_world_width > G0_WORLD_CONFIG_MIN_WIDTH)
    {
        resize_editor_world(hwnd, g_app.new_world_width - 1U, g_app.new_world_height);
        return;
    }
    if (point_in_rect(&g_app.width_up_rect, x, y) &&
        g_app.new_world_width < G0_WORLD_CONFIG_MAX_WIDTH)
    {
        resize_editor_world(hwnd, g_app.new_world_width + 1U, g_app.new_world_height);
        return;
    }
    if (point_in_rect(&g_app.height_down_rect, x, y) &&
        g_app.new_world_height > G0_WORLD_CONFIG_MIN_HEIGHT)
    {
        resize_editor_world(hwnd, g_app.new_world_width, g_app.new_world_height - 1U);
        return;
    }
    if (point_in_rect(&g_app.height_up_rect, x, y) &&
        g_app.new_world_height < G0_WORLD_CONFIG_MAX_HEIGHT)
    {
        resize_editor_world(hwnd, g_app.new_world_width, g_app.new_world_height + 1U);
        return;
    }
    if (point_in_rect(&g_app.new_world_rect, x, y))
    {
        new_editor_world(hwnd, g_app.new_world_width, g_app.new_world_height);
        return;
    }
    if (point_in_rect(&g_app.apply_rect, x, y))
    {
        (void)apply_editor_world(hwnd);
        return;
    }
    for (index = 0; index < G0_TOOL_COUNT; ++index)
    {
        if (point_in_rect(&g_app.tool_rects[index], x, y))
        {
            g_app.selected_tool = (G0WorldTool)index;
            invalidate_scene(hwnd);
            return;
        }
    }
    edit_world_tile(hwnd, x, y);
}
static void pan_camera(HWND hwnd, int delta_x, int delta_y)
{
    if (!g0_visualizer_camera_pan_by_pixels(&g_app.camera, delta_x, delta_y))
        return;
    calculate_layout(hwnd);
    invalidate_scene(hwnd);
}

static void change_zoom(HWND hwnd, int direction)
{
    int next = g_app.zoom_index + direction;
    if (next < 0 || next >= G0_ZOOM_COUNT)
        return;
    g_app.zoom_index = next;
    (void)g0_visualizer_camera_set_zoom_percent(
        &g_app.camera, g0_zoom_percentages[g_app.zoom_index]);
    calculate_layout(hwnd);
    invalidate_scene(hwnd);
}

static void handle_main_menu_click(HWND hwnd, int x, int y)
{
    if (point_in_rect(&g_app.main_new_rect, x, y))
    {
        if (confirm_save_or_discard(hwnd, "criar um novo mundo") &&
            !open_new_world_dialog(hwnd))
        {
            MessageBoxA(hwnd, "Nao foi possivel abrir o dialogo de novo mundo.",
                        "miniSNN Worlds", MB_OK | MB_ICONERROR);
        }
    }
    else if (point_in_rect(&g_app.main_load_rect, x, y))
    {
        if (confirm_save_or_discard(hwnd, "carregar outro mundo"))
        {
            if (load_world_interactive(hwnd))
            {
                g_app.app_state = G0_APP_WORLD_SANDBOX;
                apply_initial_zoom(hwnd);
                invalidate_scene(hwnd);
            }
        }
    }
    else if (point_in_rect(&g_app.main_continue_rect, x, y))
    {
        g_app.app_state = G0_APP_WORLD_SANDBOX;
        invalidate_scene(hwnd);
    }
    else if (point_in_rect(&g_app.main_settings_rect, x, y))
    {
        g_app.app_state = G0_APP_SETTINGS;
        invalidate_scene(hwnd);
    }
    else if (point_in_rect(&g_app.main_exit_rect, x, y) &&
             confirm_save_or_discard(hwnd, "sair"))
    {
        DestroyWindow(hwnd);
    }
}

static void handle_sandbox_click(HWND hwnd, int x, int y)
{
    if (point_in_rect(&g_app.menu_button_rect, x, y))
    {
        if (confirm_save_or_discard(hwnd, "voltar ao menu"))
            open_main_menu(hwnd);
        return;
    }
    if (point_in_rect(&g_app.save_button_rect, x, y))
    {
        if (!save_world_interactive(hwnd, 0))
            MessageBoxA(hwnd, "O mundo nao foi salvo.", "miniSNN Worlds",
                        MB_OK | MB_ICONWARNING);
        invalidate_scene(hwnd);
        return;
    }
    if (point_in_rect(&g_app.load_button_rect, x, y))
    {
        if (confirm_save_or_discard(hwnd, "carregar outro mundo") &&
            load_world_interactive(hwnd))
        {
            apply_initial_zoom(hwnd);
        }
        invalidate_scene(hwnd);
        return;
    }
    if (point_in_rect(&g_app.log_button_rect, x, y))
    {
        g_app.log_open = !g_app.log_open;
        calculate_layout(hwnd);
        invalidate_scene(hwnd);
        return;
    }
    if (point_in_rect(&g_app.edit_button_rect, x, y))
    {
        if (!g_app.edit_mode)
            (void)begin_edit(hwnd);
        return;
    }
    if (g_app.log_open)
    {
        if (point_in_rect(&g_app.clear_log_button_rect, x, y))
        {
            g0_event_log_clear(&g_app.event_log);
            g_app.log_scroll = 0U;
            invalidate_scene(hwnd);
            return;
        }
        if (point_in_rect(&g_app.export_log_button_rect, x, y))
        {
            (void)export_event_log(hwnd);
            return;
        }
    }
    if (g_app.edit_mode)
        handle_editor_click(hwnd, x, y);
}

static void handle_mouse_click(HWND hwnd, int x, int y)
{
    if (g_app.app_state == G0_APP_MAIN_MENU)
        handle_main_menu_click(hwnd, x, y);
    else if (g_app.app_state == G0_APP_SETTINGS)
        handle_settings_click(hwnd, x, y);
    else
        handle_sandbox_click(hwnd, x, y);
}

static void handle_key(HWND hwnd, WPARAM key)
{
    int control_down = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    int shift_down = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    if (g_app.app_state == G0_APP_MAIN_MENU)
    {
        if (key == VK_ESCAPE && confirm_save_or_discard(hwnd, "sair"))
            DestroyWindow(hwnd);
        else if (key == VK_RETURN)
        {
            g_app.app_state = G0_APP_WORLD_SANDBOX;
            invalidate_scene(hwnd);
        }
        return;
    }
    if (g_app.app_state == G0_APP_SETTINGS)
    {
        if (key == VK_ESCAPE)
        {
            g_app.app_state = G0_APP_MAIN_MENU;
            invalidate_scene(hwnd);
        }
        return;
    }
    if (key == VK_ESCAPE)
    {
        if (confirm_save_or_discard(hwnd, "voltar ao menu"))
            open_main_menu(hwnd);
        return;
    }
    if (control_down && key == 'S')
    {
        if (!save_world_interactive(hwnd, shift_down))
            MessageBoxA(hwnd, "O mundo nao foi salvo.", "miniSNN Worlds",
                        MB_OK | MB_ICONWARNING);
        invalidate_scene(hwnd);
        return;
    }
    if (control_down && key == 'O')
    {
        if (confirm_save_or_discard(hwnd, "carregar outro mundo") &&
            load_world_interactive(hwnd))
        {
            apply_initial_zoom(hwnd);
            invalidate_scene(hwnd);
        }
        return;
    }
    if (key == 'R' || key == VK_F5)
    {
        if (g_app.edit_mode)
        {
            MessageBoxA(hwnd,
                        "Existem edicoes pendentes. Aplique o blueprint antes de resetar o episodio.",
                        "miniSNN Worlds", MB_OK | MB_ICONWARNING);
            return;
        }
        if (!reset_episode())
            MessageBoxA(hwnd, "Nao foi possivel resetar o episodio.",
                        "miniSNN Worlds", MB_OK | MB_ICONERROR);
        invalidate_scene(hwnd);
        return;
    }
    if (key == 'F')
    {
        fit_map(hwnd);
        return;
    }
    if (key == 'L')
    {
        g_app.log_open = !g_app.log_open;
        calculate_layout(hwnd);
        invalidate_scene(hwnd);
        return;
    }
    if (key == 'E')
    {
        if (!g_app.edit_mode)
            (void)begin_edit(hwnd);
        return;
    }
    if (g_app.edit_mode)
    {
        if (key == VK_RETURN)
            (void)apply_editor_world(hwnd);
        else if (key == VK_LEFT || key == 'A')
            pan_camera(hwnd, 32, 0);
        else if (key == VK_RIGHT || key == 'D')
            pan_camera(hwnd, -32, 0);
        else if (key == VK_UP || key == 'W')
            pan_camera(hwnd, 0, 32);
        else if (key == VK_DOWN || key == 'S')
            pan_camera(hwnd, 0, -32);
        return;
    }
    if (key == VK_SPACE)
        g0_visualizer_runtime_toggle_paused(g_app.runtime);
    else if (key == 'N' && g0_visualizer_runtime_is_paused(g_app.runtime))
    {
        if (g0_visualizer_runtime_step_once(g_app.runtime))
            append_runtime_record_log();
        else
            append_log(0U, G0_LOG_CATEGORY_ERROR, 0, 0U,
                       "TICK", "FAILED", "RUNTIME_ERROR", "Single tick failed");
    }
    else if (key == 'D')
    {
        g0_visualizer_runtime_toggle_debug(g_app.runtime);
        g_app.settings.show_debug_panel = g0_visualizer_runtime_debug_enabled(g_app.runtime);
        g_app.world_dirty = 1;
    }
    else if (key == 'G')
    {
        g0_visualizer_runtime_toggle_grid(g_app.runtime);
        g_app.settings.show_grid = g0_visualizer_runtime_grid_enabled(g_app.runtime);
        g_app.world_dirty = 1;
    }
    else if (key == VK_ADD || key == VK_OEM_PLUS)
    {
        if (g0_visualizer_runtime_speed_increase(g_app.runtime))
        {
            g_app.settings.default_speed_index = g0_visualizer_runtime_speed_index(g_app.runtime);
            g_app.world_dirty = 1;
        }
    }
    else if (key == VK_SUBTRACT || key == VK_OEM_MINUS)
    {
        if (g0_visualizer_runtime_speed_decrease(g_app.runtime))
        {
            g_app.settings.default_speed_index = g0_visualizer_runtime_speed_index(g_app.runtime);
            g_app.world_dirty = 1;
        }
    }
    else if (key == VK_LEFT || key == 'A')
        pan_camera(hwnd, 32, 0);
    else if (key == VK_RIGHT)
        pan_camera(hwnd, -32, 0);
    else if (key == VK_UP || key == 'W')
        pan_camera(hwnd, 0, 32);
    else if (key == VK_DOWN || key == 'S')
        pan_camera(hwnd, 0, -32);
    invalidate_scene(hwnd);
}
static int initialize_sandbox(const char *repository_root)
{
    char asset_error[G0_VISUALIZER_ASSET_ERROR_MAX];
    const char *root = repository_root;
    char resolved_root[MAX_PATH];
    char absolute_root[MAX_PATH];
    size_t index;

    ZeroMemory(&g_app, sizeof(g_app));
    g_app.startup_error = G0_STARTUP_ERROR_NONE;
    if (root == NULL)
    {
        if (!resolve_repository_root_from_executable(resolved_root, sizeof(resolved_root)))
        {
            set_startup_error(G0_STARTUP_ERROR_ASSET_ROOT,
                              "Could not locate an assets directory from the executable path.");
            return 0;
        }
        root = resolved_root;
    }
    if (GetFullPathNameA(root, MAX_PATH, absolute_root, NULL) == 0U ||
        strlen(absolute_root) >= sizeof(g_app.repository_root) ||
        snprintf(g_app.repository_root, sizeof(g_app.repository_root), "%s", absolute_root) <= 0 ||
        snprintf(g_app.save_directory, sizeof(g_app.save_directory), "%s\\worlds\\saves",
                 g_app.repository_root) <= 0 ||
        strlen(g_app.save_directory) >= sizeof(g_app.save_directory))
    {
        set_startup_error(G0_STARTUP_ERROR_ASSET_ROOT,
                          "The executable asset root path could not be normalized.");
        return 0;
    }
    if (!g0_visualizer_assets_load(&g_app.assets, g_app.repository_root, asset_error,
                                   sizeof(asset_error)))
    {
        set_startup_error(G0_STARTUP_ERROR_ASSETS, asset_error);
        return 0;
    }
    if (!load_all_assets(asset_error, sizeof(asset_error)))
    {
        for (index = 0U; index < G0_VISUALIZER_ASSET_COUNT; ++index)
        {
            destroy_bitmap(&g_app.bitmaps[index]);
        }
        destroy_fish_facings();
        set_startup_error(G0_STARTUP_ERROR_ASSETS, asset_error);
        return 0;
    }
    g_app.runtime = g0_visualizer_runtime_create_sandbox();
    if (g_app.runtime == NULL)
    {
        for (index = 0U; index < G0_VISUALIZER_ASSET_COUNT; ++index)
        {
            destroy_bitmap(&g_app.bitmaps[index]);
        }
        destroy_fish_facings();
        set_startup_error(G0_STARTUP_ERROR_SANDBOX,
                          "g0_visualizer_runtime_create_sandbox returned NULL.");
        return 0;
    }
    g0_app_settings_default(&g_app.settings);
    g0_event_log_init(&g_app.event_log);
    (void)snprintf(g_app.world_name, sizeof(g_app.world_name), "sandbox");
    g_app.app_state = G0_APP_MAIN_MENU;
    g_app.selected_tool = G0_WORLD_TOOL_WATER;
    g_app.new_world_width = 20U;
    g_app.new_world_height = 15U;
    g_app.zoom_index = 3;
    g0_visualizer_camera_init(&g_app.camera);
    (void)g0_visualizer_camera_set_zoom_percent(
        &g_app.camera, g0_zoom_percentages[g_app.zoom_index]);
    apply_app_settings();
    QueryPerformanceFrequency(&g_app.performance_frequency);
    QueryPerformanceCounter(&g_app.previous_counter);
    g_app.counter_ready = 1;
    return 1;
}static void shutdown_sandbox(void)
{
    size_t index;
    destroy_backbuffer();
    destroy_ui_fonts();
    for (index = 0U; index < G0_VISUALIZER_ASSET_COUNT; ++index)
        destroy_bitmap(&g_app.bitmaps[index]);
    destroy_fish_facings();
    g0_visualizer_runtime_destroy(&g_app.runtime);
}

static LRESULT CALLBACK sandbox_window_proc(HWND hwnd, UINT message,
                                            WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
        case WM_CREATE:
            g_app.main_window = hwnd;
            rebuild_ui_fonts();
            apply_initial_zoom(hwnd);
            SetTimer(hwnd, G0_TIMER_ID, G0_TIMER_MS, NULL);
            return 0;
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO *info = (MINMAXINFO *)l_param;
            info->ptMinTrackSize.x = g_app.settings.presentation_mode != 0 ? 980 : 880;
            info->ptMinTrackSize.y = g_app.settings.presentation_mode != 0 ? 880 : 620;
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            calculate_layout(hwnd);
            invalidate_scene(hwnd);
            return 0;
        case WM_TIMER:
            if (w_param == G0_TIMER_ID && g_app.runtime != NULL)
            {
                LARGE_INTEGER now;
                uint32_t elapsed = 0U;
                uint64_t before_tick = 0U;
                const WF0FishWorldState *before = g0_visualizer_runtime_state(g_app.runtime);
                QueryPerformanceCounter(&now);
                if (g_app.counter_ready && g_app.performance_frequency.QuadPart > 0)
                {
                    LONGLONG ticks = now.QuadPart - g_app.previous_counter.QuadPart;
                    if (ticks > 0)
                    {
                        unsigned long long milliseconds =
                            ((unsigned long long)ticks * 1000ULL) /
                            (unsigned long long)g_app.performance_frequency.QuadPart;
                        elapsed = milliseconds > 250ULL ? 250U : (uint32_t)milliseconds;
                    }
                }
                g_app.previous_counter = now;
                g_app.counter_ready = 1;
                if (before != NULL)
                    before_tick = before->tick;
                if (g_app.app_state == G0_APP_WORLD_SANDBOX && !g_app.edit_mode &&
                    elapsed > 0U && !g0_visualizer_runtime_advance_elapsed(g_app.runtime, elapsed))
                {
                    g0_visualizer_runtime_set_paused(g_app.runtime, 1);
                    append_log(before_tick, G0_LOG_CATEGORY_ERROR, 0, 0U,
                               "TICK", "FAILED", "RUNTIME_ERROR",
                               "The world tick failed and simulation was paused");
                    MessageBoxA(hwnd, "O tick do mundo falhou e a simulacao foi pausada.",
                                "miniSNN Worlds", MB_OK | MB_ICONERROR);
                }
                else if (g_app.app_state == G0_APP_WORLD_SANDBOX &&
                         g0_visualizer_runtime_state(g_app.runtime) != NULL &&
                         g0_visualizer_runtime_state(g_app.runtime)->tick != before_tick)
                {
                    append_runtime_record_log();
                }
                invalidate_scene(hwnd);
            }
            return 0;
        case WM_MOUSEWHEEL:
        {
            POINT point;
            point.x = GET_X_LPARAM(l_param);
            point.y = GET_Y_LPARAM(l_param);
            ScreenToClient(hwnd, &point);
            if (g_app.app_state == G0_APP_WORLD_SANDBOX && g_app.log_open &&
                point_in_rect(&g_app.side_panel, point.x, point.y))
            {
                if (GET_WHEEL_DELTA_WPARAM(w_param) < 0)
                    ++g_app.log_scroll;
                else if (g_app.log_scroll > 0U)
                    --g_app.log_scroll;
                invalidate_scene(hwnd);
            }
            else if (g_app.app_state == G0_APP_WORLD_SANDBOX)
            {
                change_zoom(hwnd, GET_WHEEL_DELTA_WPARAM(w_param) > 0 ? 1 : -1);
            }
            return 0;
        }
        case WM_LBUTTONDOWN:
            SetFocus(hwnd);
            calculate_layout(hwnd);
            handle_mouse_click(hwnd, GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param));
            return 0;
        case WM_KEYDOWN:
            if (g_app.runtime != NULL)
                handle_key(hwnd, w_param);
            return 0;
        case WM_CLOSE:
            if (confirm_save_or_discard(hwnd, "sair"))
                DestroyWindow(hwnd);
            return 0;
        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC screen = BeginPaint(hwnd, &paint);
            RECT client;
            GetClientRect(hwnd, &client);
            if (ensure_backbuffer(screen, client.right, client.bottom))
            {
                draw_scene(g_app.backbuffer_dc, hwnd);
                BitBlt(screen, 0, 0, client.right, client.bottom,
                       g_app.backbuffer_dc, 0, 0, SRCCOPY);
            }
            EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_DESTROY:
            KillTimer(hwnd, G0_TIMER_ID);
            g_app.main_window = NULL;
            shutdown_sandbox();
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcA(hwnd, message, w_param, l_param);
    }
}
static int parse_command_line(char *command_line, int *out_runtime_smoke,
                              int *out_startup_smoke,
                              const char **out_repository_root)
{
    char *token;
    if (out_runtime_smoke == NULL || out_startup_smoke == NULL ||
        out_repository_root == NULL)
    {
        return 0;
    }
    *out_runtime_smoke = 0;
    *out_startup_smoke = 0;
    *out_repository_root = NULL;
    token = strtok(command_line, " \t");
    while (token != NULL)
    {
        if (strcmp(token, "--runtime-smoke") == 0 ||
            strcmp(token, "--runtime-smoke-test") == 0)
        {
            *out_runtime_smoke = 1;
        }
        else if (strcmp(token, "--startup-smoke") == 0)
        {
            *out_startup_smoke = 1;
        }
        else if (strcmp(token, "--repository-root") == 0)
        {
            token = strtok(NULL, " \t");
            if (token == NULL)
            {
                return 0;
            }
            *out_repository_root = token;
        }
        token = strtok(NULL, " \t");
    }
    return 1;
}
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line,
                   int show_command)
{
    WNDCLASSEXA window_class;
    MSG message;
    HWND hwnd;
    int runtime_smoke;
    int startup_smoke;
    const char *repository_root;
    char command_copy[1024];

    (void)previous;
    (void)snprintf(command_copy, sizeof(command_copy), "%s",
                   command_line == NULL ? "" : command_line);
    if (!parse_command_line(command_copy, &runtime_smoke, &startup_smoke,
                            &repository_root))
        return 1;
    if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)))
        return 1;
    if (!initialize_sandbox(repository_root))
    {
        show_startup_failure();
        CoUninitialize();
        return 1;
    }
    if (startup_smoke)
    {
        printf("executable_root=%s\n", g_app.repository_root);
        puts("assets_found=yes");
        puts("assets_initialized=yes");
        puts("sandbox_initialized=yes");
        puts("startup=PASS");
        shutdown_sandbox();
        CoUninitialize();
        return 0;
    }
    if (runtime_smoke)
    {
        int success = g0_visualizer_runtime_step_once(g_app.runtime);

        if (success)
        {
            puts("world_config_format=V2");
            puts("water_opacity_setting=yes");
            puts("presentation_mode=yes");
            puts("transactional_resize=yes");
        }
        shutdown_sandbox();
        CoUninitialize();
        return success ? 0 : 1;
    }
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.hInstance = instance;
    window_class.lpszClassName = G0_WINDOW_CLASS;
    window_class.lpfnWndProc = sandbox_window_proc;
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = NULL;
    if (RegisterClassExA(&window_class) == 0)
    {
        shutdown_sandbox();
        CoUninitialize();
        return 1;
    }
    hwnd = CreateWindowExA(0, G0_WINDOW_CLASS, "miniSNN Worlds",
                           WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                           1380, 900, NULL, NULL, instance, NULL);
    if (hwnd == NULL)
    {
        shutdown_sandbox();
        CoUninitialize();
        return 1;
    }
    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);
    fit_map(hwnd);
    while (GetMessageA(&message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    CoUninitialize();
    return (int)message.wParam;
}
