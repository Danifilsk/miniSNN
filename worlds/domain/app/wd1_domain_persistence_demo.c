#include "minisnn_worlds_domain.h"
#include "wd1_domain_snapshot_file.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

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

static int create_placed(MiniSNNWorldsKernel *kernel, MiniSNNWorldsKernelScalar x,
                         MiniSNNWorldsKernelScalar y, MiniSNNWorldsKernelEntityId *out_id)
{
    MiniSNNWorldsKernelCommandId command;
    size_t count;
    if (minisnn_worlds_kernel_queue_create_entity(kernel, minisnn_worlds_kernel_tick(kernel) + 1U,
                                                   0U, entity_id(0U), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    count = minisnn_worlds_kernel_entity_count(kernel);
    return count != 0U && minisnn_worlds_kernel_entity_at(kernel, count - 1U, out_id) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           minisnn_worlds_kernel_queue_place_entity(kernel, minisnn_worlds_kernel_tick(kernel) + 1U,
                                                     0U, entity_id(0U), *out_id, at(x, y), &command) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int join_path(char *out, size_t out_size, const char *directory, const char *name)
{
    int count = snprintf(out, out_size, "%s/%s", directory, name);
    return count >= 0 && (size_t)count < out_size;
}

static int step_eat(MiniSNNWorldsDomain *domain, MiniSNNWorldsKernelEntityId organism,
                    MiniSNNWorldsKernelEntityId food)
{
    MiniSNNWorldsDomainAction action;
    MiniSNNWorldsDomainActionResult result;
    memset(&action, 0, sizeof(action));
    action.actor = organism;
    action.type = MINISNN_WORLDS_DOMAIN_ACTION_EAT;
    action.eat_target = food;
    return minisnn_worlds_domain_step(domain, &action, 1U, &result) ==
               MINISNN_WORLDS_DOMAIN_ERROR_NONE &&
           result.status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED;
}

int main(int argc, char **argv)
{
    const char *directory = argc == 2 ? argv[1] : "build/worlds/domain/results/wd1_demo";
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError kernel_error;
    MiniSNNWorldsDomainError domain_error;
    MiniSNNWorldsKernel *kernel_a = NULL;
    MiniSNNWorldsKernel *kernel_b = NULL;
    MiniSNNWorldsDomain *domain_a = NULL;
    MiniSNNWorldsDomain *domain_b = NULL;
    MiniSNNWorldsKernelSnapshot *kernel_snapshot = NULL;
    MiniSNNWorldsDomainSnapshot *domain_snapshot = NULL;
    MiniSNNWorldsDomainSnapshot *loaded_snapshot = NULL;
    MiniSNNWorldsDomainSpeciesConfig species;
    MiniSNNWorldsKernelEntityId organism;
    MiniSNNWorldsKernelEntityId food;
    uint64_t checkpoint_kernel_hash;
    uint64_t checkpoint_domain_hash;
    uint64_t final_kernel_hash_a;
    uint64_t final_kernel_hash_b;
    uint64_t final_domain_hash_a;
    uint64_t final_domain_hash_b;
    MiniSNNWorldsTick checkpoint_tick;
    char snapshot_path[512];
    char summary_path[512];
    FILE *summary = NULL;
    int success = 0;

    config.space_bounds.min_x = -10000;
    config.space_bounds.min_y = -10000;
    config.space_bounds.max_x = 10000;
    config.space_bounds.max_y = 10000;
    kernel_a = minisnn_worlds_kernel_create(&config, &kernel_error);
    if (kernel_a == NULL || kernel_error != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !create_placed(kernel_a, 0, 0, &organism) || !create_placed(kernel_a, 1000, 0, &food))
    {
        goto cleanup;
    }
    domain_a = minisnn_worlds_domain_create(kernel_a, &domain_error);
    species.species_id = 1U;
    species.max_energy = 100U;
    species.metabolism_per_tick = 1U;
    species.move_energy_cost = 3U;
    species.eat_range = 2000;
    if (domain_a == NULL || domain_error != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_add_species(domain_a, &species) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_register_organism(domain_a, organism, species.species_id, 70U) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_register_food(domain_a, food, 20U) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_kernel_snapshot_capture(kernel_a, &kernel_snapshot) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_domain_snapshot_capture(domain_a, &domain_snapshot) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        !join_path(snapshot_path, sizeof(snapshot_path), directory, "domain_snapshot_v1.bin") ||
        minisnn_worlds_domain_snapshot_save_file(snapshot_path, domain_snapshot) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_snapshot_load_file(snapshot_path, &loaded_snapshot) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(kernel_a, &checkpoint_kernel_hash) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_domain_state_hash(domain_a, &checkpoint_domain_hash) != MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        goto cleanup;
    }
    checkpoint_tick = minisnn_worlds_kernel_tick(kernel_a);
    if (!step_eat(domain_a, organism, food) ||
        minisnn_worlds_kernel_state_hash(kernel_a, &final_kernel_hash_a) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_domain_state_hash(domain_a, &final_domain_hash_a) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_kernel_create_from_snapshot(kernel_snapshot, &kernel_b) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        (domain_b = minisnn_worlds_domain_create(kernel_b, &domain_error)) == NULL ||
        domain_error != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_snapshot_restore(domain_b, loaded_snapshot) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        !step_eat(domain_b, organism, food) ||
        minisnn_worlds_kernel_state_hash(kernel_b, &final_kernel_hash_b) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_domain_state_hash(domain_b, &final_domain_hash_b) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        final_kernel_hash_a != final_kernel_hash_b || final_domain_hash_a != final_domain_hash_b ||
        !join_path(summary_path, sizeof(summary_path), directory, "wd1_summary.txt") ||
        (summary = fopen(summary_path, "wb")) == NULL)
    {
        goto cleanup;
    }
    if (fprintf(summary,
                "format=Domain Snapshot V1\ncheckpoint_tick=%" PRIu64 "\ndomain_snapshot_size=%zu\n"
                "domain_snapshot_digest=0x%016" PRIX64 "\ncheckpoint_kernel_hash=0x%016" PRIX64 "\n"
                "checkpoint_domain_hash=0x%016" PRIX64 "\nfinal_kernel_hash=0x%016" PRIX64 "\n"
                "final_domain_hash=0x%016" PRIX64 "\nround_trip=PASS\n",
                checkpoint_tick, minisnn_worlds_domain_snapshot_size(domain_snapshot),
                minisnn_worlds_domain_snapshot_digest(domain_snapshot), checkpoint_kernel_hash,
                checkpoint_domain_hash, final_kernel_hash_a, final_domain_hash_a) < 0 || fclose(summary) != 0)
    {
        summary = NULL;
        goto cleanup;
    }
    summary = NULL;
    success = 1;
cleanup:
    if (summary != NULL) fclose(summary);
    minisnn_worlds_domain_snapshot_destroy(loaded_snapshot);
    minisnn_worlds_domain_snapshot_destroy(domain_snapshot);
    minisnn_worlds_kernel_snapshot_destroy(kernel_snapshot);
    minisnn_worlds_domain_destroy(domain_b);
    minisnn_worlds_domain_destroy(domain_a);
    minisnn_worlds_kernel_destroy(kernel_b);
    minisnn_worlds_kernel_destroy(kernel_a);
    if (!success)
    {
        fputs("WD1 Domain persistence demo failed\n", stderr);
        return 1;
    }
    puts("WD1 Domain persistence demo OK");
    return 0;
}