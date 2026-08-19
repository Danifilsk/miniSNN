#include <stdio.h>

#include "minisnn_worlds_terrain.h"
#include "wf0_fish.h"

#define CHECK(condition) \
    do { if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        goto done; \
    } } while (0)

static MiniSNNWorldsKernelEntityId no_entity(void)
{
    MiniSNNWorldsKernelEntityId value = { UINT64_C(0) };
    return value;
}

static int queue_step(MiniSNNWorldsKernel *kernel,
                      MiniSNNWorldsKernelError result)
{
    return result == MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int create_entity_at(MiniSNNWorldsKernel *kernel,
                            MiniSNNWorldsKernelPosition position,
                            int set_occupancy,
                            MiniSNNWorldsKernelEntityId *out_id)
{
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelTransform transform;
    MiniSNNWorldsKernelOccupancy occupancy;
    size_t count;

    if (!queue_step(kernel, minisnn_worlds_kernel_queue_create_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
                no_entity(), &command)))
    {
        return 0;
    }
    count = minisnn_worlds_kernel_entity_count(kernel);
    if (count == 0U ||
        minisnn_worlds_kernel_entity_at(kernel, count - 1U, out_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    transform.position = position;
    transform.orientation = 0U;
    if (!queue_step(kernel, minisnn_worlds_kernel_queue_place_entity(
                kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
                no_entity(), *out_id, transform, &command)))
    {
        return 0;
    }
    if (!set_occupancy)
    {
        return 1;
    }
    occupancy.half_extent_x = 499;
    occupancy.half_extent_y = 499;
    occupancy.category_bits = UINT32_C(2);
    occupancy.blocking_mask = 0U;
    return queue_step(kernel, minisnn_worlds_kernel_queue_set_occupancy(
        kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
        no_entity(), *out_id, occupancy, &command));
}

int main(void)
{
    MiniSNNWorldsTerrainConfig terrain_config = { 4U, 3U, 1000, -500, -500 };
    MiniSNNWorldsTerrainError terrain_error;
    MiniSNNWorldsTerrain *terrain = NULL;
    MiniSNNWorldsKernelConfig kernel_config;
    MiniSNNWorldsKernelSpaceBounds bounds;
    MiniSNNWorldsKernelError kernel_error;
    MiniSNNWorldsKernel *kernel = NULL;
    MiniSNNWorldsDomainError domain_error;
    MiniSNNWorldsDomain *domain = NULL;
    MiniSNNWorldsBrainBridgeError bridge_error;
    MiniSNNWorldsBrainBridge *bridge = NULL;
    MiniSNN *brain = NULL;
    MiniSNNWorldsDomainSpeciesConfig species;
    MiniSNNWorldsBrainBridgeConfig bridge_config;
    MiniSNNWorldsKernelEntityId actor;
    MiniSNNWorldsKernelEntityId food;
    MiniSNNWorldsKernelPosition actor_position = { 0, 0 };
    MiniSNNWorldsKernelPosition food_position = { 1000, 0 };
    MiniSNNWorldsKernelTransform before;
    MiniSNNWorldsKernelTransform after;
    MiniSNNWorldsBrainDecisionReport report;
    MiniSNNWorldsDomainAction action;
    MiniSNNWorldsDomainActionResult action_result;
    unsigned int decision_index;
    int saw_move = 0;
    int result = 1;

    terrain = minisnn_worlds_terrain_create(&terrain_config, &terrain_error);
    CHECK(terrain != NULL && terrain_error == MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    CHECK(minisnn_worlds_terrain_add_rock(terrain, 1U, 0U) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    CHECK(minisnn_worlds_terrain_finalize(terrain) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    CHECK(minisnn_worlds_terrain_space_bounds(terrain, &bounds) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    kernel_config = minisnn_worlds_kernel_config_default();
    kernel_config.space_bounds = bounds;
    kernel = minisnn_worlds_kernel_create(&kernel_config, &kernel_error);
    CHECK(kernel != NULL && kernel_error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_terrain_materialize(terrain, kernel) ==
          MINISNN_WORLDS_TERRAIN_ERROR_NONE);
    CHECK(create_entity_at(kernel, actor_position, 1, &actor));
    CHECK(create_entity_at(kernel, food_position, 0, &food));

    domain = minisnn_worlds_domain_create(kernel, &domain_error);
    CHECK(domain != NULL && domain_error == MINISNN_WORLDS_DOMAIN_ERROR_NONE);
    species = wf0_fish_species_v1();
    CHECK(minisnn_worlds_domain_add_species(domain, &species) ==
          MINISNN_WORLDS_DOMAIN_ERROR_NONE);
    CHECK(minisnn_worlds_domain_register_organism(
              domain, actor, species.species_id, UINT64_C(50)) ==
          MINISNN_WORLDS_DOMAIN_ERROR_NONE);
    CHECK(minisnn_worlds_domain_register_food(domain, food, UINT64_C(15)) ==
          MINISNN_WORLDS_DOMAIN_ERROR_NONE);
    brain = wf0_fish_brain_v1_create();
    bridge_config = wf0_fish_brain_bridge_config_v1();
    bridge = minisnn_worlds_brain_bridge_create(&bridge_config, &bridge_error);
    CHECK(brain != NULL && bridge != NULL &&
          bridge_error == MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    CHECK(minisnn_worlds_brain_bridge_bind(bridge, domain, actor, brain) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_entity_transform(kernel, actor, &before) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);

    for (decision_index = 0U; decision_index < 10U; ++decision_index)
    {
        CHECK(minisnn_worlds_brain_bridge_decide(
                  bridge, domain, actor, &action, &report) ==
              MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
        CHECK(minisnn_worlds_domain_step(domain, &action, 1U, &action_result) ==
              MINISNN_WORLDS_DOMAIN_ERROR_NONE);
        if (action.type == MINISNN_WORLDS_DOMAIN_ACTION_MOVE &&
            action.move_delta.x == 1000 && action.move_delta.y == 0)
        {
            saw_move = 1;
            CHECK(action_result.status == MINISNN_WORLDS_DOMAIN_ACTION_REJECTED);
            CHECK(action_result.reason ==
                  MINISNN_WORLDS_DOMAIN_ACTION_REASON_KERNEL_REJECTED);
            break;
        }
    }
    CHECK(saw_move);
    CHECK(minisnn_worlds_kernel_entity_transform(kernel, actor, &after) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(after.position.x == before.position.x && after.position.y == before.position.y);
    printf("WT0 Fish terrain smoke OK\n");
    result = 0;
done:
    minisnn_worlds_brain_bridge_destroy(&bridge);
    minisnn_destroy(&brain);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    minisnn_worlds_terrain_destroy(terrain);
    return result;
}
