#include "wd0_test_support.h"

int main(void)
{
    MiniSNNWorldsKernel *kernel = wd0_kernel();
    MiniSNNWorldsDomain *domain;
    MiniSNNWorldsKernelEntityId organism_a;
    MiniSNNWorldsKernelEntityId organism_b;
    MiniSNNWorldsKernelEntityId food;
    MiniSNNWorldsKernelEntityId blocker;
    MiniSNNWorldsDomainAction actions[2] = {0};
    MiniSNNWorldsDomainActionResult results[2];
    MiniSNNWorldsDomainOrganismInfo info;

    WD0_CHECK(kernel != NULL);
    WD0_CHECK(wd0_make_entity(kernel, 0, 0, &organism_a));
    WD0_CHECK(wd0_make_entity(kernel, 1000, 0, &organism_b));
    WD0_CHECK(wd0_make_entity(kernel, 1000, 1000, &food));
    WD0_CHECK(wd0_make_entity(kernel, 2000, 0, &blocker));
    WD0_CHECK(wd0_set_blocker(kernel, organism_a, 100, 100, 1U, 2U));
    WD0_CHECK(wd0_set_blocker(kernel, blocker, 100, 100, 2U, 1U));
    domain = wd0_domain_with_species(kernel);
    WD0_CHECK(domain != NULL);
    WD0_CHECK(minisnn_worlds_domain_register_organism(domain, organism_a, 1U, 50U) == 0);
    WD0_CHECK(minisnn_worlds_domain_register_organism(domain, organism_b, 1U, 50U) == 0);
    WD0_CHECK(minisnn_worlds_domain_register_food(domain, food, 40U) == 0);

    actions[0].actor = organism_a;
    actions[0].type = MINISNN_WORLDS_DOMAIN_ACTION_MOVE;
    actions[0].move_delta.x = 2000;
    actions[0].move_delta.y = 0;
    WD0_CHECK(minisnn_worlds_domain_step(domain, actions, 1U, results) == 0);
    WD0_CHECK(results[0].status == MINISNN_WORLDS_DOMAIN_ACTION_REJECTED &&
              results[0].reason == MINISNN_WORLDS_DOMAIN_ACTION_REASON_KERNEL_REJECTED);
    WD0_CHECK(minisnn_worlds_domain_organism_at(domain, 0U, &info) == 0 && info.energy == 49U);

    actions[0].actor = organism_b;
    actions[0].type = MINISNN_WORLDS_DOMAIN_ACTION_EAT;
    actions[0].eat_target = food;
    actions[1] = actions[0];
    actions[1].actor = organism_a;
    WD0_CHECK(minisnn_worlds_domain_step(domain, actions, 2U, results) == 0);
    WD0_CHECK(results[0].status == MINISNN_WORLDS_DOMAIN_ACTION_REJECTED &&
              results[0].reason == MINISNN_WORLDS_DOMAIN_ACTION_REASON_TARGET_NOT_AVAILABLE);
    WD0_CHECK(results[1].status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED);
    WD0_CHECK(minisnn_worlds_domain_food_count(domain) == 0U);
    WD0_CHECK(minisnn_worlds_domain_organism_at(domain, 0U, &info) == 0 && info.energy == 88U);

    actions[0].actor = organism_a;
    actions[0].type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    actions[1] = actions[0];
    WD0_CHECK(minisnn_worlds_domain_step(domain, actions, 2U, results) == 0);
    WD0_CHECK(results[0].status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED);
    WD0_CHECK(results[1].status == MINISNN_WORLDS_DOMAIN_ACTION_REJECTED &&
              results[1].reason == MINISNN_WORLDS_DOMAIN_ACTION_REASON_DUPLICATE_ACTOR);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    puts("WD0 WAIT, MOVE, EAT, competition and metabolism validation OK");
    return 0;
}
