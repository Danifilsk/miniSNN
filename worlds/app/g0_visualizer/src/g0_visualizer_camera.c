#include <limits.h>
#include <string.h>

#include "g0_visualizer_camera.h"

#define G0_VISUALIZER_CAMERA_BASE_TILE_PIXELS 64

static int64_t floor_divide(int64_t numerator, int64_t denominator)
{
    if (numerator >= 0)
        return numerator / denominator;
    return -(((-numerator) + denominator - 1) / denominator);
}

static int64_t ceil_divide(int64_t numerator, int64_t denominator)
{
    return -floor_divide(-numerator, denominator);
}

static int64_t clamp_i64(int64_t value, int64_t minimum, int64_t maximum)
{
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

static int units_to_pixels(int64_t units, int tile_pixels)
{
    int64_t scaled = units * (int64_t)tile_pixels;

    if (scaled >= 0)
        scaled += G0_VISUALIZER_CAMERA_PAN_SCALE / 2;
    else
        scaled -= G0_VISUALIZER_CAMERA_PAN_SCALE / 2;
    return (int)(scaled / G0_VISUALIZER_CAMERA_PAN_SCALE);
}

static int64_t pixels_to_units(int pixels, int tile_pixels)
{
    int64_t scaled = (int64_t)pixels * G0_VISUALIZER_CAMERA_PAN_SCALE;

    if (scaled >= 0)
        scaled += tile_pixels / 2;
    else
        scaled -= tile_pixels / 2;
    return scaled / tile_pixels;
}

static void layout_axis(
    int64_t *pan_units,
    int base,
    int map_pixels,
    int panel_start,
    int panel_end,
    int tile_pixels,
    int *out_pan,
    int *out_origin)
{
    int panel_pixels = panel_end - panel_start;
    int minimum_origin;
    int maximum_origin;
    int64_t minimum_units;
    int64_t maximum_units;
    int origin;

    if (map_pixels <= panel_pixels)
    {
        *out_pan = 0;
        *out_origin = base;
        return;
    }

    minimum_origin = panel_end - map_pixels + tile_pixels;
    maximum_origin = panel_start - tile_pixels;
    if (minimum_origin > maximum_origin)
    {
        *out_pan = 0;
        *out_origin = base;
        return;
    }

    minimum_units = ceil_divide(
        (int64_t)(minimum_origin - base) * G0_VISUALIZER_CAMERA_PAN_SCALE,
        tile_pixels);
    maximum_units = floor_divide(
        (int64_t)(maximum_origin - base) * G0_VISUALIZER_CAMERA_PAN_SCALE,
        tile_pixels);
    *pan_units = clamp_i64(*pan_units, minimum_units, maximum_units);
    origin = base + units_to_pixels(*pan_units, tile_pixels);
    if (origin < minimum_origin)
        origin = minimum_origin;
    if (origin > maximum_origin)
        origin = maximum_origin;
    *out_pan = origin - base;
    *out_origin = origin;
}

void g0_visualizer_camera_init(G0VisualizerCamera *camera)
{
    if (camera == NULL)
        return;
    memset(camera, 0, sizeof(*camera));
    camera->zoom_percent = 100;
    camera->tile_pixels = G0_VISUALIZER_CAMERA_BASE_TILE_PIXELS;
}

int g0_visualizer_camera_set_zoom_percent(
    G0VisualizerCamera *camera,
    int zoom_percent)
{
    if (camera == NULL || zoom_percent <= 0 ||
        zoom_percent > INT_MAX / G0_VISUALIZER_CAMERA_BASE_TILE_PIXELS)
    {
        return 0;
    }
    camera->zoom_percent = zoom_percent;
    return 1;
}

void g0_visualizer_camera_reset_pan(G0VisualizerCamera *camera)
{
    if (camera == NULL)
        return;
    camera->pan_x_units = 0;
    camera->pan_y_units = 0;
    camera->pan_x = 0;
    camera->pan_y = 0;
}

int g0_visualizer_camera_pan_by_pixels(
    G0VisualizerCamera *camera,
    int delta_x,
    int delta_y)
{
    if (camera == NULL || camera->tile_pixels <= 0)
        return 0;
    camera->pan_x_units += pixels_to_units(delta_x, camera->tile_pixels);
    camera->pan_y_units += pixels_to_units(delta_y, camera->tile_pixels);
    return 1;
}

int g0_visualizer_camera_layout(
    G0VisualizerCamera *camera,
    uint32_t map_width_tiles,
    uint32_t map_height_tiles,
    int panel_left,
    int panel_top,
    int panel_right,
    int panel_bottom)
{
    int panel_width;
    int panel_height;
    int map_width;
    int map_height;
    int base_x;
    int base_y;

    if (camera == NULL || map_width_tiles == 0U || map_height_tiles == 0U ||
        panel_right <= panel_left || panel_bottom <= panel_top ||
        camera->zoom_percent <= 0)
    {
        return 0;
    }
    camera->tile_pixels = (G0_VISUALIZER_CAMERA_BASE_TILE_PIXELS *
                           camera->zoom_percent) / 100;
    if (camera->tile_pixels < 1 ||
        map_width_tiles > (uint32_t)(INT_MAX / camera->tile_pixels) ||
        map_height_tiles > (uint32_t)(INT_MAX / camera->tile_pixels))
    {
        return 0;
    }
    panel_width = panel_right - panel_left;
    panel_height = panel_bottom - panel_top;
    map_width = (int)map_width_tiles * camera->tile_pixels;
    map_height = (int)map_height_tiles * camera->tile_pixels;
    base_x = panel_left + (panel_width - map_width) / 2;
    base_y = panel_top + (panel_height - map_height) / 2;
    layout_axis(&camera->pan_x_units, base_x, map_width, panel_left, panel_right,
                camera->tile_pixels, &camera->pan_x, &camera->map_origin_x);
    layout_axis(&camera->pan_y_units, base_y, map_height, panel_top, panel_bottom,
                camera->tile_pixels, &camera->pan_y, &camera->map_origin_y);
    return 1;
}
