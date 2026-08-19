#include "wd0_test_support.h"

#include <string.h>

#define WD2_CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "WD2 lifecycle test failed: %s at line %d\n", #condition, __LINE__); \
    return 1; } } while (0)

static int make_entity_and_domain(MiniSNNWorldsKernel **out_kernel,
                                  MiniSNNWorldsDomain **out_domain,
                                  MiniSNNWorldsKernelEntityId *out_actor,
                                  MiniSNNWorldsDomainEnergy energy,
                                  MiniSNNWorldsDomainEnergy metabolism)
{
    MiniSNNWorldsKernel *kernel = wd0_kernel();
    MiniSNNWorldsDomain *domain;
    MiniSNNWorldsDomainSpeciesConfig species = wd0_species();

    if (kernel == NULL || !wd0_make_entity(kernel, 0, 0, out_actor) ||
        (domain = wd0_domain_with_species(kernel)) == NULL)
    {
        minisnn_worlds_domain_destroy(domain);
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    species.metabolism_per_tick = metabolism;
    if (minisnn_worlds_domain_add_species(domain, &species) ==
        MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        /* wd0_domain_with_species already registered id 1. */
        minisnn_worlds_domain_destroy(domain);
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    /* Replace the standard helper so a custom metabolism is actually used. */
    minisnn_worlds_domain_destroy(domain);
    domain = minisnn_worlds_domain_create(kernel, NULL);
    if (domain == NULL || minisnn_worlds_domain_add_species(domain, &species) !=
        MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_register_organism(domain, *out_actor, species.species_id, energy) !=
        MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        minisnn_worlds_domain_destroy(domain);
        minisnn_worlds_kernel_destroy(kernel);
        return 0;
    }
    *out_kernel = kernel;
    *out_domain = domain;
    return 1;
}

static size_t death_event_count(const MiniSNNWorldsDomain *domain,
                                MiniSNNWorldsKernelEntityId actor,
                                MiniSNNWorldsDomainEvent *out_event)
{
    size_t index;
    size_t count = 0U;
    for (index = 0U; index < minisnn_worlds_domain_event_count(domain); ++index)
    {
        MiniSNNWorldsDomainEvent event;
        if (minisnn_worlds_domain_event_at(domain, index, &event) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE)
        {
            return SIZE_MAX;
        }
        if (event.type == MINISNN_WORLDS_DOMAIN_EVENT_ORGANISM_DIED &&
            event.subject.value == actor.value)
        {
            ++count;
            if (out_event != NULL) *out_event = event;
        }
    }
    return count;
}

static int test_starvation_dead_actions_and_perception(void)
{
    MiniSNNWorldsKernel *kernel = NULL;
    MiniSNNWorldsDomain *domain = NULL;
    MiniSNNWorldsKernelEntityId actor;
    MiniSNNWorldsDomainAction action;
    MiniSNNWorldsDomainActionResult result;
    MiniSNNWorldsDomainOrganismInfo info;
    MiniSNNWorldsDomainPerception perception;
    MiniSNNWorldsDomainEvent death;
    MiniSNNWorldsTick death_tick;

    WD2_CHECK(make_entity_and_domain(&kernel, &domain, &actor, 1U, 1U));
    memset(&action, 0, sizeof(action));
    action.actor = actor;
    action.type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    WD2_CHECK(minisnn_worlds_domain_step(domain, &action, 1U, &result) ==
              MINISNN_WORLDS_DOMAIN_ERROR_NONE);
    WD2_CHECK(result.status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED);
    WD2_CHECK(minisnn_worlds_domain_organism_at(domain, 0U, &info) ==
              MINISNN_WORLDS_DOMAIN_ERROR_NONE);
    WD2_CHECK(info.energy == 0U && info.life_state == MINISNN_WORLDS_DOMAIN_LIFE_DEAD &&
              info.death_cause == MINISNN_WORLDS_DOMAIN_DEATH_CAUSE_STARVATION &&
              info.death_tick == minisnn_worlds_domain_tick(domain));
    WD2_CHECK(death_event_count(domain, actor, &death) == 1U);
    WD2_CHECK(death.tick == info.death_tick && death.death_cause == info.death_cause &&
              death.action_type == MINISNN_WORLDS_DOMAIN_ACTION_WAIT);
    death_tick = info.death_tick;
    WD2_CHECK(minisnn_worlds_domain_perceive(domain, actor, &perception) ==
              MINISNN_WORLDS_DOMAIN_ERROR_ACTOR_DEAD);

    action.type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    WD2_CHECK(minisnn_worlds_domain_step(domain, &action, 1U, &result) ==
              MINISNN_WORLDS_DOMAIN_ERROR_NONE);
    WD2_CHECK(result.status == MINISNN_WORLDS_DOMAIN_ACTION_REJECTED &&
              result.reason == MINISNN_WORLDS_DOMAIN_ACTION_REASON_ACTOR_DEAD);
    action.type = MINISNN_WORLDS_DOMAIN_ACTION_MOVE;
    action.move_delta.x = 1000;
    WD2_CHECK(minisnn_worlds_domain_step(domain, &action, 1U, &result) ==
              MINISNN_WORLDS_DOMAIN_ERROR_NONE);
    WD2_CHECK(result.status == MINISNN_WORLDS_DOMAIN_ACTION_REJECTED &&
              result.reason == MINISNN_WORLDS_DOMAIN_ACTION_REASON_ACTOR_DEAD);
    action.type = MINISNN_WORLDS_DOMAIN_ACTION_EAT;
    WD2_CHECK(minisnn_worlds_domain_step(domain, &action, 1U, &result) ==
              MINISNN_WORLDS_DOMAIN_ERROR_NONE);
    WD2_CHECK(result.status == MINISNN_WORLDS_DOMAIN_ACTION_REJECTED &&
              result.reason == MINISNN_WORLDS_DOMAIN_ACTION_REASON_ACTOR_DEAD);
    WD2_CHECK(minisnn_worlds_domain_organism_at(domain, 0U, &info) ==
              MINISNN_WORLDS_DOMAIN_ERROR_NONE && info.energy == 0U &&
              info.death_tick == death_tick && death_event_count(domain, actor, NULL) == 1U);

    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    return 0;
}

static int test_food_saves_before_metabolism(void)
{
    MiniSNNWorldsKernel *kernel = wd0_kernel();
    MiniSNNWorldsDomain *domain;
    MiniSNNWorldsDomainSpeciesConfig species = wd0_species();
    MiniSNNWorldsKernelEntityId actor;
    MiniSNNWorldsKernelEntityId food;
    MiniSNNWorldsDomainAction action;
    MiniSNNWorldsDomainActionResult result;
    MiniSNNWorldsDomainOrganismInfo info;

    WD2_CHECK(kernel != NULL && wd0_make_entity(kernel, 0, 0, &actor) &&
              wd0_make_entity(kernel, 0, 0, &food));
    species.metabolism_per_tick = 1U;
    domain = minisnn_worlds_domain_create(kernel, NULL);
    WD2_CHECK(domain != NULL && minisnn_worlds_domain_add_species(domain, &species) == 0 &&
              minisnn_worlds_domain_register_organism(domain, actor, species.species_id, 1U) == 0 &&
              minisnn_worlds_domain_register_food(domain, food, 10U) == 0);
    memset(&action, 0, sizeof(action));
    action.actor = actor;
    action.type = MINISNN_WORLDS_DOMAIN_ACTION_EAT;
    action.eat_target = food;
    WD2_CHECK(minisnn_worlds_domain_step(domain, &action, 1U, &result) == 0 &&
              result.status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED);
    WD2_CHECK(minisnn_worlds_domain_organism_at(domain, 0U, &info) == 0 &&
              info.energy == 10U && info.life_state == MINISNN_WORLDS_DOMAIN_LIFE_ALIVE &&
              death_event_count(domain, actor, NULL) == 0U);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    return 0;
}

static int test_multiple_organisms_canonical_deaths(void)
{
    MiniSNNWorldsKernel *kernel = wd0_kernel();
    MiniSNNWorldsDomain *domain;
    MiniSNNWorldsDomainSpeciesConfig species = wd0_species();
    MiniSNNWorldsKernelEntityId a, b;
    MiniSNNWorldsDomainAction actions[2];
    MiniSNNWorldsDomainActionResult results[2];
    MiniSNNWorldsDomainEvent first, second;
    MiniSNNWorldsDomainOrganismInfo info;
    size_t index, found = 0U;

    WD2_CHECK(kernel != NULL && wd0_make_entity(kernel, 0, 0, &a) &&
              wd0_make_entity(kernel, 1000, 0, &b));
    species.metabolism_per_tick = 1U;
    domain = minisnn_worlds_domain_create(kernel, NULL);
    WD2_CHECK(domain != NULL && minisnn_worlds_domain_add_species(domain, &species) == 0 &&
              minisnn_worlds_domain_register_organism(domain, a, 1U, 1U) == 0 &&
              minisnn_worlds_domain_register_organism(domain, b, 1U, 2U) == 0);
    memset(actions, 0, sizeof(actions));
    actions[0].actor = b; actions[0].type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    actions[1].actor = a; actions[1].type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    WD2_CHECK(minisnn_worlds_domain_step(domain, actions, 2U, results) == 0);
    WD2_CHECK(minisnn_worlds_domain_organism_at(domain, 0U, &info) == 0 &&
              info.entity_id.value == a.value && info.life_state == MINISNN_WORLDS_DOMAIN_LIFE_DEAD);
    WD2_CHECK(minisnn_worlds_domain_organism_at(domain, 1U, &info) == 0 &&
              info.entity_id.value == b.value && info.life_state == MINISNN_WORLDS_DOMAIN_LIFE_ALIVE &&
              info.energy == 1U);
    memset(&first, 0, sizeof(first)); memset(&second, 0, sizeof(second));
    for (index = 0U; index < minisnn_worlds_domain_event_count(domain); ++index)
    {
        MiniSNNWorldsDomainEvent event;
        WD2_CHECK(minisnn_worlds_domain_event_at(domain, index, &event) == 0);
        if (event.type == MINISNN_WORLDS_DOMAIN_EVENT_ORGANISM_DIED)
        {
            if (found++ == 0U) first = event; else second = event;
        }
    }
    WD2_CHECK(found == 1U && first.subject.value == a.value);
    WD2_CHECK(minisnn_worlds_domain_step(domain, NULL, 0U, NULL) == 0);
    WD2_CHECK(death_event_count(domain, b, &second) == 1U && second.subject.value == b.value &&
              death_event_count(domain, a, NULL) == 1U);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    return 0;
}

static int test_simultaneous_deaths_are_canonical(void)
{
    MiniSNNWorldsKernel *kernel = wd0_kernel();
    MiniSNNWorldsDomain *domain;
    MiniSNNWorldsDomainSpeciesConfig species = wd0_species();
    MiniSNNWorldsKernelEntityId a, b;
    MiniSNNWorldsDomainAction actions[2];
    MiniSNNWorldsDomainActionResult results[2];
    MiniSNNWorldsDomainEvent deaths[2];
    size_t index;
    size_t count = 0U;

    WD2_CHECK(kernel != NULL && wd0_make_entity(kernel, 0, 0, &a) &&
              wd0_make_entity(kernel, 1000, 0, &b));
    species.metabolism_per_tick = 1U;
    domain = minisnn_worlds_domain_create(kernel, NULL);
    WD2_CHECK(domain != NULL && minisnn_worlds_domain_add_species(domain, &species) == 0 &&
              minisnn_worlds_domain_register_organism(domain, a, 1U, 1U) == 0 &&
              minisnn_worlds_domain_register_organism(domain, b, 1U, 1U) == 0);
    memset(actions, 0, sizeof(actions));
    actions[0].actor = b;
    actions[0].type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    actions[1].actor = a;
    actions[1].type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    WD2_CHECK(minisnn_worlds_domain_step(domain, actions, 2U, results) == 0);
    for (index = 0U; index < minisnn_worlds_domain_event_count(domain); ++index)
    {
        MiniSNNWorldsDomainEvent event;
        WD2_CHECK(minisnn_worlds_domain_event_at(domain, index, &event) == 0);
        if (event.type == MINISNN_WORLDS_DOMAIN_EVENT_ORGANISM_DIED)
        {
            WD2_CHECK(count < 2U);
            deaths[count++] = event;
        }
    }
    WD2_CHECK(count == 2U && deaths[0].subject.value == a.value &&
              deaths[1].subject.value == b.value &&
              deaths[0].tick == deaths[1].tick &&
              deaths[0].event_id < deaths[1].event_id);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    return 0;
}
int main(void)
{
    WD2_CHECK(test_starvation_dead_actions_and_perception() == 0);
    WD2_CHECK(test_food_saves_before_metabolism() == 0);
    WD2_CHECK(test_multiple_organisms_canonical_deaths() == 0);
    WD2_CHECK(test_simultaneous_deaths_are_canonical() == 0);
    puts("WD2 lifecycle, starvation, dead actions and ordering OK");
    return 0;
}