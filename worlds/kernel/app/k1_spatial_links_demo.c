#include "k1_c4_config.h"
#include "minisnn_worlds_kernel.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define K1_C4_MAX_RECORDED_COMMANDS 128U
#define K1_C4_MAX_RECORDED_EVENTS 256U
#define K1_C4_PATH_MAX_LENGTH 512U

typedef struct
{
    MiniSNNWorldsKernelCommandInfo commands[K1_C4_MAX_RECORDED_COMMANDS];
    size_t command_count;
    MiniSNNWorldsKernelEvent events[K1_C4_MAX_RECORDED_EVENTS];
    size_t event_count;
    MiniSNNWorldsKernelDiagnostics diagnostics;
    uint64_t state_hash;
    uint64_t moved_after_tick_five;
    uint64_t moved_after_tick_six;
} K1C4Run;

static MiniSNNWorldsKernelEntityId entity_id(uint64_t value)
{
    MiniSNNWorldsKernelEntityId result;
    result.value = value;
    return result;
}

static const char *command_name(MiniSNNWorldsKernelCommandType type)
{
    switch (type)
    {
        case MINISNN_WORLDS_KERNEL_COMMAND_CREATE_ENTITY: return "create";
        case MINISNN_WORLDS_KERNEL_COMMAND_DESTROY_ENTITY: return "destroy";
        case MINISNN_WORLDS_KERNEL_COMMAND_PLACE_ENTITY: return "place";
        case MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_ENTITY_FROM_SPACE: return "remove_from_space";
        case MINISNN_WORLDS_KERNEL_COMMAND_SET_OCCUPANCY: return "set_occupancy";
        case MINISNN_WORLDS_KERNEL_COMMAND_CLEAR_OCCUPANCY: return "clear_occupancy";
        case MINISNN_WORLDS_KERNEL_COMMAND_MOVE_ENTITY: return "move";
        case MINISNN_WORLDS_KERNEL_COMMAND_CREATE_SPATIAL_LINK: return "create_link";
        case MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_SPATIAL_LINK: return "remove_link";
        default: return "invalid";
    }
}

static const char *event_name(MiniSNNWorldsKernelEventType type)
{
    switch (type)
    {
        case MINISNN_WORLDS_KERNEL_EVENT_ENTITY_CREATED: return "entity_created";
        case MINISNN_WORLDS_KERNEL_EVENT_ENTITY_DESTROYED: return "entity_destroyed";
        case MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED: return "command_rejected";
        case MINISNN_WORLDS_KERNEL_EVENT_ENTITY_PLACED: return "entity_placed";
        case MINISNN_WORLDS_KERNEL_EVENT_ENTITY_REMOVED_FROM_SPACE: return "entity_removed_from_space";
        case MINISNN_WORLDS_KERNEL_EVENT_OCCUPANCY_SET: return "occupancy_set";
        case MINISNN_WORLDS_KERNEL_EVENT_OCCUPANCY_CLEARED: return "occupancy_cleared";
        case MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED: return "entity_moved";
        case MINISNN_WORLDS_KERNEL_EVENT_SPATIAL_LINK_CREATED: return "spatial_link_created";
        case MINISNN_WORLDS_KERNEL_EVENT_SPATIAL_LINK_REMOVED: return "spatial_link_removed";
        default: return "invalid";
    }
}

static const char *rejection_name(MiniSNNWorldsKernelCommandRejection rejection)
{
    switch (rejection)
    {
        case MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_NONE: return "none";
        case MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_CONFLICT: return "occupancy_conflict";
        case MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_DESTINATION_OVERFLOW: return "destination_overflow";
        case MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_HAS_SPATIAL_PARENT: return "target_has_spatial_parent";
        case MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_HAS_SPATIAL_LINKS: return "target_has_spatial_links";
        case MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_PARENT_NOT_ALIVE: return "spatial_link_parent_not_alive";
        case MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_CHILD_NOT_ALIVE: return "spatial_link_child_not_alive";
        case MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_SELF: return "spatial_link_self";
        case MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_PARENT_NOT_PLACED: return "spatial_link_parent_not_placed";
        case MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_CHILD_NOT_PLACED: return "spatial_link_child_not_placed";
        case MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_DUPLICATE: return "spatial_link_duplicate";
        case MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_CHILD_HAS_PARENT: return "spatial_link_child_has_parent";
        case MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_CYCLE: return "spatial_link_cycle";
        case MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_OFFSET_OVERFLOW: return "spatial_link_offset_overflow";
        case MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_SPATIAL_LINK_NOT_FOUND: return "spatial_link_not_found";
        default: return "other";
    }
}

static int make_path(char *out_path, size_t out_path_size, const char *directory, const char *filename)
{
    int written;

    if (out_path == NULL || directory == NULL || filename == NULL)
    {
        return 0;
    }
    written = snprintf(out_path, out_path_size, "%s/%s", directory, filename);
    return written >= 0 && (size_t)written < out_path_size;
}

static int file_exists(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        return 0;
    }
    (void)fclose(file);
    return 1;
}

static int queue_spec(MiniSNNWorldsKernel *kernel, const K1C4CommandSpec *spec)
{
    MiniSNNWorldsKernelCommandId command_id;
    MiniSNNWorldsKernelError error;

    switch (spec->type)
    {
        case MINISNN_WORLDS_KERNEL_COMMAND_DESTROY_ENTITY:
            error = minisnn_worlds_kernel_queue_destroy_entity(kernel, spec->tick, spec->priority,
                spec->issuer, spec->target, &command_id);
            break;
        case MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_ENTITY_FROM_SPACE:
            error = minisnn_worlds_kernel_queue_remove_entity_from_space(kernel, spec->tick, spec->priority,
                spec->issuer, spec->target, &command_id);
            break;
        case MINISNN_WORLDS_KERNEL_COMMAND_SET_OCCUPANCY:
            error = minisnn_worlds_kernel_queue_set_occupancy(kernel, spec->tick, spec->priority,
                spec->issuer, spec->target, spec->occupancy, &command_id);
            break;
        case MINISNN_WORLDS_KERNEL_COMMAND_CLEAR_OCCUPANCY:
            error = minisnn_worlds_kernel_queue_clear_occupancy(kernel, spec->tick, spec->priority,
                spec->issuer, spec->target, &command_id);
            break;
        case MINISNN_WORLDS_KERNEL_COMMAND_MOVE_ENTITY:
            error = minisnn_worlds_kernel_queue_move_entity(kernel, spec->tick, spec->priority,
                spec->issuer, spec->target, spec->x, spec->y, &command_id);
            break;
        case MINISNN_WORLDS_KERNEL_COMMAND_CREATE_SPATIAL_LINK:
            error = minisnn_worlds_kernel_queue_create_spatial_link(kernel, spec->tick, spec->priority,
                spec->issuer, spec->parent, spec->child, &command_id);
            break;
        case MINISNN_WORLDS_KERNEL_COMMAND_REMOVE_SPATIAL_LINK:
            error = minisnn_worlds_kernel_queue_remove_spatial_link(kernel, spec->tick, spec->priority,
                spec->issuer, spec->parent, spec->child, &command_id);
            break;
        default:
            return 0;
    }
    return error == MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int capture_pending_commands(MiniSNNWorldsKernel *kernel, K1C4Run *run)
{
    size_t count = minisnn_worlds_kernel_pending_command_count(kernel);
    size_t index;

    if (run->command_count + count > K1_C4_MAX_RECORDED_COMMANDS)
    {
        return 0;
    }
    for (index = 0U; index < count; ++index)
    {
        if (minisnn_worlds_kernel_pending_command_at(kernel, index,
                &run->commands[run->command_count]) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return 0;
        }
        ++run->command_count;
    }
    return 1;
}

static int capture_events(MiniSNNWorldsKernel *kernel, K1C4Run *run)
{
    size_t count = minisnn_worlds_kernel_last_tick_event_count(kernel);
    size_t index;

    if (run->event_count + count > K1_C4_MAX_RECORDED_EVENTS)
    {
        return 0;
    }
    for (index = 0U; index < count; ++index)
    {
        if (minisnn_worlds_kernel_last_tick_event_at(kernel, index,
                &run->events[run->event_count]) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return 0;
        }
        ++run->event_count;
    }
    return 1;
}

static int step_and_record(MiniSNNWorldsKernel *kernel, K1C4Run *run)
{
    if (!capture_pending_commands(kernel, run) ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_testing_validate_invariants(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !capture_events(kernel, run))
    {
        return 0;
    }
    return 1;
}
static int event_matches(const MiniSNNWorldsKernelEvent *event, uint64_t tick,
    MiniSNNWorldsKernelEventType type, uint64_t subject)
{
    return event->tick == tick && event->type == type && event->subject.value == subject;
}

static int find_rejection(const K1C4Run *run, uint64_t tick,
    MiniSNNWorldsKernelCommandRejection rejection, uint64_t subject,
    uint64_t related, uint64_t affected)
{
    size_t index;
    for (index = 0U; index < run->event_count; ++index)
    {
        const MiniSNNWorldsKernelEvent *event = &run->events[index];
        if (event_matches(event, tick, MINISNN_WORLDS_KERNEL_EVENT_COMMAND_REJECTED, subject) &&
            event->rejection == rejection && event->related_entity.value == related &&
            event->affected_entity.value == affected)
        {
            return 1;
        }
    }
    return 0;
}

static size_t event_count_for_tick(const K1C4Run *run, uint64_t tick)
{
    size_t index;
    size_t count = 0U;
    for (index = 0U; index < run->event_count; ++index)
    {
        if (run->events[index].tick == tick)
        {
            ++count;
        }
    }
    return count;
}

static int validate_demo_contract(const MiniSNNWorldsKernel *kernel, const K1C4Run *run)
{
    MiniSNNWorldsKernelSpatialLink link;
    size_t index;
    size_t tick_five_index = 0U;
    size_t tick_eleven_link = 0U;
    size_t tick_eleven_move = 0U;
    size_t tick_twelve_remove = 0U;
    size_t tick_twelve_move = 0U;
    size_t tick_thirteen_move = 0U;
    size_t tick_thirteen_remove = 0U;

    if (run->diagnostics.state_hash_version != UINT32_C(5) ||
        run->diagnostics.active_spatial_links != UINT64_C(2) ||
        run->moved_after_tick_five == UINT64_C(0) ||
        run->moved_after_tick_five != run->moved_after_tick_six ||
        event_count_for_tick(run, UINT64_C(6)) != 1U ||
        !find_rejection(run, UINT64_C(7), MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_HAS_SPATIAL_PARENT,
            UINT64_C(2), UINT64_C(1), UINT64_C(0)) ||
        !find_rejection(run, UINT64_C(9), MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_OCCUPANCY_CONFLICT,
            UINT64_C(1), UINT64_C(7), UINT64_C(3)) ||
        !find_rejection(run, UINT64_C(10), MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_DESTINATION_OVERFLOW,
            UINT64_C(5), UINT64_C(0), UINT64_C(5)) ||
        !find_rejection(run, UINT64_C(14), MINISNN_WORLDS_KERNEL_COMMAND_REJECTION_TARGET_HAS_SPATIAL_LINKS,
            UINT64_C(1), UINT64_C(2), UINT64_C(0)) ||
        minisnn_worlds_kernel_spatial_link_count(kernel) != 2U ||
        minisnn_worlds_kernel_spatial_link_at(kernel, 0U, &link) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        link.parent.value != UINT64_C(2) || link.child.value != UINT64_C(3) ||
        minisnn_worlds_kernel_spatial_link_at(kernel, 1U, &link) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        link.parent.value != UINT64_C(5) || link.child.value != UINT64_C(6))
    {
        return 0;
    }

    for (index = 0U; index < run->event_count; ++index)
    {
        const MiniSNNWorldsKernelEvent *event = &run->events[index];
        if (event->event_id.value != (uint64_t)(index + 1U))
        {
            return 0;
        }
        if (event->tick == UINT64_C(5) && event->type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED)
        {
            static const uint64_t expected_bfs[] = { 1U, 2U, 4U, 3U };
            if (tick_five_index >= sizeof(expected_bfs) / sizeof(expected_bfs[0]) ||
                event->subject.value != expected_bfs[tick_five_index] ||
                (tick_five_index == 0U ? event->related_entity.value != 0U :
                 event->related_entity.value != (tick_five_index == 3U ? 2U : 1U)))
            {
                return 0;
            }
            ++tick_five_index;
        }
        if (event->tick == UINT64_C(11) && event->type == MINISNN_WORLDS_KERNEL_EVENT_SPATIAL_LINK_CREATED)
            tick_eleven_link = index + 1U;
        if (event->tick == UINT64_C(11) && event->type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED && event->subject.value == UINT64_C(1))
            tick_eleven_move = index + 1U;
        if (event->tick == UINT64_C(12) && event->type == MINISNN_WORLDS_KERNEL_EVENT_SPATIAL_LINK_REMOVED)
            tick_twelve_remove = index + 1U;
        if (event->tick == UINT64_C(12) && event->type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED && event->subject.value == UINT64_C(7))
            tick_twelve_move = index + 1U;
        if (event->tick == UINT64_C(13) && event->type == MINISNN_WORLDS_KERNEL_EVENT_ENTITY_MOVED && event->subject.value == UINT64_C(1))
            tick_thirteen_move = index + 1U;
        if (event->tick == UINT64_C(13) && event->type == MINISNN_WORLDS_KERNEL_EVENT_SPATIAL_LINK_REMOVED)
            tick_thirteen_remove = index + 1U;
    }
    return tick_five_index == 4U && tick_eleven_link < tick_eleven_move &&
           tick_twelve_remove < tick_twelve_move && tick_thirteen_move < tick_thirteen_remove;
}

static int run_demo(const K1C4Config *config, MiniSNNWorldsKernel **out_kernel, K1C4Run *out_run)
{
    MiniSNNWorldsKernelConfig kernel_config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsKernelError error;
    K1C4Run run;
    size_t index;
    uint64_t tick;

    memset(&run, 0, sizeof(run));
    kernel_config.master_seed = config->master_seed;
    kernel_config.space_bounds = config->bounds;
    kernel = minisnn_worlds_kernel_create(&kernel_config, &error);
    if (kernel == NULL)
    {
        return 0;
    }

    for (index = 0U; index < config->entity_count; ++index)
    {
        MiniSNNWorldsKernelCommandId command_id;
        if (minisnn_worlds_kernel_queue_create_entity(kernel, UINT64_C(1), 0U, entity_id(0U), &command_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
            goto fail;
    }
    if (!step_and_record(kernel, &run)) goto fail;
    for (index = 0U; index < config->entity_count; ++index)
    {
        MiniSNNWorldsKernelEntityId observed;
        if (minisnn_worlds_kernel_entity_at(kernel, index, &observed) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
            observed.value != config->entities[index].id.value)
            goto fail;
    }

    for (index = 0U; index < config->entity_count; ++index)
    {
        MiniSNNWorldsKernelCommandId command_id;
        const K1C4EntitySpec *entity = &config->entities[index];
        if (minisnn_worlds_kernel_queue_place_entity(kernel, UINT64_C(2), 0U, entity_id(0U),
                entity->id, entity->transform, &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
            goto fail;
    }
    if (!step_and_record(kernel, &run)) goto fail;

    for (index = 0U; index < config->entity_count; ++index)
    {
        MiniSNNWorldsKernelCommandId command_id;
        const K1C4EntitySpec *entity = &config->entities[index];
        if (entity->has_occupancy != 0 &&
            minisnn_worlds_kernel_queue_set_occupancy(kernel, UINT64_C(3), 0U, entity_id(0U),
                entity->id, entity->occupancy, &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
            goto fail;
    }
    if (!step_and_record(kernel, &run)) goto fail;

    for (index = 0U; index < config->link_count; ++index)
    {
        MiniSNNWorldsKernelCommandId command_id;
        if (minisnn_worlds_kernel_queue_create_spatial_link(kernel, UINT64_C(4), 0U, entity_id(0U),
                config->links[index].parent, config->links[index].child, &command_id) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
            goto fail;
    }
    if (!step_and_record(kernel, &run)) goto fail;

    for (tick = UINT64_C(5); tick <= config->ticks; ++tick)
    {
        for (index = 0U; index < config->command_count; ++index)
        {
            if (config->commands[index].tick == tick && !queue_spec(kernel, &config->commands[index]))
                goto fail;
        }
        if (!step_and_record(kernel, &run)) goto fail;
        if (tick == UINT64_C(5))
        {
            if (minisnn_worlds_kernel_get_diagnostics(kernel, &run.diagnostics) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
                goto fail;
            run.moved_after_tick_five = run.diagnostics.total_entities_moved;
        }
        if (tick == UINT64_C(6))
        {
            if (minisnn_worlds_kernel_get_diagnostics(kernel, &run.diagnostics) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
                goto fail;
            run.moved_after_tick_six = run.diagnostics.total_entities_moved;
        }
    }
    if (minisnn_worlds_kernel_get_diagnostics(kernel, &run.diagnostics) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(kernel, &run.state_hash) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_testing_validate_invariants(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !validate_demo_contract(kernel, &run))
        goto fail;

    *out_kernel = kernel;
    *out_run = run;
    return 1;
fail:
    minisnn_worlds_kernel_destroy(kernel);
    return 0;
}

static int write_commands(const char *path, const K1C4Run *run)
{
    FILE *file = fopen(path, "wb");
    size_t index;
    if (file == NULL) return 0;
    if (fputs("command_id,target_tick,priority,issuer,type,target,parent,child,delta_x,delta_y,half_extent_x,half_extent_y,category_bits,blocking_mask\n", file) == EOF) goto fail;
    for (index = 0U; index < run->command_count; ++index)
    {
        const MiniSNNWorldsKernelCommandInfo *command = &run->commands[index];
        if (fprintf(file, "%" PRIu64 ",%" PRIu64 ",%u,%" PRIu64 ",%s,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRId64 ",%" PRId64 ",%" PRId64 ",%" PRId64 ",%u,%u\n",
            command->command_id.value, command->target_tick, command->priority, command->issuer.value,
            command_name(command->type), command->target_entity.value,
            command->has_spatial_link_endpoints ? command->spatial_link_endpoints.parent.value : UINT64_C(0),
            command->has_spatial_link_endpoints ? command->spatial_link_endpoints.child.value : UINT64_C(0),
            command->has_displacement ? command->displacement.x : INT64_C(0),
            command->has_displacement ? command->displacement.y : INT64_C(0),
            command->has_occupancy ? command->occupancy.half_extent_x : INT64_C(0),
            command->has_occupancy ? command->occupancy.half_extent_y : INT64_C(0),
            command->has_occupancy ? command->occupancy.category_bits : UINT32_C(0),
            command->has_occupancy ? command->occupancy.blocking_mask : UINT32_C(0)) < 0) goto fail;
    }
    return fclose(file) == 0;
fail:
    (void)fclose(file);
    return 0;
}

static int write_events(const char *path, const K1C4Run *run)
{
    FILE *file = fopen(path, "wb");
    size_t index;
    if (file == NULL) return 0;
    if (fputs("event_id,tick,type,command_id,issuer,subject,related_entity,affected_entity,rejection,previous_x,previous_y,previous_orientation,x,y,orientation,delta_x,delta_y,link_parent,link_child,offset_x,offset_y\n", file) == EOF) goto fail;
    for (index = 0U; index < run->event_count; ++index)
    {
        const MiniSNNWorldsKernelEvent *event = &run->events[index];
        if (fprintf(file, "%" PRIu64 ",%" PRIu64 ",%s,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%s,%" PRId64 ",%" PRId64 ",%u,%" PRId64 ",%" PRId64 ",%u,%" PRId64 ",%" PRId64 ",%" PRIu64 ",%" PRIu64 ",%" PRId64 ",%" PRId64 "\n",
            event->event_id.value, event->tick, event_name(event->type), event->command_id.value,
            event->issuer.value, event->subject.value, event->related_entity.value, event->affected_entity.value,
            rejection_name(event->rejection), event->has_previous_transform ? event->previous_transform.position.x : INT64_C(0),
            event->has_previous_transform ? event->previous_transform.position.y : INT64_C(0),
            event->has_previous_transform ? event->previous_transform.orientation : 0U,
            event->has_transform ? event->transform.position.x : INT64_C(0),
            event->has_transform ? event->transform.position.y : INT64_C(0),
            event->has_transform ? event->transform.orientation : 0U,
            event->has_displacement ? event->displacement.x : INT64_C(0),
            event->has_displacement ? event->displacement.y : INT64_C(0),
            event->has_spatial_link ? event->spatial_link.parent.value : UINT64_C(0),
            event->has_spatial_link ? event->spatial_link.child.value : UINT64_C(0),
            event->has_spatial_link ? event->spatial_link.offset_x : INT64_C(0),
            event->has_spatial_link ? event->spatial_link.offset_y : INT64_C(0)) < 0) goto fail;
    }
    return fclose(file) == 0;
fail:
    (void)fclose(file);
    return 0;
}

static int write_entities(const char *path, const MiniSNNWorldsKernel *kernel)
{
    FILE *file = fopen(path, "wb");
    size_t index;
    if (file == NULL) return 0;
    if (fputs("entity_id,placed,x,y,orientation,has_occupancy,half_extent_x,half_extent_y,category_bits,blocking_mask\n", file) == EOF) goto fail;
    for (index = 0U; index < minisnn_worlds_kernel_entity_count(kernel); ++index)
    {
        MiniSNNWorldsKernelEntityId id;
        MiniSNNWorldsKernelTransform transform;
        MiniSNNWorldsKernelOccupancy occupancy;
        int placed;
        int has_occupancy;
        if (minisnn_worlds_kernel_entity_at(kernel, index, &id) != MINISNN_WORLDS_KERNEL_ERROR_NONE) goto fail;
        placed = minisnn_worlds_kernel_entity_is_placed(kernel, id) ? 1 : 0;
        has_occupancy = minisnn_worlds_kernel_entity_has_occupancy(kernel, id) ? 1 : 0;
        memset(&transform, 0, sizeof(transform));
        memset(&occupancy, 0, sizeof(occupancy));
        if ((placed && minisnn_worlds_kernel_entity_transform(kernel, id, &transform) != MINISNN_WORLDS_KERNEL_ERROR_NONE) ||
            (has_occupancy && minisnn_worlds_kernel_entity_occupancy(kernel, id, &occupancy) != MINISNN_WORLDS_KERNEL_ERROR_NONE)) goto fail;
        if (fprintf(file, "%" PRIu64 ",%d,%" PRId64 ",%" PRId64 ",%u,%d,%" PRId64 ",%" PRId64 ",%u,%u\n",
            id.value, placed, transform.position.x, transform.position.y, transform.orientation, has_occupancy,
            occupancy.half_extent_x, occupancy.half_extent_y, occupancy.category_bits, occupancy.blocking_mask) < 0) goto fail;
    }
    return fclose(file) == 0;
fail:
    (void)fclose(file);
    return 0;
}

static int write_links(const char *path, const MiniSNNWorldsKernel *kernel)
{
    FILE *file = fopen(path, "wb");
    size_t index;
    if (file == NULL) return 0;
    if (fputs("parent,child,offset_x,offset_y\n", file) == EOF) goto fail;
    for (index = 0U; index < minisnn_worlds_kernel_spatial_link_count(kernel); ++index)
    {
        MiniSNNWorldsKernelSpatialLink link;
        if (minisnn_worlds_kernel_spatial_link_at(kernel, index, &link) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
            fprintf(file, "%" PRIu64 ",%" PRIu64 ",%" PRId64 ",%" PRId64 "\n", link.parent.value, link.child.value,
                link.offset_x, link.offset_y) < 0) goto fail;
    }
    return fclose(file) == 0;
fail:
    (void)fclose(file);
    return 0;
}

static int write_diagnostics(const char *path, const K1C4Run *run)
{
    const MiniSNNWorldsKernelDiagnostics *d = &run->diagnostics;
    FILE *file = fopen(path, "wb");
    if (file == NULL) return 0;
    if (fputs("completed_ticks,state_hash_version,state_hash,alive_entities,placed_entities,active_occupancies,blocking_occupancies,active_spatial_links,total_commands_submitted,total_commands_applied,total_commands_rejected,total_events_emitted,total_entities_moved,total_movement_overflows_rejected,total_occupancy_conflicts_rejected,total_spatial_links_created,total_spatial_links_removed\n", file) == EOF ||
        fprintf(file, "%" PRIu64 ",%u,0x%016" PRIX64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
            d->completed_ticks, d->state_hash_version, run->state_hash, d->alive_entities, d->placed_entities,
            d->active_occupancies, d->blocking_occupancies, d->active_spatial_links, d->total_commands_submitted,
            d->total_commands_applied, d->total_commands_rejected, d->total_events_emitted, d->total_entities_moved,
            d->total_movement_overflows_rejected, d->total_occupancy_conflicts_rejected,
            d->total_spatial_links_created, d->total_spatial_links_removed) < 0)
    { (void)fclose(file); return 0; }
    return fclose(file) == 0;
}

static int write_text_outputs(const char *state_path, const char *summary_path, const K1C4Config *config, const K1C4Run *run)
{
    FILE *file = fopen(state_path, "wb");
    if (file == NULL ||
        fprintf(file, "state_hash_version=V5\n") < 0 ||
        fprintf(file, "state_hash=0x%016" PRIX64 "\n", run->state_hash) < 0 ||
        fputs("invariants=PASS\n", file) == EOF || fclose(file) != 0)
    {
        return 0;
    }
    file = fopen(summary_path, "wb");
    if (file == NULL ||
        fprintf(file, "scenario_id=%s\n", config->scenario_id) < 0 ||
        fprintf(file, "config_signature=0x%016" PRIX64 "\n", k1_c4_config_signature(config)) < 0 ||
        fprintf(file, "master_seed=%" PRIu64 "\n", config->master_seed) < 0 ||
        fprintf(file, "completed_ticks=%" PRIu64 "\n", run->diagnostics.completed_ticks) < 0 ||
        fputs("state_hash_version=V5\n", file) == EOF ||
        fprintf(file, "state_hash=0x%016" PRIX64 "\n", run->state_hash) < 0 ||
        fprintf(file, "commands=%zu\n", run->command_count) < 0 ||
        fprintf(file, "events=%zu\n", run->event_count) < 0 ||
        fprintf(file, "active_spatial_links=%" PRIu64 "\n", run->diagnostics.active_spatial_links) < 0 ||
        fprintf(file, "total_entities_moved=%" PRIu64 "\n", run->diagnostics.total_entities_moved) < 0 ||
        fputs("invariants=PASS\n", file) == EOF || fclose(file) != 0)
    {
        return 0;
    }
    return 1;
}
static int write_all_outputs(const char *directory, const K1C4Config *config,
    const MiniSNNWorldsKernel *kernel, const K1C4Run *run)
{
    char config_path[K1_C4_PATH_MAX_LENGTH];
    char command_path[K1_C4_PATH_MAX_LENGTH];
    char event_path[K1_C4_PATH_MAX_LENGTH];
    char entity_path[K1_C4_PATH_MAX_LENGTH];
    char link_path[K1_C4_PATH_MAX_LENGTH];
    char diagnostics_path[K1_C4_PATH_MAX_LENGTH];
    char state_path[K1_C4_PATH_MAX_LENGTH];
    char summary_path[K1_C4_PATH_MAX_LENGTH];
    char error_message[128] = "";

    if (!make_path(config_path, sizeof(config_path), directory, "config_used.ini") ||
        !make_path(command_path, sizeof(command_path), directory, "commands.csv") ||
        !make_path(event_path, sizeof(event_path), directory, "events.csv") ||
        !make_path(entity_path, sizeof(entity_path), directory, "entities.csv") ||
        !make_path(link_path, sizeof(link_path), directory, "spatial_links.csv") ||
        !make_path(diagnostics_path, sizeof(diagnostics_path), directory, "diagnostics.csv") ||
        !make_path(state_path, sizeof(state_path), directory, "state_hash.txt") ||
        !make_path(summary_path, sizeof(summary_path), directory, "summary.txt")) return 0;
    return k1_c4_config_write_canonical(config, config_path, error_message, sizeof(error_message)) &&
        write_commands(command_path, run) && write_events(event_path, run) && write_entities(entity_path, kernel) &&
        write_links(link_path, kernel) && write_diagnostics(diagnostics_path, run) &&
        write_text_outputs(state_path, summary_path, config, run);
}

int main(int argc, char **argv)
{
    const char *config_path = argc > 1 ? argv[1] : "configs/k1_spatial_links_demo.ini";
    const char *output_directory = argc > 2 ? argv[2] : "build/worlds/kernel/demo/k1_c4";
    char summary_path[K1_C4_PATH_MAX_LENGTH];
    char error_message[256] = "";
    K1C4Config config;
    K1C4Run run;
    MiniSNNWorldsKernel *kernel = NULL;

    if (argc > 3 || !make_path(summary_path, sizeof(summary_path), output_directory, "summary.txt"))
    {
        fprintf(stderr, "usage: %s [config.ini] [existing_output_directory]\n", argv[0]);
        return 1;
    }
    if (file_exists(summary_path))
    {
        fprintf(stderr, "output already exists: %s\n", summary_path);
        return 1;
    }
    if (!k1_c4_config_load_file(config_path, &config, error_message, sizeof(error_message)))
    {
        fprintf(stderr, "invalid K1-C4 config: %s\n", error_message);
        return 1;
    }
    if (!run_demo(&config, &kernel, &run) || !write_all_outputs(output_directory, &config, kernel, &run))
    {
        fprintf(stderr, "K1-C4 demo failed; no contract result accepted\n");
        minisnn_worlds_kernel_destroy(kernel);
        return 1;
    }
    printf("K1-C4 integrated spatial links demo OK: hash=0x%016" PRIX64 " events=%zu links=%" PRIu64 "\n",
        run.state_hash, run.event_count, run.diagnostics.active_spatial_links);
    minisnn_worlds_kernel_destroy(kernel);
    return 0;
}