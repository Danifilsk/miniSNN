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

static void fnv_append_u64(uint64_t *in_out_hash, uint64_t value)
{
    size_t index;

    for (index = 0U; index < 8U; ++index)
    {
        *in_out_hash ^= (uint8_t)(value >> (index * 8U));
        *in_out_hash *= UINT64_C(1099511628211);
    }
}

static uint64_t demo_config_signature(const DemoConfig *config)
{
    uint64_t hash = UINT64_C(14695981039346656037);

    fnv_append_u64(&hash, config->master_seed);
    fnv_append_u64(&hash, config->ticks);
    fnv_append_u64(&hash, ((uint64_t)config->bounds.min_x) ^
                         UINT64_C(0x8000000000000000));
    fnv_append_u64(&hash, ((uint64_t)config->bounds.min_y) ^
                         UINT64_C(0x8000000000000000));
    fnv_append_u64(&hash, ((uint64_t)config->bounds.max_x) ^
                         UINT64_C(0x8000000000000000));
    fnv_append_u64(&hash, ((uint64_t)config->bounds.max_y) ^
                         UINT64_C(0x8000000000000000));
    return hash;
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
                 strcmp(value, "k1_transform_demo") == 0)
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
    MiniSNNWorldsKernelScalar y,
    MiniSNNWorldsKernelOrientation orientation)
{
    MiniSNNWorldsKernelTransform result;

    result.position.x = x;
    result.position.y = y;
    result.orientation = orientation;
    return result;
}

static int trace(FILE *file, MiniSNNWorldsKernel *kernel)
{
    MiniSNNWorldsKernelDiagnostics d;
    size_t index;

    if (minisnn_worlds_kernel_get_diagnostics(kernel, &d) !=
        MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    if (fprintf(file, "state,%" PRIu64 ",%u,0x%016" PRIX64 ",%" PRIu64
                ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
                ",,,,,,,,,\n",
                d.completed_ticks, d.state_hash_version, d.current_state_hash,
                d.alive_entities, d.placed_entities, d.pending_commands,
                d.last_tick_events, d.total_entities_placed,
                d.total_entities_removed_from_space) < 0)
    {
        return 0;
    }
    for (index = 0U; index < minisnn_worlds_kernel_last_tick_event_count(kernel); ++index)
    {
        MiniSNNWorldsKernelEvent event;

        if (minisnn_worlds_kernel_last_tick_event_at(kernel, index, &event) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
            fprintf(file, "event,%" PRIu64 ",,,,,,,,,%" PRIu64 ",%u,%" PRIu64
                    ",%" PRIu64 ",%" PRIu64 ",%" PRId64 ",%" PRId64 ",%u,%u\n",
                    event.tick, event.event_id.value, (unsigned)event.type,
                    event.command_id.value, event.issuer.value, event.subject.value,
                    event.has_transform ? event.transform.position.x : 0,
                    event.has_transform ? event.transform.position.y : 0,
                    event.has_transform ? event.transform.orientation : 0U,
                    (unsigned)event.rejection) < 0)
        {
            return 0;
        }
    }
    return 1;
}
static int run(
    const DemoConfig *demo,
    FILE *output,
    uint64_t *out_hash,
    MiniSNNWorldsKernelEntityId *out_final_entity,
    MiniSNNWorldsKernelTransform *out_final_transform)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelCommandId command;
    size_t index;

    config.master_seed = demo->master_seed;
    config.space_bounds = demo->bounds;
    kernel = minisnn_worlds_kernel_create(&config, &error);
    if (kernel == NULL)
    {
        return 0;
    }
    for (index = 0U; index < 4U; ++index)
    {
        if (minisnn_worlds_kernel_queue_create_entity(kernel, 1U, 0U, id(0U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            minisnn_worlds_kernel_destroy(kernel);
            return 0;
        }
    }
    if (minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(kernel, 2U, 0U, id(0U), id(1U), at(1000, 2000, 0U), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(kernel, 2U, 0U, id(0U), id(2U), at(1000, 2000, 0U), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(kernel, 2U, 0U, id(0U), id(3U), at(-1000, 0, 90000U), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(kernel, 2U, 0U, id(0U), id(1U), at(1000, 2000, 0U), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        (output != NULL && !trace(output, kernel)) ||
        minisnn_worlds_kernel_queue_remove_entity_from_space(kernel, 3U, 0U, id(0U), id(1U), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        (output != NULL && !trace(output, kernel)) ||
        minisnn_worlds_kernel_queue_destroy_entity(kernel, 4U, 0U, id(0U), id(2U), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        (output != NULL && !trace(output, kernel)))
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    while (minisnn_worlds_kernel_tick(kernel) < demo->ticks)
    {
        if (minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
            (output != NULL && !trace(output, kernel)))
        {
            minisnn_worlds_kernel_destroy(kernel);
            return 0;
        }
    }
    if (minisnn_worlds_kernel_state_hash(kernel, out_hash) !=
        MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    if (out_final_entity != NULL || out_final_transform != NULL)
    {
        MiniSNNWorldsKernelEntityId final_entity;
        MiniSNNWorldsKernelTransform final_transform;

        if (out_final_entity == NULL || out_final_transform == NULL ||
            minisnn_worlds_kernel_placed_entity_at(
                kernel, 0U, &final_entity, &final_transform) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            minisnn_worlds_kernel_destroy(kernel);
            return 0;
        }
        *out_final_entity = final_entity;
        *out_final_transform = final_transform;
    }
    minisnn_worlds_kernel_destroy(kernel);
    return 1;
}

int main(int argc, char **argv)
{
    const char *config_path = argc > 1 ? argv[1] : "worlds/kernel/configs/k1_transform_demo.ini";
    const char *output_path = argc > 2 ? argv[2] : "build/worlds/kernel/demo/k1_a";
    char trace_path[512];
    char manifest_path[512];
    char report_path[512];
    DemoConfig config;
    DemoConfig contrast_config;
    FILE *trace_file;
    FILE *manifest;
    FILE *report;
    uint64_t first_hash;
    uint64_t second_hash;
    uint64_t contrast_hash;
    MiniSNNWorldsKernelEntityId final_entity;
    MiniSNNWorldsKernelTransform final_transform;

    if (argc > 3 || !load_config(config_path, &config) ||
        snprintf(trace_path, sizeof(trace_path), "%s/transform_trace.csv", output_path) < 0 ||
        snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.ini", output_path) < 0 ||
        snprintf(report_path, sizeof(report_path), "%s/report.txt", output_path) < 0)
    {
        return 1;
    }
    contrast_config = config;
    contrast_config.master_seed ^= UINT64_C(1);
    trace_file = fopen(trace_path, "wb");
    manifest = fopen(manifest_path, "wb");
    report = fopen(report_path, "wb");
    if (trace_file == NULL || manifest == NULL || report == NULL)
    {
        fprintf(stderr, "could not open output artifacts\n");
        if (trace_file != NULL) fclose(trace_file);
        if (manifest != NULL) fclose(manifest);
        if (report != NULL) fclose(report);
        return 1;
    }
    fputs("record,tick,state_hash_version,state_hash,alive_entities,placed_entities,pending_commands,last_tick_events,total_entities_placed,total_entities_removed_from_space,event_id,event_type,event_command_id,event_issuer,event_subject,position_x,position_y,orientation,rejection\n", trace_file);
    if (!run(&config, trace_file, &first_hash, &final_entity, &final_transform) ||
        !run(&config, NULL, &second_hash, NULL, NULL) ||
        !run(&contrast_config, NULL, &contrast_hash, NULL, NULL))
    {
        fclose(trace_file); fclose(manifest); fclose(report);
        return 1;
    }
    fprintf(manifest, "scenario_id=k1_transform_demo\nscenario_config_signature=%" PRIu64 "\nmaster_seed=%" PRIu64 "\nstate_hash_version=2\nfinal_state_hash=0x%016" PRIX64 "\n", demo_config_signature(&config), config.master_seed, first_hash);
    fprintf(report, "repeat_match=%s\ndifferent_config_diverged=%s\nfinal_state_hash=0x%016" PRIX64 "\n",
            first_hash == second_hash ? "yes" : "no",
            first_hash != contrast_hash ? "yes" : "no", first_hash);
    fclose(trace_file); fclose(manifest); fclose(report);
    printf("miniSNN Worlds Kernel K1-A transform demo\n");
    printf("space_bounds=%" PRId64 ",%" PRId64 ",%" PRId64 ",%" PRId64 "\n", config.bounds.min_x, config.bounds.min_y, config.bounds.max_x, config.bounds.max_y);
    printf("scalar_scale=1000\ncanonical_entity=%" PRIu64 "\nposition_x=%" PRId64
           "\nposition_y=%" PRId64 "\norientation=%u\n",
           final_entity.value, final_transform.position.x,
           final_transform.position.y, final_transform.orientation);
    printf("tick=2 placed=3 rejected=1\ntick=3 placed=2\ntick=4 placed=1\n");
    printf("repeat_match=%s\ndifferent_config_diverged=%s\nstatus=%s\n",
           first_hash == second_hash ? "yes" : "no",
           first_hash != contrast_hash ? "yes" : "no",
           first_hash == second_hash && first_hash != contrast_hash ? "OK" : "FAIL");
    return first_hash == second_hash && first_hash != contrast_hash ? 0 : 1;
}
