#include "g0_visualizer_ui_layout.h"

static int g0_visualizer_ui_scaled(int value, int presentation_mode)
{
    if (presentation_mode == 0)
    {
        return value;
    }
    return (value * 3 + 1) / 2;
}

static void g0_visualizer_ui_rect_set(
    G0VisualizerUiRect *rect,
    int left,
    int top,
    int width,
    int height)
{
    rect->left = left;
    rect->top = top;
    rect->right = left + width;
    rect->bottom = top + height;
}

int g0_visualizer_ui_rects_overlap(
    const G0VisualizerUiRect *left,
    const G0VisualizerUiRect *right)
{
    if (left == NULL || right == NULL)
    {
        return 1;
    }
    return left->left < right->right && right->left < left->right &&
           left->top < right->bottom && right->top < left->bottom;
}

int g0_visualizer_header_controls_are_disjoint(
    const G0VisualizerUiRect controls[G0_VISUALIZER_HEADER_CONTROL_COUNT])
{
    size_t left;
    size_t right;

    if (controls == NULL)
    {
        return 0;
    }
    for (left = 0U; left < G0_VISUALIZER_HEADER_CONTROL_COUNT; ++left)
    {
        if (controls[left].left >= controls[left].right ||
            controls[left].top >= controls[left].bottom)
        {
            return 0;
        }
        for (right = left + 1U; right < G0_VISUALIZER_HEADER_CONTROL_COUNT; ++right)
        {
            if (g0_visualizer_ui_rects_overlap(&controls[left], &controls[right]))
            {
                return 0;
            }
        }
    }
    return 1;
}

int g0_visualizer_header_control_layout(
    int client_width,
    int presentation_mode,
    G0VisualizerUiRect out_controls[G0_VISUALIZER_HEADER_CONTROL_COUNT])
{
    int button_right;
    int button_height;
    int button_y;
    int small_width;
    int edit_width;
    int gap;

    if (out_controls == NULL || (presentation_mode != 0 && presentation_mode != 1))
    {
        return 0;
    }
    button_height = g0_visualizer_ui_scaled(28, presentation_mode);
    button_y = g0_visualizer_ui_scaled(16, presentation_mode);
    small_width = g0_visualizer_ui_scaled(58, presentation_mode);
    edit_width = g0_visualizer_ui_scaled(88, presentation_mode);
    gap = g0_visualizer_ui_scaled(6, presentation_mode);
    button_right = client_width - g0_visualizer_ui_scaled(18, presentation_mode);
    if (button_right < 4 * small_width + edit_width + 4 * gap)
    {
        return 0;
    }
    g0_visualizer_ui_rect_set(
        &out_controls[G0_VISUALIZER_HEADER_EDITING],
        button_right - edit_width,
        button_y,
        edit_width,
        button_height);
    button_right = out_controls[G0_VISUALIZER_HEADER_EDITING].left - gap;
    g0_visualizer_ui_rect_set(
        &out_controls[G0_VISUALIZER_HEADER_LOG],
        button_right - small_width,
        button_y,
        small_width,
        button_height);
    button_right = out_controls[G0_VISUALIZER_HEADER_LOG].left - gap;
    g0_visualizer_ui_rect_set(
        &out_controls[G0_VISUALIZER_HEADER_LOAD],
        button_right - small_width,
        button_y,
        small_width,
        button_height);
    button_right = out_controls[G0_VISUALIZER_HEADER_LOAD].left - gap;
    g0_visualizer_ui_rect_set(
        &out_controls[G0_VISUALIZER_HEADER_SAVE],
        button_right - small_width,
        button_y,
        small_width,
        button_height);
    button_right = out_controls[G0_VISUALIZER_HEADER_SAVE].left - gap;
    g0_visualizer_ui_rect_set(
        &out_controls[G0_VISUALIZER_HEADER_MENU],
        button_right - small_width,
        button_y,
        small_width,
        button_height);
    return g0_visualizer_header_controls_are_disjoint(out_controls);
}

static int g0_visualizer_ui_rect_inside(
    const G0VisualizerUiRect *inner,
    const G0VisualizerUiRect *outer)
{
    return inner != NULL && outer != NULL &&
           inner->left >= outer->left && inner->top >= outer->top &&
           inner->right <= outer->right && inner->bottom <= outer->bottom;
}

int g0_visualizer_settings_layout(
    int client_width,
    int client_height,
    int presentation_mode,
    G0VisualizerSettingsLayout *out_layout)
{
    int scale_padding;
    int title_height;
    int section_label_height;
    int row_height;
    int row_gap;
    int title_gap;
    int label_gap;
    int section_gap;
    int water_label_gap;
    int back_gap;
    int content_width;
    int panel_width;
    int panel_height;
    int panel_left;
    int panel_top;
    int water_width;
    int y;
    int index;

    if (out_layout == NULL || client_width <= 0 || client_height <= 0 ||
        (presentation_mode != 0 && presentation_mode != 1))
    {
        return 0;
    }
    scale_padding = g0_visualizer_ui_scaled(24, presentation_mode);
    title_height = g0_visualizer_ui_scaled(22, presentation_mode);
    section_label_height = g0_visualizer_ui_scaled(12, presentation_mode);
    row_height = g0_visualizer_ui_scaled(30, presentation_mode);
    row_gap = g0_visualizer_ui_scaled(6, presentation_mode);
    title_gap = g0_visualizer_ui_scaled(10, presentation_mode);
    label_gap = g0_visualizer_ui_scaled(4, presentation_mode);
    section_gap = g0_visualizer_ui_scaled(12, presentation_mode);
    water_label_gap = g0_visualizer_ui_scaled(4, presentation_mode);
    back_gap = g0_visualizer_ui_scaled(14, presentation_mode);
    content_width = g0_visualizer_ui_scaled(472, presentation_mode);
    panel_width = content_width + 2 * scale_padding;
    water_width = (content_width - 3 * row_gap) /
                  G0_VISUALIZER_SETTINGS_WATER_OPACITY_COUNT;
    if (water_width <= 0)
    {
        return 0;
    }

    y = scale_padding;
    y += title_height + title_gap;
    y += section_label_height + label_gap;
    y += 2 * row_height + row_gap;
    y += section_gap;
    y += section_label_height + label_gap;
    y += section_label_height + water_label_gap;
    y += row_height + row_gap;
    y += 4 * (row_height + row_gap);
    y += section_gap;
    y += section_label_height + label_gap;
    y += row_height + back_gap;
    y += row_height;
    panel_height = y + scale_padding;
    if (panel_width > client_width || panel_height > client_height)
    {
        return 0;
    }
    panel_left = (client_width - panel_width) / 2;
    panel_top = (client_height - panel_height) / 2;
    g0_visualizer_ui_rect_set(&out_layout->panel, panel_left, panel_top,
                              panel_width, panel_height);
    out_layout->bottom_padding = scale_padding;
    y = panel_top + scale_padding;
    g0_visualizer_ui_rect_set(&out_layout->controls[G0_VISUALIZER_SETTINGS_TITLE],
                              panel_left + scale_padding, y, content_width, title_height);
    y += title_height + title_gap;
    g0_visualizer_ui_rect_set(
        &out_layout->controls[G0_VISUALIZER_SETTINGS_SIMULATION_LABEL],
        panel_left + scale_padding, y, content_width, section_label_height);
    y += section_label_height + label_gap;
    g0_visualizer_ui_rect_set(&out_layout->controls[G0_VISUALIZER_SETTINGS_SPEED],
                              panel_left + scale_padding, y, content_width, row_height);
    y += row_height + row_gap;
    g0_visualizer_ui_rect_set(&out_layout->controls[G0_VISUALIZER_SETTINGS_START_PAUSED],
                              panel_left + scale_padding, y, content_width, row_height);
    y += row_height + section_gap;
    g0_visualizer_ui_rect_set(&out_layout->controls[G0_VISUALIZER_SETTINGS_VISUAL_LABEL],
                              panel_left + scale_padding, y, content_width, section_label_height);
    y += section_label_height + label_gap;
    g0_visualizer_ui_rect_set(&out_layout->controls[G0_VISUALIZER_SETTINGS_WATER_LABEL],
                              panel_left + scale_padding, y, content_width, section_label_height);
    y += section_label_height + water_label_gap;
    for (index = 0; index < G0_VISUALIZER_SETTINGS_WATER_OPACITY_COUNT; ++index)
    {
        g0_visualizer_ui_rect_set(
            &out_layout->controls[G0_VISUALIZER_SETTINGS_WATER_25 + index],
            panel_left + scale_padding + index * (water_width + row_gap), y,
            water_width, row_height);
    }
    y += row_height + row_gap;
    g0_visualizer_ui_rect_set(&out_layout->controls[G0_VISUALIZER_SETTINGS_PRESENTATION],
                              panel_left + scale_padding, y, content_width, row_height);
    y += row_height + row_gap;
    g0_visualizer_ui_rect_set(&out_layout->controls[G0_VISUALIZER_SETTINGS_GRID],
                              panel_left + scale_padding, y, content_width, row_height);
    y += row_height + row_gap;
    g0_visualizer_ui_rect_set(&out_layout->controls[G0_VISUALIZER_SETTINGS_DEBUG],
                              panel_left + scale_padding, y, content_width, row_height);
    y += row_height + row_gap;
    g0_visualizer_ui_rect_set(&out_layout->controls[G0_VISUALIZER_SETTINGS_INITIAL_ZOOM],
                              panel_left + scale_padding, y, content_width, row_height);
    y += row_height + section_gap;
    g0_visualizer_ui_rect_set(&out_layout->controls[G0_VISUALIZER_SETTINGS_LOG_LABEL],
                              panel_left + scale_padding, y, content_width, section_label_height);
    y += section_label_height + label_gap;
    g0_visualizer_ui_rect_set(&out_layout->controls[G0_VISUALIZER_SETTINGS_EVENT_LOG],
                              panel_left + scale_padding, y, content_width, row_height);
    y += row_height + back_gap;
    g0_visualizer_ui_rect_set(&out_layout->controls[G0_VISUALIZER_SETTINGS_BACK],
                              panel_left + scale_padding, y, content_width, row_height);
    return g0_visualizer_settings_layout_is_valid(out_layout, client_width,
                                                   client_height, presentation_mode);
}

int g0_visualizer_settings_layout_is_valid(
    const G0VisualizerSettingsLayout *layout,
    int client_width,
    int client_height,
    int presentation_mode)
{
    G0VisualizerUiRect client;
    int minimum_bottom_padding;
    size_t left;
    size_t right;

    if (layout == NULL || client_width <= 0 || client_height <= 0 ||
        (presentation_mode != 0 && presentation_mode != 1))
    {
        return 0;
    }
    g0_visualizer_ui_rect_set(&client, 0, 0, client_width, client_height);
    minimum_bottom_padding = g0_visualizer_ui_scaled(24, presentation_mode);
    if (!g0_visualizer_ui_rect_inside(&layout->panel, &client) ||
        layout->bottom_padding < minimum_bottom_padding ||
        layout->panel.bottom -
                layout->controls[G0_VISUALIZER_SETTINGS_BACK].bottom <
            minimum_bottom_padding)
    {
        return 0;
    }
    for (left = 0U; left < G0_VISUALIZER_SETTINGS_CONTROL_COUNT; ++left)
    {
        if (!g0_visualizer_ui_rect_inside(&layout->controls[left], &layout->panel))
        {
            return 0;
        }
        for (right = left + 1U; right < G0_VISUALIZER_SETTINGS_CONTROL_COUNT; ++right)
        {
            if (g0_visualizer_ui_rects_overlap(&layout->controls[left],
                                                &layout->controls[right]))
            {
                return 0;
            }
        }
    }
    return 1;
}