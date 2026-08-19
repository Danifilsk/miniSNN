#include <stdio.h>
#include <string.h>

#include "g0_event_log.h"
#include "g0_visualizer_runtime.h"
#include "g0_world_document.h"

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

static const char *action_reason_name(MiniSNNWorldsDomainActionReason reason)
{
    switch (reason)
    {
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_NONE: return "NONE";
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_ACTOR_NOT_ORGANISM: return "ACTOR_NOT_ORGANISM";
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_TARGET_NOT_FOOD: return "TARGET_NOT_FOOD";
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_TARGET_NOT_AVAILABLE: return "TARGET_NOT_AVAILABLE";
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_TARGET_OUT_OF_RANGE: return "TARGET_OUT_OF_RANGE";
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_INSUFFICIENT_ENERGY: return "INSUFFICIENT_ENERGY";
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_INVALID_ACTION: return "INVALID_ACTION";
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_KERNEL_REJECTED: return "KERNEL_REJECTED";
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_DUPLICATE_ACTOR: return "DUPLICATE_ACTOR";
        default: return "UNKNOWN";
    }
}
int main(int argc, char **argv)
{
    G0WorldConfig config;
    G0AppSettings settings;
    G0WorldDocument saved;
    G0WorldDocument loaded;
    G0WorldDocumentError error;
    G0VisualizerRuntime *runtime = NULL;
    G0EventLog log;
    WF0FishWorldState initial;
    WF0FishWorldState before_reset;
    WF0FishWorldState after_reset;
    uint64_t episode_ticks_before_reset;
    char world_path[1024];
    char log_path[1024];
    FILE *summary = NULL;
    unsigned int step;
    int success = 0;

    if (argc != 2 || snprintf(world_path, sizeof(world_path), "%s.world", argv[1]) <= 0 ||
        snprintf(log_path, sizeof(log_path), "%s.events.csv", argv[1]) <= 0 ||
        strlen(world_path) >= sizeof(world_path) || strlen(log_path) >= sizeof(log_path))
    {
        return 1;
    }
    memset(&config, 0, sizeof(config));
    g0_world_document_init(&saved);
    g0_world_document_init(&loaded);
    g0_event_log_init(&log);
    g0_app_settings_default(&settings);
    settings.start_paused = 1;
    if (!build_demo_config(&config) ||
        !g0_world_document_create(&saved, "g0 d demo", &config, &settings) ||
        !g0_world_document_save(world_path, &saved, &error) ||
        !g0_world_document_load(world_path, &loaded, &error) ||
        !g0_world_document_equal(&saved, &loaded))
    {
        goto done;
    }
    runtime = g0_visualizer_runtime_create_from_config(&loaded.world);
    if (runtime == NULL)
        goto done;
    initial = *g0_visualizer_runtime_state(runtime);
    if (!g0_event_log_append(&log, g0_visualizer_runtime_episode_tick(runtime), G0_LOG_CATEGORY_SYSTEM, 0, 0U,
                             "EPISODE_STARTED", "NA", "NONE", "Episode started"))
    {
        goto done;
    }
    for (step = 0U; step < 8U; ++step)
    {
        const WF0FishTickRecord *record;
        if (!g0_visualizer_runtime_step_once(runtime))
            goto done;
        record = g0_visualizer_runtime_last_record(runtime);
        if (record == NULL || !g0_event_log_append(
                &log, g0_visualizer_runtime_episode_tick(runtime),
                G0_LOG_CATEGORY_ACTION, 1, 1U,
                g0_visualizer_action_name(record->decision.action.type),
                record->action_result.status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED ?
                    "APPLIED" : "REJECTED",
                action_reason_name(record->action_result.reason),
                "WF0 action result"))
        {
            goto done;
        }
    }
    before_reset = *g0_visualizer_runtime_state(runtime);
    episode_ticks_before_reset = g0_visualizer_runtime_episode_tick(runtime);
    if (!g0_visualizer_runtime_reset(runtime))
        goto done;
    after_reset = *g0_visualizer_runtime_state(runtime);
    if (!g0_event_log_append(&log, g0_visualizer_runtime_episode_tick(runtime), G0_LOG_CATEGORY_SYSTEM, 0, 0U,
                             "EPISODE_RESET", "APPLIED", "NONE", "Episode reset"))
    {
        goto done;
    }
    summary = fopen(argv[1], "w");
    if (summary == NULL)
        goto done;
    fprintf(summary, "G0-D product shell demo\n");
    fprintf(summary, "format=%s\n", G0_WORLD_DOCUMENT_FORMAT);
    fprintf(summary, "world_equivalent=%s\n",
            g0_world_document_equal(&saved, &loaded) ? "yes" : "no");
    fprintf(summary, "domain_ticks_before_reset=%llu\n",
            (unsigned long long)before_reset.tick);
    fprintf(summary, "episode_ticks_before_reset=%llu\n",
            (unsigned long long)episode_ticks_before_reset);
    fprintf(summary, "initial_domain_tick=%llu\n", (unsigned long long)initial.tick);
    fprintf(summary, "reset_domain_tick=%llu\n", (unsigned long long)after_reset.tick);
    fprintf(summary, "reset_episode_tick=%llu\n",
            (unsigned long long)g0_visualizer_runtime_episode_tick(runtime));
    fprintf(summary, "next_episode_tick_after_reset=%llu\n",
            (unsigned long long)(g0_visualizer_runtime_episode_tick(runtime) + 1U));
    fprintf(summary, "reset_energy_restored=%s\n",
            initial.actor_energy == after_reset.actor_energy ? "yes" : "no");
    fprintf(summary, "reset_food_restored=%s\n",
            initial.remaining_food == after_reset.remaining_food ? "yes" : "no");
    fprintf(summary, "event_count=%llu\n", (unsigned long long)g0_event_log_count(&log));
    fprintf(summary, "event_log=generated\n");
    if (fclose(summary) != 0)
    {
        summary = NULL;
        goto done;
    }
    summary = NULL;
    if (!g0_event_log_export_csv(&log, log_path))
        goto done;
    success = 1;
done:
    if (summary != NULL)
        (void)fclose(summary);
    g0_visualizer_runtime_destroy(&runtime);
    g0_world_document_destroy(&saved);
    g0_world_document_destroy(&loaded);
    g0_world_config_destroy(&config);
    (void)remove(world_path);
    return success ? 0 : 1;
}
