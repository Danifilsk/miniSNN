#include "wd0_test_support.h"
#include "minisnn_worlds_kernel_snapshot.h"

#include <string.h>

typedef struct
{
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsDomain *domain;
    MiniSNNWorldsKernelEntityId a;
    MiniSNNWorldsKernelEntityId b;
} Scenario;

static int make_scenario(Scenario *s)
{
    memset(s, 0, sizeof(*s));
    s->kernel = wd0_kernel();
    if (s->kernel == NULL ||
        !wd0_make_entity(s->kernel, 0, 0, &s->a) ||
        !wd0_make_entity(s->kernel, 5000, 0, &s->b)) return 0;
    s->domain = wd0_domain_with_species(s->kernel);
    return s->domain != NULL &&
           minisnn_worlds_domain_register_organism(s->domain, s->a, 1U, 50U) == 0 &&
           minisnn_worlds_domain_register_organism(s->domain, s->b, 1U, 50U) == 0;
}

static void destroy_scenario(Scenario *s)
{
    minisnn_worlds_domain_destroy(s->domain);
    minisnn_worlds_kernel_destroy(s->kernel);
}

static int snapshot_equal(const MiniSNNWorldsKernelSnapshot *a,
                          const MiniSNNWorldsKernelSnapshot *b)
{
    return minisnn_worlds_kernel_snapshot_size(a) == minisnn_worlds_kernel_snapshot_size(b) &&
           memcmp(minisnn_worlds_kernel_snapshot_data(a),
                  minisnn_worlds_kernel_snapshot_data(b),
                  minisnn_worlds_kernel_snapshot_size(a)) == 0;
}

int main(void)
{
    Scenario failed;
    Scenario clean;
    MiniSNNWorldsDomainAction actions[2] = {0};
    MiniSNNWorldsDomainActionResult fail_results[2];
    MiniSNNWorldsDomainActionResult retry_results[2];
    MiniSNNWorldsDomainActionResult clean_results[2];
    MiniSNNWorldsKernelSnapshot *before = NULL;
    MiniSNNWorldsKernelSnapshot *after = NULL;
    MiniSNNWorldsKernelTransform pos_before;
    MiniSNNWorldsKernelTransform pos_after;
    MiniSNNWorldsDomainOrganismInfo energy_before;
    MiniSNNWorldsDomainOrganismInfo energy_after;
    uint64_t kernel_hash_before, kernel_hash_after;
    uint64_t domain_hash_before, domain_hash_after;

    WD0_CHECK(make_scenario(&failed));
    WD0_CHECK(make_scenario(&clean));
    actions[0].actor = failed.a;
    actions[0].type = MINISNN_WORLDS_DOMAIN_ACTION_MOVE;
    actions[0].move_delta.x = 1000;
    actions[1] = actions[0];
    actions[1].actor = failed.b;
    actions[1].move_delta.x = -1000;

    WD0_CHECK(minisnn_worlds_kernel_snapshot_capture(failed.kernel, &before) == 0);
    WD0_CHECK(minisnn_worlds_kernel_state_hash(failed.kernel, &kernel_hash_before) == 0);
    WD0_CHECK(minisnn_worlds_domain_state_hash(failed.domain, &domain_hash_before) == 0);
    WD0_CHECK(minisnn_worlds_kernel_entity_transform(failed.kernel, failed.a, &pos_before) == 0);
    WD0_CHECK(minisnn_worlds_domain_organism_at(failed.domain, 0U, &energy_before) == 0);
    WD0_CHECK(minisnn_worlds_kernel_pending_command_count(failed.kernel) == 0U);

    minisnn_worlds_kernel_testing_fail_next_allocation();
    WD0_CHECK(minisnn_worlds_domain_step(failed.domain, actions, 2U, fail_results) ==
              MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_FAILURE);
    WD0_CHECK(!minisnn_worlds_kernel_command_batch_active(failed.kernel));
    WD0_CHECK(minisnn_worlds_kernel_pending_command_count(failed.kernel) == 0U);
    WD0_CHECK(minisnn_worlds_domain_tick(failed.domain) == minisnn_worlds_kernel_tick(failed.kernel));
    WD0_CHECK(minisnn_worlds_kernel_snapshot_capture(failed.kernel, &after) == 0);
    WD0_CHECK(snapshot_equal(before, after));
    WD0_CHECK(minisnn_worlds_kernel_state_hash(failed.kernel, &kernel_hash_after) == 0 &&
              kernel_hash_after == kernel_hash_before);
    WD0_CHECK(minisnn_worlds_domain_state_hash(failed.domain, &domain_hash_after) == 0 &&
              domain_hash_after == domain_hash_before);
    WD0_CHECK(minisnn_worlds_kernel_entity_transform(failed.kernel, failed.a, &pos_after) == 0 &&
              pos_after.position.x == pos_before.position.x && pos_after.position.y == pos_before.position.y);
    WD0_CHECK(minisnn_worlds_domain_organism_at(failed.domain, 0U, &energy_after) == 0 &&
              energy_after.energy == energy_before.energy);
    minisnn_worlds_kernel_snapshot_destroy(after); after = NULL;

    /* Retry must be indistinguishable from an execution that never saw the failed attempt. */
    WD0_CHECK(minisnn_worlds_domain_step(failed.domain, actions, 2U, retry_results) == 0);
    actions[0].actor = clean.a;
    actions[1].actor = clean.b;
    WD0_CHECK(minisnn_worlds_domain_step(clean.domain, actions, 2U, clean_results) == 0);
    WD0_CHECK(retry_results[0].status == clean_results[0].status &&
              retry_results[1].status == clean_results[1].status);
    WD0_CHECK(minisnn_worlds_kernel_state_hash(failed.kernel, &kernel_hash_before) == 0);
    WD0_CHECK(minisnn_worlds_kernel_state_hash(clean.kernel, &kernel_hash_after) == 0 &&
              kernel_hash_before == kernel_hash_after);
    WD0_CHECK(minisnn_worlds_domain_state_hash(failed.domain, &domain_hash_before) == 0);
    WD0_CHECK(minisnn_worlds_domain_state_hash(clean.domain, &domain_hash_after) == 0 &&
              domain_hash_before == domain_hash_after);
    WD0_CHECK(minisnn_worlds_kernel_last_tick_event_count(failed.kernel) ==
              minisnn_worlds_kernel_last_tick_event_count(clean.kernel));
    WD0_CHECK(minisnn_worlds_domain_event_count(failed.domain) ==
              minisnn_worlds_domain_event_count(clean.domain));

    minisnn_worlds_kernel_snapshot_destroy(before);
    destroy_scenario(&failed);
    destroy_scenario(&clean);
    puts("WD0 Domain->Kernel failure atomicity and retry validation OK");
    return 0;
}
