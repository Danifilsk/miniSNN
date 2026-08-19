#include "wf0_fish_internal.h"

#include <stdlib.h>
#include <string.h>

#define WF0_FISH_SPECIES_ID UINT64_C(1)
#define WF0_FISH_MAX_ENERGY UINT64_C(100)
#define WF0_FISH_INITIAL_ENERGY UINT64_C(50)
#define WF0_FISH_METABOLISM_PER_TICK UINT64_C(1)
#define WF0_FISH_MOVE_ENERGY_COST UINT64_C(2)
#define WF0_FISH_EAT_RANGE 0
#define WF0_FISH_FOOD_NUTRITION UINT64_C(15)
#define WF0_FISH_MOVE_STEP 1000
#define WF0_FISH_CORE_STEPS_PER_DECISION UINT32_C(8)
#define WF0_FISH_INPUT_GAIN 5000.0
#define WF0_FISH_PERCEPTION_SCALE 1000.0

#define WF0_SENSOR_FOOD_PRESENT 2
#define WF0_SENSOR_FOOD_DX 3
#define WF0_SENSOR_FOOD_DISTANCE 5
#define WF0_ACTION_WAIT 6
#define WF0_ACTION_MOVE_POS_X 7
#define WF0_ACTION_MOVE_NEG_X 8
#define WF0_ACTION_MOVE_POS_Y 9
#define WF0_ACTION_MOVE_NEG_Y 10
#define WF0_ACTION_EAT 11

static MiniSNNWorldsKernelEntityId no_entity(void)
{
    MiniSNNWorldsKernelEntityId result = { UINT64_C(0) };
    return result;
}

static MiniSNNWorldsKernelTransform transform_at(
    MiniSNNWorldsKernelScalar x,
    MiniSNNWorldsKernelScalar y)
{
    MiniSNNWorldsKernelTransform result;

    result.position.x = x;
    result.position.y = y;
    result.orientation = UINT32_C(0);
    return result;
}

static int create_placed_entity(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelScalar x,
    MiniSNNWorldsKernelScalar y,
    MiniSNNWorldsKernelEntityId *out_id)
{
    MiniSNNWorldsKernelCommandId command;
    size_t count;

    if (kernel == NULL || out_id == NULL ||
        minisnn_worlds_kernel_queue_create_entity(
            kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
            no_entity(), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    count = minisnn_worlds_kernel_entity_count(kernel);
    if (count == 0U ||
        minisnn_worlds_kernel_entity_at(kernel, count - 1U, out_id) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(
            kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
            no_entity(), *out_id, transform_at(x, y), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    return 1;
}

static int set_actor_occupancy(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId actor)
{
    MiniSNNWorldsKernelEntityId no_entity = { UINT64_C(0) };
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelOccupancy occupancy;
    MiniSNNWorldsTick target_tick;

    if (kernel == NULL)
    {
        return 0;
    }
    target_tick = minisnn_worlds_kernel_tick(kernel) + UINT64_C(1);
    occupancy.half_extent_x = 499;
    occupancy.half_extent_y = 499;
    occupancy.category_bits = UINT32_C(2);
    occupancy.blocking_mask = UINT32_C(0);
    return target_tick != UINT64_C(0) &&
           minisnn_worlds_kernel_queue_set_occupancy(
               kernel, target_tick, 0U, no_entity, actor, occupancy,
               &command) == MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE;
}
static int organism_info(
    const MiniSNNWorldsDomain *domain,
    MiniSNNWorldsKernelEntityId actor,
    MiniSNNWorldsDomainOrganismInfo *out_info)
{
    size_t index;
    MiniSNNWorldsDomainOrganismInfo info;

    if (domain == NULL || out_info == NULL)
    {
        return 0;
    }
    for (index = 0U; index < minisnn_worlds_domain_organism_count(domain); ++index)
    {
        if (minisnn_worlds_domain_organism_at(domain, index, &info) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE)
        {
            return 0;
        }
        if (info.entity_id.value == actor.value)
        {
            *out_info = info;
            return 1;
        }
    }
    return 0;
}

MiniSNNWorldsDomainSpeciesConfig wf0_fish_species_v1(void)
{
    MiniSNNWorldsDomainSpeciesConfig species;

    species.species_id = WF0_FISH_SPECIES_ID;
    species.max_energy = WF0_FISH_MAX_ENERGY;
    species.metabolism_per_tick = WF0_FISH_METABOLISM_PER_TICK;
    species.move_energy_cost = WF0_FISH_MOVE_ENERGY_COST;
    species.eat_range = WF0_FISH_EAT_RANGE;
    return species;
}

MiniSNNWorldsBrainBridgeConfig wf0_fish_brain_bridge_config_v1(void)
{
    MiniSNNWorldsBrainBridgeConfig config =
        minisnn_worlds_brain_bridge_config_default();

    config.perception_scale = WF0_FISH_PERCEPTION_SCALE;
    config.input_gain = WF0_FISH_INPUT_GAIN;
    config.move_step = WF0_FISH_MOVE_STEP;
    config.core_steps_per_decision = WF0_FISH_CORE_STEPS_PER_DECISION;
    config.action_neuron[MINISNN_WORLDS_BRAIN_ACTION_WAIT] = WF0_ACTION_WAIT;
    config.action_neuron[MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_X] = WF0_ACTION_MOVE_POS_X;
    config.action_neuron[MINISNN_WORLDS_BRAIN_ACTION_MOVE_NEG_X] = WF0_ACTION_MOVE_NEG_X;
    config.action_neuron[MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_Y] = WF0_ACTION_MOVE_POS_Y;
    config.action_neuron[MINISNN_WORLDS_BRAIN_ACTION_MOVE_NEG_Y] = WF0_ACTION_MOVE_NEG_Y;
    config.action_neuron[MINISNN_WORLDS_BRAIN_ACTION_EAT] = WF0_ACTION_EAT;
    return config;
}

MiniSNN *wf0_fish_brain_v1_create(void)
{
    MiniSNNConfig config = minisnn_default_config();
    MiniSNN *brain;

    config.neuron_count = WF0_FISH_NEURON_COUNT_V1;
    config.max_synaptic_delay = 1;
    brain = minisnn_create_with_config(&config);
    if (brain == NULL ||
        !minisnn_set_neuron_type(
            brain, WF0_SENSOR_FOOD_DISTANCE, MINISNN_NEURON_INHIBITORY) ||
        !minisnn_connect_delayed(
            brain, WF0_SENSOR_FOOD_PRESENT, WF0_ACTION_MOVE_NEG_X, 200.0, 1) ||
        !minisnn_connect_delayed(
            brain, WF0_SENSOR_FOOD_PRESENT, WF0_ACTION_EAT, 1000.0, 1) ||
        !minisnn_connect_delayed(
            brain, WF0_SENSOR_FOOD_DX, WF0_ACTION_MOVE_POS_X, 400.0, 1) ||
        !minisnn_connect_delayed(
            brain, WF0_SENSOR_FOOD_DISTANCE, WF0_ACTION_EAT, -1000.0, 1))
    {
        minisnn_destroy(&brain);
        return NULL;
    }
    return brain;
}

static WF0FishWorld *create_world_on_kernel(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelScalar food_x,
    MiniSNNWorldsKernelScalar food_y,
    int actor_has_occupancy)
{
    MiniSNNWorldsDomainError domain_error;
    MiniSNNWorldsBrainBridgeError brain_error;
    MiniSNNWorldsBrainBridgeConfig bridge_config =
        wf0_fish_brain_bridge_config_v1();
    MiniSNNWorldsDomainSpeciesConfig species = wf0_fish_species_v1();
    WF0FishWorld *world = calloc(1U, sizeof(*world));

    if (world == NULL)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return NULL;
    }
    world->kernel = kernel;
    if (world->kernel == NULL ||
        !create_placed_entity(world->kernel, 0, 0, &world->actor) ||
        !create_placed_entity(world->kernel, food_x, food_y, &world->food) ||
        (actor_has_occupancy != 0 && !set_actor_occupancy(world->kernel, world->actor)))
    {
        wf0_fish_world_destroy(&world);
        return NULL;
    }
    world->domain = minisnn_worlds_domain_create(world->kernel, &domain_error);
    if (world->domain == NULL || domain_error != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_add_species(world->domain, &species) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_register_organism(
            world->domain, world->actor, species.species_id, WF0_FISH_INITIAL_ENERGY) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_register_food(
            world->domain, world->food, WF0_FISH_FOOD_NUTRITION) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        wf0_fish_world_destroy(&world);
        return NULL;
    }
    world->brain = wf0_fish_brain_v1_create();
    world->brain_bridge = minisnn_worlds_brain_bridge_create(
        &bridge_config, &brain_error);
    if (world->brain == NULL || world->brain_bridge == NULL ||
        brain_error != MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE ||
        minisnn_worlds_brain_bridge_bind(
            world->brain_bridge, world->domain, world->actor, world->brain) !=
            MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE)
    {
        wf0_fish_world_destroy(&world);
        return NULL;
    }
    return world;
}

WF0FishWorld *wf0_fish_world_create(
    MiniSNNWorldsKernelScalar food_x,
    MiniSNNWorldsKernelScalar food_y)
{
    MiniSNNWorldsKernelConfig kernel_config =
        minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError kernel_error;
    MiniSNNWorldsKernel *kernel;

    kernel_config.master_seed = WF0_FISH_WORLD_SEED_V1;
    kernel_config.space_bounds.min_x = -10000;
    kernel_config.space_bounds.min_y = -10000;
    kernel_config.space_bounds.max_x = 10000;
    kernel_config.space_bounds.max_y = 10000;
    kernel = minisnn_worlds_kernel_create(&kernel_config, &kernel_error);
    if (kernel == NULL || kernel_error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        minisnn_worlds_kernel_destroy(kernel);
        return NULL;
    }
    return create_world_on_kernel(kernel, food_x, food_y, 0);
}

WF0FishWorld *wf0_fish_world_create_on_kernel(
    MiniSNNWorldsKernel **in_out_kernel,
    MiniSNNWorldsKernelScalar food_x,
    MiniSNNWorldsKernelScalar food_y,
    int actor_has_occupancy)
{
    MiniSNNWorldsKernel *kernel;

    if (in_out_kernel == NULL || *in_out_kernel == NULL)
    {
        return NULL;
    }
    kernel = *in_out_kernel;
    *in_out_kernel = NULL;
    return create_world_on_kernel(kernel, food_x, food_y, actor_has_occupancy);
}
void wf0_fish_world_destroy(WF0FishWorld **world_ptr)
{
    WF0FishWorld *world;

    if (world_ptr == NULL || *world_ptr == NULL)
    {
        return;
    }
    world = *world_ptr;
    minisnn_worlds_brain_bridge_destroy(&world->brain_bridge);
    minisnn_destroy(&world->brain);
    minisnn_worlds_domain_destroy(world->domain);
    minisnn_worlds_kernel_destroy(world->kernel);
    free(world);
    *world_ptr = NULL;
}

MiniSNNWorldsKernelEntityId wf0_fish_world_actor(const WF0FishWorld *world)
{
    return world == NULL ? no_entity() : world->actor;
}

MiniSNNWorldsKernelEntityId wf0_fish_world_food(const WF0FishWorld *world)
{
    return world == NULL ? no_entity() : world->food;
}

int wf0_fish_world_core_step(const WF0FishWorld *world)
{
    return world == NULL ? -1 : world->core_step;
}

MiniSNNWorldsTick wf0_fish_world_tick(const WF0FishWorld *world)
{
    return world == NULL ? UINT64_C(0) : minisnn_worlds_domain_tick(world->domain);
}

uint64_t wf0_fish_world_food_count(const WF0FishWorld *world)
{
    return world == NULL ? 0U : (uint64_t)minisnn_worlds_domain_food_count(world->domain);
}

int wf0_fish_world_state(
    const WF0FishWorld *world,
    WF0FishWorldState *out_state)
{
    WF0FishWorldState state;
    MiniSNNWorldsDomainOrganismInfo organism;

    if (world == NULL || out_state == NULL ||
        !organism_info(world->domain, world->actor, &organism) ||
        minisnn_worlds_kernel_entity_transform(
            world->kernel, world->actor, &state.actor_transform) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(world->kernel, &state.kernel_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_domain_state_hash(world->domain, &state.domain_hash) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        return 0;
    }
    state.actor_energy = organism.energy;
    state.actor_life_state = organism.life_state;
    state.actor_death_cause = organism.death_cause;
    state.actor_death_tick = organism.death_tick;
    state.remaining_food = wf0_fish_world_food_count(world);
    state.tick = minisnn_worlds_domain_tick(world->domain);
    state.core_step = world->core_step;
    *out_state = state;
    return 1;
}
int wf0_fish_world_reset_brain(WF0FishWorld *world)
{
    return world != NULL && world->brain != NULL &&
           minisnn_reset_transient_state(world->brain);
}

int wf0_fish_world_tick_once(WF0FishWorld *world, WF0FishTickRecord *out_record)
{
    WF0FishTickRecord record;
    MiniSNNWorldsDomainOrganismInfo organism;

    if (world == NULL || out_record == NULL)
    {
        return 0;
    }
    memset(&record, 0, sizeof(record));
    record.tick_before = minisnn_worlds_domain_tick(world->domain);
    record.core_step_start = world->core_step;
    if (!organism_info(world->domain, world->actor, &organism))
    {
        return 0;
    }
    if (organism.life_state == MINISNN_WORLDS_DOMAIN_LIFE_ALIVE)
    {
        if (minisnn_worlds_brain_bridge_decide(
                world->brain_bridge, world->domain, world->actor,
                &record.decision.action, &record.decision) !=
            MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE ||
            minisnn_worlds_domain_step(
                world->domain, &record.decision.action, 1U,
                &record.action_result) != MINISNN_WORLDS_DOMAIN_ERROR_NONE)
        {
            return 0;
        }
        record.decision_performed = 1;
    }
    else if (minisnn_worlds_domain_step(world->domain, NULL, 0U, NULL) !=
             MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        return 0;
    }
    if (
        !organism_info(world->domain, world->actor, &organism) ||
        minisnn_worlds_kernel_entity_transform(
            world->kernel, world->actor, &record.transform_after) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(world->kernel, &record.kernel_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_domain_state_hash(world->domain, &record.domain_hash) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        return 0;
    }
    world->core_step += (int)record.decision.core_steps_executed;
    record.core_step_end = world->core_step;
    record.energy_after_tick = organism.energy;
    record.metabolism_cost = record.action_result.energy_after >= organism.energy ?
        record.action_result.energy_after - organism.energy : 0U;
    if (record.decision.action.type == MINISNN_WORLDS_DOMAIN_ACTION_MOVE &&
        record.action_result.status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED)
    {
        record.move_cost = record.action_result.energy_before -
            record.action_result.energy_after;
    }
    if (record.decision.action.type == MINISNN_WORLDS_DOMAIN_ACTION_EAT &&
        record.action_result.status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED)
    {
        record.food_nutrition = record.action_result.energy_after -
            record.action_result.energy_before;
    }
    record.remaining_food = wf0_fish_world_food_count(world);
    *out_record = record;
    return 1;
}