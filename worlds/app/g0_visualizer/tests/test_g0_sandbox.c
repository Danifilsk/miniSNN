#include <stdio.h>
#include <string.h>

#include "g0_visualizer_camera.h"
#include "g0_visualizer_runtime.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "G0-C test failure: %s\n", #condition); \
        return 0; \
    } \
} while (0)

static int test_world_config(void)
{
    G0WorldConfig config;
    MiniSNNWorldsTerrainTile tile;
    int present;

    memset(&config, 0, sizeof(config));
    CHECK(g0_world_config_init(&config, 10U, 10U));
    CHECK(g0_world_config_validate(&config));
    CHECK(!g0_world_config_init(&config, 4U, 10U));
    CHECK(g0_world_config_set_tile(
        &config, 2U, 2U, MINISNN_WORLDS_TERRAIN_TILE_LAND));
    CHECK(g0_world_config_set_rock(&config, 3U, 3U, 1));
    CHECK(g0_world_config_set_food(&config, 4U, 4U, 1));
    CHECK(g0_world_config_set_fish_spawn(&config, 5U, 5U));
    CHECK(g0_world_config_tile(&config, 2U, 2U, &tile) &&
          tile == MINISNN_WORLDS_TERRAIN_TILE_LAND);
    CHECK(g0_world_config_has_rock(&config, 3U, 3U, &present) && present);
    CHECK(g0_world_config_has_food(&config, 4U, 4U, &present) && present);
    CHECK(g0_world_config_food_count(&config) == 1U);
    CHECK(!g0_world_config_set_food(&config, 2U, 2U, 1));
    CHECK(g0_world_config_validate(&config));
    g0_world_config_destroy(&config);
    return 1;
}

static int test_camera_conversion(void)
{
    uint32_t tile_x = 0U;
    uint32_t tile_y = 0U;

    CHECK(g0_visualizer_screen_to_tile(
        10U, 10U, 40, 80, 96, 40 + 3 * 96 + 95, 80 + 4 * 96 + 2,
        &tile_x, &tile_y));
    CHECK(tile_x == 3U && tile_y == 4U);
    CHECK(!g0_visualizer_screen_to_tile(
        10U, 10U, 40, 80, 96, 39, 80, &tile_x, &tile_y));
    CHECK(!g0_visualizer_screen_to_tile(
        10U, 10U, 40, 80, 0, 100, 100, &tile_x, &tile_y));
    return 1;
}

static int test_scaled_camera_geometry(void)
{
    static const int tile_pixels[] = { 16, 48, 64, 128, 256 };
    size_t index;

    for (index = 0U; index < sizeof(tile_pixels) / sizeof(tile_pixels[0]); ++index)
    {
        G0VisualizerScreenPoint point;
        G0VisualizerScreenPoint next_point;
        uint32_t tile_x = 0U;
        uint32_t tile_y = 0U;
        int pixels = tile_pixels[index];

        CHECK(g0_visualizer_tile_to_screen_scaled(3U, 4U, 40, 80, pixels, &point));
        CHECK(point.screen_x == 40 + 3 * pixels);
        CHECK(point.screen_y == 80 + 4 * pixels);
        CHECK(g0_visualizer_tile_to_screen_scaled(3U, 4U, 0, 0, pixels,
                                                   &next_point));
        CHECK(next_point.screen_x == 3 * pixels);
        CHECK(next_point.screen_y == 4 * pixels);
        CHECK(g0_visualizer_tile_to_screen_scaled(4U, 4U, 40, 80, pixels,
                                                   &next_point));
        CHECK(next_point.screen_x - point.screen_x == pixels);
        CHECK(next_point.screen_y == point.screen_y);
        CHECK(g0_visualizer_screen_to_tile(
            10U, 10U, 40, 80, pixels,
            point.screen_x + pixels / 2, point.screen_y + pixels / 2,
            &tile_x, &tile_y));
        CHECK(tile_x == 3U && tile_y == 4U);
    }
    CHECK(!g0_visualizer_tile_to_screen_scaled(0U, 0U, 0, 0, 0, NULL));
    return 1;
}
static int test_scaled_world_geometry(void)
{
    static const int tile_pixels[] = { 16, 48, 64, 128, 256 };
    G0VisualizerRuntime *runtime = g0_visualizer_runtime_create_sandbox();
    const WF0FishWorldState *state;
    const MiniSNNWorldsTerrain *terrain;
    uint32_t tile_x;
    uint32_t tile_y;
    size_t index;
    int result = 0;

    CHECK(runtime != NULL);
    state = g0_visualizer_runtime_state(runtime);
    terrain = g0_visualizer_runtime_terrain(runtime);
    CHECK(state != NULL && terrain != NULL);
    CHECK(minisnn_worlds_terrain_world_to_tile(
        terrain, state->actor_transform.position, &tile_x, &tile_y) ==
        MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    for (index = 0U; index < sizeof(tile_pixels) / sizeof(tile_pixels[0]); ++index)
    {
        G0VisualizerScreenPoint point;
        int pixels = tile_pixels[index];

        CHECK(g0_visualizer_world_to_screen_scaled(
            runtime, state->actor_transform.position, 40, 80, pixels, &point));
        CHECK(point.screen_x == 40 + (int)tile_x * pixels);
        CHECK(point.screen_y == 80 + (int)tile_y * pixels);
    }
    result = 1;
    g0_visualizer_runtime_destroy(&runtime);
    return result;
}
static int test_apply_and_edit(void)
{
    G0VisualizerRuntime *runtime = g0_visualizer_runtime_create_sandbox();
    const G0WorldConfig *config;
    WF0FishWorldState before;
    WF0FishWorldState after;
    int result = 0;

    CHECK(runtime != NULL);
    config = g0_visualizer_runtime_config(runtime);
    CHECK(config != NULL && config->width == 20U && config->height == 15U);
    CHECK(g0_visualizer_runtime_food_count(runtime) == 3U);
    CHECK(g0_visualizer_runtime_new_world(runtime, 10U, 10U));
    CHECK(g0_visualizer_runtime_edit_tile(runtime, G0_WORLD_TOOL_LAND, 1U, 1U));
    CHECK(g0_visualizer_runtime_edit_tile(runtime, G0_WORLD_TOOL_ROCK, 2U, 2U));
    CHECK(g0_visualizer_runtime_edit_tile(runtime, G0_WORLD_TOOL_FOOD, 4U, 4U));
    CHECK(g0_visualizer_runtime_edit_tile(runtime, G0_WORLD_TOOL_FISH_SPAWN, 5U, 5U));
    CHECK(g0_visualizer_runtime_apply_config(runtime));
    CHECK(g0_visualizer_runtime_food_count(runtime) == 1U);
    before = *g0_visualizer_runtime_state(runtime);
    CHECK(g0_visualizer_runtime_edit_tile(runtime, G0_WORLD_TOOL_LAND, 5U, 5U));
    CHECK(!g0_visualizer_runtime_apply_config(runtime));
    after = *g0_visualizer_runtime_state(runtime);
    CHECK(before.kernel_hash == after.kernel_hash &&
          before.domain_hash == after.domain_hash && before.tick == after.tick);
    CHECK(g0_visualizer_runtime_edit_tile(runtime, G0_WORLD_TOOL_WATER, 5U, 5U));
    CHECK(g0_visualizer_runtime_apply_config(runtime));
    CHECK(g0_visualizer_runtime_prepare_edit(runtime));
    result = 1;
    g0_visualizer_runtime_destroy(&runtime);
    return result;
}

static int test_pacing_determinism(void)
{
    G0VisualizerRuntime *slow = g0_visualizer_runtime_create_sandbox();
    G0VisualizerRuntime *fast = g0_visualizer_runtime_create_sandbox();
    WF0FishWorldState slow_state;
    WF0FishWorldState fast_state;
    int result = 0;

    CHECK(slow != NULL && fast != NULL);
    CHECK(g0_visualizer_runtime_advance_elapsed(slow, 1000U));
    CHECK(g0_visualizer_runtime_speed_increase(fast));
    CHECK(g0_visualizer_runtime_speed_increase(fast));
    CHECK(g0_visualizer_runtime_speed_increase(fast));
    CHECK(g0_visualizer_runtime_advance_elapsed(fast, 125U));
    slow_state = *g0_visualizer_runtime_state(slow);
    fast_state = *g0_visualizer_runtime_state(fast);
    CHECK(slow_state.tick == fast_state.tick);
    CHECK(slow_state.kernel_hash == fast_state.kernel_hash);
    CHECK(slow_state.domain_hash == fast_state.domain_hash);
    g0_visualizer_runtime_set_paused(slow, 1);
    CHECK(g0_visualizer_runtime_advance_elapsed(slow, 4000U));
    CHECK(g0_visualizer_runtime_state(slow)->tick == slow_state.tick);
    CHECK(g0_visualizer_runtime_step_once(slow));
    CHECK(g0_visualizer_runtime_state(slow)->tick == slow_state.tick + 1U);
    result = 1;
    g0_visualizer_runtime_destroy(&slow);
    g0_visualizer_runtime_destroy(&fast);
    return result;
}

static int camera_layout_stays_stable(
    G0VisualizerCamera *camera,
    uint32_t width,
    uint32_t height,
    int left,
    int top,
    int right,
    int bottom)
{
    G0VisualizerCamera expected;
    unsigned int frame;

    CHECK(g0_visualizer_camera_layout(camera, width, height,
                                      left, top, right, bottom));
    expected = *camera;
    for (frame = 0U; frame < 100U; ++frame)
    {
        CHECK(g0_visualizer_camera_layout(camera, width, height,
                                          left, top, right, bottom));
        CHECK(camera->zoom_percent == expected.zoom_percent);
        CHECK(camera->tile_pixels == expected.tile_pixels);
        CHECK(camera->pan_x_units == expected.pan_x_units);
        CHECK(camera->pan_y_units == expected.pan_y_units);
        CHECK(camera->pan_x == expected.pan_x);
        CHECK(camera->pan_y == expected.pan_y);
        CHECK(camera->map_origin_x == expected.map_origin_x);
        CHECK(camera->map_origin_y == expected.map_origin_y);
    }
    return 1;
}

static int test_camera_zoom_stability(void)
{
    static const int zoom_percentages[] = { 25, 50, 75, 100, 150, 200, 400 };
    static const int panel_layouts[][4] =
    {
        { 16, 78, 1364, 842 }, /* normal */
        { 16, 78, 1084, 842 }, /* debug */
        { 16, 78, 1064, 842 }, /* editor */
        { 24, 117, 1704, 1254 } /* presentation */
    };
    size_t index;
    size_t layout_index;

    for (index = 0U; index < sizeof(zoom_percentages) / sizeof(zoom_percentages[0]); ++index)
    {
        G0VisualizerCamera camera;
        G0VisualizerScreenPoint point;
        uint32_t tile_x = 0U;
        uint32_t tile_y = 0U;

        g0_visualizer_camera_init(&camera);
        CHECK(g0_visualizer_camera_set_zoom_percent(&camera, zoom_percentages[index]));
        CHECK(camera_layout_stays_stable(&camera, 10U, 8U, 16, 78, 1364, 842));
        CHECK(g0_visualizer_tile_to_screen_scaled(
            3U, 4U, camera.map_origin_x, camera.map_origin_y,
            camera.tile_pixels, &point));
        CHECK(g0_visualizer_screen_to_tile(
            10U, 8U, camera.map_origin_x, camera.map_origin_y,
            camera.tile_pixels, point.screen_x + camera.tile_pixels / 2,
            point.screen_y + camera.tile_pixels / 2, &tile_x, &tile_y));
        CHECK(tile_x == 3U && tile_y == 4U);
    }

    {
        G0VisualizerCamera camera;

        g0_visualizer_camera_init(&camera);
        CHECK(g0_visualizer_camera_set_zoom_percent(&camera, 100));
        CHECK(g0_visualizer_camera_layout(&camera, 80U, 60U, 16, 78, 1364, 842));
        CHECK(g0_visualizer_camera_pan_by_pixels(&camera, 91, -37));
        CHECK(camera_layout_stays_stable(&camera, 80U, 60U, 16, 78, 1364, 842));
        CHECK(g0_visualizer_camera_set_zoom_percent(&camera, 150));
        CHECK(camera_layout_stays_stable(&camera, 80U, 60U, 16, 78, 1364, 842));
        CHECK(camera.tile_pixels == 96);
        CHECK(g0_visualizer_camera_set_zoom_percent(&camera, 100));
        CHECK(camera_layout_stays_stable(&camera, 80U, 60U, 16, 78, 1364, 842));
        CHECK(camera.tile_pixels == 64);
    }

    for (layout_index = 0U;
         layout_index < sizeof(panel_layouts) / sizeof(panel_layouts[0]);
         ++layout_index)
    {
        G0VisualizerCamera camera;

        g0_visualizer_camera_init(&camera);
        CHECK(g0_visualizer_camera_set_zoom_percent(&camera, 150));
        CHECK(g0_visualizer_camera_layout(
            &camera, 80U, 60U,
            panel_layouts[layout_index][0], panel_layouts[layout_index][1],
            panel_layouts[layout_index][2], panel_layouts[layout_index][3]));
        CHECK(g0_visualizer_camera_pan_by_pixels(&camera, 73, -51));
        CHECK(camera_layout_stays_stable(
            &camera, 80U, 60U,
            panel_layouts[layout_index][0], panel_layouts[layout_index][1],
            panel_layouts[layout_index][2], panel_layouts[layout_index][3]));
    }

    /* FIT selects a discrete 75%% zoom for this panel; the idle layout is stable. */
    {
        G0VisualizerCamera camera;
        g0_visualizer_camera_init(&camera);
        CHECK(g0_visualizer_camera_set_zoom_percent(&camera, 75));
        g0_visualizer_camera_reset_pan(&camera);
        CHECK(camera_layout_stays_stable(&camera, 20U, 15U, 16, 78, 1364, 842));
    }
    return 1;
}
static int test_camera_does_not_change_world(void)
{
    G0VisualizerRuntime *runtime = g0_visualizer_runtime_create_sandbox();
    G0VisualizerCamera camera;
    WF0FishWorldState before;
    WF0FishWorldState after;
    uint64_t terrain_hash_before = 0U;
    uint64_t terrain_hash_after = 0U;
    unsigned int frame;

    CHECK(runtime != NULL);
    before = *g0_visualizer_runtime_state(runtime);
    CHECK(minisnn_worlds_terrain_hash(g0_visualizer_runtime_terrain(runtime),
                                      &terrain_hash_before) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    g0_visualizer_camera_init(&camera);
    CHECK(g0_visualizer_camera_set_zoom_percent(&camera, 100));
    CHECK(g0_visualizer_camera_layout(&camera, 20U, 15U, 16, 78, 1364, 842));
    CHECK(g0_visualizer_camera_pan_by_pixels(&camera, 77, -39));
    CHECK(g0_visualizer_camera_set_zoom_percent(&camera, 150));
    for (frame = 0U; frame < 100U; ++frame)
    {
        CHECK(g0_visualizer_camera_layout(&camera, 20U, 15U, 16, 78, 1364, 842));
    }
    after = *g0_visualizer_runtime_state(runtime);
    CHECK(minisnn_worlds_terrain_hash(g0_visualizer_runtime_terrain(runtime),
                                      &terrain_hash_after) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    CHECK(memcmp(&before, &after, sizeof(before)) == 0);
    CHECK(terrain_hash_before == terrain_hash_after);
    g0_visualizer_runtime_destroy(&runtime);
    return 1;
}
int main(void)
{
    if (!test_world_config() || !test_camera_conversion() ||
        !test_scaled_camera_geometry() || !test_scaled_world_geometry() ||
        !test_apply_and_edit() ||
        !test_camera_zoom_stability() ||
        !test_camera_does_not_change_world() ||
        !test_pacing_determinism())
    {
        return 1;
    }
    puts("G0-C sandbox configuration, editor, camera, and pacing OK");
    return 0;
}
