#ifndef G0_VISUALIZER_UI_LAYOUT_H
#define G0_VISUALIZER_UI_LAYOUT_H

#include <stddef.h>

typedef struct
{
    int left;
    int top;
    int right;
    int bottom;
} G0VisualizerUiRect;

typedef enum
{
    G0_VISUALIZER_HEADER_MENU = 0,
    G0_VISUALIZER_HEADER_SAVE,
    G0_VISUALIZER_HEADER_LOAD,
    G0_VISUALIZER_HEADER_LOG,
    G0_VISUALIZER_HEADER_EDITING,
    G0_VISUALIZER_HEADER_CONTROL_COUNT
} G0VisualizerHeaderControl;

#define G0_VISUALIZER_SETTINGS_WATER_OPACITY_COUNT 4

typedef enum
{
    G0_VISUALIZER_SETTINGS_TITLE = 0,
    G0_VISUALIZER_SETTINGS_SIMULATION_LABEL,
    G0_VISUALIZER_SETTINGS_SPEED,
    G0_VISUALIZER_SETTINGS_START_PAUSED,
    G0_VISUALIZER_SETTINGS_VISUAL_LABEL,
    G0_VISUALIZER_SETTINGS_WATER_LABEL,
    G0_VISUALIZER_SETTINGS_WATER_25,
    G0_VISUALIZER_SETTINGS_WATER_35,
    G0_VISUALIZER_SETTINGS_WATER_50,
    G0_VISUALIZER_SETTINGS_WATER_65,
    G0_VISUALIZER_SETTINGS_PRESENTATION,
    G0_VISUALIZER_SETTINGS_GRID,
    G0_VISUALIZER_SETTINGS_DEBUG,
    G0_VISUALIZER_SETTINGS_INITIAL_ZOOM,
    G0_VISUALIZER_SETTINGS_LOG_LABEL,
    G0_VISUALIZER_SETTINGS_EVENT_LOG,
    G0_VISUALIZER_SETTINGS_BACK,
    G0_VISUALIZER_SETTINGS_CONTROL_COUNT
} G0VisualizerSettingsControl;

typedef struct
{
    G0VisualizerUiRect panel;
    G0VisualizerUiRect controls[G0_VISUALIZER_SETTINGS_CONTROL_COUNT];
    int bottom_padding;
} G0VisualizerSettingsLayout;

/* Computes the visible header controls for normal or presentation mode. */
int g0_visualizer_header_control_layout(
    int client_width,
    int presentation_mode,
    G0VisualizerUiRect out_controls[G0_VISUALIZER_HEADER_CONTROL_COUNT]);
int g0_visualizer_ui_rects_overlap(
    const G0VisualizerUiRect *left,
    const G0VisualizerUiRect *right);
int g0_visualizer_header_controls_are_disjoint(
    const G0VisualizerUiRect controls[G0_VISUALIZER_HEADER_CONTROL_COUNT]);

/* Computes the complete Settings panel from its content for either UI mode. */
int g0_visualizer_settings_layout(
    int client_width,
    int client_height,
    int presentation_mode,
    G0VisualizerSettingsLayout *out_layout);

/* Validates client bounds, control containment, non-overlap, and bottom padding. */
int g0_visualizer_settings_layout_is_valid(
    const G0VisualizerSettingsLayout *layout,
    int client_width,
    int client_height,
    int presentation_mode);

#endif