#include "g0_visualizer_runtime.h"

#include <stdio.h>
#include <string.h>

static int assert_true(int condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "G0-A test failure: %s\n", message);
        return 0;
    }
    return 1;
}

static int test_assets(const char *repository_root)
{
    G0VisualizerAssets assets;
    char error[G0_VISUALIZER_ASSET_ERROR_MAX];

    return assert_true(g0_visualizer_assets_load(&assets, repository_root, error,
                                                 sizeof(error)),
                       "required PNG assets load") &&
           assert_true(strstr(g0_visualizer_asset_path(
                           &assets, G0_VISUALIZER_ASSET_FISH), "peixe.png") != NULL,
                       "fish asset resolves to existing source") &&
           assert_true(!g0_visualizer_assets_load(&assets, "missing-g0-root", error,
                                                  sizeof(error)) &&
                       strstr(error, "missing or invalid") != NULL,
                       "missing asset fails clearly");
}

static int test_render_contract(void)
{
    G0VisualizerRuntime *runtime = g0_visualizer_runtime_create(0);
    G0VisualizerLayer order[G0_VISUALIZER_LAYER_COUNT];
    const MiniSNNWorldsTerrain *terrain;
    G0VisualizerScreenPoint fish;
    MiniSNNWorldsTerrainTile tile;
    bool rock = false;
    int passed = 0;

    if (!assert_true(runtime != NULL, "normal runtime creation"))
    {
        return 0;
    }
    terrain = g0_visualizer_runtime_terrain(runtime);
    if (!assert_true(g0_visualizer_layer_order(order, G0_VISUALIZER_LAYER_COUNT) ==
                         G0_VISUALIZER_LAYER_COUNT &&
                     order[0] == G0_VISUALIZER_LAYER_BASE_TERRAIN &&
                     order[1] == G0_VISUALIZER_LAYER_TERRAIN_OBJECTS &&
                     order[2] == G0_VISUALIZER_LAYER_WORLD_ENTITIES &&
                     order[3] == G0_VISUALIZER_LAYER_WATER_OVERLAY &&
                     order[4] == G0_VISUALIZER_LAYER_UI,
                     "base, rock, entities, water, UI layer contract") ||
        !assert_true(minisnn_worlds_terrain_get_tile(terrain, 1U, 3U, &tile) ==
                         MINISNN_WORLDS_TERRAIN_ERROR_NONE &&
                     tile == MINISNN_WORLDS_TERRAIN_TILE_LAND &&
                     minisnn_worlds_terrain_has_rock(terrain, 2U, 1U, &rock) ==
                         MINISNN_WORLDS_TERRAIN_ERROR_NONE && rock,
                     "LAND and ROCK map to their logical cells") ||
        !assert_true(g0_visualizer_world_to_screen(
                         runtime, g0_visualizer_runtime_state(runtime)->actor_transform.position,
                         20, 40, &fish) &&
                     fish.screen_x == 20 && fish.screen_y == 40,
                     "fish world position aligns with water tile origin") ||
        !assert_true(g0_visualizer_runtime_food_visible(runtime),
                     "food is visible before it is eaten"))
    {
        goto done;
    }
    passed = 1;

done:
    g0_visualizer_runtime_destroy(&runtime);
    return passed;
}

static int test_runtime_controls(void)
{
    G0VisualizerRuntime *runtime = g0_visualizer_runtime_create(0);
    MiniSNNWorldsTick initial_tick;
    int passed = 0;

    if (!assert_true(runtime != NULL, "runtime control world creation"))
    {
        return 0;
    }
    initial_tick = g0_visualizer_runtime_state(runtime)->tick;
    g0_visualizer_runtime_toggle_paused(runtime);
    if (!assert_true(g0_visualizer_runtime_is_paused(runtime) &&
                     g0_visualizer_runtime_advance(runtime) &&
                     g0_visualizer_runtime_state(runtime)->tick == initial_tick,
                     "paused advance does not change real state") ||
        !assert_true(g0_visualizer_runtime_step_once(runtime) &&
                     g0_visualizer_runtime_state(runtime)->tick > initial_tick,
                     "single step changes real state while paused"))
    {
        goto done;
    }
    g0_visualizer_runtime_toggle_grid(runtime);
    g0_visualizer_runtime_toggle_debug(runtime);
    if (!assert_true(!g0_visualizer_runtime_grid_enabled(runtime) &&
                     !g0_visualizer_runtime_debug_enabled(runtime) &&
                     g0_visualizer_runtime_reset(runtime) &&
                     g0_visualizer_runtime_state(runtime)->actor_transform.position.x == 0 &&
                     g0_visualizer_runtime_state(runtime)->remaining_food == 1U,
                     "grid/debug toggles and reset preserve UI while rebuilding scene"))
    {
        goto done;
    }
    passed = 1;

done:
    g0_visualizer_runtime_destroy(&runtime);
    return passed;
}

static int test_blocked_movement_is_visual_state(void)
{
    G0VisualizerRuntime *runtime = g0_visualizer_runtime_create(1);
    const WF0FishTickRecord *record;
    const WF0FishWorldState *state;
    int passed = 0;

    if (!assert_true(runtime != NULL, "blocked runtime creation") ||
        !assert_true(g0_visualizer_runtime_step_once(runtime), "blocked first tick"))
    {
        g0_visualizer_runtime_destroy(&runtime);
        return 0;
    }
    record = g0_visualizer_runtime_last_record(runtime);
    state = g0_visualizer_runtime_state(runtime);
    if (assert_true(record != NULL && state != NULL &&
                    record->decision.action.type == MINISNN_WORLDS_DOMAIN_ACTION_MOVE &&
                    record->decision.action.move_delta.x == 1000 &&
                    record->action_result.status == MINISNN_WORLDS_DOMAIN_ACTION_REJECTED &&
                    record->action_result.reason ==
                        MINISNN_WORLDS_DOMAIN_ACTION_REASON_KERNEL_REJECTED &&
                    state->actor_transform.position.x == 0 &&
                    state->actor_transform.position.y == 0,
                    "SNN +X action blocked by ROCK leaves rendered fish tile unchanged"))
    {
        passed = 1;
    }
    g0_visualizer_runtime_destroy(&runtime);
    return passed;
}

static int test_realtime_scheduler(void)
{
    G0VisualizerRuntime *runtime = g0_visualizer_runtime_create(0);
    MiniSNNWorldsTick initial_tick;
    int passed = 0;

    if (!assert_true(runtime != NULL, "runtime scheduler creation"))
    {
        return 0;
    }
    initial_tick = g0_visualizer_runtime_state(runtime)->tick;
    if (!assert_true(g0_visualizer_runtime_ticks_per_second(runtime) == 4U,
                     "runtime has visible four ticks per second") ||
        !assert_true(g0_visualizer_runtime_advance_elapsed(runtime, 249U) &&
                     g0_visualizer_runtime_state(runtime)->tick == initial_tick,
                     "subinterval elapsed time does not advance a tick") ||
        !assert_true(g0_visualizer_runtime_advance_elapsed(runtime, 1U) &&
                     g0_visualizer_runtime_state(runtime)->tick == initial_tick + 1U,
                     "one full interval advances exactly one tick"))
    {
        goto done;
    }
    g0_visualizer_runtime_toggle_paused(runtime);
    if (!assert_true(g0_visualizer_runtime_advance_elapsed(runtime, 1000U) &&
                     g0_visualizer_runtime_state(runtime)->tick == initial_tick + 1U,
                     "paused scheduler preserves the world state"))
    {
        goto done;
    }
    passed = 1;

done:
    g0_visualizer_runtime_destroy(&runtime);
    return passed;
}

static int test_facing_contract(void)
{
    G0VisualizerRuntime *normal = g0_visualizer_runtime_create(0);
    G0VisualizerRuntime *blocked = g0_visualizer_runtime_create(1);
    MiniSNNWorldsKernelPosition delta;
    const WF0FishTickRecord *record;
    int passed = 0;

    delta.x = 0;
    delta.y = -1000;
    if (!assert_true(g0_visualizer_facing_from_move_delta(
                         G0_VISUALIZER_FACING_SOUTH, delta) ==
                     G0_VISUALIZER_FACING_NORTH,
                     "negative Y maps to NORTH") ||
        !assert_true((delta.x = -1000, delta.y = 0,
                      g0_visualizer_facing_from_move_delta(
                          G0_VISUALIZER_FACING_SOUTH, delta) ==
                      G0_VISUALIZER_FACING_WEST),
                     "negative X maps to WEST") ||
        !assert_true(normal != NULL && blocked != NULL,
                     "facing runtimes creation") ||
        !assert_true(g0_visualizer_runtime_facing(normal) ==
                         G0_VISUALIZER_FACING_SOUTH &&
                     g0_visualizer_runtime_facing(blocked) ==
                         G0_VISUALIZER_FACING_SOUTH,
                     "initial facing is SOUTH"))
    {
        goto done;
    }
    if (!assert_true(g0_visualizer_runtime_step_once(normal),
                     "normal movement step") ||
        !assert_true(g0_visualizer_runtime_facing(normal) ==
                         G0_VISUALIZER_FACING_EAST,
                     "successful +X movement updates facing to EAST") ||
        !assert_true(g0_visualizer_runtime_step_once(blocked),
                     "blocked movement step"))
    {
        goto done;
    }
    record = g0_visualizer_runtime_last_record(blocked);
    if (!assert_true(record != NULL &&
                     record->action_result.status ==
                         MINISNN_WORLDS_DOMAIN_ACTION_REJECTED &&
                     g0_visualizer_runtime_facing(blocked) ==
                         G0_VISUALIZER_FACING_SOUTH,
                     "blocked movement preserves the last successful facing"))
    {
        goto done;
    }
    passed = 1;

done:
    g0_visualizer_runtime_destroy(&normal);
    g0_visualizer_runtime_destroy(&blocked);
    return passed;
}
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <repository_root>\n", argv[0]);
        return 1;
    }
    if (!test_assets(argv[1]) || !test_render_contract() ||
        !test_runtime_controls() || !test_blocked_movement_is_visual_state() ||
        !test_realtime_scheduler() || !test_facing_contract())
    {
        return 1;
    }
    printf("G0-A visualizer runtime, assets, layering, and blocked movement OK\n");
    return 0;
}
