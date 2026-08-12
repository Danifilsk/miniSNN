#include "k1_c4_config.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FNV_OFFSET UINT64_C(14695981039346656037)
#define FNV_PRIME UINT64_C(1099511628211)

typedef enum
{
    SECTION_NONE = 0,
    SECTION_SCENARIO,
    SECTION_SPACE,
    SECTION_ENTITIES,
    SECTION_LINKS,
    SECTION_COMMANDS
} K1C4ConfigSection;

static void set_error(char *message, size_t message_size, const char *format, ...)
{
    va_list arguments;

    if (message == NULL || message_size == 0U)
    {
        return;
    }
    va_start(arguments, format);
    (void)vsnprintf(message, message_size, format, arguments);
    va_end(arguments);
}

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

    if (text == NULL || *text == '\0' || *text == '-')
    {
        return 0;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno == ERANGE || *end != '\0')
    {
        return 0;
    }
    *out_value = (uint64_t)value;
    return 1;
}

static int parse_u32(const char *text, uint32_t *out_value)
{
    uint64_t value;

    return parse_u64(text, &value) && value <= UINT32_MAX &&
           ((*out_value = (uint32_t)value), 1);
}

static int parse_scalar(const char *text, MiniSNNWorldsKernelScalar *out_value)
{
    char *end;
    long long value;

    if (text == NULL || *text == '\0')
    {
        return 0;
    }
    errno = 0;
    value = strtoll(text, &end, 10);
    if (errno == ERANGE || *end != '\0')
    {
        return 0;
    }
    *out_value = (MiniSNNWorldsKernelScalar)value;
    return 1;
}

static int parse_orientation(const char *text, MiniSNNWorldsKernelOrientation *out_value)
{
    uint32_t value;

    return parse_u32(text, &value) && value < UINT32_C(360000) &&
           ((*out_value = (MiniSNNWorldsKernelOrientation)value), 1);
}

static int parse_entity_index(const char *key, const char *prefix, size_t maximum, size_t *out_index)
{
    const char *number;
    uint64_t index;

    if (strncmp(key, prefix, strlen(prefix)) != 0)
    {
        return 0;
    }
    number = key + strlen(prefix);
    if (!parse_u64(number, &index) || index == 0U || index > maximum)
    {
        return 0;
    }
    *out_index = (size_t)(index - UINT64_C(1));
    return 1;
}

static int parse_fields(char *value, char **fields, size_t expected_count)
{
    size_t index;
    char *cursor = value;

    for (index = 0U; index < expected_count; ++index)
    {
        char *separator;

        fields[index] = trim(cursor);
        separator = strchr(cursor, ',');
        if (index + 1U == expected_count)
        {
            if (separator != NULL || *fields[index] == '\0')
            {
                return 0;
            }
            return 1;
        }
        if (separator == NULL)
        {
            return 0;
        }
        *separator = '\0';
        cursor = separator + 1;
    }
    return 0;
}

static int command_type_from_name(const char *name, MiniSNNWorldsKernelCommandType *out_type)
{
    if (strcmp(name, "move") == 0)
    {
        *out_type = MINISNN_WORLDS_KERNEL_COMMAND_MOVE_ENTITY;
    }
    else if (strcmp(name, "create_link") == 0)
    {
        *out_type = MINISNN_WORLDS_KERNEL_COMMAND_CREATE_SPATIAL_LINK;
    }
    else if (strcmp(name, "remove_link") == 0)
    {
        *out_type = MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_SPATIAL_LINK;
    }
    else if (strcmp(name, "destroy") == 0)
    {
        *out_type = MINISNN_WORLDS_KERNEL_COMMAND_DESTROY_ENTITY;
    }
    else if (strcmp(name, "remove_from_space") == 0)
    {
        *out_type = MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_ENTITY_FROM_SPACE;
    }
    else if (strcmp(name, "set_occupancy") == 0)
    {
        *out_type = MINISNN_WORLDS_KERNEL_COMMAND_SET_OCCUPANCY;
    }
    else if (strcmp(name, "clear_occupancy") == 0)
    {
        *out_type = MINISNN_WORLDS_KERNEL_COMMAND_CLEAR_OCCUPANCY;
    }
    else
    {
        return 0;
    }
    return 1;
}

static const char *command_type_name(MiniSNNWorldsKernelCommandType type)
{
    switch (type)
    {
        case MINISNN_WORLDS_KERNEL_COMMAND_MOVE_ENTITY: return "move";
        case MINISNN_WORLDS_KERNEL_COMMAND_CREATE_SPATIAL_LINK: return "create_link";
        case MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_SPATIAL_LINK: return "remove_link";
        case MINISNN_WORLDS_KERNEL_COMMAND_DESTROY_ENTITY: return "destroy";
        case MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_ENTITY_FROM_SPACE: return "remove_from_space";
        case MINISNN_WORLDS_KERNEL_COMMAND_SET_OCCUPANCY: return "set_occupancy";
        case MINISNN_WORLDS_KERNEL_COMMAND_CLEAR_OCCUPANCY: return "clear_occupancy";
        default: return "invalid";
    }
}

static int entity_exists(const K1C4Config *config, MiniSNNWorldsKernelEntityId id)
{
    size_t index;

    for (index = 0U; index < config->entity_count; ++index)
    {
        if (config->entities[index].id.value == id.value)
        {
            return 1;
        }
    }
    return 0;
}

static int is_valid_id_or_zero(const K1C4Config *config, MiniSNNWorldsKernelEntityId id)
{
    return id.value == UINT64_C(0) || entity_exists(config, id);
}

static int is_valid_scenario_id(const char *value)
{
    size_t index;

    if (value == NULL || *value == '\0' || strlen(value) > K1_C4_SCENARIO_ID_MAX_LENGTH)
    {
        return 0;
    }
    for (index = 0U; value[index] != '\0'; ++index)
    {
        unsigned char character = (unsigned char)value[index];
        if (!(character >= 'A' && character <= 'Z') &&
            !(character >= 'a' && character <= 'z') &&
            !(character >= '0' && character <= '9') &&
            character != '_' && character != '-')
        {
            return 0;
        }
    }
    return 1;
}

static int occupancy_is_valid(MiniSNNWorldsKernelOccupancy occupancy)
{
    return occupancy.half_extent_x >= INT64_C(0) && occupancy.half_extent_y >= INT64_C(0) &&
           occupancy.category_bits != UINT32_C(0);
}

int k1_c4_config_validate(const K1C4Config *config, char *error_message, size_t error_message_size)
{
    size_t index;

    if (config == NULL || config->scenario_version != UINT32_C(1) ||
        !is_valid_scenario_id(config->scenario_id) || config->ticks < UINT64_C(15) ||
        config->entity_count == 0U || config->entity_count > K1_C4_MAX_ENTITIES ||
        config->link_count > K1_C4_MAX_LINKS || config->command_count > K1_C4_MAX_COMMANDS ||
        config->bounds.min_x >= config->bounds.max_x || config->bounds.min_y >= config->bounds.max_y)
    {
        set_error(error_message, error_message_size, "invalid scenario bounds or identity");
        return 0;
    }
    for (index = 0U; index < config->entity_count; ++index)
    {
        size_t other;
        const K1C4EntitySpec *entity = &config->entities[index];

        if (entity->id.value == UINT64_C(0) ||
            entity->transform.position.x < config->bounds.min_x ||
            entity->transform.position.x > config->bounds.max_x ||
            entity->transform.position.y < config->bounds.min_y ||
            entity->transform.position.y > config->bounds.max_y ||
            entity->transform.orientation >= UINT32_C(360000) ||
            (entity->has_occupancy != 0 && !occupancy_is_valid(entity->occupancy)))
        {
            set_error(error_message, error_message_size, "invalid entity %llu", (unsigned long long)(index + 1U));
            return 0;
        }
        for (other = 0U; other < index; ++other)
        {
            if (config->entities[other].id.value == entity->id.value)
            {
                set_error(error_message, error_message_size, "duplicate entity id");
                return 0;
            }
        }
    }
    for (index = 0U; index < config->link_count; ++index)
    {
        const K1C4LinkSpec *link = &config->links[index];
        if (!entity_exists(config, link->parent) || !entity_exists(config, link->child) ||
            link->parent.value == link->child.value)
        {
            set_error(error_message, error_message_size, "invalid link %llu", (unsigned long long)(index + 1U));
            return 0;
        }
    }
    for (index = 0U; index < config->command_count; ++index)
    {
        const K1C4CommandSpec *command = &config->commands[index];
        if (command->tick == UINT64_C(0) || command->tick > config->ticks ||
            !is_valid_id_or_zero(config, command->issuer) ||
            !is_valid_id_or_zero(config, command->target) ||
            !is_valid_id_or_zero(config, command->parent) ||
            !is_valid_id_or_zero(config, command->child) ||
            command_type_name(command->type)[0] == 'i' ||
            (command->type == MINISNN_WORLDS_KERNEL_COMMAND_SET_OCCUPANCY &&
             !occupancy_is_valid(command->occupancy)))
        {
            set_error(error_message, error_message_size, "invalid command %llu", (unsigned long long)(index + 1U));
            return 0;
        }
    }
    return 1;
}

static uint64_t fnv_append_u64(uint64_t hash, uint64_t value)
{
    size_t index;
    for (index = 0U; index < 8U; ++index)
    {
        hash ^= (value >> (index * 8U)) & UINT64_C(0xff);
        hash *= FNV_PRIME;
    }
    return hash;
}

uint64_t k1_c4_config_signature(const K1C4Config *config)
{
    uint64_t hash = FNV_OFFSET;
    size_t index;

    if (config == NULL)
    {
        return UINT64_C(0);
    }
    hash = fnv_append_u64(hash, config->scenario_version);
    for (index = 0U; config->scenario_id[index] != '\0'; ++index)
    {
        hash ^= (unsigned char)config->scenario_id[index];
        hash *= FNV_PRIME;
    }
    hash = fnv_append_u64(hash, config->master_seed);
    hash = fnv_append_u64(hash, config->ticks);
    hash = fnv_append_u64(hash, (uint64_t)config->bounds.min_x);
    hash = fnv_append_u64(hash, (uint64_t)config->bounds.min_y);
    hash = fnv_append_u64(hash, (uint64_t)config->bounds.max_x);
    hash = fnv_append_u64(hash, (uint64_t)config->bounds.max_y);
    hash = fnv_append_u64(hash, config->entity_count);
    for (index = 0U; index < config->entity_count; ++index)
    {
        const K1C4EntitySpec *entity = &config->entities[index];
        hash = fnv_append_u64(hash, entity->id.value);
        hash = fnv_append_u64(hash, (uint64_t)entity->transform.position.x);
        hash = fnv_append_u64(hash, (uint64_t)entity->transform.position.y);
        hash = fnv_append_u64(hash, entity->transform.orientation);
        hash = fnv_append_u64(hash, (uint64_t)entity->has_occupancy);
        hash = fnv_append_u64(hash, (uint64_t)entity->occupancy.half_extent_x);
        hash = fnv_append_u64(hash, (uint64_t)entity->occupancy.half_extent_y);
        hash = fnv_append_u64(hash, entity->occupancy.category_bits);
        hash = fnv_append_u64(hash, entity->occupancy.blocking_mask);
    }
    hash = fnv_append_u64(hash, config->link_count);
    for (index = 0U; index < config->link_count; ++index)
    {
        hash = fnv_append_u64(hash, config->links[index].parent.value);
        hash = fnv_append_u64(hash, config->links[index].child.value);
    }
    hash = fnv_append_u64(hash, config->command_count);
    for (index = 0U; index < config->command_count; ++index)
    {
        const K1C4CommandSpec *command = &config->commands[index];
        hash = fnv_append_u64(hash, command->tick);
        hash = fnv_append_u64(hash, command->priority);
        hash = fnv_append_u64(hash, command->issuer.value);
        hash = fnv_append_u64(hash, command->type);
        hash = fnv_append_u64(hash, command->target.value);
        hash = fnv_append_u64(hash, command->parent.value);
        hash = fnv_append_u64(hash, command->child.value);
        hash = fnv_append_u64(hash, (uint64_t)command->x);
        hash = fnv_append_u64(hash, (uint64_t)command->y);
        hash = fnv_append_u64(hash, (uint64_t)command->occupancy.half_extent_x);
        hash = fnv_append_u64(hash, (uint64_t)command->occupancy.half_extent_y);
        hash = fnv_append_u64(hash, command->occupancy.category_bits);
        hash = fnv_append_u64(hash, command->occupancy.blocking_mask);
    }
    return hash;
}

int k1_c4_config_load_file(const char *filename, K1C4Config *out_config, char *error_message, size_t error_message_size)
{
    enum
    {
        SCENARIO_VERSION_SEEN = 1U,
        SCENARIO_ID_SEEN = 2U,
        MASTER_SEED_SEEN = 4U,
        TICKS_SEEN = 8U,
        SPACE_MIN_X_SEEN = 1U,
        SPACE_MIN_Y_SEEN = 2U,
        SPACE_MAX_X_SEEN = 4U,
        SPACE_MAX_Y_SEEN = 8U
    };
    FILE *file;
    K1C4Config config;
    K1C4ConfigSection section = SECTION_NONE;
    unsigned int section_mask = 0U;
    unsigned int scenario_seen = 0U;
    unsigned int space_seen = 0U;
    unsigned int entity_mask = 0U;
    unsigned int link_mask = 0U;
    uint64_t command_mask = UINT64_C(0);
    char line[K1_C4_CONFIG_LINE_MAX_LENGTH];
    unsigned long line_number = 0UL;

    if (filename == NULL || out_config == NULL)
    {
        set_error(error_message, error_message_size, "config filename or output is null");
        return 0;
    }
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        set_error(error_message, error_message_size, "could not open config");
        return 0;
    }
    memset(&config, 0, sizeof(config));
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *text;
        char *equals;
        char *key;
        char *value;
        size_t index;
        uint64_t parsed_u64;

        ++line_number;
        text = trim(line);
        if (*text == '\0' || *text == '#' || *text == ';')
        {
            continue;
        }
        if (*text == '[')
        {
            if (strcmp(text, "[scenario]") == 0 && (section_mask & 1U) == 0U)
            { section = SECTION_SCENARIO; section_mask |= 1U; continue; }
            if (strcmp(text, "[space]") == 0 && (section_mask & 2U) == 0U)
            { section = SECTION_SPACE; section_mask |= 2U; continue; }
            if (strcmp(text, "[entities]") == 0 && (section_mask & 4U) == 0U)
            { section = SECTION_ENTITIES; section_mask |= 4U; continue; }
            if (strcmp(text, "[links]") == 0 && (section_mask & 8U) == 0U)
            { section = SECTION_LINKS; section_mask |= 8U; continue; }
            if (strcmp(text, "[commands]") == 0 && (section_mask & 16U) == 0U)
            { section = SECTION_COMMANDS; section_mask |= 16U; continue; }
            set_error(error_message, error_message_size, "invalid or duplicate section at line %lu", line_number);
            fclose(file);
            return 0;
        }
        equals = strchr(text, '=');
        if (section == SECTION_NONE || equals == NULL || strchr(equals + 1, '=') != NULL)
        {
            set_error(error_message, error_message_size, "malformed key at line %lu", line_number);
            fclose(file);
            return 0;
        }
        *equals = '\0';
        key = trim(text);
        value = trim(equals + 1);
        if (*key == '\0' || *value == '\0')
        {
            set_error(error_message, error_message_size, "missing key or value at line %lu", line_number);
            fclose(file);
            return 0;
        }
        if (section == SECTION_SCENARIO && strcmp(key, "scenario_version") == 0 &&
            (scenario_seen & SCENARIO_VERSION_SEEN) == 0U && parse_u64(value, &parsed_u64) && parsed_u64 == 1U)
        {
            config.scenario_version = UINT32_C(1);
            scenario_seen |= SCENARIO_VERSION_SEEN;
        }
        else if (section == SECTION_SCENARIO && strcmp(key, "scenario_id") == 0 &&
                 (scenario_seen & SCENARIO_ID_SEEN) == 0U && is_valid_scenario_id(value))
        {
            (void)strncpy(config.scenario_id, value, sizeof(config.scenario_id) - 1U);
            scenario_seen |= SCENARIO_ID_SEEN;
        }
        else if (section == SECTION_SCENARIO && strcmp(key, "master_seed") == 0 &&
                 (scenario_seen & MASTER_SEED_SEEN) == 0U && parse_u64(value, &config.master_seed))
        {
            scenario_seen |= MASTER_SEED_SEEN;
        }
        else if (section == SECTION_SCENARIO && strcmp(key, "ticks") == 0 &&
                 (scenario_seen & TICKS_SEEN) == 0U && parse_u64(value, &config.ticks))
        {
            scenario_seen |= TICKS_SEEN;
        }
        else if (section == SECTION_SPACE && strcmp(key, "min_x") == 0 &&
                 (space_seen & SPACE_MIN_X_SEEN) == 0U && parse_scalar(value, &config.bounds.min_x))
        {
            space_seen |= SPACE_MIN_X_SEEN;
        }
        else if (section == SECTION_SPACE && strcmp(key, "min_y") == 0 &&
                 (space_seen & SPACE_MIN_Y_SEEN) == 0U && parse_scalar(value, &config.bounds.min_y))
        {
            space_seen |= SPACE_MIN_Y_SEEN;
        }
        else if (section == SECTION_SPACE && strcmp(key, "max_x") == 0 &&
                 (space_seen & SPACE_MAX_X_SEEN) == 0U && parse_scalar(value, &config.bounds.max_x))
        {
            space_seen |= SPACE_MAX_X_SEEN;
        }
        else if (section == SECTION_SPACE && strcmp(key, "max_y") == 0 &&
                 (space_seen & SPACE_MAX_Y_SEEN) == 0U && parse_scalar(value, &config.bounds.max_y))
        {
            space_seen |= SPACE_MAX_Y_SEEN;
        }
        else if (section == SECTION_ENTITIES && parse_entity_index(key, "entity_", K1_C4_MAX_ENTITIES, &index) &&
                 (entity_mask & (1U << index)) == 0U)
        {
            char *fields[9];
            K1C4EntitySpec *entity = &config.entities[index];
            uint64_t occupancy_flag;
            if (!parse_fields(value, fields, 9U) || !parse_u64(fields[0], &entity->id.value) ||
                !parse_scalar(fields[1], &entity->transform.position.x) || !parse_scalar(fields[2], &entity->transform.position.y) ||
                !parse_orientation(fields[3], &entity->transform.orientation) || !parse_u64(fields[4], &occupancy_flag) || occupancy_flag > 1U ||
                !parse_scalar(fields[5], &entity->occupancy.half_extent_x) || !parse_scalar(fields[6], &entity->occupancy.half_extent_y) ||
                !parse_u32(fields[7], &entity->occupancy.category_bits) || !parse_u32(fields[8], &entity->occupancy.blocking_mask))
            {
                set_error(error_message, error_message_size, "invalid entity at line %lu", line_number);
                fclose(file);
                return 0;
            }
            entity->has_occupancy = (int)occupancy_flag;
            entity_mask |= 1U << index;
            if (index + 1U > config.entity_count) config.entity_count = index + 1U;
        }
        else if (section == SECTION_LINKS && parse_entity_index(key, "link_", K1_C4_MAX_LINKS, &index) &&
                 (link_mask & (1U << index)) == 0U)
        {
            char *fields[2];
            if (!parse_fields(value, fields, 2U) || !parse_u64(fields[0], &config.links[index].parent.value) ||
                !parse_u64(fields[1], &config.links[index].child.value))
            {
                set_error(error_message, error_message_size, "invalid link at line %lu", line_number);
                fclose(file);
                return 0;
            }
            link_mask |= 1U << index;
            if (index + 1U > config.link_count) config.link_count = index + 1U;
        }
        else if (section == SECTION_COMMANDS && parse_entity_index(key, "command_", K1_C4_MAX_COMMANDS, &index) &&
                 (command_mask & (UINT64_C(1) << index)) == UINT64_C(0))
        {
            char *fields[13];
            K1C4CommandSpec *command = &config.commands[index];
            if (!parse_fields(value, fields, 13U) || !parse_u64(fields[0], &command->tick) ||
                !parse_u32(fields[1], &command->priority) || !parse_u64(fields[2], &command->issuer.value) ||
                !command_type_from_name(fields[3], &command->type) || !parse_u64(fields[4], &command->target.value) ||
                !parse_u64(fields[5], &command->parent.value) || !parse_u64(fields[6], &command->child.value) ||
                !parse_scalar(fields[7], &command->x) || !parse_scalar(fields[8], &command->y) ||
                !parse_scalar(fields[9], &command->occupancy.half_extent_x) || !parse_scalar(fields[10], &command->occupancy.half_extent_y) ||
                !parse_u32(fields[11], &command->occupancy.category_bits) || !parse_u32(fields[12], &command->occupancy.blocking_mask))
            {
                set_error(error_message, error_message_size, "invalid command at line %lu", line_number);
                fclose(file);
                return 0;
            }
            command_mask |= UINT64_C(1) << index;
            if (index + 1U > config.command_count) config.command_count = index + 1U;
        }
        else
        {
            set_error(error_message, error_message_size, "unknown, duplicate, or invalid key at line %lu", line_number);
            fclose(file);
            return 0;
        }
    }
    fclose(file);
    if (section_mask != 31U ||
        scenario_seen != (SCENARIO_VERSION_SEEN | SCENARIO_ID_SEEN | MASTER_SEED_SEEN | TICKS_SEEN) ||
        space_seen != (SPACE_MIN_X_SEEN | SPACE_MIN_Y_SEEN | SPACE_MAX_X_SEEN | SPACE_MAX_Y_SEEN) ||
        entity_mask == 0U || link_mask == 0U || command_mask == UINT64_C(0) ||
        !k1_c4_config_validate(&config, error_message, error_message_size))
    {
        if (error_message != NULL && error_message[0] == '\0')
            set_error(error_message, error_message_size, "incomplete K1-C4 configuration");
        return 0;
    }
    *out_config = config;
    return 1;
}

int k1_c4_config_write_canonical(const K1C4Config *config, const char *filename, char *error_message, size_t error_message_size)
{
    FILE *file;
    size_t index;

    if (!k1_c4_config_validate(config, error_message, error_message_size) || filename == NULL)
    {
        return 0;
    }
    file = fopen(filename, "wb");
    if (file == NULL)
    {
        set_error(error_message, error_message_size, "could not write canonical config");
        return 0;
    }
    if (fprintf(file, "[scenario]\nscenario_version=1\nscenario_id=%s\nmaster_seed=%" PRIu64 "\nticks=%" PRIu64 "\n\n[space]\nmin_x=%" PRId64 "\nmin_y=%" PRId64 "\nmax_x=%" PRId64 "\nmax_y=%" PRId64 "\n\n[entities]\n", config->scenario_id, config->master_seed, config->ticks, config->bounds.min_x, config->bounds.min_y, config->bounds.max_x, config->bounds.max_y) < 0)
    { fclose(file); return 0; }
    for (index = 0U; index < config->entity_count; ++index)
    {
        const K1C4EntitySpec *entity = &config->entities[index];
        if (fprintf(file, "entity_%llu=%" PRIu64 ",%" PRId64 ",%" PRId64 ",%u,%d,%" PRId64 ",%" PRId64 ",%u,%u\n", (unsigned long long)(index + 1U), entity->id.value, entity->transform.position.x, entity->transform.position.y, entity->transform.orientation, entity->has_occupancy, entity->occupancy.half_extent_x, entity->occupancy.half_extent_y, entity->occupancy.category_bits, entity->occupancy.blocking_mask) < 0)
        { fclose(file); return 0; }
    }
    if (fputs("\n[links]\n", file) == EOF) { fclose(file); return 0; }
    for (index = 0U; index < config->link_count; ++index)
    {
        if (fprintf(file, "link_%llu=%" PRIu64 ",%" PRIu64 "\n", (unsigned long long)(index + 1U), config->links[index].parent.value, config->links[index].child.value) < 0)
        { fclose(file); return 0; }
    }
    if (fputs("\n[commands]\n", file) == EOF) { fclose(file); return 0; }
    for (index = 0U; index < config->command_count; ++index)
    {
        const K1C4CommandSpec *command = &config->commands[index];
        if (fprintf(file, "command_%llu=%" PRIu64 ",%u,%" PRIu64 ",%s,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRId64 ",%" PRId64 ",%" PRId64 ",%" PRId64 ",%u,%u\n", (unsigned long long)(index + 1U), command->tick, command->priority, command->issuer.value, command_type_name(command->type), command->target.value, command->parent.value, command->child.value, command->x, command->y, command->occupancy.half_extent_x, command->occupancy.half_extent_y, command->occupancy.category_bits, command->occupancy.blocking_mask) < 0)
        { fclose(file); return 0; }
    }
    if (fclose(file) != 0)
    {
        set_error(error_message, error_message_size, "could not close canonical config");
        return 0;
    }
    return 1;
}