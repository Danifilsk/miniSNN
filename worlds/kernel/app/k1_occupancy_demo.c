#include "minisnn_worlds_kernel.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    uint64_t master_seed;
    uint64_t ticks;
    MiniSNNWorldsKernelSpaceBounds bounds;
} DemoConfig;

typedef struct
{
    uint64_t final_hash;
    uint64_t active_occupancies;
    uint64_t blocking_occupancies;
    int conflict_rejected;
    int edge_contact_allowed;
    int nonblocking_overlap_allowed;
    int barrier_cleared;
    int placement_after_clear;
} DemoResult;

static char *trim(char *text)
{
    char *end;

    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
    {
        ++text;
    }
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' ||
                          end[-1] == '\r' || end[-1] == '\n'))
    {
        --end;
    }
    *end = '\0';
    return text;
}

static int parse_u64(const char *text, uint64_t *out_value)
{
    char *end;
    unsigned long long value;

    if (*text == '\0' || *text == '-')
    {
        return 0;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || *end != '\0')
    {
        return 0;
    }
    *out_value = (uint64_t)value;
    return 1;
}

static int parse_scalar(const char *text, MiniSNNWorldsKernelScalar *out_value)
{
    char *end;
    long long value;

    if (*text == '\0')
    {
        return 0;
    }
    errno = 0;
    value = strtoll(text, &end, 10);
    if (errno != 0 || *end != '\0')
    {
        return 0;
    }
    *out_value = (MiniSNNWorldsKernelScalar)value;
    return 1;
}

static int load_config(const char *filename, DemoConfig *out_config)
{
    enum { V = 1, I = 2, S = 4, T = 8, X0 = 16, Y0 = 32, X1 = 64, Y1 = 128 };
    FILE *file = fopen(filename, "rb");
    char line[256];
    unsigned seen = 0U;
    int section = 0;
    DemoConfig config;

    if (file == NULL)
    {
        fprintf(stderr, "could not open config: %s\n", filename);
        return 0;
    }
    memset(&config, 0, sizeof(config));
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *text = trim(line);
        char *equals;
        char *key;
        char *value;
        uint64_t parsed;

        if (*text == '\0' || *text == '#' || *text == ';')
        {
            continue;
        }
        if (strcmp(text, "[scenario]") == 0)
        {
            section = 1;
            continue;
        }
        if (strcmp(text, "[space]") == 0)
        {
            section = 2;
            continue;
        }
        equals = strchr(text, '=');
        if (equals == NULL || strchr(equals + 1, '=') != NULL)
        {
            fclose(file);
            return 0;
        }
        *equals = '\0';
        key = trim(text);
        value = trim(equals + 1);
        if (section == 1 && strcmp(key, "scenario_version") == 0 && !(seen & V) &&
            parse_u64(value, &parsed) && parsed == 1U)
        {
            seen |= V;
        }
        else if (section == 1 && strcmp(key, "scenario_id") == 0 && !(seen & I) &&
                 strcmp(value, "k1_occupancy_demo") == 0)
        {
            seen |= I;
        }
        else if (section == 1 && strcmp(key, "master_seed") == 0 && !(seen & S) &&
                 parse_u64(value, &config.master_seed))
        {
            seen |= S;
        }
        else if (section == 1 && strcmp(key, "ticks") == 0 && !(seen & T) &&
                 parse_u64(value, &config.ticks) && config.ticks >= 5U)
        {
            seen |= T;
        }
        else if (section == 2 && strcmp(key, "min_x_milli") == 0 && !(seen & X0) &&
                 parse_scalar(value, &config.bounds.min_x))
        {
            seen |= X0;
        }
        else if (section == 2 && strcmp(key, "min_y_milli") == 0 && !(seen & Y0) &&
                 parse_scalar(value, &config.bounds.min_y))
        {
            seen |= Y0;
        }
        else if (section == 2 && strcmp(key, "max_x_milli") == 0 && !(seen & X1) &&
                 parse_scalar(value, &config.bounds.max_x))
        {
            seen |= X1;
        }
        else if (section == 2 && strcmp(key, "max_y_milli") == 0 && !(seen & Y1) &&
                 parse_scalar(value, &config.bounds.max_y))
        {
            seen |= Y1;
        }
        else
        {
            fclose(file);
            fprintf(stderr, "unknown, duplicate, or invalid key: %s\n", key);
            return 0;
        }
    }
    fclose(file);
    if (seen != (V | I | S | T | X0 | Y0 | X1 | Y1) ||
        config.bounds.min_x >= config.bounds.max_x ||
        config.bounds.min_y >= config.bounds.max_y)
    {
        fprintf(stderr, "incomplete or invalid config\n");
        return 0;
    }
    *out_config = config;
    return 1;
}

static MiniSNNWorldsKernelEntityId id(uint64_t value)
{
    MiniSNNWorldsKernelEntityId result = { value };
    return result;
}

static MiniSNNWorldsKernelTransform at(
    MiniSNNWorldsKernelScalar x,
    MiniSNNWorldsKernelScalar y)
{
    MiniSNNWorldsKernelTransform result;

    result.position.x = x;
    result.position.y = y;
    result.orientation = UINT32_C(0);
    return result;
}

static MiniSNNWorldsKernelOccupancy occupancy(
    uint32_t category,
    uint32_t blocking_mask)
{
    MiniSNNWorldsKernelOccupancy result;

    result.half_extent_x = 1000;
    result.half_extent_y = 1000;
    result.category_bits = category;
    result.blocking_mask = blocking_mask;
    return result;
}

static int write_events(FILE *file, MiniSNNWorldsKernel *kernel)
{
    size_t index;

    if (file == NULL)
    {
        return 1;
    }
    for (index = 0U; index < minisnn_worlds_kernel_last_tick_event_count(kernel); ++index)
    {
        MiniSNNWorldsKernelEvent event;

        if (minisnn_worlds_kernel_last_tick_event_at(kernel, index, &event) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
            fprintf(file, "%" PRIu64 ",%" PRIu64 ",%u,%" PRIu64 ",%" PRIu64
                    ",%" PRId64 ",%" PRId64 ",%u,%u\n",
                    event.tick, event.event_id.value, (unsigned)event.type,
                    event.subject.value, event.related_entity.value,
                    event.has_occupancy ? event.occupancy.half_extent_x : 0,
                    event.has_occupancy ? event.occupancy.half_extent_y : 0,
                    event.has_occupancy ? event.occupancy.category_bits : 0U,
                    event.has_occupancy ? event.occupancy.blocking_mask :
                        (unsigned)event.rejection) < 0)
        {
            return 0;
        }
    }
    return 1;
}

static int trace(FILE *file, MiniSNNWorldsKernel *kernel)
{
    MiniSNNWorldsKernelDiagnostics diagnostics;

    if (file == NULL)
    {
        return 1;
    }
    if (minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) !=
        MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    return fprintf(file, "%" PRIu64 ",%u,0x%016" PRIX64 ",%" PRIu64
                   ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
                   diagnostics.completed_ticks, diagnostics.state_hash_version,
                   diagnostics.current_state_hash, diagnostics.active_occupancies,
                   diagnostics.blocking_occupancies,
                   diagnostics.total_occupancies_set,
                   diagnostics.total_occupancy_conflicts_rejected) >= 0;
}

static int run(
    const DemoConfig *config,
    uint32_t first_blocking_mask,
    FILE *trace_file,
    FILE *events_file,
    DemoResult *out_result)
{
    MiniSNNWorldsKernelConfig kernel_config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelDiagnostics diagnostics;
    DemoResult result;
    size_t index;

    memset(&result, 0, sizeof(result));
    kernel_config.master_seed = config->master_seed;
    kernel_config.space_bounds = config->bounds;
    kernel = minisnn_worlds_kernel_create(&kernel_config, &error);
    if (kernel == NULL)
    {
        return 0;
    }
    for (index = 0U; index < 5U; ++index)
    {
        if (minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 0U, id(0U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            minisnn_worlds_kernel_destroy(kernel);
            return 0;
        }
    }
    if (minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !trace(trace_file, kernel) || !write_events(events_file, kernel) ||
        minisnn_worlds_kernel_queue_set_occupancy(kernel, 2U, 0U, id(0U), id(1U), occupancy(1U, first_blocking_mask), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_set_occupancy(kernel, 2U, 0U, id(0U), id(2U), occupancy(2U, 0U), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_set_occupancy(kernel, 2U, 0U, id(0U), id(3U), occupancy(4U, 0U), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_set_occupancy(kernel, 2U, 0U, id(0U), id(4U), occupancy(8U, 1U), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_set_occupancy(kernel, 2U, 0U, id(0U), id(5U), occupancy(16U, 1U), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !trace(trace_file, kernel) || !write_events(events_file, kernel) ||
        minisnn_worlds_kernel_queue_place_entity(kernel, 3U, 0U, id(0U), id(1U), at(0, 0), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(kernel, 3U, 0U, id(0U), id(2U), at(0, 0), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(kernel, 3U, 0U, id(0U), id(3U), at(0, 0), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(kernel, 3U, 0U, id(0U), id(4U), at(2000, 0), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !trace(trace_file, kernel) || !write_events(events_file, kernel))
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    result.conflict_rejected = minisnn_worlds_kernel_entity_is_placed(kernel, id(2U)) ? 0 : 1;
    result.edge_contact_allowed = minisnn_worlds_kernel_entity_is_placed(kernel, id(4U)) ? 1 : 0;
    result.nonblocking_overlap_allowed = minisnn_worlds_kernel_entity_is_placed(kernel, id(3U)) ? 1 : 0;
    if (minisnn_worlds_kernel_queue_clear_occupancy(kernel, 4U, 0U, id(0U), id(1U), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(kernel, 4U, 1U, id(0U), id(2U), at(0, 0), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !trace(trace_file, kernel) || !write_events(events_file, kernel))
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    result.barrier_cleared = !minisnn_worlds_kernel_entity_has_occupancy(kernel, id(1U));
    result.placement_after_clear = minisnn_worlds_kernel_entity_is_placed(kernel, id(2U));
    if (minisnn_worlds_kernel_queue_remove_entity_from_space(kernel, 5U, 0U, id(0U), id(4U), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_destroy_entity(kernel, 5U, 0U, id(0U), id(3U), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !trace(trace_file, kernel) || !write_events(events_file, kernel) ||
        minisnn_worlds_kernel_get_diagnostics(kernel, &diagnostics) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(kernel, &result.final_hash) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    result.active_occupancies = diagnostics.active_occupancies;
    result.blocking_occupancies = diagnostics.blocking_occupancies;
    while (minisnn_worlds_kernel_tick(kernel) < config->ticks)
    {
        if (minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
            !trace(trace_file, kernel) || !write_events(events_file, kernel))
        {
            minisnn_worlds_kernel_destroy(kernel);
            return 0;
        }
    }
    if (minisnn_worlds_kernel_state_hash(kernel, &result.final_hash) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    *out_result = result;
    minisnn_worlds_kernel_destroy(kernel);
    return 1;
}

int main(int argc, char **argv)
{
    const char *config_path = argc > 1 ? argv[1] : "worlds/kernel/configs/k1_occupancy_demo.ini";
    const char *output_path = argc > 2 ? argv[2] : "build/worlds/kernel/demo/k1_b1";
    char trace_path[512];
    char events_path[512];
    char manifest_path[512];
    char report_path[512];
    FILE *trace_file;
    FILE *events_file;
    FILE *manifest;
    FILE *report;
    DemoConfig config;
    DemoResult first;
    DemoResult second;
    DemoResult contrast;

    if (argc > 3 || !load_config(config_path, &config) ||
        snprintf(trace_path, sizeof(trace_path), "%s/trace.csv", output_path) < 0 ||
        snprintf(events_path, sizeof(events_path), "%s/events.csv", output_path) < 0 ||
        snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.ini", output_path) < 0 ||
        snprintf(report_path, sizeof(report_path), "%s/report.txt", output_path) < 0)
    {
        return 1;
    }
    trace_file = fopen(trace_path, "wb");
    events_file = fopen(events_path, "wb");
    manifest = fopen(manifest_path, "wb");
    report = fopen(report_path, "wb");
    if (trace_file == NULL || events_file == NULL || manifest == NULL || report == NULL)
    {
        fprintf(stderr, "could not open output artifacts\n");
        if (trace_file != NULL) fclose(trace_file);
        if (events_file != NULL) fclose(events_file);
        if (manifest != NULL) fclose(manifest);
        if (report != NULL) fclose(report);
        return 1;
    }
    fputs("tick,state_hash_version,state_hash,active_occupancies,blocking_occupancies,total_occupancies_set,total_occupancy_conflicts_rejected\n", trace_file);
    fputs("tick,event_id,type,subject,related_entity,half_extent_x,half_extent_y,category_bits,blocking_mask_or_rejection\n", events_file);
    if (!run(&config, 2U, trace_file, events_file, &first) ||
        !run(&config, 2U, NULL, NULL, &second) ||
        !run(&config, 0U, NULL, NULL, &contrast))
    {
        fclose(trace_file); fclose(events_file); fclose(manifest); fclose(report);
        return 1;
    }
    fprintf(manifest, "scenario_id=k1_occupancy_demo\nmaster_seed=%" PRIu64
            "\nstate_hash_version=3\nfinal_state_hash=0x%016" PRIX64 "\n",
            config.master_seed, first.final_hash);
    fprintf(report, "repeat_match=%s\ndifferent_mask_diverged=%s\nfinal_state_hash=0x%016" PRIX64 "\n",
            first.final_hash == second.final_hash ? "yes" : "no",
            first.final_hash != contrast.final_hash ? "yes" : "no", first.final_hash);
    fclose(trace_file); fclose(events_file); fclose(manifest); fclose(report);
    printf("miniSNN Worlds Kernel K1-B1 occupancy demo\n");
    printf("tick=3 active_occupancies=3\nblocking_occupancies=2\n");
    printf("conflict_rejected=%s\nedge_contact_allowed=%s\nnonblocking_overlap_allowed=%s\n",
           first.conflict_rejected ? "yes" : "no",
           first.edge_contact_allowed ? "yes" : "no",
           first.nonblocking_overlap_allowed ? "yes" : "no");
    printf("tick=4 barrier_cleared=%s\nplacement_after_clear=%s\n",
           first.barrier_cleared ? "yes" : "no",
           first.placement_after_clear ? "yes" : "no");
    printf("tick=5 active_occupancies=%" PRIu64 "\n", first.active_occupancies);
    printf("repeat_match=%s\ndifferent_mask_diverged=%s\nstate_hash_version=3\nfinal_state_hash=0x%016" PRIX64 "\nstatus=%s\n",
           first.final_hash == second.final_hash ? "yes" : "no",
           first.final_hash != contrast.final_hash ? "yes" : "no", first.final_hash,
           first.conflict_rejected && first.edge_contact_allowed &&
           first.nonblocking_overlap_allowed && first.barrier_cleared &&
           first.placement_after_clear && first.active_occupancies == 1U &&
           first.final_hash == second.final_hash && first.final_hash != contrast.final_hash ?
           "OK" : "FAIL");
    return first.conflict_rejected && first.edge_contact_allowed &&
           first.nonblocking_overlap_allowed && first.barrier_cleared &&
           first.placement_after_clear && first.active_occupancies == 1U &&
           first.final_hash == second.final_hash && first.final_hash != contrast.final_hash ? 0 : 1;
}
