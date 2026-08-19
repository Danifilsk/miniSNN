#include <stdio.h>
#include <string.h>

#include "g0_visualizer_runtime.h"

static int build_demo_config(G0WorldConfig *config)
{
    return g0_world_config_init(config, 10U, 10U) &&
           g0_world_config_set_tile(config, 3U, 3U,
                                    MINISNN_WORLDS_TERRAIN_TILE_LAND) &&
           g0_world_config_set_rock(config, 5U, 4U, 1) &&
           g0_world_config_set_food(config, 2U, 1U, 1) &&
           g0_world_config_set_food(config, 6U, 6U, 1) &&
           g0_world_config_set_food(config, 8U, 2U, 1) &&
           g0_world_config_set_fish_spawn(config, 1U, 1U);
}

int main(int argc, char **argv)
{
    G0WorldConfig config;
    G0VisualizerRuntime *runtime = NULL;
    WF0FishWorldState first;
    WF0FishWorldState second;
    FILE *summary;
    unsigned int step;
    int success = 0;

    if (argc != 2)
        return 1;
    memset(&config, 0, sizeof(config));
    if (!build_demo_config(&config))
        goto done;
    runtime = g0_visualizer_runtime_create_from_config(&config);
    if (runtime == NULL)
        goto done;
    for (step = 0U; step < 12U; ++step)
    {
        if (!g0_visualizer_runtime_step_once(runtime))
            goto done;
    }
    first = *g0_visualizer_runtime_state(runtime);
    g0_visualizer_runtime_destroy(&runtime);
    runtime = g0_visualizer_runtime_create_from_config(&config);
    if (runtime == NULL)
        goto done;
    while (g0_visualizer_runtime_speed_increase(runtime))
    {
    }
    if (!g0_visualizer_runtime_advance_elapsed(runtime, 375U))
        goto done;
    second = *g0_visualizer_runtime_state(runtime);
    summary = fopen(argv[1], "w");
    if (summary == NULL)
        goto done;
    fprintf(summary, "G0-C interactive sandbox demo\n");
    fprintf(summary, "dimensions=10x10\n");
    fprintf(summary, "food_configured=3\n");
    fprintf(summary, "ticks_explicit=%llu\n", (unsigned long long)first.tick);
    fprintf(summary, "kernel_hash_explicit=%llu\n", (unsigned long long)first.kernel_hash);
    fprintf(summary, "ticks_fast=%llu\n", (unsigned long long)second.tick);
    fprintf(summary, "kernel_hash_fast=%llu\n", (unsigned long long)second.kernel_hash);
    fprintf(summary, "speed_changes_only_pacing=%s\n",
            second.tick == first.tick && second.kernel_hash == first.kernel_hash ? "yes" : "no");
    fclose(summary);
    success = 1;
done:
    g0_visualizer_runtime_destroy(&runtime);
    g0_world_config_destroy(&config);
    return success ? 0 : 1;
}
