#include "g0_world_document.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define G0_WORLD_DOCUMENT_LINE_MAX 8192U
#define G0_WORLD_DOCUMENT_TILE_SIZE 1000

static void set_error(G0WorldDocumentError *out_error, G0WorldDocumentError error)
{
    if (out_error != NULL)
        *out_error = error;
}

static int name_valid(const char *name)
{
    const unsigned char *cursor = (const unsigned char *)name;
    size_t length = 0U;

    if (name == NULL || name[0] == '\0')
        return 0;
    while (*cursor != '\0')
    {
        if (*cursor < 0x20U || *cursor > 0x7eU || *cursor == '=' ||
            ++length >= G0_WORLD_DOCUMENT_NAME_MAX)
        {
            return 0;
        }
        ++cursor;
    }
    return 1;
}

static int parse_u32(const char *text, uint32_t *out_value)
{
    unsigned long value;
    char *end = NULL;

    if (text == NULL || out_value == NULL || text[0] == '\0')
        return 0;
    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value > UINT32_MAX)
        return 0;
    *out_value = (uint32_t)value;
    return 1;
}

static int parse_int(const char *text, int *out_value)
{
    long value;
    char *end = NULL;

    if (text == NULL || out_value == NULL || text[0] == '\0')
        return 0;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < INT_MIN || value > INT_MAX)
        return 0;
    *out_value = (int)value;
    return 1;
}

static int read_line(FILE *file, char *line, size_t capacity)
{
    size_t length;

    if (file == NULL || line == NULL || capacity < 2U || fgets(line, (int)capacity, file) == NULL)
        return 0;
    length = strlen(line);
    if (length == 0U || line[length - 1U] != '\n')
        return 0;
    line[--length] = '\0';
    if (length > 0U && line[length - 1U] == '\r')
        line[length - 1U] = '\0';
    return 1;
}

static int write_string(FILE *file, const char *key, const char *value)
{
    return file != NULL && key != NULL && value != NULL &&
           fprintf(file, "%s=%s\n", key, value) >= 0;
}

static int write_bytes(FILE *file, const char *key, const uint8_t *values, size_t count,
                       char zero, char one)
{
    size_t index;

    if (file == NULL || key == NULL || values == NULL || fprintf(file, "%s=", key) < 0)
        return 0;
    for (index = 0U; index < count; ++index)
    {
        if (fputc(values[index] == 0U ? zero : one, file) == EOF)
            return 0;
    }
    return fputc('\n', file) != EOF;
}

static int write_terrain(FILE *file, const G0WorldConfig *world, size_t count)
{
    size_t index;

    if (file == NULL || world == NULL || fprintf(file, "terrain=") < 0)
        return 0;
    for (index = 0U; index < count; ++index)
    {
        char value = world->tiles[index] == MINISNN_WORLDS_TERRAIN_TILE_WATER ? 'W' : 'L';
        if (fputc(value, file) == EOF)
            return 0;
    }
    return fputc('\n', file) != EOF;
}

static int parse_bytes(const char *text, uint8_t *out_values, size_t count,
                       char zero, char one)
{
    size_t index;

    if (text == NULL || out_values == NULL || strlen(text) != count)
        return 0;
    for (index = 0U; index < count; ++index)
    {
        if (text[index] == zero)
            out_values[index] = 0U;
        else if (text[index] == one)
            out_values[index] = 1U;
        else
            return 0;
    }
    return 1;
}

static int parse_terrain(const char *text, G0WorldConfig *world, size_t count)
{
    size_t index;

    if (text == NULL || world == NULL || strlen(text) != count)
        return 0;
    for (index = 0U; index < count; ++index)
    {
        if (text[index] == 'W')
            world->tiles[index] = MINISNN_WORLDS_TERRAIN_TILE_WATER;
        else if (text[index] == 'L')
            world->tiles[index] = MINISNN_WORLDS_TERRAIN_TILE_LAND;
        else
            return 0;
    }
    return 1;
}

static int parse_spawn(const char *text, uint32_t *out_x, uint32_t *out_y)
{
    const char *separator;
    char x_text[32];
    size_t x_length;

    if (text == NULL || out_x == NULL || out_y == NULL ||
        (separator = strchr(text, ',')) == NULL || strchr(separator + 1, ',') != NULL)
    {
        return 0;
    }
    x_length = (size_t)(separator - text);
    if (x_length == 0U || x_length >= sizeof(x_text))
        return 0;
    memcpy(x_text, text, x_length);
    x_text[x_length] = '\0';
    return parse_u32(x_text, out_x) && parse_u32(separator + 1, out_y);
}

static int key_value(const char *line, const char *key, const char **out_value)
{
    size_t key_length;

    if (line == NULL || key == NULL || out_value == NULL)
        return 0;
    key_length = strlen(key);
    if (strncmp(line, key, key_length) != 0 || line[key_length] != '=')
        return 0;
    *out_value = line + key_length + 1U;
    return 1;
}

void g0_world_document_init(G0WorldDocument *document)
{
    if (document != NULL)
    {
        memset(document, 0, sizeof(*document));
        g0_app_settings_default(&document->settings);
    }
}

void g0_world_document_destroy(G0WorldDocument *document)
{
    if (document != NULL)
    {
        g0_world_config_destroy(&document->world);
        g0_world_document_init(document);
    }
}

int g0_world_document_set_name(G0WorldDocument *document, const char *name)
{
    if (document == NULL || !name_valid(name))
        return 0;
    memcpy(document->name, name, strlen(name) + 1U);
    return 1;
}

int g0_world_document_create(
    G0WorldDocument *document,
    const char *name,
    const G0WorldConfig *world,
    const G0AppSettings *settings)
{
    G0WorldDocument candidate;

    if (document == NULL || !name_valid(name) || !g0_world_config_validate(world) ||
        !g0_app_settings_validate(settings))
    {
        return 0;
    }
    g0_world_document_init(&candidate);
    if (!g0_world_document_set_name(&candidate, name) ||
        !g0_world_config_clone(world, &candidate.world))
    {
        g0_world_document_destroy(&candidate);
        return 0;
    }
    candidate.settings = *settings;
    g0_world_document_destroy(document);
    *document = candidate;
    return 1;
}

int g0_world_document_clone(const G0WorldDocument *source, G0WorldDocument *out_copy)
{
    if (source == NULL || out_copy == NULL)
        return 0;
    return g0_world_document_create(out_copy, source->name, &source->world,
                                    &source->settings);
}

int g0_world_document_validate(const G0WorldDocument *document)
{
    return document != NULL && name_valid(document->name) &&
           g0_world_config_validate(&document->world) &&
           g0_app_settings_validate(&document->settings);
}

int g0_world_document_equal(const G0WorldDocument *left, const G0WorldDocument *right)
{
    size_t count;

    if (!g0_world_document_validate(left) || !g0_world_document_validate(right) ||
        strcmp(left->name, right->name) != 0 ||
        left->world.width != right->world.width || left->world.height != right->world.height ||
        left->world.fish_spawn_x != right->world.fish_spawn_x ||
        left->world.fish_spawn_y != right->world.fish_spawn_y ||
        !g0_app_settings_equal(&left->settings, &right->settings))
    {
        return 0;
    }
    count = (size_t)left->world.width * (size_t)left->world.height;
    return memcmp(left->world.tiles, right->world.tiles,
                  count * sizeof(*left->world.tiles)) == 0 &&
           memcmp(left->world.rocks, right->world.rocks,
                  count * sizeof(*left->world.rocks)) == 0 &&
           memcmp(left->world.foods, right->world.foods,
                  count * sizeof(*left->world.foods)) == 0;
}

static int commit_file(const char *temporary, const char *filename)
{
#ifdef _WIN32
    return MoveFileExA(temporary, filename,
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary, filename) == 0;
#endif
}

int g0_world_document_save(
    const char *filename,
    const G0WorldDocument *document,
    G0WorldDocumentError *out_error)
{
    char temporary[1024];
    FILE *file;
    size_t count;
    int wrote;

    set_error(out_error, G0_WORLD_DOCUMENT_ERROR_INVALID_ARGUMENT);
    if (filename == NULL || filename[0] == '\0' || !g0_world_document_validate(document) ||
        snprintf(temporary, sizeof(temporary), "%s.tmp", filename) <= 0 ||
        strlen(temporary) >= sizeof(temporary))
    {
        return 0;
    }
    file = fopen(temporary, "w");
    if (file == NULL)
    {
        set_error(out_error, G0_WORLD_DOCUMENT_ERROR_IO);
        return 0;
    }
    count = (size_t)document->world.width * (size_t)document->world.height;
    wrote = fprintf(file, "%s\n", G0_WORLD_DOCUMENT_FORMAT) >= 0 &&
            write_string(file, "name", document->name) &&
            fprintf(file, "width=%u\nheight=%u\ntile_size=%d\n",
                    document->world.width, document->world.height,
                    G0_WORLD_DOCUMENT_TILE_SIZE) >= 0 &&
            fprintf(file, "default_speed_index=%u\nstart_paused=%d\nshow_grid=%d\n"
                    "show_debug_panel=%d\ninitial_zoom_index=%d\nevent_log_enabled=%d\n"
                    "water_opacity_percent=%d\npresentation_mode=%d\n",
                    document->settings.default_speed_index, document->settings.start_paused,
                    document->settings.show_grid, document->settings.show_debug_panel,
                    document->settings.initial_zoom_index, document->settings.event_log_enabled,
                    document->settings.water_opacity_percent,
                    document->settings.presentation_mode) >= 0 &&
            fprintf(file, "fish_spawn=%u,%u\n", document->world.fish_spawn_x,
                    document->world.fish_spawn_y) >= 0 &&
            write_terrain(file, &document->world, count) &&
            write_bytes(file, "rocks", document->world.rocks, count, '0', '1') &&
            write_bytes(file, "foods", document->world.foods, count, '0', '1');
    if (!wrote || fclose(file) != 0 || !commit_file(temporary, filename))
    {
        (void)remove(temporary);
        set_error(out_error, G0_WORLD_DOCUMENT_ERROR_IO);
        return 0;
    }
    set_error(out_error, G0_WORLD_DOCUMENT_ERROR_NONE);
    return 1;
}

static int parse_document_field(
    size_t key_index,
    const char *value,
    G0WorldDocument *candidate,
    uint32_t *width,
    uint32_t *height,
    uint32_t *spawn_x,
    uint32_t *spawn_y)
{
    size_t count;

    switch (key_index)
    {
        case 0U:
            return g0_world_document_set_name(candidate, value);
        case 1U:
            return parse_u32(value, width);
        case 2U:
            return parse_u32(value, height) &&
                   g0_world_config_init(&candidate->world, *width, *height);
        case 3U:
            return strcmp(value, "1000") == 0;
        case 4U:
            return parse_u32(value, &candidate->settings.default_speed_index);
        case 5U:
            return parse_int(value, &candidate->settings.start_paused);
        case 6U:
            return parse_int(value, &candidate->settings.show_grid);
        case 7U:
            return parse_int(value, &candidate->settings.show_debug_panel);
        case 8U:
            return parse_int(value, &candidate->settings.initial_zoom_index);
        case 9U:
            return parse_int(value, &candidate->settings.event_log_enabled);
        case 10U:
            return parse_int(value, &candidate->settings.water_opacity_percent);
        case 11U:
            return parse_int(value, &candidate->settings.presentation_mode);
        case 12U:
            if (!parse_spawn(value, spawn_x, spawn_y))
                return 0;
            candidate->world.fish_spawn_x = *spawn_x;
            candidate->world.fish_spawn_y = *spawn_y;
            return 1;
        case 13U:
            count = (size_t)candidate->world.width * (size_t)candidate->world.height;
            return parse_terrain(value, &candidate->world, count);
        case 14U:
            count = (size_t)candidate->world.width * (size_t)candidate->world.height;
            return parse_bytes(value, candidate->world.rocks, count, '0', '1');
        case 15U:
            count = (size_t)candidate->world.width * (size_t)candidate->world.height;
            return parse_bytes(value, candidate->world.foods, count, '0', '1');
        default:
            return 0;
    }
}

int g0_world_document_load(
    const char *filename,
    G0WorldDocument *out_document,
    G0WorldDocumentError *out_error)
{
    static const char *const v1_keys[] =
    {
        "name", "width", "height", "tile_size", "default_speed_index",
        "start_paused", "show_grid", "show_debug_panel", "initial_zoom_index",
        "event_log_enabled", "fish_spawn", "terrain", "rocks", "foods"
    };
    static const char *const v2_keys[] =
    {
        "name", "width", "height", "tile_size", "default_speed_index",
        "start_paused", "show_grid", "show_debug_panel", "initial_zoom_index",
        "event_log_enabled", "water_opacity_percent", "presentation_mode",
        "fish_spawn", "terrain", "rocks", "foods"
    };
    const char *const *keys = NULL;
    size_t key_count = 0U;
    char line[G0_WORLD_DOCUMENT_LINE_MAX];
    FILE *file = NULL;
    G0WorldDocument candidate;
    uint32_t width = 0U;
    uint32_t height = 0U;
    uint32_t spawn_x = 0U;
    uint32_t spawn_y = 0U;
    size_t key_index;
    int is_v2 = 0;

    set_error(out_error, G0_WORLD_DOCUMENT_ERROR_INVALID_ARGUMENT);
    if (filename == NULL || filename[0] == '\0' || out_document == NULL)
        return 0;
    file = fopen(filename, "r");
    if (file == NULL)
    {
        set_error(out_error, G0_WORLD_DOCUMENT_ERROR_IO);
        return 0;
    }
    g0_world_document_init(&candidate);
    if (!read_line(file, line, sizeof(line)))
        goto format;
    if (strcmp(line, G0_WORLD_DOCUMENT_FORMAT) == 0)
    {
        keys = v2_keys;
        key_count = sizeof(v2_keys) / sizeof(v2_keys[0]);
        is_v2 = 1;
    }
    else if (strcmp(line, G0_WORLD_DOCUMENT_FORMAT_V1) == 0)
    {
        keys = v1_keys;
        key_count = sizeof(v1_keys) / sizeof(v1_keys[0]);
    }
    else
    {
        set_error(out_error, G0_WORLD_DOCUMENT_ERROR_VERSION);
        goto done;
    }
    for (key_index = 0U; key_index < key_count; ++key_index)
    {
        const char *value;
        size_t field_index = is_v2 ? key_index :
            (key_index < 10U ? key_index : key_index + 2U);

        if (!read_line(file, line, sizeof(line)) ||
            !key_value(line, keys[key_index], &value) ||
            !parse_document_field(field_index, value, &candidate, &width, &height,
                                  &spawn_x, &spawn_y))
        {
            goto format;
        }
    }
    if (read_line(file, line, sizeof(line)))
        goto format;
    if (!g0_world_document_validate(&candidate))
    {
        set_error(out_error, G0_WORLD_DOCUMENT_ERROR_VALIDATION);
        goto done;
    }
    g0_world_document_destroy(out_document);
    *out_document = candidate;
    set_error(out_error, G0_WORLD_DOCUMENT_ERROR_NONE);
    (void)fclose(file);
    return 1;

format:
    set_error(out_error, G0_WORLD_DOCUMENT_ERROR_FORMAT);
done:
    if (file != NULL)
        (void)fclose(file);
    g0_world_document_destroy(&candidate);
    return 0;
}

const char *g0_world_document_error_name(G0WorldDocumentError error)
{
    switch (error)
    {
        case G0_WORLD_DOCUMENT_ERROR_NONE: return "NONE";
        case G0_WORLD_DOCUMENT_ERROR_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case G0_WORLD_DOCUMENT_ERROR_IO: return "IO";
        case G0_WORLD_DOCUMENT_ERROR_FORMAT: return "FORMAT";
        case G0_WORLD_DOCUMENT_ERROR_VERSION: return "VERSION";
        case G0_WORLD_DOCUMENT_ERROR_VALIDATION: return "VALIDATION";
        case G0_WORLD_DOCUMENT_ERROR_ALLOCATION: return "ALLOCATION";
        default: return "UNKNOWN";
    }
}