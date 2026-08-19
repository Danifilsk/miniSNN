#include "wd0_test_support.h"
#include "minisnn_worlds_kernel_snapshot.h"

#include <string.h>

#define WD2_CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "WD2 atomicity test failed: %s at line %d\n", #condition, __LINE__); \
    return 1; } } while (0)

typedef struct
{
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsDomain *domain;
    MiniSNNWorldsKernelEntityId actor;
} Scenario;

static int make_scenario(Scenario *scenario)
{
    MiniSNNWorldsDomainSpeciesConfig species = wd0_species();
    memset(scenario, 0, sizeof(*scenario));
    scenario->kernel = wd0_kernel();
    species.metabolism_per_tick = 1U;
    species.move_energy_cost = 1U;
    if (scenario->kernel == NULL ||
        !wd0_make_entity(scenario->kernel, 0, 0, &scenario->actor)) return 0;
    scenario->domain = minisnn_worlds_domain_create(scenario->kernel, NULL);
    return scenario->domain != NULL &&
           minisnn_worlds_domain_add_species(scenario->domain, &species) == 0 &&
           minisnn_worlds_domain_register_organism(scenario->domain, scenario->actor, 1U, 1U) == 0;
}
static void destroy_scenario(Scenario *scenario)
{
    minisnn_worlds_domain_destroy(scenario->domain);
    minisnn_worlds_kernel_destroy(scenario->kernel);
}
static size_t deaths(const MiniSNNWorldsDomain *domain)
{
    size_t index, count = 0U;
    for (index = 0U; index < minisnn_worlds_domain_event_count(domain); ++index)
    {
        MiniSNNWorldsDomainEvent event;
        if (minisnn_worlds_domain_event_at(domain, index, &event) != 0) return SIZE_MAX;
        if (event.type == MINISNN_WORLDS_DOMAIN_EVENT_ORGANISM_DIED) ++count;
    }
    return count;
}

int main(void)
{
    Scenario failed, clean;
    MiniSNNWorldsDomainAction action = {0};
    MiniSNNWorldsDomainActionResult failed_result, retry_result, clean_result;
    MiniSNNWorldsDomainOrganismInfo before, after;
    MiniSNNWorldsKernelSnapshot *kernel_before = NULL, *kernel_after = NULL;
    uint64_t domain_before, domain_after, clean_hash;

    WD2_CHECK(make_scenario(&failed) && make_scenario(&clean));
    action.actor = failed.actor;
    action.type = MINISNN_WORLDS_DOMAIN_ACTION_MOVE;
    action.move_delta.x = 1000;
    WD2_CHECK(minisnn_worlds_kernel_snapshot_capture(failed.kernel, &kernel_before) == 0 &&
              minisnn_worlds_domain_state_hash(failed.domain, &domain_before) == 0 &&
              minisnn_worlds_domain_organism_at(failed.domain, 0U, &before) == 0);
    minisnn_worlds_kernel_testing_fail_next_allocation();
    WD2_CHECK(minisnn_worlds_domain_step(failed.domain, &action, 1U, &failed_result) ==
              MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_FAILURE &&
              !minisnn_worlds_kernel_command_batch_active(failed.kernel) &&
              minisnn_worlds_domain_organism_at(failed.domain, 0U, &after) == 0 &&
              after.life_state == MINISNN_WORLDS_DOMAIN_LIFE_ALIVE && after.energy == before.energy &&
              deaths(failed.domain) == 0U && minisnn_worlds_domain_state_hash(failed.domain, &domain_after) == 0 &&
              domain_after == domain_before && minisnn_worlds_kernel_snapshot_capture(failed.kernel, &kernel_after) == 0 &&
              minisnn_worlds_kernel_snapshot_size(kernel_before) == minisnn_worlds_kernel_snapshot_size(kernel_after) &&
              memcmp(minisnn_worlds_kernel_snapshot_data(kernel_before),
                     minisnn_worlds_kernel_snapshot_data(kernel_after),
                     minisnn_worlds_kernel_snapshot_size(kernel_before)) == 0);
    minisnn_worlds_kernel_snapshot_destroy(kernel_after); kernel_after = NULL;

    WD2_CHECK(minisnn_worlds_domain_step(failed.domain, &action, 1U, &retry_result) == 0 &&
              retry_result.status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED && deaths(failed.domain) == 1U);
    action.actor = clean.actor;
    WD2_CHECK(minisnn_worlds_domain_step(clean.domain, &action, 1U, &clean_result) == 0 &&
              clean_result.status == retry_result.status && deaths(clean.domain) == 1U &&
              minisnn_worlds_domain_state_hash(failed.domain, &domain_after) == 0 &&
              minisnn_worlds_domain_state_hash(clean.domain, &clean_hash) == 0 && domain_after == clean_hash);

    minisnn_worlds_kernel_snapshot_destroy(kernel_before);
    destroy_scenario(&failed); destroy_scenario(&clean);
    puts("WD2 starvation failure/retry atomicity OK");
    return 0;
}