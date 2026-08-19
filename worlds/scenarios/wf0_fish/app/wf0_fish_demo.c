#include "wf0_fish.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WF0_DEMO_TICKS 3U
#define WF0_FNV_OFFSET UINT64_C(14695981039346656037)
#define WF0_FNV_PRIME UINT64_C(1099511628211)

static const char *action_name(MiniSNNWorldsDomainActionType action)
{
    switch (action)
    {
        case MINISNN_WORLDS_DOMAIN_ACTION_WAIT:
            return "WAIT";
        case MINISNN_WORLDS_DOMAIN_ACTION_MOVE:
            return "MOVE";
        case MINISNN_WORLDS_DOMAIN_ACTION_EAT:
            return "EAT";
        default:
            return "UNKNOWN";
    }
}

static uint64_t fnv1a_update(uint64_t hash, const char *text)
{
    size_t index;

    for (index = 0U; text[index] != '\0'; ++index)
    {
        hash ^= (unsigned char)text[index];
        hash *= WF0_FNV_PRIME;
    }
    return hash;
}

static int write_trace_header(FILE *trace)
{
    return fprintf(
        trace,
        "tick,x,y,energy_before_action,energy_after_action,metabolism,"
        "move_cost,food_nutrition,energy_after_tick,hunger,food_present,"
        "food_dx,food_dy,food_distance,sensor_energy,sensor_hunger,sensor_food_present,sensor_food_dx,sensor_food_dy,sensor_food_distance,selected_channel,domain_action,"
        "cache_hit,core_step_start,core_step_end,wait_score,move_pos_x_score,"
        "move_neg_x_score,move_pos_y_score,move_neg_y_score,eat_score,"
        "remaining_food,kernel_hash,domain_hash\n") >= 0;
}

static int write_trace_row(
    FILE *trace,
    const MiniSNNWorldsDomainPerception *perception,
    const WF0FishTickRecord *record,
    uint64_t *digest)
{
    char line[1024];
    int length;

    length = snprintf(
        line, sizeof(line),
        "%" PRIu64 ",%" PRId64 ",%" PRId64 ",%" PRIu64 ",%" PRIu64
        ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
        ",%d,%" PRId64 ",%" PRId64 ",%" PRIu64 ",%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%s,%u,%d,%d"
        ",%u,%u,%u,%u,%u,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
        record->tick_before,
        record->transform_after.position.x,
        record->transform_after.position.y,
        record->action_result.energy_before,
        record->action_result.energy_after,
        record->metabolism_cost,
        record->move_cost,
        record->food_nutrition,
        record->energy_after_tick,
        perception->self_hunger,
        perception->nearest_food_present ? 1 : 0,
        perception->nearest_food_delta_x,
        perception->nearest_food_delta_y,
        perception->nearest_food_distance,
        record->decision.sensor_frame.values[MINISNN_WORLDS_BRAIN_SENSOR_SELF_ENERGY],
        record->decision.sensor_frame.values[MINISNN_WORLDS_BRAIN_SENSOR_SELF_HUNGER],
        record->decision.sensor_frame.values[MINISNN_WORLDS_BRAIN_SENSOR_NEAREST_FOOD_PRESENT],
        record->decision.sensor_frame.values[MINISNN_WORLDS_BRAIN_SENSOR_NEAREST_FOOD_DELTA_X],
        record->decision.sensor_frame.values[MINISNN_WORLDS_BRAIN_SENSOR_NEAREST_FOOD_DELTA_Y],
        record->decision.sensor_frame.values[MINISNN_WORLDS_BRAIN_SENSOR_NEAREST_FOOD_DISTANCE],
        (int)record->decision.selected_channel,
        action_name(record->decision.action.type),
        (unsigned int)record->decision.cache_hit,
        record->core_step_start,
        record->core_step_end,
        record->decision.output_scores[MINISNN_WORLDS_BRAIN_ACTION_WAIT],
        record->decision.output_scores[MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_X],
        record->decision.output_scores[MINISNN_WORLDS_BRAIN_ACTION_MOVE_NEG_X],
        record->decision.output_scores[MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_Y],
        record->decision.output_scores[MINISNN_WORLDS_BRAIN_ACTION_MOVE_NEG_Y],
        record->decision.output_scores[MINISNN_WORLDS_BRAIN_ACTION_EAT],
        record->remaining_food,
        record->kernel_hash,
        record->domain_hash);
    if (length < 0 || (size_t)length >= sizeof(line) || fputs(line, trace) == EOF)
    {
        return 0;
    }
    *digest = fnv1a_update(*digest, line);
    return 1;
}

int main(int argc, char **argv)
{
    const char *output_dir = argc == 2 ? argv[1] : "results";
    char summary_path[512];
    char trace_path[512];
    FILE *summary = NULL;
    FILE *trace = NULL;
    WF0FishWorld *world = NULL;
    WF0FishTickRecord records[WF0_DEMO_TICKS];
    MiniSNNWorldsDomainPerception perceptions[WF0_DEMO_TICKS];
    MiniSNNWorldsDomainSpeciesConfig species;
    uint64_t trace_digest = WF0_FNV_OFFSET;
    unsigned int index;
    unsigned int move_count = 0U;
    unsigned int eat_count = 0U;

    if (argc > 2)
    {
        fprintf(stderr, "Usage: %s [output_directory]\n", argv[0]);
        return 1;
    }
    if (snprintf(summary_path, sizeof(summary_path), "%s/wf0_summary.txt", output_dir) < 0 ||
        snprintf(trace_path, sizeof(trace_path), "%s/wf0_fish_trace.csv", output_dir) < 0)
    {
        fprintf(stderr, "WF0 output path is too long.\n");
        return 1;
    }
    summary = fopen(summary_path, "wb");
    trace = fopen(trace_path, "wb");
    world = wf0_fish_world_create(1000, 0);
    if (summary == NULL || trace == NULL || world == NULL || !write_trace_header(trace))
    {
        fprintf(stderr, "WF0 failed to create its deterministic fish world or outputs.\n");
        fclose(summary);
        fclose(trace);
        wf0_fish_world_destroy(&world);
        return 1;
    }

    for (index = 0U; index < WF0_DEMO_TICKS; ++index)
    {
        if (!wf0_fish_world_perceive(world, &perceptions[index]) ||
            !wf0_fish_world_tick_once(world, &records[index]) ||
            !write_trace_row(trace, &perceptions[index], &records[index], &trace_digest))
        {
            fprintf(stderr, "WF0 tick %u failed.\n", index);
            fclose(summary);
            fclose(trace);
            wf0_fish_world_destroy(&world);
            return 1;
        }
        if (records[index].decision.action.type == MINISNN_WORLDS_DOMAIN_ACTION_MOVE)
        {
            ++move_count;
        }
        if (records[index].decision.action.type == MINISNN_WORLDS_DOMAIN_ACTION_EAT &&
            records[index].action_result.status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED)
        {
            ++eat_count;
        }
    }

    species = wf0_fish_species_v1();
    fprintf(
        summary,
        "format=WF0 Fish V1\n"
        "brain=Fixed deterministic LIF network, no plasticity or learning\n"
        "ticks=%u\n"
        "decisions=%u\n"
        "moves=%u\n"
        "eats=%u\n"
        "food_consumed=%u\n"
        "initial_energy=%" PRIu64 "\n"
        "final_energy=%" PRIu64 "\n"
        "metabolism_per_tick=%" PRIu64 "\n"
        "move_energy_cost=%" PRIu64 "\n"
        "food_nutrition=%u\n"
        "core_steps_per_decision=%u\n"
        "final_core_step=%d\n"
        "final_kernel_hash=%" PRIu64 "\n"
        "final_domain_hash=%" PRIu64 "\n"
        "trace_digest=%" PRIu64 "\n",
        WF0_DEMO_TICKS,
        WF0_DEMO_TICKS,
        move_count,
        eat_count,
        eat_count,
        UINT64_C(50),
        records[WF0_DEMO_TICKS - 1U].energy_after_tick,
        species.metabolism_per_tick,
        species.move_energy_cost,
        15U,
        8U,
        records[WF0_DEMO_TICKS - 1U].core_step_end,
        records[WF0_DEMO_TICKS - 1U].kernel_hash,
        records[WF0_DEMO_TICKS - 1U].domain_hash,
        trace_digest);
    fclose(summary);
    fclose(trace);

    printf("WF0 Fish V1\n");
    printf("  ticks: %u\n", WF0_DEMO_TICKS);
    printf("  moves: %u\n", move_count);
    printf("  eats: %u\n", eat_count);
    printf("  remaining_food: %" PRIu64 "\n", wf0_fish_world_food_count(world));
    printf("  final_energy: %" PRIu64 "\n", records[WF0_DEMO_TICKS - 1U].energy_after_tick);
    printf("  trace: %s\n", trace_path);
    printf("  summary: %s\n", summary_path);

    wf0_fish_world_destroy(&world);
    return (move_count == 1U && eat_count == 1U && records[2].decision.action.type ==
        MINISNN_WORLDS_DOMAIN_ACTION_WAIT) ? 0 : 1;
}
