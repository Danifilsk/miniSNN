#include "wd0_test_support.h"
#include "wd0_test_controller.h"

#include <inttypes.h>

#ifndef ORGANISM_COUNT
#define ORGANISM_COUNT 256U
#endif
#ifndef FOOD_COUNT
#define FOOD_COUNT 512U
#endif
#ifndef TICK_COUNT
#define TICK_COUNT 200U
#endif

static int create_all(MiniSNNWorldsKernel *kernel, size_t count)
{
    MiniSNNWorldsKernelCommandId command;
    size_t index;
    for (index = 0U; index < count; ++index)
    {
        if (minisnn_worlds_kernel_queue_create_entity(kernel,
                minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
                wd0_id(0U), &command) != 0)
        {
            return 0;
        }
    }
    return minisnn_worlds_kernel_step(kernel) == 0;
}

static int place_all(MiniSNNWorldsKernel *kernel, size_t count)
{
    MiniSNNWorldsKernelCommandId command;
    size_t index;
    for (index = 0U; index < count; ++index)
    {
        MiniSNNWorldsKernelEntityId id;
        if (minisnn_worlds_kernel_entity_at(kernel, index, &id) != 0 ||
            minisnn_worlds_kernel_queue_place_entity(kernel,
                minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
                wd0_id(0U), id,
                wd0_at((MiniSNNWorldsKernelScalar)(index % ORGANISM_COUNT) * 1000,
                       (MiniSNNWorldsKernelScalar)(index / ORGANISM_COUNT) * 1000),
                &command) != 0)
        {
            return 0;
        }
    }
    return minisnn_worlds_kernel_step(kernel) == 0;
}

int main(void)
{
    MiniSNNWorldsKernel *kernel = wd0_kernel();
    MiniSNNWorldsDomain *domain;
    MiniSNNWorldsDomainAction actions[ORGANISM_COUNT];
    MiniSNNWorldsDomainActionResult results[ORGANISM_COUNT];
    uint64_t hash;
    size_t tick;
    size_t index;
    size_t occupancy_rejections = 0U;
    size_t competition_attempts = 0U;
    size_t competition_rejections = 0U;

    WD0_CHECK(kernel != NULL);
    WD0_CHECK(create_all(kernel, ORGANISM_COUNT + FOOD_COUNT));
    WD0_CHECK(place_all(kernel, ORGANISM_COUNT + FOOD_COUNT));
    WD0_CHECK(ORGANISM_COUNT >= 4U && FOOD_COUNT >= 1U);
    {
        MiniSNNWorldsKernelEntityId blocker_a;
        MiniSNNWorldsKernelEntityId blocker_b;
        WD0_CHECK(minisnn_worlds_kernel_entity_at(kernel, 2U, &blocker_a) == 0);
        WD0_CHECK(minisnn_worlds_kernel_entity_at(kernel, 3U, &blocker_b) == 0);
        WD0_CHECK(wd0_set_blocker(kernel, blocker_a, 400, 400, 1U, 1U));
        WD0_CHECK(wd0_set_blocker(kernel, blocker_b, 400, 400, 1U, 1U));
    }
    domain = wd0_domain_with_species(kernel);
    WD0_CHECK(domain != NULL);
    for (index = 0U; index < ORGANISM_COUNT; ++index)
    {
        MiniSNNWorldsKernelEntityId id;
        WD0_CHECK(minisnn_worlds_kernel_entity_at(kernel, index, &id) == 0);
        WD0_CHECK(minisnn_worlds_domain_register_organism(domain, id, 1U, 100U) == 0);
    }
    for (index = 0U; index < FOOD_COUNT; ++index)
    {
        MiniSNNWorldsKernelEntityId id;
        WD0_CHECK(minisnn_worlds_kernel_entity_at(kernel, ORGANISM_COUNT + index, &id) == 0);
        WD0_CHECK(minisnn_worlds_domain_register_food(domain, id, 3U) == 0);
    }
    for (tick = 0U; tick < TICK_COUNT; ++tick)
    {
        for (index = 0U; index < ORGANISM_COUNT; ++index)
        {
            MiniSNNWorldsKernelEntityId organism;
            WD0_CHECK(minisnn_worlds_kernel_entity_at(kernel, index, &organism) == 0);
            actions[index].actor = organism;
            actions[index].move_delta.x = 0;
            actions[index].move_delta.y = 0;
            actions[index].eat_target.value = 0U;
            if (tick == 0U && (index == 0U || index == 1U))
            {
                MiniSNNWorldsKernelEntityId shared_food;
                WD0_CHECK(minisnn_worlds_kernel_entity_at(kernel, ORGANISM_COUNT, &shared_food) == 0);
                actions[index].type = MINISNN_WORLDS_DOMAIN_ACTION_EAT;
                actions[index].eat_target = shared_food;
            }
            else if (tick == 1U && index == 2U)
            {
                actions[index].type = MINISNN_WORLDS_DOMAIN_ACTION_MOVE;
                actions[index].move_delta.x = 1000;
            }
            else if (tick < 2U)
            {
                actions[index].type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
            }
            else if (tick % 4U == 0U)
            {
                MiniSNNWorldsKernelEntityId food = wd0_id(
                    ORGANISM_COUNT + ((tick / 4U) * ORGANISM_COUNT + index) % FOOD_COUNT + 1U);
                actions[index].type = MINISNN_WORLDS_DOMAIN_ACTION_EAT;
                actions[index].eat_target = food;
            }
            else if (tick % 4U == 1U)
            {
                actions[index].type = MINISNN_WORLDS_DOMAIN_ACTION_MOVE;
                actions[index].move_delta.x = index % 2U == 0U ? 1 : -1;
            }
            else
            {
                actions[index].type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
            }
        }
        WD0_CHECK(minisnn_worlds_domain_step(domain, actions, ORGANISM_COUNT, results) == 0);
        if (tick == 0U)
        {
            ++competition_attempts;
            WD0_CHECK(results[0].status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED);
            WD0_CHECK(results[1].status == MINISNN_WORLDS_DOMAIN_ACTION_REJECTED &&
                      results[1].reason == MINISNN_WORLDS_DOMAIN_ACTION_REASON_TARGET_NOT_AVAILABLE);
            ++competition_rejections;
        }
        if (tick == 1U)
        {
            WD0_CHECK(results[2].status == MINISNN_WORLDS_DOMAIN_ACTION_REJECTED &&
                      results[2].reason == MINISNN_WORLDS_DOMAIN_ACTION_REASON_KERNEL_REJECTED);
            ++occupancy_rejections;
        }
        WD0_CHECK(minisnn_worlds_domain_testing_validate_invariants(domain) == 0);
    }
    WD0_CHECK(minisnn_worlds_domain_state_hash(domain, &hash) == 0);
    WD0_CHECK(occupancy_rejections > 0U && competition_attempts > 0U && competition_rejections > 0U);
    printf("WD0 stress validation OK organisms=%u foods=%u ticks=%u occupancy_rejections=%zu competition_attempts=%zu competition_rejections=%zu final_domain_hash=0x%016" PRIX64 "\n",
           ORGANISM_COUNT, FOOD_COUNT, TICK_COUNT, occupancy_rejections, competition_attempts,
           competition_rejections, hash);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    return 0;
}