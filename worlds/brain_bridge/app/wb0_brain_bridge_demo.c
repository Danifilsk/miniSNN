#include "minisnn_worlds_brain_bridge.h"
#include "minisnn_worlds_kernel.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    FILE *sensor_frames;
    FILE *neural_outputs;
    FILE *decisions;
    FILE *world_hashes;
} DemoFiles;

static MiniSNNWorldsKernelEntityId entity_id(uint64_t value)
{
    MiniSNNWorldsKernelEntityId result = { value };
    return result;
}

static MiniSNNWorldsKernelTransform transform_at(MiniSNNWorldsKernelScalar x,
                                                   MiniSNNWorldsKernelScalar y)
{
    MiniSNNWorldsKernelTransform result;
    result.position.x = x;
    result.position.y = y;
    result.orientation = UINT32_C(0);
    return result;
}

static int join_path(char *out, size_t size, const char *directory, const char *name)
{
    int written = snprintf(out, size, "%s/%s", directory, name);
    return written >= 0 && (size_t)written < size;
}

static int create_placed_entity(MiniSNNWorldsKernel *kernel,
                                 MiniSNNWorldsKernelScalar x,
                                 MiniSNNWorldsKernelScalar y,
                                 MiniSNNWorldsKernelEntityId *out_id)
{
    MiniSNNWorldsKernelCommandId command;
    size_t count;
    MiniSNNWorldsTick tick = minisnn_worlds_kernel_tick(kernel);

    if (minisnn_worlds_kernel_queue_create_entity(
            kernel, tick + UINT64_C(1), 0U, entity_id(0U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    count = minisnn_worlds_kernel_entity_count(kernel);
    if (count == 0U || minisnn_worlds_kernel_entity_at(kernel, count - 1U, out_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(
            kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
            entity_id(0U), *out_id, transform_at(x, y), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    return 1;
}

static const char *channel_name(MiniSNNWorldsBrainActionChannel channel)
{
    static const char *const names[] = {
        "WAIT", "MOVE_POS_X", "MOVE_NEG_X", "MOVE_POS_Y", "MOVE_NEG_Y", "EAT"
    };
    return channel < MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1 ? names[channel] : "INVALID";
}

static const char *action_name(MiniSNNWorldsDomainActionType action)
{
    switch (action)
    {
        case MINISNN_WORLDS_DOMAIN_ACTION_WAIT: return "WAIT";
        case MINISNN_WORLDS_DOMAIN_ACTION_MOVE: return "MOVE";
        case MINISNN_WORLDS_DOMAIN_ACTION_EAT: return "EAT";
        default: return "INVALID";
    }
}

static const char *fallback_name(MiniSNNWorldsBrainBridgeFallback fallback)
{
    switch (fallback)
    {
        case MINISNN_WORLDS_BRAIN_BRIDGE_FALLBACK_NONE: return "NONE";
        case MINISNN_WORLDS_BRAIN_BRIDGE_FALLBACK_NO_OUTPUT: return "NO_OUTPUT";
        case MINISNN_WORLDS_BRAIN_BRIDGE_FALLBACK_EAT_WITHOUT_FOOD: return "EAT_WITHOUT_FOOD";
        default: return "INVALID";
    }
}

static int log_report(DemoFiles *files, const MiniSNNWorldsBrainDecisionReport *report)
{
    const MiniSNNWorldsBrainSensorFrameV1 *sensor = &report->sensor_frame;
    const uint32_t *scores = report->output_scores;
    return fprintf(files->sensor_frames,
                   "%" PRIu64 ",%" PRIu64 ",%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                   report->domain_tick, report->actor.value,
                   sensor->values[0], sensor->values[1], sensor->values[2],
                   sensor->values[3], sensor->values[4], sensor->values[5]) > 0 &&
           fprintf(files->neural_outputs,
                   "%" PRIu64 ",%" PRIu64 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 "\n",
                   report->domain_tick, report->actor.value, scores[0], scores[1], scores[2],
                   scores[3], scores[4], scores[5]) > 0 &&
           fprintf(files->decisions,
                   "%" PRIu64 ",%" PRIu64 ",%s,%s,%" PRIu64 ",%" PRId64 ",%" PRId64 ",%u,%s,%u,%" PRIu32 "\n",
                   report->domain_tick, report->actor.value, channel_name(report->selected_channel),
                   action_name(report->action.type), report->action.eat_target.value,
                   report->action.move_delta.x, report->action.move_delta.y,
                   (unsigned int)report->tie_break_used, fallback_name(report->fallback),
                   (unsigned int)report->cache_hit, report->core_steps_executed) > 0;
}

static int log_hash(DemoFiles *files, const MiniSNNWorldsDomain *domain,
                    const MiniSNNWorldsKernel *kernel)
{
    uint64_t domain_hash;
    uint64_t kernel_hash;
    return minisnn_worlds_domain_state_hash(domain, &domain_hash) ==
               MINISNN_WORLDS_DOMAIN_ERROR_NONE &&
           minisnn_worlds_kernel_state_hash(kernel, &kernel_hash) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           fprintf(files->world_hashes, "%" PRIu64 ",0x%016" PRIX64 ",0x%016" PRIX64 "\n",
                   minisnn_worlds_domain_tick(domain), kernel_hash, domain_hash) > 0;
}

int main(int argc, char **argv)
{
    const char *directory = argc == 2 ? argv[1] : "build/worlds/brain_bridge/results/wb0_demo";
    MiniSNNWorldsKernelConfig kernel_config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsDomainSpeciesConfig species;
    MiniSNNWorldsKernelError kernel_error;
    MiniSNNWorldsDomainError domain_error;
    MiniSNNWorldsBrainBridgeError bridge_error;
    MiniSNNWorldsKernel *kernel = NULL;
    MiniSNNWorldsDomain *domain = NULL;
    MiniSNNWorldsBrainBridge *bridge = NULL;
    MiniSNN *brain = NULL;
    MiniSNNWorldsKernelEntityId actor;
    MiniSNNWorldsKernelEntityId food;
    MiniSNNWorldsBrainBridgeConfig bridge_config;
    MiniSNNWorldsBrainBridgeDiagnostics diagnostics;
    MiniSNNWorldsDomainAction action;
    MiniSNNWorldsDomainActionResult result;
    MiniSNNWorldsBrainDecisionReport report;
    DemoFiles files = {0};
    char path[512];
    FILE *summary = NULL;
    uint64_t final_kernel_hash;
    uint64_t final_domain_hash;
    uint32_t tick_index;
    int success = 0;

    kernel_config.space_bounds.min_x = -10000;
    kernel_config.space_bounds.min_y = -10000;
    kernel_config.space_bounds.max_x = 10000;
    kernel_config.space_bounds.max_y = 10000;
    kernel = minisnn_worlds_kernel_create(&kernel_config, &kernel_error);
    if (kernel == NULL || kernel_error != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !create_placed_entity(kernel, 0, 0, &actor) ||
        !create_placed_entity(kernel, 5000, 0, &food)) goto cleanup;

    domain = minisnn_worlds_domain_create(kernel, &domain_error);
    species.species_id = UINT64_C(1);
    species.max_energy = UINT64_C(100);
    species.metabolism_per_tick = UINT64_C(1);
    species.move_energy_cost = UINT64_C(1);
    species.eat_range = 1000;
    if (domain == NULL || domain_error != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_add_species(domain, &species) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_register_organism(domain, actor, species.species_id, UINT64_C(50)) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_register_food(domain, food, UINT64_C(10)) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE) goto cleanup;

    bridge_config = minisnn_worlds_brain_bridge_config_default();
    bridge_config.input_gain = 1000.0;
    bridge_config.core_steps_per_decision = 8U;
    bridge_config.action_neuron[MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_X] = 0;
    bridge_config.action_neuron[MINISNN_WORLDS_BRAIN_ACTION_WAIT] = 6;
    bridge_config.action_neuron[MINISNN_WORLDS_BRAIN_ACTION_MOVE_NEG_X] = 7;
    bridge_config.action_neuron[MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_Y] = 8;
    bridge_config.action_neuron[MINISNN_WORLDS_BRAIN_ACTION_MOVE_NEG_Y] = 9;
    bridge_config.action_neuron[MINISNN_WORLDS_BRAIN_ACTION_EAT] = 10;
    bridge = minisnn_worlds_brain_bridge_create(&bridge_config, &bridge_error);
    brain = minisnn_create(12);
    if (bridge == NULL || brain == NULL || bridge_error != MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE ||
        minisnn_worlds_brain_bridge_bind(bridge, domain, actor, brain) !=
            MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE) goto cleanup;

    if (!join_path(path, sizeof(path), directory, "sensor_frames.csv") ||
        (files.sensor_frames = fopen(path, "w")) == NULL ||
        fprintf(files.sensor_frames, "tick,actor,self_energy,self_hunger,nearest_food_present,nearest_food_delta_x,nearest_food_delta_y,nearest_food_distance\n") <= 0 ||
        !join_path(path, sizeof(path), directory, "neural_outputs.csv") ||
        (files.neural_outputs = fopen(path, "w")) == NULL ||
        fprintf(files.neural_outputs, "tick,actor,wait,move_pos_x,move_neg_x,move_pos_y,move_neg_y,eat\n") <= 0 ||
        !join_path(path, sizeof(path), directory, "decisions.csv") ||
        (files.decisions = fopen(path, "w")) == NULL ||
        fprintf(files.decisions, "tick,actor,channel,action,target,delta_x,delta_y,tie_break,fallback,cache_hit,core_steps\n") <= 0 ||
        !join_path(path, sizeof(path), directory, "world_hashes.csv") ||
        (files.world_hashes = fopen(path, "w")) == NULL ||
        fprintf(files.world_hashes, "tick,kernel_hash,domain_hash\n") <= 0 ||
        !log_hash(&files, domain, kernel)) goto cleanup;

    for (tick_index = 0U; tick_index < 3U; ++tick_index)
    {
        if (minisnn_worlds_brain_bridge_decide(bridge, domain, actor, &action, &report) !=
                MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE ||
            !log_report(&files, &report)) goto cleanup;
        if (tick_index == 0U)
        {
            MiniSNNWorldsDomainAction cached_action;
            MiniSNNWorldsBrainDecisionReport cached_report;
            if (minisnn_worlds_brain_bridge_decide(bridge, domain, actor, &cached_action, &cached_report) !=
                    MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE ||
                !cached_report.cache_hit ||
                memcmp(&action, &cached_action, sizeof(action)) != 0) goto cleanup;
        }
        if (minisnn_worlds_domain_step(domain, &action, 1U, &result) !=
                MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
            !log_hash(&files, domain, kernel)) goto cleanup;
    }

    if (minisnn_worlds_brain_bridge_get_diagnostics(bridge, &diagnostics) !=
            MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(kernel, &final_kernel_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_domain_state_hash(domain, &final_domain_hash) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        !join_path(path, sizeof(path), directory, "summary.txt") ||
        (summary = fopen(path, "w")) == NULL ||
        fprintf(summary,
                "decisions_computed=%" PRIu64 "\ncache_hits=%" PRIu64 "\ncore_steps_per_decision=%" PRIu32
                "\nfinal_domain_tick=%" PRIu64 "\nfinal_kernel_hash=0x%016" PRIX64
                "\nfinal_domain_hash=0x%016" PRIX64 "\n",
                diagnostics.total_decisions_computed, diagnostics.total_cache_hits,
                bridge_config.core_steps_per_decision, minisnn_worlds_domain_tick(domain),
                final_kernel_hash, final_domain_hash) <= 0) goto cleanup;
    success = 1;

cleanup:
    if (summary != NULL) fclose(summary);
    if (files.sensor_frames != NULL) fclose(files.sensor_frames);
    if (files.neural_outputs != NULL) fclose(files.neural_outputs);
    if (files.decisions != NULL) fclose(files.decisions);
    if (files.world_hashes != NULL) fclose(files.world_hashes);
    minisnn_worlds_brain_bridge_destroy(&bridge);
    minisnn_destroy(&brain);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    if (!success)
    {
        fputs("WB0 Brain Bridge demo failed\n", stderr);
        return 1;
    }
    puts("WB0 deterministic Brain Bridge demo OK");
    return 0;
}