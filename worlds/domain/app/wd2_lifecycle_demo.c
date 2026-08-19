#include "minisnn_worlds_domain.h"
#include "minisnn_worlds_kernel_snapshot.h"
#include "wd1_domain_snapshot_file.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    FILE *actions;
    FILE *events;
} DemoFiles;

static MiniSNNWorldsKernelEntityId entity_id(uint64_t value)
{
    MiniSNNWorldsKernelEntityId result = { value };
    return result;
}

static MiniSNNWorldsKernelTransform at(MiniSNNWorldsKernelScalar x,
                                       MiniSNNWorldsKernelScalar y)
{
    MiniSNNWorldsKernelTransform result;
    result.position.x = x;
    result.position.y = y;
    result.orientation = 0U;
    return result;
}

static int create_placed(MiniSNNWorldsKernel *kernel,
                         MiniSNNWorldsKernelEntityId *out_id)
{
    MiniSNNWorldsKernelCommandId command;
    size_t count;

    if (minisnn_worlds_kernel_queue_create_entity(
            kernel, minisnn_worlds_kernel_tick(kernel) + 1U, 0U, entity_id(0U),
            &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    count = minisnn_worlds_kernel_entity_count(kernel);
    return count != 0U &&
           minisnn_worlds_kernel_entity_at(kernel, count - 1U, out_id) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           minisnn_worlds_kernel_queue_place_entity(
               kernel, minisnn_worlds_kernel_tick(kernel) + 1U, 0U, entity_id(0U),
               *out_id, at(0, 0), &command) == MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int join_path(char *out, size_t out_size, const char *directory, const char *name)
{
    int count = snprintf(out, out_size, "%s/%s", directory, name);
    return count >= 0 && (size_t)count < out_size;
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

static const char *status_name(MiniSNNWorldsDomainActionStatus status)
{
    return status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED ? "APPLIED" : "REJECTED";
}

static const char *reason_name(MiniSNNWorldsDomainActionReason reason)
{
    switch (reason)
    {
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_NONE: return "NONE";
        case MINISNN_WORLDS_DOMAIN_ACTION_REASON_ACTOR_DEAD: return "ACTOR_DEAD";
        default: return "OTHER";
    }
}

static const char *event_name(MiniSNNWorldsDomainEventType event_type)
{
    switch (event_type)
    {
        case MINISNN_WORLDS_DOMAIN_EVENT_ACTION_APPLIED: return "ACTION_APPLIED";
        case MINISNN_WORLDS_DOMAIN_EVENT_ACTION_REJECTED: return "ACTION_REJECTED";
        case MINISNN_WORLDS_DOMAIN_EVENT_ENERGY_CHANGED: return "ENERGY_CHANGED";
        case MINISNN_WORLDS_DOMAIN_EVENT_ORGANISM_DIED: return "ORGANISM_DIED";
        default: return "OTHER";
    }
}

static int write_events(FILE *file, const MiniSNNWorldsDomain *domain)
{
    size_t index;
    for (index = 0U; index < minisnn_worlds_domain_event_count(domain); ++index)
    {
        MiniSNNWorldsDomainEvent event;
        if (minisnn_worlds_domain_event_at(domain, index, &event) !=
                MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
            fprintf(file, "%" PRIu64 ",%" PRIu64 ",%s,%" PRIu64 ",%" PRIu64 ",%s,%" PRIu64 ",%" PRIu64 ",%s\n",
                    event.event_id, event.tick, event_name(event.type), event.subject.value,
                    event.related.value, action_name(event.action_type), event.energy_before,
                    event.energy_after,
                    event.death_cause == MINISNN_WORLDS_DOMAIN_DEATH_CAUSE_STARVATION ?
                        "STARVATION" : "NONE") <= 0)
        {
            return 0;
        }
    }
    return 1;
}

static int step_and_log(DemoFiles *files, MiniSNNWorldsDomain *domain,
                        MiniSNNWorldsDomainAction *action,
                        MiniSNNWorldsDomainActionResult *out_result)
{
    MiniSNNWorldsDomainActionResult result;
    MiniSNNWorldsTick tick = minisnn_worlds_domain_tick(domain);
    if (minisnn_worlds_domain_step(domain, action, 1U, &result) !=
        MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        return 0;
    }
    if (fprintf(files->actions, "%" PRIu64 ",%s,%s,%s,%" PRIu64 ",%" PRIu64 "\n",
                tick, action_name(action->type), status_name(result.status),
                reason_name(result.reason), result.energy_before, result.energy_after) <= 0)
    {
        return 0;
    }
    *out_result = result;
    return 1;
}

int main(int argc, char **argv)
{
    const char *directory = argc == 2 ? argv[1] : "build/worlds/domain/results/wd2_demo";
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernel *kernel = NULL;
    MiniSNNWorldsKernel *restored_kernel = NULL;
    MiniSNNWorldsDomain *domain = NULL;
    MiniSNNWorldsDomain *restored_domain = NULL;
    MiniSNNWorldsKernelSnapshot *kernel_snapshot = NULL;
    MiniSNNWorldsDomainSnapshot *snapshot = NULL;
    MiniSNNWorldsDomainSnapshot *loaded_snapshot = NULL;
    MiniSNNWorldsKernelEntityId actor;
    MiniSNNWorldsDomainSpeciesConfig species;
    MiniSNNWorldsDomainAction action;
    MiniSNNWorldsDomainActionResult result;
    MiniSNNWorldsDomainActionResult restored_result;
    MiniSNNWorldsDomainOrganismInfo info;
    MiniSNNWorldsDomainEvent death_event;
    DemoFiles files = {0};
    FILE *summary = NULL;
    char path[512];
    uint64_t domain_hash;
    uint64_t kernel_hash;
    uint64_t restored_domain_hash;
    uint64_t restored_kernel_hash;
    size_t event_index;
    int found_death = 0;
    int success = 0;

    config.space_bounds.min_x = -10000;
    config.space_bounds.min_y = -10000;
    config.space_bounds.max_x = 10000;
    config.space_bounds.max_y = 10000;
    kernel = minisnn_worlds_kernel_create(&config, NULL);
    if (kernel == NULL || !create_placed(kernel, &actor)) goto cleanup;
    domain = minisnn_worlds_domain_create(kernel, NULL);
    species.species_id = 1U;
    species.max_energy = 10U;
    species.metabolism_per_tick = 1U;
    species.move_energy_cost = 1U;
    species.eat_range = 1000;
    if (domain == NULL ||
        minisnn_worlds_domain_add_species(domain, &species) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_register_organism(domain, actor, species.species_id, 2U) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        !join_path(path, sizeof(path), directory, "actions.csv") ||
        (files.actions = fopen(path, "wb")) == NULL ||
        fprintf(files.actions, "tick,action,status,reason,energy_before,energy_after\n") <= 0 ||
        !join_path(path, sizeof(path), directory, "events.csv") ||
        (files.events = fopen(path, "wb")) == NULL ||
        fprintf(files.events, "event_id,tick,type,subject,related,action,energy_before,energy_after,death_cause\n") <= 0)
    {
        goto cleanup;
    }
    memset(&action, 0, sizeof(action));
    action.actor = actor;
    action.type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    if (!step_and_log(&files, domain, &action, &result) ||
        !step_and_log(&files, domain, &action, &result) ||
        minisnn_worlds_domain_organism_at(domain, 0U, &info) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        info.life_state != MINISNN_WORLDS_DOMAIN_LIFE_DEAD ||
        minisnn_worlds_kernel_snapshot_capture(kernel, &kernel_snapshot) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_domain_snapshot_capture(domain, &snapshot) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        !join_path(path, sizeof(path), directory, "dead_snapshot_v2.bin") ||
        minisnn_worlds_domain_snapshot_save_file(path, snapshot) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_snapshot_load_file(path, &loaded_snapshot) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_kernel_create_from_snapshot(kernel_snapshot, &restored_kernel) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        goto cleanup;
    }
    restored_domain = minisnn_worlds_domain_create(restored_kernel, NULL);
    if (restored_domain == NULL ||
        minisnn_worlds_domain_snapshot_restore(restored_domain, loaded_snapshot) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_step(domain, &action, 1U, &result) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_step(restored_domain, &action, 1U, &restored_result) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        result.status != restored_result.status || result.reason != restored_result.reason ||
        result.energy_before != restored_result.energy_before ||
        result.energy_after != restored_result.energy_after ||
        fprintf(files.actions, "%" PRIu64 ",%s,%s,%s,%" PRIu64 ",%" PRIu64 "\n",
                minisnn_worlds_domain_tick(domain) - 1U, action_name(action.type),
                status_name(result.status), reason_name(result.reason), result.energy_before,
                result.energy_after) <= 0 ||
        minisnn_worlds_domain_state_hash(domain, &domain_hash) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(kernel, &kernel_hash) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_domain_state_hash(restored_domain, &restored_domain_hash) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(restored_kernel, &restored_kernel_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        domain_hash != restored_domain_hash || kernel_hash != restored_kernel_hash)
    {
        goto cleanup;
    }
    for (event_index = 0U; event_index < minisnn_worlds_domain_event_count(domain); ++event_index)
    {
        MiniSNNWorldsDomainEvent event;
        if (minisnn_worlds_domain_event_at(domain, event_index, &event) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE)
        {
            goto cleanup;
        }
        if (event.type == MINISNN_WORLDS_DOMAIN_EVENT_ORGANISM_DIED)
        {
            death_event = event;
            found_death = 1;
        }
    }
    if (!found_death || !write_events(files.events, domain) ||
        !join_path(path, sizeof(path), directory, "summary.txt") ||
        (summary = fopen(path, "wb")) == NULL ||
        fprintf(summary,
                "format=WD2 lifecycle V1\ndeath_tick=%" PRIu64 "\ndeath_event_id=%" PRIu64
                "\ndeath_cause=STARVATION\nevent_count=%zu\nfinal_kernel_hash=0x%016" PRIX64
                "\nfinal_domain_hash=0x%016" PRIX64 "\ndead_restore=PASS\n",
                info.death_tick, death_event.event_id, minisnn_worlds_domain_event_count(domain),
                kernel_hash, domain_hash) <= 0 || fclose(summary) != 0)
    {
        summary = NULL;
        goto cleanup;
    }
    summary = NULL;
    success = 1;
cleanup:
    if (summary != NULL) fclose(summary);
    if (files.events != NULL) fclose(files.events);
    if (files.actions != NULL) fclose(files.actions);
    minisnn_worlds_domain_snapshot_destroy(loaded_snapshot);
    minisnn_worlds_domain_snapshot_destroy(snapshot);
    minisnn_worlds_kernel_snapshot_destroy(kernel_snapshot);
    minisnn_worlds_domain_destroy(restored_domain);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(restored_kernel);
    minisnn_worlds_kernel_destroy(kernel);
    if (!success)
    {
        fputs("WD2 lifecycle demo failed\n", stderr);
        return 1;
    }
    puts("WD2 lifecycle demo OK");
    return 0;
}