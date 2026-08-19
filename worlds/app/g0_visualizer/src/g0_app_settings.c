#include "g0_app_settings.h"

#include <string.h>

static const int g0_water_opacity_values[G0_APP_WATER_OPACITY_COUNT] =
{ 25, 35, 50, 65 };

void g0_app_settings_default(G0AppSettings *settings)
{
    if (settings == NULL)
        return;
    memset(settings, 0, sizeof(*settings));
    settings->default_speed_index = 2U;
    settings->show_grid = 1;
    settings->show_debug_panel = 1;
    settings->initial_zoom_index = G0_APP_INITIAL_ZOOM_FIT;
    settings->event_log_enabled = 1;
    settings->water_opacity_percent = G0_APP_WATER_OPACITY_DEFAULT;
    settings->presentation_mode = 0;
}

int g0_app_settings_water_opacity_supported(int percent)
{
    unsigned int index;

    for (index = 0U; index < G0_APP_WATER_OPACITY_COUNT; ++index)
    {
        if (g0_water_opacity_values[index] == percent)
            return 1;
    }
    return 0;
}

int g0_app_settings_validate(const G0AppSettings *settings)
{
    return settings != NULL &&
           settings->default_speed_index < G0_VISUALIZER_SPEED_COUNT &&
           (settings->start_paused == 0 || settings->start_paused == 1) &&
           (settings->show_grid == 0 || settings->show_grid == 1) &&
           (settings->show_debug_panel == 0 || settings->show_debug_panel == 1) &&
           (settings->initial_zoom_index == G0_APP_INITIAL_ZOOM_FIT ||
            (settings->initial_zoom_index >= G0_APP_ZOOM_INDEX_MIN &&
             settings->initial_zoom_index <= G0_APP_ZOOM_INDEX_MAX)) &&
           (settings->event_log_enabled == 0 || settings->event_log_enabled == 1) &&
           g0_app_settings_water_opacity_supported(settings->water_opacity_percent) &&
           (settings->presentation_mode == 0 || settings->presentation_mode == 1);
}

int g0_app_settings_equal(const G0AppSettings *left, const G0AppSettings *right)
{
    return left != NULL && right != NULL &&
           left->default_speed_index == right->default_speed_index &&
           left->start_paused == right->start_paused &&
           left->show_grid == right->show_grid &&
           left->show_debug_panel == right->show_debug_panel &&
           left->initial_zoom_index == right->initial_zoom_index &&
           left->event_log_enabled == right->event_log_enabled &&
           left->water_opacity_percent == right->water_opacity_percent &&
           left->presentation_mode == right->presentation_mode;
}

int g0_app_settings_effective_water_opacity_percent(const G0AppSettings *settings)
{
    if (!g0_app_settings_validate(settings))
        return 0;
    return settings->presentation_mode != 0 ?
        G0_APP_WATER_OPACITY_PRESENTATION : settings->water_opacity_percent;
}

uint8_t g0_app_settings_effective_water_alpha(const G0AppSettings *settings)
{
    int percent = g0_app_settings_effective_water_opacity_percent(settings);

    if (percent <= 0)
        return UINT8_C(0);
    return (uint8_t)((percent * 255 + 50) / 100);
}