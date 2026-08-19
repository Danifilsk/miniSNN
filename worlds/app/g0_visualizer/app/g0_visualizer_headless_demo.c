#include "g0_visualizer_runtime.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define G0_DEMO_TICKS 3U
#define G0_FNV_OFFSET UINT64_C(14695981039346656037)
#define G0_FNV_PRIME UINT64_C(1099511628211)

static uint64_t fnv1a_update(uint64_t hash, const char *text)
{
    size_t index;

    for (index = 0U; text[index] != '\0'; ++index)
    {
        hash ^= (unsigned char)text[index];
        hash *= G0_FNV_PRIME;
    }
    return hash;
}

int main(int argc, char **argv)
{
    const char *output_directory;
    const char *repository_root;
    char summary_path[512];
    char trace_path[512];
    char assets_error[G0_VISUALIZER_ASSET_ERROR_MAX];
    G0VisualizerAssets assets;
    G0VisualizerRuntime *runtime = NULL;
    FILE *summary = NULL;
    FILE *trace = NULL;
    uint64_t terrain_hash;
    uint64_t trace_digest = G0_FNV_OFFSET;
    unsigned int index;
    unsigned int moves = 0U;
    unsigned int eats = 0U;
    const WF0FishWorldState *state;

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <output_directory> <repository_root>\n", argv[0]);
        return 1;
    }
    output_directory = argv[1];
    repository_root = argv[2];
    if (snprintf(summary_path, sizeof(summary_path), "%s/g0_summary.txt", output_directory) < 0 ||
        snprintf(trace_path, sizeof(trace_path), "%s/g0_trace.csv", output_directory) < 0 ||
        !g0_visualizer_assets_load(&assets, repository_root, assets_error,
                                   sizeof(assets_error)))
    {
        fprintf(stderr, "G0 asset/output setup failed: %s\n", assets_error);
        return 1;
    }
    runtime = g0_visualizer_runtime_create(0);
    summary = fopen(summary_path, "wb");
    trace = fopen(trace_path, "wb");
    if (runtime == NULL || summary == NULL || trace == NULL ||
        fprintf(trace, "tick,x,y,energy,action,action_status,remaining_food,kernel_hash,domain_hash,core_step\n") < 0)
    {
        fprintf(stderr, "G0 failed to create runtime or outputs.\n");
        fclose(summary);
        fclose(trace);
        g0_visualizer_runtime_destroy(&runtime);
        return 1;
    }
    for (index = 0U; index < G0_DEMO_TICKS; ++index)
    {
        const WF0FishTickRecord *record;
        char line[512];
        int length;

        if (!g0_visualizer_runtime_step_once(runtime))
        {
            fprintf(stderr, "G0 tick %u failed.\n", index);
            fclose(summary);
            fclose(trace);
            g0_visualizer_runtime_destroy(&runtime);
            return 1;
        }
        record = g0_visualizer_runtime_last_record(runtime);
        state = g0_visualizer_runtime_state(runtime);
        if (record == NULL || state == NULL)
        {
            fprintf(stderr, "G0 runtime state was unavailable.\n");
            fclose(summary);
            fclose(trace);
            g0_visualizer_runtime_destroy(&runtime);
            return 1;
        }
        length = snprintf(
            line, sizeof(line),
            "%" PRIu64 ",%" PRId64 ",%" PRId64 ",%" PRIu64 ",%s,%d,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%d\n",
            state->tick, state->actor_transform.position.x,
            state->actor_transform.position.y, state->actor_energy,
            g0_visualizer_action_name(record->decision.action.type),
            (int)record->action_result.status, state->remaining_food,
            state->kernel_hash, state->domain_hash, state->core_step);
        if (length < 0 || (size_t)length >= sizeof(line) || fputs(line, trace) == EOF)
        {
            fprintf(stderr, "G0 trace write failed.\n");
            fclose(summary);
            fclose(trace);
            g0_visualizer_runtime_destroy(&runtime);
            return 1;
        }
        trace_digest = fnv1a_update(trace_digest, line);
        if (record->decision.action.type == MINISNN_WORLDS_DOMAIN_ACTION_MOVE &&
            record->action_result.status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED)
        {
            ++moves;
        }
        if (record->decision.action.type == MINISNN_WORLDS_DOMAIN_ACTION_EAT &&
            record->action_result.status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED)
        {
            ++eats;
        }
    }
    state = g0_visualizer_runtime_state(runtime);
    if (state == NULL || minisnn_worlds_terrain_hash(
            g0_visualizer_runtime_terrain(runtime), &terrain_hash) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        fprintf(summary,
                "format=G0-A Visualizer V1\n"
                "tile_pixels=%d\n"
                "entity_pixels=%d\n"
                "water_overlay_alpha=0.50\n"
                "draw_order=base,rock,food_fish,water,ui\n"
                "ticks=%u\n"
                "moves=%u\n"
                "eats=%u\n"
                "final_x=%" PRId64 "\n"
                "final_y=%" PRId64 "\n"
                "final_energy=%" PRIu64 "\n"
                "remaining_food=%" PRIu64 "\n"
                "terrain_hash=%" PRIu64 "\n"
                "final_kernel_hash=%" PRIu64 "\n"
                "final_domain_hash=%" PRIu64 "\n"
                "trace_digest=%" PRIu64 "\n",
                G0_VISUALIZER_TILE_PIXELS, G0_VISUALIZER_ENTITY_PIXELS,
                G0_DEMO_TICKS, moves, eats, state->actor_transform.position.x,
                state->actor_transform.position.y, state->actor_energy,
                state->remaining_food, terrain_hash, state->kernel_hash,
                state->domain_hash, trace_digest) < 0)
    {
        fprintf(stderr, "G0 summary write failed.\n");
        fclose(summary);
        fclose(trace);
        g0_visualizer_runtime_destroy(&runtime);
        return 1;
    }
    fclose(summary);
    fclose(trace);
    printf("G0-A Visualizer\n");
    printf("  moves: %u\n", moves);
    printf("  eats: %u\n", eats);
    printf("  trace: %s\n", trace_path);
    printf("  summary: %s\n", summary_path);
    g0_visualizer_runtime_destroy(&runtime);
    return moves == 1U && eats == 1U ? 0 : 1;
}
