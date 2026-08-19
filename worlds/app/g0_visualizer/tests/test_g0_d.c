#include <stdio.h>
#include <string.h>

#include "g0_event_log.h"
#include "g0_visualizer_runtime.h"
#include "g0_visualizer_ui_layout.h"
#include "g0_world_document.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "G0-D test failure: %s\n", #condition); \
        return 0; \
    } \
} while (0)

#define TEST_WORLD_FILE "../../../build/worlds/app/g0_visualizer/tests/g0_d_roundtrip.world"
#define TEST_CORRUPT_FILE "../../../build/worlds/app/g0_visualizer/tests/g0_d_corrupt.world"
#define TEST_LOG_FILE "../../../build/worlds/app/g0_visualizer/tests/g0_d_events.csv"
#define TEST_LEGACY_FILE "../../../build/worlds/app/g0_visualizer/tests/g0_d_legacy_v1.world"
#define TEST_RESIZE_FILE "../../../build/worlds/app/g0_visualizer/tests/g0_d_resize.world"

static int build_config(G0WorldConfig *config)
{
    return g0_world_config_init(config, 10U, 10U) &&
           g0_world_config_set_tile(config, 3U, 3U,
                                    MINISNN_WORLDS_TERRAIN_TILE_LAND) &&
           g0_world_config_set_rock(config, 5U, 4U, 1) &&
           g0_world_config_set_food(config, 2U, 1U, 1) &&
           g0_world_config_set_food(config, 6U, 6U, 1) &&
           g0_world_config_set_fish_spawn(config, 1U, 1U);
}

static int write_text(const char *filename, const char *text)
{
    FILE *file = fopen(filename, "w");
    if (file == NULL)
        return 0;
    if (fputs(text, file) == EOF || fclose(file) != 0)
        return 0;
    return 1;
}

static int file_starts_with(const char *filename, const char *prefix)
{
    char line[128];
    FILE *file = fopen(filename, "r");
    int result = file != NULL && fgets(line, sizeof(line), file) != NULL &&
                 strncmp(line, prefix, strlen(prefix)) == 0;
    if (file != NULL)
        (void)fclose(file);
    return result;
}

static int replace_text(const char *filename, const char *before, const char *after)
{
    char source[16384];
    char output[16384];
    FILE *file;
    char *match;
    size_t source_length;
    size_t before_length;
    size_t after_length;
    size_t prefix_length;

    if (filename == NULL || before == NULL || after == NULL)
        return 0;
    file = fopen(filename, "r");
    if (file == NULL)
        return 0;
    source_length = fread(source, 1U, sizeof(source) - 1U, file);
    if (ferror(file) || fclose(file) != 0)
        return 0;
    source[source_length] = '\0';
    match = strstr(source, before);
    if (match == NULL)
        return 0;
    before_length = strlen(before);
    after_length = strlen(after);
    prefix_length = (size_t)(match - source);
    if (prefix_length + after_length + (source_length - prefix_length - before_length) >=
        sizeof(output))
    {
        return 0;
    }
    memcpy(output, source, prefix_length);
    memcpy(output + prefix_length, after, after_length);
    memcpy(output + prefix_length + after_length, match + before_length,
           source_length - prefix_length - before_length + 1U);
    return write_text(filename, output);
}

static int replace_field_character(const char *filename, const char *field,
                                   size_t offset, char value)
{
    char source[16384];
    FILE *file;
    char *position;
    size_t length;

    file = fopen(filename, "r+");
    if (file == NULL)
        return 0;
    length = fread(source, 1U, sizeof(source) - 1U, file);
    if (ferror(file))
    {
        (void)fclose(file);
        return 0;
    }
    source[length] = '\0';
    position = strstr(source, field);
    if (position == NULL || strlen(position) <= strlen(field) + offset)
    {
        (void)fclose(file);
        return 0;
    }
    position[strlen(field) + offset] = value;
    if (fseek(file, 0L, SEEK_SET) != 0 ||
        fwrite(source, 1U, length, file) != length || fclose(file) != 0)
    {
        return 0;
    }
    return 1;
}
static int config_equal(const G0WorldConfig *left, const G0WorldConfig *right)
{
    size_t count;

    if (left == NULL || right == NULL || left->width != right->width ||
        left->height != right->height || left->fish_spawn_x != right->fish_spawn_x ||
        left->fish_spawn_y != right->fish_spawn_y)
    {
        return 0;
    }
    count = (size_t)left->width * (size_t)left->height;
    return memcmp(left->tiles, right->tiles, count * sizeof(*left->tiles)) == 0 &&
           memcmp(left->rocks, right->rocks, count * sizeof(*left->rocks)) == 0 &&
           memcmp(left->foods, right->foods, count * sizeof(*left->foods)) == 0;
}

static int test_document_roundtrip_and_determinism(void)
{
    G0WorldConfig config;
    G0AppSettings settings;
    G0WorldDocument source;
    G0WorldDocument loaded;
    G0WorldDocumentError error;
    G0VisualizerRuntime *first = NULL;
    G0VisualizerRuntime *second = NULL;
    WF0FishWorldState first_state;
    WF0FishWorldState second_state;
    uint64_t first_terrain_hash;
    uint64_t second_terrain_hash;
    int result = 0;

    memset(&config, 0, sizeof(config));
    g0_world_document_init(&source);
    g0_world_document_init(&loaded);
    g0_app_settings_default(&settings);
    settings.default_speed_index = 4U;
    settings.start_paused = 1;
    settings.initial_zoom_index = 2;
    CHECK(build_config(&config));
    CHECK(!g0_world_config_set_rock(&config, config.width, 0U, 1));
    CHECK(!g0_world_config_set_food(&config, 0U, config.height, 1));
    CHECK(g0_world_document_create(&source, "round trip world", &config, &settings));
    CHECK(g0_world_document_save(TEST_WORLD_FILE, &source, &error));
    CHECK(error == G0_WORLD_DOCUMENT_ERROR_NONE);
    CHECK(g0_world_document_load(TEST_WORLD_FILE, &loaded, &error));
    CHECK(error == G0_WORLD_DOCUMENT_ERROR_NONE);
    CHECK(g0_world_document_equal(&source, &loaded));
    first = g0_visualizer_runtime_create_from_config(&source.world);
    second = g0_visualizer_runtime_create_from_config(&loaded.world);
    CHECK(first != NULL && second != NULL);
    CHECK(minisnn_worlds_terrain_hash(g0_visualizer_runtime_terrain(first),
                                      &first_terrain_hash) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    CHECK(minisnn_worlds_terrain_hash(g0_visualizer_runtime_terrain(second),
                                      &second_terrain_hash) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    CHECK(first_terrain_hash == second_terrain_hash);
    CHECK(g0_visualizer_runtime_step_once(first));
    CHECK(g0_visualizer_runtime_step_once(second));
    first_state = *g0_visualizer_runtime_state(first);
    second_state = *g0_visualizer_runtime_state(second);
    CHECK(first_state.tick == second_state.tick);
    CHECK(first_state.kernel_hash == second_state.kernel_hash);
    CHECK(first_state.domain_hash == second_state.domain_hash);
    result = 1;
    g0_visualizer_runtime_destroy(&first);
    g0_visualizer_runtime_destroy(&second);
    g0_world_document_destroy(&source);
    g0_world_document_destroy(&loaded);
    g0_world_config_destroy(&config);
    (void)remove(TEST_WORLD_FILE);
    return result;
}

static int test_corrupt_load_preserves_document(void)
{
    G0WorldConfig config;
    G0AppSettings settings;
    G0WorldDocument preserved;
    G0WorldDocument before;
    G0WorldDocument source;
    G0WorldDocumentError error;
    int result = 0;

    memset(&config, 0, sizeof(config));
    g0_world_document_init(&preserved);
    g0_world_document_init(&before);
    g0_world_document_init(&source);
    g0_app_settings_default(&settings);
    CHECK(build_config(&config));
    CHECK(g0_world_document_create(&preserved, "preserved", &config, &settings));
    CHECK(g0_world_document_clone(&preserved, &before));
    CHECK(write_text(TEST_CORRUPT_FILE, "not a worlds file\n"));
    CHECK(!g0_world_document_load(TEST_CORRUPT_FILE, &preserved, &error));
    CHECK(error == G0_WORLD_DOCUMENT_ERROR_VERSION);
    CHECK(g0_world_document_equal(&preserved, &before));
    CHECK(write_text(TEST_CORRUPT_FILE,
        "miniSNN Worlds World Config V1\nname=bad\nwidth=4\nheight=4\n"));
    CHECK(!g0_world_document_load(TEST_CORRUPT_FILE, &preserved, &error));
    CHECK(g0_world_document_equal(&preserved, &before));
    CHECK(g0_world_document_create(&source, "corrupt source", &config, &settings));

    CHECK(g0_world_document_save(TEST_CORRUPT_FILE, &source, &error));
    CHECK(replace_text(TEST_CORRUPT_FILE, "terrain=W", "terrain=X"));
    CHECK(!g0_world_document_load(TEST_CORRUPT_FILE, &preserved, &error));
    CHECK(error == G0_WORLD_DOCUMENT_ERROR_FORMAT);
    CHECK(g0_world_document_equal(&preserved, &before));

    CHECK(g0_world_document_save(TEST_CORRUPT_FILE, &source, &error));
    CHECK(replace_text(TEST_CORRUPT_FILE, "fish_spawn=1,1", "fish_spawn=99,1"));
    CHECK(!g0_world_document_load(TEST_CORRUPT_FILE, &preserved, &error));
    CHECK(error == G0_WORLD_DOCUMENT_ERROR_VALIDATION);
    CHECK(g0_world_document_equal(&preserved, &before));

    CHECK(g0_world_document_save(TEST_CORRUPT_FILE, &source, &error));
    CHECK(replace_field_character(TEST_CORRUPT_FILE, "foods=", 33U, '1'));
    CHECK(!g0_world_document_load(TEST_CORRUPT_FILE, &preserved, &error));
    CHECK(error == G0_WORLD_DOCUMENT_ERROR_VALIDATION);
    CHECK(g0_world_document_equal(&preserved, &before));

    CHECK(g0_world_document_save(TEST_CORRUPT_FILE, &source, &error));
    CHECK(replace_field_character(TEST_CORRUPT_FILE, "rocks=", 11U, '1'));
    CHECK(!g0_world_document_load(TEST_CORRUPT_FILE, &preserved, &error));
    CHECK(error == G0_WORLD_DOCUMENT_ERROR_VALIDATION);
    CHECK(g0_world_document_equal(&preserved, &before));

    result = 1;
    g0_world_document_destroy(&source);
    g0_world_document_destroy(&preserved);
    g0_world_document_destroy(&before);
    g0_world_config_destroy(&config);
    (void)remove(TEST_CORRUPT_FILE);
    return result;
}
static int test_prepare_edit_preserves_blueprint(void)
{
    G0WorldConfig config;
    G0WorldConfig expected;
    G0VisualizerRuntime *runtime = NULL;
    int result = 0;

    memset(&config, 0, sizeof(config));
    memset(&expected, 0, sizeof(expected));
    CHECK(build_config(&config));
    CHECK(g0_world_config_clone(&config, &expected));
    runtime = g0_visualizer_runtime_create_from_config(&config);
    CHECK(runtime != NULL);
    CHECK(g0_visualizer_runtime_step_once(runtime));
    CHECK(g0_visualizer_runtime_prepare_edit(runtime));
    CHECK(config_equal(&expected, g0_visualizer_runtime_config(runtime)));
    result = 1;
    g0_visualizer_runtime_destroy(&runtime);
    g0_world_config_destroy(&expected);
    g0_world_config_destroy(&config);
    return result;
}

static int test_reset_episode(void)
{
    G0WorldConfig config;
    G0VisualizerRuntime *runtime = NULL;
    WF0FishWorldState initial;
    WF0FishWorldState reset;
    G0VisualizerFacing initial_facing;
    int result = 0;

    memset(&config, 0, sizeof(config));
    CHECK(build_config(&config));
    runtime = g0_visualizer_runtime_create_from_config(&config);
    CHECK(runtime != NULL);
    initial = *g0_visualizer_runtime_state(runtime);
    initial_facing = g0_visualizer_runtime_facing(runtime);
    CHECK(g0_visualizer_runtime_episode_tick(runtime) == 0U);
    CHECK(g0_visualizer_runtime_step_once(runtime));
    CHECK(g0_visualizer_runtime_step_once(runtime));
    CHECK(g0_visualizer_runtime_episode_tick(runtime) == 2U);
    CHECK(g0_visualizer_runtime_reset(runtime));
    reset = *g0_visualizer_runtime_state(runtime);
    CHECK(reset.tick == initial.tick);
    CHECK(reset.actor_energy == initial.actor_energy);
    CHECK(reset.remaining_food == initial.remaining_food);
    CHECK(reset.actor_transform.position.x == initial.actor_transform.position.x);
    CHECK(reset.actor_transform.position.y == initial.actor_transform.position.y);
    CHECK(g0_visualizer_runtime_facing(runtime) == initial_facing);
    CHECK(g0_visualizer_runtime_episode_tick(runtime) == 0U);
    CHECK(g0_visualizer_runtime_step_once(runtime));
    CHECK(g0_visualizer_runtime_episode_tick(runtime) == 1U);
    CHECK(g0_visualizer_runtime_state(runtime)->tick == initial.tick + 1U);
    result = 1;
    g0_visualizer_runtime_destroy(&runtime);
    g0_world_config_destroy(&config);
    return result;
}

static int test_event_log(void)
{
    G0EventLog log;
    G0LogEntry entry;
    unsigned int index;

    g0_event_log_init(&log);
    CHECK(g0_event_log_append(&log, 1U, G0_LOG_CATEGORY_SYSTEM, 0, 0U,
                              "EPISODE_STARTED", "NA", "NONE", "Episode started"));
    CHECK(g0_event_log_append(&log, 2U, G0_LOG_CATEGORY_ACTION, 1, 1U,
                              "MOVE", "REJECTED", "INSUFFICIENT_ENERGY", "Fish MOVE"));
    CHECK(g0_event_log_count(&log) == 2U);
    CHECK(g0_event_log_at(&log, 1U, &entry));
    CHECK(entry.sequence == 2U && entry.has_actor && entry.actor == 1U);
    for (index = 0U; index < G0_EVENT_LOG_CAPACITY + 4U; ++index)
    {
        CHECK(g0_event_log_append(&log, 3U + index, G0_LOG_CATEGORY_ACTION, 0, 0U,
                                  "WAIT", "APPLIED", "NONE", "bounded log"));
    }
    CHECK(g0_event_log_count(&log) == G0_EVENT_LOG_CAPACITY);
    CHECK(g0_event_log_at(&log, 0U, &entry));
    CHECK(entry.sequence > 1U);
    CHECK(g0_event_log_export_csv(&log, TEST_LOG_FILE));
    CHECK(file_starts_with(TEST_LOG_FILE, "sequence,tick,category,actor,event,result,reason,text"));
    (void)remove(TEST_LOG_FILE);
    return 1;
}

static int test_settings_and_visual_independence(void)
{
    static const int opacity_values[] = { 25, 35, 50, 65 };
    static const uint8_t opacity_alpha_values[] = { 64U, 89U, 128U, 166U };
    G0AppSettings settings;
    G0WorldConfig config;
    WF0FishWorldState baseline;
    int have_baseline = 0;
    size_t index;
    int presentation_mode;

    g0_app_settings_default(&settings);
    CHECK(settings.water_opacity_percent == 35);
    CHECK(settings.presentation_mode == 0);
    CHECK(g0_app_settings_effective_water_opacity_percent(&settings) == 35);
    CHECK(g0_app_settings_effective_water_alpha(&settings) == 89U);
    for (index = 0U; index < sizeof(opacity_values) / sizeof(opacity_values[0]); ++index)
    {
        settings.water_opacity_percent = opacity_values[index];
        settings.presentation_mode = 0;
        CHECK(g0_app_settings_validate(&settings));
        CHECK(g0_app_settings_effective_water_opacity_percent(&settings) ==
              opacity_values[index]);
        CHECK(g0_app_settings_effective_water_alpha(&settings) ==
              opacity_alpha_values[index]);
        settings.presentation_mode = 1;
        CHECK(g0_app_settings_effective_water_opacity_percent(&settings) == 25);
        CHECK(g0_app_settings_effective_water_alpha(&settings) == 64U);
        settings.presentation_mode = 0;
        CHECK(settings.water_opacity_percent == opacity_values[index]);
    }

    memset(&config, 0, sizeof(config));
    CHECK(build_config(&config));
    for (presentation_mode = 0; presentation_mode <= 1; ++presentation_mode)
    {
        for (index = 0U; index < sizeof(opacity_values) / sizeof(opacity_values[0]); ++index)
        {
            G0VisualizerRuntime *runtime = g0_visualizer_runtime_create_from_config(&config);
            WF0FishWorldState current;
            unsigned int step;

            CHECK(runtime != NULL);
            settings.water_opacity_percent = opacity_values[index];
            settings.presentation_mode = presentation_mode;
            CHECK(g0_app_settings_validate(&settings));
            for (step = 0U; step < 6U; ++step)
                CHECK(g0_visualizer_runtime_step_once(runtime));
            current = *g0_visualizer_runtime_state(runtime);
            if (!have_baseline)
            {
                baseline = current;
                have_baseline = 1;
            }
            else
            {
                CHECK(current.tick == baseline.tick);
                CHECK(current.kernel_hash == baseline.kernel_hash);
                CHECK(current.domain_hash == baseline.domain_hash);
                CHECK(current.actor_energy == baseline.actor_energy);
                CHECK(current.remaining_food == baseline.remaining_food);
                CHECK(current.actor_transform.position.x == baseline.actor_transform.position.x);
                CHECK(current.actor_transform.position.y == baseline.actor_transform.position.y);
            }
            g0_visualizer_runtime_destroy(&runtime);
        }
    }
    g0_world_config_destroy(&config);
    return 1;
}

static int test_v1_load_defaults_and_v2_roundtrip(void)
{
    static const char legacy_document[] =
        "miniSNN Worlds World Config V1\n"
        "name=legacy\n"
        "width=5\nheight=5\ntile_size=1000\n"
        "default_speed_index=2\nstart_paused=0\nshow_grid=1\n"
        "show_debug_panel=1\ninitial_zoom_index=-1\nevent_log_enabled=1\n"
        "fish_spawn=2,2\n"
        "terrain=WWWWWWWWWWWWWWWWWWWWWWWWW\n"
        "rocks=0000000000000000000000000\n"
        "foods=0000000000000000000000000\n";
    G0WorldConfig config;
    G0AppSettings settings;
    G0WorldDocument source;
    G0WorldDocument loaded;
    G0WorldDocumentError error;
    int result = 0;

    memset(&config, 0, sizeof(config));
    g0_world_document_init(&source);
    g0_world_document_init(&loaded);
    CHECK(write_text(TEST_LEGACY_FILE, legacy_document));
    CHECK(g0_world_document_load(TEST_LEGACY_FILE, &loaded, &error));
    CHECK(error == G0_WORLD_DOCUMENT_ERROR_NONE);
    CHECK(loaded.settings.water_opacity_percent == G0_APP_WATER_OPACITY_DEFAULT);
    CHECK(loaded.settings.presentation_mode == 0);
    g0_world_document_destroy(&loaded);

    g0_app_settings_default(&settings);
    settings.water_opacity_percent = 65;
    settings.presentation_mode = 1;
    CHECK(build_config(&config));
    CHECK(g0_world_document_create(&source, "v2 settings", &config, &settings));
    CHECK(g0_world_document_save(TEST_WORLD_FILE, &source, &error));
    CHECK(file_starts_with(TEST_WORLD_FILE, G0_WORLD_DOCUMENT_FORMAT));
    CHECK(g0_world_document_load(TEST_WORLD_FILE, &loaded, &error));
    CHECK(g0_world_document_equal(&source, &loaded));
    result = 1;
    g0_world_document_destroy(&source);
    g0_world_document_destroy(&loaded);
    g0_world_config_destroy(&config);
    (void)remove(TEST_LEGACY_FILE);
    (void)remove(TEST_WORLD_FILE);
    return result;
}

static int expect_cell(
    const G0WorldConfig *config,
    uint32_t tile_x,
    uint32_t tile_y,
    MiniSNNWorldsTerrainTile expected_tile,
    int expected_rock,
    int expected_food)
{
    MiniSNNWorldsTerrainTile tile;
    int rock;
    int food;

    return g0_world_config_tile(config, tile_x, tile_y, &tile) &&
           g0_world_config_has_rock(config, tile_x, tile_y, &rock) &&
           g0_world_config_has_food(config, tile_x, tile_y, &food) &&
           tile == expected_tile && rock == expected_rock && food == expected_food;
}

static int test_transactional_resize(void)
{
    G0WorldConfig config;
    G0WorldConfig before_failure;
    G0WorldConfig runtime_before;
    G0WorldDocument source;
    G0WorldDocument loaded;
    G0AppSettings settings;
    G0WorldDocumentError error;
    G0VisualizerRuntime *runtime = NULL;
    G0VisualizerUiRect controls[G0_VISUALIZER_HEADER_CONTROL_COUNT];
    int result = 0;

    memset(&config, 0, sizeof(config));
    memset(&before_failure, 0, sizeof(before_failure));
    memset(&runtime_before, 0, sizeof(runtime_before));
    g0_world_document_init(&source);
    g0_world_document_init(&loaded);
    CHECK(g0_world_config_init(&config, 10U, 10U));
    CHECK(g0_world_config_set_tile(&config, 1U, 1U, MINISNN_WORLDS_TERRAIN_TILE_LAND));
    CHECK(g0_world_config_set_rock(&config, 2U, 2U, 1));
    CHECK(g0_world_config_set_food(&config, 3U, 3U, 1));
    CHECK(g0_world_config_set_fish_spawn(&config, 4U, 4U));

    CHECK(g0_world_config_resize(&config, 12U, 10U));
    CHECK(config.width == 12U && config.height == 10U);
    CHECK(expect_cell(&config, 1U, 1U, MINISNN_WORLDS_TERRAIN_TILE_LAND, 0, 0));
    CHECK(expect_cell(&config, 2U, 2U, MINISNN_WORLDS_TERRAIN_TILE_WATER, 1, 0));
    CHECK(expect_cell(&config, 3U, 3U, MINISNN_WORLDS_TERRAIN_TILE_WATER, 0, 1));
    CHECK(expect_cell(&config, 11U, 9U, MINISNN_WORLDS_TERRAIN_TILE_WATER, 0, 0));

    CHECK(g0_world_config_resize(&config, 12U, 12U));
    CHECK(config.width == 12U && config.height == 12U);
    CHECK(expect_cell(&config, 11U, 11U, MINISNN_WORLDS_TERRAIN_TILE_WATER, 0, 0));
    CHECK(g0_world_config_resize(&config, 5U, 5U));
    CHECK(config.width == 5U && config.height == 5U);
    CHECK(expect_cell(&config, 3U, 3U, MINISNN_WORLDS_TERRAIN_TILE_WATER, 0, 1));
    CHECK(config.fish_spawn_x == 4U && config.fish_spawn_y == 4U);
    CHECK(g0_world_config_resize(&config, 8U, 7U));
    CHECK(g0_world_config_resize(&config, 6U, 6U));
    CHECK(g0_world_config_resize(&config, 8U, 8U));
    CHECK(expect_cell(&config, 1U, 1U, MINISNN_WORLDS_TERRAIN_TILE_LAND, 0, 0));

    CHECK(g0_world_config_set_fish_spawn(&config, 5U, 5U));
    CHECK(g0_world_config_clone(&config, &before_failure));
    CHECK(!g0_world_config_resize(&config, 5U, 6U));
    CHECK(config_equal(&config, &before_failure));

    runtime = g0_visualizer_runtime_create_from_config(&before_failure);
    CHECK(runtime != NULL);
    CHECK(g0_visualizer_runtime_resize_world(runtime, 8U, 8U));
    CHECK(expect_cell(g0_visualizer_runtime_config(runtime), 1U, 1U,
                      MINISNN_WORLDS_TERRAIN_TILE_LAND, 0, 0));
    CHECK(g0_world_config_clone(g0_visualizer_runtime_config(runtime), &runtime_before));
    CHECK(!g0_visualizer_runtime_resize_world(runtime, 5U, 6U));
    CHECK(config_equal(g0_visualizer_runtime_config(runtime), &runtime_before));

    g0_app_settings_default(&settings);
    CHECK(g0_world_document_create(&source, "resized", &config, &settings));
    CHECK(g0_world_document_save(TEST_RESIZE_FILE, &source, &error));
    CHECK(g0_world_document_load(TEST_RESIZE_FILE, &loaded, &error));
    CHECK(g0_world_document_equal(&source, &loaded));

    CHECK(g0_visualizer_header_control_layout(880, 0, controls));
    CHECK(g0_visualizer_header_controls_are_disjoint(controls));
    CHECK(g0_visualizer_header_control_layout(980, 1, controls));
    CHECK(g0_visualizer_header_controls_are_disjoint(controls));
    result = 1;
    g0_visualizer_runtime_destroy(&runtime);
    g0_world_document_destroy(&source);
    g0_world_document_destroy(&loaded);
    g0_world_config_destroy(&before_failure);
    g0_world_config_destroy(&runtime_before);
    g0_world_config_destroy(&config);
    (void)remove(TEST_RESIZE_FILE);
    return result;
}

static int test_settings_layout(void)
{
    G0VisualizerSettingsLayout normal;
    G0VisualizerSettingsLayout presentation;
    size_t left;
    size_t right;

    CHECK(g0_visualizer_settings_layout(880, 620, 0, &normal));
    CHECK(g0_visualizer_settings_layout_is_valid(&normal, 880, 620, 0));
    CHECK(normal.panel.right - normal.panel.left == 520);
    CHECK(normal.panel.bottom - normal.panel.top == 488);
    CHECK(normal.panel.bottom -
              normal.controls[G0_VISUALIZER_SETTINGS_BACK].bottom >= 24);

    CHECK(g0_visualizer_settings_layout(980, 840, 1, &presentation));
    CHECK(g0_visualizer_settings_layout_is_valid(&presentation, 980, 840, 1));
    CHECK(presentation.panel.right - presentation.panel.left == 780);
    CHECK(presentation.panel.bottom - presentation.panel.top == 732);
    CHECK(presentation.panel.bottom -
              presentation.controls[G0_VISUALIZER_SETTINGS_BACK].bottom >= 36);

    for (left = 0U; left < G0_VISUALIZER_SETTINGS_CONTROL_COUNT; ++left)
    {
        for (right = left + 1U; right < G0_VISUALIZER_SETTINGS_CONTROL_COUNT; ++right)
        {
            CHECK(!g0_visualizer_ui_rects_overlap(&normal.controls[left],
                                                  &normal.controls[right]));
            CHECK(!g0_visualizer_ui_rects_overlap(&presentation.controls[left],
                                                  &presentation.controls[right]));
        }
    }
    CHECK(!g0_visualizer_settings_layout(519, 620, 0, &normal));
    CHECK(!g0_visualizer_settings_layout(980, 731, 1, &presentation));
    return 1;
}
int main(void)
{
    if (!test_document_roundtrip_and_determinism() ||
        !test_corrupt_load_preserves_document() ||
        !test_prepare_edit_preserves_blueprint() || !test_reset_episode() ||
        !test_event_log() || !test_settings_and_visual_independence() ||
        !test_v1_load_defaults_and_v2_roundtrip() || !test_transactional_resize() ||
         !test_settings_layout())
    {
        return 1;
    }
    puts("G0-D product shell, presentation, and resize contracts OK");
    return 0;
}
