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
    int free_move;
    int nonblocking_overlap;
    int blocking_rejected;
    int canonical_related_entity;
    int overflow_rejected;
    int zero_move;
    int move_after_clear;
    int removal_then_move_rejected;
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
                 strcmp(value, "k1_movement_demo") == 0)
        {
            seen |= I;
        }
        else if (section == 1 && strcmp(key, "master_seed") == 0 && !(seen & S) &&
                 parse_u64(value, &config.master_seed))
        {
            seen |= S;
        }
        else if (section == 1 && strcmp(key, "ticks") == 0 && !(seen & T) &&
                 parse_u64(value, &config.ticks) && config.ticks >= 8U)
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
    MiniSNNWorldsKernelScalar y,
    MiniSNNWorldsKernelOrientation orientation)
{
    MiniSNNWorldsKernelTransform result;

    result.position.x = x;
    result.position.y = y;
    result.orientation = orientation;
    return result;
}

static MiniSNNWorldsKernelOccupancy occupancy(uint32_t category, uint32_t mask)
{
    MiniSNNWorldsKernelOccupancy result;

    result.half_extent_x = 1000;
    result.half_extent_y = 1000;
    result.category_bits = category;
    result.blocking_mask = mask;
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
                    ",%" PRIu64 ",%u,%" PRId64 ",%" PRId64 ",%u,%" PRId64
                    ",%" PRId64 ",%u,%" PRId64 ",%" PRId64 ",%u,%u\n",
                    event.tick, event.event_id.value, (unsigned)event.type,
                    event.command_id.value, event.subject.value, event.related_entity.value,
                    (unsigned)event.rejection,
                    event.has_previous_transform ? event.previous_transform.position.x : 0,
                    event.has_previous_transform ? event.previous_transform.position.y : 0,
                    event.has_previous_transform ? event.previous_transform.orientation : 0U,
                    event.has_transform ? event.transform.position.x : 0,
                    event.has_transform ? event.transform.position.y : 0,
                    event.has_transform ? event.transform.orientation : 0U,
                    event.has_occupancy ? event.occupancy.half_extent_x : 0,
                    event.has_occupancy ? event.occupancy.half_extent_y : 0,
                    event.has_occupancy ? event.occupancy.category_bits : 0U,
                    event.has_occupancy ? event.occupancy.blocking_mask : 0U) < 0)
        {
            return 0;
        }
    }
    return 1;
}

static int write_trace(FILE *file, MiniSNNWorldsKernel *kernel)
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
                   ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
                   ",%" PRIu64 ",%" PRIu64 "\n",
                   diagnostics.completed_ticks, diagnostics.state_hash_version,
                   diagnostics.current_state_hash, diagnostics.placed_entities,
                   diagnostics.active_occupancies, diagnostics.blocking_occupancies,
                   diagnostics.total_movement_commands_processed,
                   diagnostics.total_entities_moved,
                   diagnostics.total_movement_overflows_rejected,
                   diagnostics.total_occupancy_conflicts_rejected) >= 0;
}

static int write_positions(FILE *file, MiniSNNWorldsKernel *kernel)
{
    size_t count = minisnn_worlds_kernel_entity_count(kernel);
    size_t index;

    if (file == NULL)
    {
        return 1;
    }
    for (index = 0U; index < count; ++index)
    {
        MiniSNNWorldsKernelEntityId entity;
        MiniSNNWorldsKernelTransform transform;
        MiniSNNWorldsKernelOccupancy descriptor;
        int placed;
        int has_occupancy;

        if (minisnn_worlds_kernel_entity_at(kernel, index, &entity) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return 0;
        }
        placed = minisnn_worlds_kernel_entity_is_placed(kernel, entity) ? 1 : 0;
        has_occupancy = minisnn_worlds_kernel_entity_has_occupancy(kernel, entity) ? 1 : 0;
        if (placed != 0 && minisnn_worlds_kernel_entity_transform(kernel, entity, &transform) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return 0;
        }
        if (has_occupancy != 0 &&
            minisnn_worlds_kernel_entity_occupancy(kernel, entity, &descriptor) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return 0;
        }
        if (fprintf(file, "%" PRIu64 ",%d,%" PRId64 ",%" PRId64 ",%u,%d,%" PRId64
                    ",%" PRId64 ",%u,%u\n",
                    entity.value, placed, placed ? transform.position.x : 0,
                    placed ? transform.position.y : 0,
                    placed ? transform.orientation : 0U, has_occupancy,
                    has_occupancy ? descriptor.half_extent_x : 0,
                    has_occupancy ? descriptor.half_extent_y : 0,
                    has_occupancy ? descriptor.category_bits : 0U,
                    has_occupancy ? descriptor.blocking_mask : 0U) < 0)
        {
            return 0;
        }
    }
    return 1;
}

static int step_and_record(
    MiniSNNWorldsKernel *kernel,
    FILE *trace_file,
    FILE *events_file)
{
    return minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           write_trace(trace_file, kernel) && write_events(events_file, kernel);
}

static int run(
    const DemoConfig *config,
    FILE *trace_file,
    FILE *events_file,
    FILE *positions_file,
    DemoResult *out_result)
{
    MiniSNNWorldsKernelConfig kernel_config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelEvent event;
    MiniSNNWorldsKernelTransform transform;
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
    for (index = 0U; index < 6U; ++index)
    {
        if (minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 0U, id(0U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            minisnn_worlds_kernel_destroy(kernel);
            return 0;
        }
    }
    if (!step_and_record(kernel, trace_file, events_file) ||
        minisnn_worlds_kernel_queue_set_occupancy(
            kernel, 2U, 0U, id(0U), id(2U), occupancy(2U, 0U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_set_occupancy(
            kernel, 2U, 0U, id(0U), id(3U), occupancy(2U, 0U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_set_occupancy(
            kernel, 2U, 0U, id(0U), id(4U), occupancy(1U, 2U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_set_occupancy(
            kernel, 2U, 0U, id(0U), id(5U), occupancy(1U, 0U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !step_and_record(kernel, trace_file, events_file) ||
        minisnn_worlds_kernel_queue_place_entity(kernel, 3U, 0U, id(0U), id(1U),
                                                  at(-20000, 0, 120U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(kernel, 3U, 0U, id(0U), id(2U),
                                                  at(10000, 0, 0U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(kernel, 3U, 0U, id(0U), id(3U),
                                                  at(10000, 0, 0U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(kernel, 3U, 0U, id(0U), id(4U),
                                                  at(0, 0, 270U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(kernel, 3U, 0U, id(0U), id(5U),
                                                  at(30000, 0, 33U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(kernel, 3U, 0U, id(0U), id(6U),
                                                  at(INT64_MAX, 0, 1U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !step_and_record(kernel, trace_file, events_file))
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    if (minisnn_worlds_kernel_queue_move_entity(kernel, 4U, 0U, id(0U), id(1U),
                                                 5000, -2000, &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_move_entity(kernel, 4U, 0U, id(0U), id(5U),
                                                 -20000, 0, &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_move_entity(kernel, 4U, 0U, id(0U), id(4U),
                                                 10000, 0, &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_move_entity(kernel, 4U, 0U, id(0U), id(6U),
                                                 1, 0, &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !step_and_record(kernel, trace_file, events_file))
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    if (minisnn_worlds_kernel_entity_transform(kernel, id(1U), &transform) !=
        MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    result.free_move = transform.position.x == -15000 && transform.position.y == -2000 &&
                       transform.orientation == 120U;
    result.nonblocking_overlap = minisnn_worlds_kernel_entity_transform(kernel, id(5U), &transform) ==
        MINISNN_WORLDS_KERNEL_ERROR_NONE && transform.position.x == 10000;
    if (minisnn_worlds_kernel_last_tick_event_at(kernel, 2U, &event) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        event.rejection != MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_CONFLICT)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    result.blocking_rejected = 1;
    result.canonical_related_entity = event.related_entity.value == 2U;
    if (minisnn_worlds_kernel_last_tick_event_at(kernel, 3U, &event) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        event.rejection != MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_DESTINATION_OVERFLOW)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    result.overflow_rejected = 1;
    if (minisnn_worlds_kernel_queue_clear_occupancy(kernel, 5U, 0U, id(0U), id(2U),
                                                     &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_move_entity(kernel, 5U, 1U, id(0U), id(4U),
                                                 10000, 0, &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !step_and_record(kernel, trace_file, events_file))
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    if (minisnn_worlds_kernel_last_tick_event_at(kernel, 1U, &event) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE || event.related_entity.value != 3U)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    if (minisnn_worlds_kernel_queue_clear_occupancy(kernel, 6U, 0U, id(0U), id(3U),
                                                     &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_move_entity(kernel, 6U, 1U, id(0U), id(4U),
                                                 10000, 0, &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !step_and_record(kernel, trace_file, events_file) ||
        minisnn_worlds_kernel_entity_transform(kernel, id(4U), &transform) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    result.move_after_clear = transform.position.x == 10000 && transform.orientation == 270U;
    if (minisnn_worlds_kernel_queue_move_entity(kernel, 7U, 0U, id(0U), id(4U),
                                                 0, 0, &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !step_and_record(kernel, trace_file, events_file) ||
        minisnn_worlds_kernel_last_tick_event_at(kernel, 0U, &event) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    result.zero_move = event.type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED &&
                       event.has_previous_transform && event.has_transform &&
                       event.previous_transform.position.x == event.transform.position.x;
    if (minisnn_worlds_kernel_queue_remove_entity_from_space(kernel, 8U, 0U, id(0U),
                                                              id(5U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_move_entity(kernel, 8U, 1U, id(0U), id(5U),
                                                 1000, 0, &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !step_and_record(kernel, trace_file, events_file) ||
        minisnn_worlds_kernel_last_tick_event_at(kernel, 1U, &event) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    result.removal_then_move_rejected = event.type == MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED &&
        event.rejection == MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_NOT_PLACED;
    while (minisnn_worlds_kernel_tick(kernel) < config->ticks)
    {
        if (!step_and_record(kernel, trace_file, events_file))
        {
            minisnn_worlds_kernel_destroy(kernel);
            return 0;
        }
    }
    if (!write_positions(positions_file, kernel) ||
        minisnn_worlds_kernel_state_hash(kernel, &result.final_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
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
    const char *config_path = argc > 1 ? argv[1] : "worlds/kernel/configs/k1_movement_demo.ini";
    const char *output_path = argc > 2 ? argv[2] : "build/worlds/kernel/demo/k1_b2";
    char trace_path[512];
    char events_path[512];
    char positions_path[512];
    char manifest_path[512];
    char report_path[512];
    FILE *trace_file;
    FILE *events_file;
    FILE *positions_file;
    FILE *manifest;
    FILE *report;
    DemoConfig config;
    DemoConfig contrast_config;
    DemoResult first;
    DemoResult second;
    DemoResult contrast;
    int status;

    if (argc > 3 || !load_config(config_path, &config) ||
        snprintf(trace_path, sizeof(trace_path), "%s/trace.csv", output_path) < 0 ||
        snprintf(events_path, sizeof(events_path), "%s/events.csv", output_path) < 0 ||
        snprintf(positions_path, sizeof(positions_path), "%s/positions.csv", output_path) < 0 ||
        snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.ini", output_path) < 0 ||
        snprintf(report_path, sizeof(report_path), "%s/report.txt", output_path) < 0)
    {
        return 1;
    }
    trace_file = fopen(trace_path, "wb");
    events_file = fopen(events_path, "wb");
    positions_file = fopen(positions_path, "wb");
    manifest = fopen(manifest_path, "wb");
    report = fopen(report_path, "wb");
    if (trace_file == NULL || events_file == NULL || positions_file == NULL ||
        manifest == NULL || report == NULL)
    {
        fprintf(stderr, "could not open output artifacts\n");
        if (trace_file != NULL) fclose(trace_file);
        if (events_file != NULL) fclose(events_file);
        if (positions_file != NULL) fclose(positions_file);
        if (manifest != NULL) fclose(manifest);
        if (report != NULL) fclose(report);
        return 1;
    }
    fputs("tick,state_hash_version,state_hash,placed_entities,active_occupancies,blocking_occupancies,total_movement_commands_processed,total_entities_moved,total_movement_overflows_rejected,total_occupancy_conflicts_rejected\n", trace_file);
    fputs("tick,event_id,type,command_id,subject,related_entity,rejection,previous_x,previous_y,previous_orientation,x,y,orientation,half_extent_x,half_extent_y,category_bits,blocking_mask\n", events_file);
    fputs("entity_id,placed,x,y,orientation,has_occupancy,half_extent_x,half_extent_y,category_bits,blocking_mask\n", positions_file);
    contrast_config = config;
    ++contrast_config.master_seed;
    if (!run(&config, trace_file, events_file, positions_file, &first) ||
        !run(&config, NULL, NULL, NULL, &second) ||
        !run(&contrast_config, NULL, NULL, NULL, &contrast))
    {
        fclose(trace_file); fclose(events_file); fclose(positions_file);
        fclose(manifest); fclose(report);
        return 1;
    }
    status = first.free_move && first.nonblocking_overlap && first.blocking_rejected &&
             first.canonical_related_entity && first.overflow_rejected && first.zero_move &&
             first.move_after_clear && first.removal_then_move_rejected &&
             first.final_hash == second.final_hash && first.final_hash != contrast.final_hash;
    fprintf(manifest, "scenario_id=k1_movement_demo\nmaster_seed=%" PRIu64
            "\nstate_hash_version=4\nfinal_state_hash=0x%016" PRIX64
            "\nfree_move=%s\nnonblocking_overlap=%s\nblocking_rejected=%s"
            "\ncanonical_related_entity=%s\noverflow_rejected=%s\nzero_move=%s"
            "\nmove_after_clear=%s\nremoval_then_move_rejected=%s\n",
            config.master_seed, first.final_hash,
            first.free_move ? "yes" : "no",
            first.nonblocking_overlap ? "yes" : "no",
            first.blocking_rejected ? "yes" : "no",
            first.canonical_related_entity ? "yes" : "no",
            first.overflow_rejected ? "yes" : "no",
            first.zero_move ? "yes" : "no",
            first.move_after_clear ? "yes" : "no",
            first.removal_then_move_rejected ? "yes" : "no");    fprintf(report, "free_move=%s\nnonblocking_overlap=%s\nblocking_rejected=%s\n"
            "canonical_related_entity=%s\noverflow_rejected=%s\nzero_move=%s\n"
            "move_after_clear=%s\nremoval_then_move_rejected=%s\nrepeat_match=%s\n"
            "different_seed_diverged=%s\nfinal_state_hash=0x%016" PRIX64 "\n",
            first.free_move ? "yes" : "no", first.nonblocking_overlap ? "yes" : "no",
            first.blocking_rejected ? "yes" : "no",
            first.canonical_related_entity ? "yes" : "no",
            first.overflow_rejected ? "yes" : "no", first.zero_move ? "yes" : "no",
            first.move_after_clear ? "yes" : "no",
            first.removal_then_move_rejected ? "yes" : "no",
            first.final_hash == second.final_hash ? "yes" : "no",
            first.final_hash != contrast.final_hash ? "yes" : "no", first.final_hash);
    fclose(trace_file); fclose(events_file); fclose(positions_file);
    fclose(manifest); fclose(report);
    printf("miniSNN Worlds Kernel K1-B2 movement demo\n");
    printf("free_move=%s\nnonblocking_overlap=%s\nblocking_rejected=%s\n"
           "canonical_related_entity=%s\noverflow_rejected=%s\nzero_move=%s\n"
           "move_after_clear=%s\nremoval_then_move_rejected=%s\n",
           first.free_move ? "yes" : "no", first.nonblocking_overlap ? "yes" : "no",
           first.blocking_rejected ? "yes" : "no",
           first.canonical_related_entity ? "yes" : "no",
           first.overflow_rejected ? "yes" : "no", first.zero_move ? "yes" : "no",
           first.move_after_clear ? "yes" : "no",
           first.removal_then_move_rejected ? "yes" : "no");
    printf("repeat_match=%s\ndifferent_seed_diverged=%s\nstate_hash_version=4\n"
           "final_state_hash=0x%016" PRIX64 "\nstatus=%s\n",
           first.final_hash == second.final_hash ? "yes" : "no",
           first.final_hash != contrast.final_hash ? "yes" : "no", first.final_hash,
           status ? "OK" : "FAIL");
    return status ? 0 : 1;
}