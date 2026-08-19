#ifndef G0_APP_SETTINGS_H
#define G0_APP_SETTINGS_H

#include <stdint.h>

#include "g0_visualizer_runtime.h"

#define G0_APP_INITIAL_ZOOM_FIT (-1)
#define G0_APP_ZOOM_INDEX_MIN 0
#define G0_APP_ZOOM_INDEX_MAX 7
#define G0_APP_WATER_OPACITY_DEFAULT 35
#define G0_APP_WATER_OPACITY_PRESENTATION 25
#define G0_APP_WATER_OPACITY_COUNT 4

/*
 * Application settings are deliberately separate from Kernel, Domain, and the
 * editable world blueprint. A saved world may carry these presentation and
 * scheduler preferences, but they never change the deterministic WF0 model.
 */
typedef struct
{
    unsigned int default_speed_index;
    int start_paused;
    int show_grid;
    int show_debug_panel;
    int initial_zoom_index;
    int event_log_enabled;
    int water_opacity_percent;
    int presentation_mode;
} G0AppSettings;

void g0_app_settings_default(G0AppSettings *settings);
int g0_app_settings_validate(const G0AppSettings *settings);
int g0_app_settings_equal(const G0AppSettings *left, const G0AppSettings *right);
int g0_app_settings_water_opacity_supported(int percent);
int g0_app_settings_effective_water_opacity_percent(const G0AppSettings *settings);
uint8_t g0_app_settings_effective_water_alpha(const G0AppSettings *settings);

#endif