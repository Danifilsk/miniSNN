#ifndef G0_VISUALIZER_CAMERA_H
#define G0_VISUALIZER_CAMERA_H

#include <stdint.h>

/* Pan is retained in tile-relative fixed-point units and only rounded for paint. */
#define G0_VISUALIZER_CAMERA_PAN_SCALE 1024

typedef struct
{
    int zoom_percent;
    int tile_pixels;
    int64_t pan_x_units;
    int64_t pan_y_units;
    int pan_x;
    int pan_y;
    int map_origin_x;
    int map_origin_y;
} G0VisualizerCamera;

void g0_visualizer_camera_init(G0VisualizerCamera *camera);
int g0_visualizer_camera_set_zoom_percent(
    G0VisualizerCamera *camera,
    int zoom_percent);
void g0_visualizer_camera_reset_pan(G0VisualizerCamera *camera);
int g0_visualizer_camera_pan_by_pixels(
    G0VisualizerCamera *camera,
    int delta_x,
    int delta_y);

/* Recomputes paint-space values without deriving logical pan from rounded pixels. */
int g0_visualizer_camera_layout(
    G0VisualizerCamera *camera,
    uint32_t map_width_tiles,
    uint32_t map_height_tiles,
    int panel_left,
    int panel_top,
    int panel_right,
    int panel_bottom);

#endif
