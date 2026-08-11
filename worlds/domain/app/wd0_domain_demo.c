#include "minisnn_worlds_domain.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    FILE *actions;
    FILE *perceptions;
    FILE *kernel_hashes;
    FILE *domain_hashes;
} DemoLogs;

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
    result.orientation = UINT32_C(0);
    return result;
}

static const char *action_name(MiniSNNWorldsDomainActionType value)
{
    switch (value)
    {
        case MINISNN_WORLDS_DOMAIN_ACTION_WAIT: return "WAIT";
        case MINISNN_WORLDS_DOMAIN_ACTION_MOVE: return "MOVE";
        case MINISNN_WORLDS_DOMAIN_ACTION_EAT: return "EAT";
        default: return "INVALID";
    }
}

static const char *status_name(MiniSNNWorldsDomainActionStatus value)
{
    return value == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED ? "APPLIED" : "REJECTED";
}

static const char *reason_name(MiniSNNWorldsDomainActionReason value)
{
    static const char *const names[] = {
        "NONE", "ACTOR_NOT_ORGANISM", "TARGET_NOT_FOOD", "TARGET_NOT_AVAILABLE",
        "TARGET_OUT_OF_RANGE", "INSUFFICIENT_ENERGY", "INVALID_ACTION",
        "KERNEL_REJECTED", "DUPLICATE_ACTOR"
    };
    return value <= MINISNN_WORLDS_DOMAIN_ACTION_REASON_DUPLICATE_ACTOR ? names[value] : "UNKNOWN";
}

static const char *event_name(MiniSNNWorldsDomainEventType value)
{
    static const char *const names[] = {
        "UNKNOWN", "ACTION_APPLIED", "ACTION_REJECTED", "FOOD_CONSUMED", "ENERGY_CHANGED"
    };
    return value <= MINISNN_WORLDS_DOMAIN_EVENT_ENERGY_CHANGED ? names[value] : "UNKNOWN";
}

static int path_join(char *out, size_t size, const char *directory, const char *name)
{
    int count = snprintf(out, size, "%s/%s", directory, name);
    return count >= 0 && (size_t)count < size;
}

static int create_placed(MiniSNNWorldsKernel *kernel, MiniSNNWorldsKernelScalar x,
                         MiniSNNWorldsKernelScalar y, MiniSNNWorldsKernelEntityId *out_id)
{
    MiniSNNWorldsKernelCommandId command;
    size_t count;
    MiniSNNWorldsTick tick = minisnn_worlds_kernel_tick(kernel);

    if (minisnn_worlds_kernel_queue_create_entity(kernel, tick + 1U, 0U, entity_id(0U), &command) != 0 ||
        minisnn_worlds_kernel_step(kernel) != 0)
    {
        return 0;
    }
    count = minisnn_worlds_kernel_entity_count(kernel);
    if (count == 0U || minisnn_worlds_kernel_entity_at(kernel, count - 1U, out_id) != 0 ||
        minisnn_worlds_kernel_queue_place_entity(kernel, minisnn_worlds_kernel_tick(kernel) + 1U,
            0U, entity_id(0U), *out_id, at(x, y), &command) != 0 ||
        minisnn_worlds_kernel_step(kernel) != 0)
    {
        return 0;
    }
    return 1;
}

static int set_blocker(MiniSNNWorldsKernel *kernel, MiniSNNWorldsKernelEntityId id,
                       MiniSNNWorldsKernelScalar half_x, MiniSNNWorldsKernelScalar half_y,
                       uint32_t category, uint32_t mask)
{
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelOccupancy occupancy;
    occupancy.half_extent_x = half_x;
    occupancy.half_extent_y = half_y;
    occupancy.category_bits = category;
    occupancy.blocking_mask = mask;
    return minisnn_worlds_kernel_queue_set_occupancy(kernel, minisnn_worlds_kernel_tick(kernel) + 1U,
               0U, entity_id(0U), id, occupancy, &command) == 0 &&
           minisnn_worlds_kernel_step(kernel) == 0;
}

static int log_hashes(DemoLogs *logs, const MiniSNNWorldsDomain *domain,
                      const MiniSNNWorldsKernel *kernel)
{
    uint64_t domain_hash;
    uint64_t kernel_hash;
    return minisnn_worlds_domain_state_hash(domain, &domain_hash) == 0 &&
           minisnn_worlds_kernel_state_hash(kernel, &kernel_hash) == 0 &&
           fprintf(logs->domain_hashes, "%" PRIu64 ",0x%016" PRIX64 "\n",
                   minisnn_worlds_domain_tick(domain), domain_hash) > 0 &&
           fprintf(logs->kernel_hashes, "%" PRIu64 ",0x%016" PRIX64 "\n",
                   minisnn_worlds_domain_tick(domain), kernel_hash) > 0;
}

static int log_perception(DemoLogs *logs, const MiniSNNWorldsDomain *domain,
                          MiniSNNWorldsKernelEntityId organism)
{
    MiniSNNWorldsDomainPerception perception;
    return minisnn_worlds_domain_perceive(domain, organism, &perception) == 0 &&
           fprintf(logs->perceptions,
                   "%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%d,%" PRIu64 ",%" PRId64 ",%" PRId64 ",%" PRIu64 "\n",
                   minisnn_worlds_domain_tick(domain), organism.value, perception.self_energy,
                   perception.self_hunger, perception.nearest_food_present ? 1 : 0,
                   perception.nearest_food_entity.value, perception.nearest_food_delta_x,
                   perception.nearest_food_delta_y, perception.nearest_food_distance) > 0;
}

static int step_and_log(DemoLogs *logs, MiniSNNWorldsDomain *domain,
                        MiniSNNWorldsKernel *kernel, const MiniSNNWorldsDomainAction *actions,
                        size_t action_count)
{
    MiniSNNWorldsDomainActionResult results[2];
    MiniSNNWorldsTick tick = minisnn_worlds_domain_tick(domain);
    size_t index;
    if (action_count > 2U || minisnn_worlds_domain_step(domain, actions, action_count, results) != 0)
    {
        return 0;
    }
    for (index = 0U; index < action_count; ++index)
    {
        if (fprintf(logs->actions,
                    "%" PRIu64 ",%" PRIu64 ",%s,%" PRIu64 ",%" PRId64 ",%" PRId64 ",%s,%s,%" PRIu64 ",%" PRIu64 "\n",
                    tick, actions[index].actor.value, action_name(actions[index].type),
                    actions[index].eat_target.value, actions[index].move_delta.x,
                    actions[index].move_delta.y, status_name(results[index].status),
                    reason_name(results[index].reason), results[index].energy_before,
                    results[index].energy_after) <= 0)
        {
            return 0;
        }
    }
    return log_hashes(logs, domain, kernel);
}

static int write_entities(const char *directory, const MiniSNNWorldsDomain *domain)
{
    char path[512];
    FILE *file;
    size_t index;
    if (!path_join(path, sizeof(path), directory, "domain_entities.csv") || (file = fopen(path, "w")) == NULL)
    {
        return 0;
    }
    if (fprintf(file, "kind,entity_id,species_id,energy,max_energy,hunger,nutrition\n") <= 0) goto fail;
    for (index = 0U; index < minisnn_worlds_domain_organism_count(domain); ++index)
    {
        MiniSNNWorldsDomainOrganismInfo info;
        if (minisnn_worlds_domain_organism_at(domain, index, &info) != 0 ||
            fprintf(file, "ORGANISM,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",NA\n",
                    info.entity_id.value, info.species_id, info.energy, info.max_energy, info.hunger) <= 0) goto fail;
    }
    for (index = 0U; index < minisnn_worlds_domain_food_count(domain); ++index)
    {
        MiniSNNWorldsDomainFoodInfo info;
        if (minisnn_worlds_domain_food_at(domain, index, &info) != 0 ||
            fprintf(file, "FOOD,%" PRIu64 ",NA,NA,NA,NA,%" PRIu64 "\n", info.entity_id.value, info.nutrition) <= 0) goto fail;
    }
    fclose(file);
    return 1;
fail:
    fclose(file);
    return 0;
}

static int write_events(const char *directory, const MiniSNNWorldsDomain *domain)
{
    char path[512];
    FILE *file;
    size_t index;
    if (!path_join(path, sizeof(path), directory, "domain_events.csv") || (file = fopen(path, "w")) == NULL)
    {
        return 0;
    }
    if (fprintf(file, "event_id,tick,type,subject,related,action,reason,energy_before,energy_after\n") <= 0) goto fail;
    for (index = 0U; index < minisnn_worlds_domain_event_count(domain); ++index)
    {
        MiniSNNWorldsDomainEvent event;
        if (minisnn_worlds_domain_event_at(domain, index, &event) != 0 ||
            fprintf(file, "%" PRIu64 ",%" PRIu64 ",%s,%" PRIu64 ",%" PRIu64 ",%s,%s,%" PRIu64 ",%" PRIu64 "\n",
                    event.event_id, event.tick, event_name(event.type), event.subject.value,
                    event.related.value, action_name(event.action_type), reason_name(event.reason),
                    event.energy_before, event.energy_after) <= 0) goto fail;
    }
    fclose(file);
    return 1;
fail:
    fclose(file);
    return 0;
}

int main(int argc, char **argv)
{
    const char *directory = argc == 2 ? argv[1] : "build/worlds/domain/results/wd0_demo";
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError kernel_error;
    MiniSNNWorldsDomainError domain_error;
    MiniSNNWorldsKernel *kernel = NULL;
    MiniSNNWorldsDomain *domain = NULL;
    MiniSNNWorldsKernelEntityId a, b, food_x, food_y, food_z, obstacle;
    MiniSNNWorldsDomainSpeciesConfig species;
    MiniSNNWorldsDomainAction actions[2];
    MiniSNNWorldsDomainDiagnostics diagnostics;
    DemoLogs logs = {0};
    char path[512];
    FILE *summary = NULL;
    uint64_t kernel_hash, domain_hash;
    int success = 0;

    config.space_bounds.min_x = -10000;
    config.space_bounds.min_y = -10000;
    config.space_bounds.max_x = 10000;
    config.space_bounds.max_y = 10000;
    kernel = minisnn_worlds_kernel_create(&config, &kernel_error);
    if (kernel == NULL || kernel_error != 0 ||
        !create_placed(kernel, 0, 0, &a) || !create_placed(kernel, 5000, 0, &b) ||
        !create_placed(kernel, 2000, 0, &food_x) || !create_placed(kernel, 3000, 0, &food_y) ||
        !create_placed(kernel, -2000, 0, &food_z) || !create_placed(kernel, 3000, 0, &obstacle) ||
        !set_blocker(kernel, a, 100, 100, 1U, 2U) || !set_blocker(kernel, obstacle, 100, 100, 2U, 1U)) goto cleanup;
    domain = minisnn_worlds_domain_create(kernel, &domain_error);
    species.species_id = 1U; species.max_energy = 100U; species.metabolism_per_tick = 1U;
    species.move_energy_cost = 3U; species.eat_range = 2000;
    if (domain == NULL || domain_error != 0 || minisnn_worlds_domain_add_species(domain, &species) != 0 ||
        minisnn_worlds_domain_register_organism(domain, a, 1U, 70U) != 0 ||
        minisnn_worlds_domain_register_organism(domain, b, 1U, 70U) != 0 ||
        minisnn_worlds_domain_register_food(domain, food_x, 25U) != 0 ||
        minisnn_worlds_domain_register_food(domain, food_y, 20U) != 0 ||
        minisnn_worlds_domain_register_food(domain, food_z, 10U) != 0) goto cleanup;
    if (!path_join(path, sizeof(path), directory, "domain_actions.csv") || (logs.actions = fopen(path, "w")) == NULL ||
        fprintf(logs.actions, "tick,actor,action,target,delta_x,delta_y,status,reason,energy_before,energy_after\n") <= 0 ||
        !path_join(path, sizeof(path), directory, "perceptions.csv") || (logs.perceptions = fopen(path, "w")) == NULL ||
        fprintf(logs.perceptions, "tick,organism,energy,hunger,nearest_present,nearest_food,delta_x,delta_y,distance\n") <= 0 ||
        !path_join(path, sizeof(path), directory, "kernel_hashes.csv") || (logs.kernel_hashes = fopen(path, "w")) == NULL ||
        fprintf(logs.kernel_hashes, "tick,kernel_hash\n") <= 0 ||
        !path_join(path, sizeof(path), directory, "domain_hashes.csv") || (logs.domain_hashes = fopen(path, "w")) == NULL ||
        fprintf(logs.domain_hashes, "tick,domain_hash\n") <= 0 ||
        !log_perception(&logs, domain, a) || !log_perception(&logs, domain, b) || !log_hashes(&logs, domain, kernel)) goto cleanup;

    memset(actions, 0, sizeof(actions));
    actions[0].actor = a; actions[0].type = MINISNN_WORLDS_DOMAIN_ACTION_MOVE; actions[0].move_delta.x = 1000;
    actions[1].actor = b; actions[1].type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    if (!step_and_log(&logs, domain, kernel, actions, 2U)) goto cleanup;
    memset(actions, 0, sizeof(actions));
    actions[0].actor = a; actions[0].type = MINISNN_WORLDS_DOMAIN_ACTION_MOVE; actions[0].move_delta.x = 2000;
    actions[1].actor = b; actions[1].type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    if (!step_and_log(&logs, domain, kernel, actions, 2U)) goto cleanup;
    memset(actions, 0, sizeof(actions));
    actions[0].actor = a; actions[0].type = MINISNN_WORLDS_DOMAIN_ACTION_EAT; actions[0].eat_target = food_x;
    actions[1].actor = b; actions[1].type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    if (!step_and_log(&logs, domain, kernel, actions, 2U)) goto cleanup;
    memset(actions, 0, sizeof(actions));
    actions[0].actor = b; actions[0].type = MINISNN_WORLDS_DOMAIN_ACTION_EAT; actions[0].eat_target = food_x;
    actions[1].actor = a; actions[1].type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    if (!step_and_log(&logs, domain, kernel, actions, 2U)) goto cleanup;
    memset(actions, 0, sizeof(actions));
    actions[0].actor = b; actions[0].type = MINISNN_WORLDS_DOMAIN_ACTION_EAT; actions[0].eat_target = food_y;
    actions[1].actor = a; actions[1].type = MINISNN_WORLDS_DOMAIN_ACTION_EAT; actions[1].eat_target = food_y;
    if (!step_and_log(&logs, domain, kernel, actions, 2U)) goto cleanup;
    memset(actions, 0, sizeof(actions));
    actions[0].actor = a; actions[0].type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    actions[1].actor = b; actions[1].type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    if (!step_and_log(&logs, domain, kernel, actions, 2U)) goto cleanup;

    if (!write_entities(directory, domain) || !write_events(directory, domain) ||
        minisnn_worlds_domain_get_diagnostics(domain, &diagnostics) != 0 ||
        minisnn_worlds_kernel_state_hash(kernel, &kernel_hash) != 0 ||
        minisnn_worlds_domain_state_hash(domain, &domain_hash) != 0 ||
        !path_join(path, sizeof(path), directory, "summary.txt") || (summary = fopen(path, "w")) == NULL ||
        fprintf(summary, "organisms=2\nfoods_initial=3\nfoods_remaining=%zu\ntotal_actions=%" PRIu64 "\nactions_applied=%" PRIu64 "\nactions_rejected=%" PRIu64 "\nfood_consumed=%" PRIu64 "\nenergy_gained=%" PRIu64 "\nenergy_spent=%" PRIu64 "\nfinal_tick=%" PRIu64 "\nfinal_kernel_hash=0x%016" PRIX64 "\nfinal_domain_hash=0x%016" PRIX64 "\n",
                minisnn_worlds_domain_food_count(domain), diagnostics.total_actions, diagnostics.total_actions_applied,
                diagnostics.total_actions_rejected, diagnostics.total_food_consumed, diagnostics.total_energy_gained,
                diagnostics.total_energy_spent, minisnn_worlds_domain_tick(domain), kernel_hash, domain_hash) <= 0) goto cleanup;
    success = 1;
cleanup:
    if (summary != NULL) fclose(summary);
    if (logs.actions != NULL) fclose(logs.actions);
    if (logs.perceptions != NULL) fclose(logs.perceptions);
    if (logs.kernel_hashes != NULL) fclose(logs.kernel_hashes);
    if (logs.domain_hashes != NULL) fclose(logs.domain_hashes);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    if (!success) { fputs("WD0 demo failed\n", stderr); return 1; }
    puts("WD0 deterministic Domain demo OK");
    return 0;
}