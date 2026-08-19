#include "g0_visualizer_runtime.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define G0_TILE_SIZE INT64_C(1000)
#define G0_LEGACY_ORIGIN INT64_C(-500)
#define G0_INITIAL_ENERGY UINT64_C(50)
#define G0_FOOD_NUTRITION UINT64_C(15)
#define G0_OCCUPANCY_HALF_EXTENT INT64_C(499)
#define G0_OCCUPANCY_CATEGORY UINT32_C(2)
#define G0_TICK_INTERVAL_MILLISECONDS UINT64_C(250)

typedef struct
{
    MiniSNNWorldsTerrain *terrain;
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsDomain *domain;
    MiniSNN *brain;
    MiniSNNWorldsBrainBridge *brain_bridge;
    int core_step;
    MiniSNNWorldsKernelEntityId actor;
} G0WorldInstance;

struct G0VisualizerRuntime
{
    G0WorldConfig config;
    G0WorldInstance active;
    WF0FishWorldState state;
    WF0FishTickRecord last_record;
    uint64_t elapsed_units;
    uint64_t episode_tick;
    int has_last_record;
    int paused;
    int grid_enabled;
    int debug_enabled;
    int blocked_first_move;
    int legacy_layout;
    unsigned int speed_index;
    G0VisualizerFacing facing;
};

static const unsigned int g0_speed_numerators[G0_VISUALIZER_SPEED_COUNT] =
{ 1U, 1U, 1U, 2U, 4U, 8U };
static const unsigned int g0_speed_denominators[G0_VISUALIZER_SPEED_COUNT] =
{ 4U, 2U, 1U, 1U, 1U, 1U };
static const unsigned int g0_speed_ticks_per_second[G0_VISUALIZER_SPEED_COUNT] =
{ 1U, 2U, 4U, 8U, 16U, 32U };
static const char *const g0_speed_names[G0_VISUALIZER_SPEED_COUNT] =
{ "0.25x", "0.5x", "1x", "2x", "4x", "8x" };

static MiniSNNWorldsKernelEntityId no_entity(void)
{
    MiniSNNWorldsKernelEntityId entity = { UINT64_C(0) };
    return entity;
}

static MiniSNNWorldsKernelTransform transform_at(
    MiniSNNWorldsKernelScalar x, MiniSNNWorldsKernelScalar y)
{
    MiniSNNWorldsKernelTransform transform;
    transform.position.x = x;
    transform.position.y = y;
    transform.orientation = UINT32_C(0);
    return transform;
}

static void g0_world_instance_destroy(G0WorldInstance *instance)
{
    if (instance == NULL)
        return;
    minisnn_worlds_brain_bridge_destroy(&instance->brain_bridge);
    minisnn_destroy(&instance->brain);
    minisnn_worlds_domain_destroy(instance->domain);
    minisnn_worlds_kernel_destroy(instance->kernel);
    minisnn_worlds_terrain_destroy(instance->terrain);
    memset(instance, 0, sizeof(*instance));
}

static int create_placed_entity(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelPosition position,
    MiniSNNWorldsKernelEntityId *out_entity)
{
    MiniSNNWorldsKernelCommandId command;
    size_t count;

    if (kernel == NULL || out_entity == NULL ||
        minisnn_worlds_kernel_queue_create_entity(
            kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
            no_entity(), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        return 0;
    count = minisnn_worlds_kernel_entity_count(kernel);
    if (count == 0U ||
        minisnn_worlds_kernel_entity_at(kernel, count - 1U, out_entity) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(
            kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
            no_entity(), *out_entity, transform_at(position.x, position.y), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        return 0;
    return 1;
}

static int set_actor_occupancy(
    MiniSNNWorldsKernel *kernel, MiniSNNWorldsKernelEntityId actor)
{
    MiniSNNWorldsKernelOccupancy occupancy;
    MiniSNNWorldsKernelCommandId command;

    if (kernel == NULL)
        return 0;
    occupancy.half_extent_x = G0_OCCUPANCY_HALF_EXTENT;
    occupancy.half_extent_y = G0_OCCUPANCY_HALF_EXTENT;
    occupancy.category_bits = G0_OCCUPANCY_CATEGORY;
    occupancy.blocking_mask = UINT32_C(0);
    return minisnn_worlds_kernel_queue_set_occupancy(
               kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
               no_entity(), actor, occupancy, &command) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

static int organism_info(const MiniSNNWorldsDomain *domain,
                         MiniSNNWorldsKernelEntityId actor,
                         MiniSNNWorldsDomainOrganismInfo *out_info)
{
    size_t index;
    MiniSNNWorldsDomainOrganismInfo info;

    if (domain == NULL || out_info == NULL)
        return 0;
    for (index = 0U; index < minisnn_worlds_domain_organism_count(domain); ++index)
    {
        if (minisnn_worlds_domain_organism_at(domain, index, &info) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE)
            return 0;
        if (info.entity_id.value == actor.value)
        {
            *out_info = info;
            return 1;
        }
    }
    return 0;
}

static int runtime_capture_state(const G0WorldInstance *instance,
                                 WF0FishWorldState *out_state)
{
    MiniSNNWorldsDomainOrganismInfo organism;
    WF0FishWorldState state;
    MiniSNNWorldsKernelError kernel_error;
    MiniSNNWorldsDomainError domain_error;

    if (instance == NULL || out_state == NULL || instance->kernel == NULL ||
        instance->domain == NULL || instance->brain == NULL ||
        instance->brain_bridge == NULL ||
        !organism_info(instance->domain, instance->actor, &organism))
    {
        return 0;
    }
    kernel_error = minisnn_worlds_kernel_entity_transform(
        instance->kernel, instance->actor, &state.actor_transform);
    if (kernel_error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    kernel_error = minisnn_worlds_kernel_state_hash(instance->kernel, &state.kernel_hash);
    if (kernel_error != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    domain_error = minisnn_worlds_domain_state_hash(instance->domain, &state.domain_hash);
    if (domain_error != MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        return 0;
    }
    state.actor_energy = organism.energy;
    state.actor_life_state = organism.life_state;
    state.actor_death_cause = organism.death_cause;
    state.actor_death_tick = organism.death_tick;
    state.remaining_food = (uint64_t)minisnn_worlds_domain_food_count(instance->domain);
    state.tick = minisnn_worlds_domain_tick(instance->domain);
    state.core_step = instance->core_step;
    *out_state = state;
    return 1;
}

static int build_terrain_from_config(const G0WorldConfig *config,
                                     int legacy_layout,
                                     MiniSNNWorldsTerrain **out_terrain)
{
    MiniSNNWorldsTerrainConfig terrain_config;
    MiniSNNWorldsTerrainError error;
    MiniSNNWorldsTerrain *terrain;
    size_t index;

    if (config == NULL || out_terrain == NULL || !g0_world_config_validate(config))
        return 0;
    terrain_config.width = config->width;
    terrain_config.height = config->height;
    terrain_config.tile_size = G0_TILE_SIZE;
    terrain_config.origin_x = legacy_layout != 0 ? G0_LEGACY_ORIGIN :
        -((MiniSNNWorldsKernelScalar)config->width * G0_TILE_SIZE) / INT64_C(2);
    terrain_config.origin_y = legacy_layout != 0 ? G0_LEGACY_ORIGIN :
        -((MiniSNNWorldsKernelScalar)config->height * G0_TILE_SIZE) / INT64_C(2);
    terrain = minisnn_worlds_terrain_create(&terrain_config, &error);
    if (terrain == NULL || error != MINISNN_WORLDS_TERRAIN_ERROR_NONE)
    {
        minisnn_worlds_terrain_destroy(terrain);
        return 0;
    }
    for (index = 0U; index < (size_t)config->width * (size_t)config->height; ++index)
    {
        uint32_t x = (uint32_t)(index % (size_t)config->width);
        uint32_t y = (uint32_t)(index / (size_t)config->width);
        if ((config->tiles[index] == MINISNN_WORLDS_TERRAIN_TILE_LAND &&
             minisnn_worlds_terrain_set_tile(
                 terrain, x, y, MINISNN_WORLDS_TERRAIN_TILE_LAND) !=
                 MINISNN_WORLDS_TERRAIN_ERROR_NONE) ||
            (config->rocks[index] != 0U &&
             minisnn_worlds_terrain_add_rock(terrain, x, y) !=
                 MINISNN_WORLDS_TERRAIN_ERROR_NONE))
        {
            minisnn_worlds_terrain_destroy(terrain);
            return 0;
        }
    }
    if (minisnn_worlds_terrain_finalize(terrain) != MINISNN_WORLDS_TERRAIN_ERROR_NONE)
    {
        minisnn_worlds_terrain_destroy(terrain);
        return 0;
    }
    *out_terrain = terrain;
    return 1;
}
static int g0_world_instance_build(const G0WorldConfig *config,
                                   int legacy_layout,
                                   G0WorldInstance *out_instance)
{
    MiniSNNWorldsKernelConfig kernel_config;
    MiniSNNWorldsKernelError kernel_error;
    MiniSNNWorldsKernelSpaceBounds bounds;
    MiniSNNWorldsDomainError domain_error;
    MiniSNNWorldsBrainBridgeError brain_error;
    MiniSNNWorldsBrainBridgeConfig brain_config;
    MiniSNNWorldsDomainSpeciesConfig species;
    MiniSNNWorldsKernelPosition spawn;
    size_t index;
    G0WorldInstance candidate;

    if (config == NULL || out_instance == NULL || !g0_world_config_validate(config))
        return 0;
    memset(&candidate, 0, sizeof(candidate));
    if (!build_terrain_from_config(config, legacy_layout, &candidate.terrain) ||
        minisnn_worlds_terrain_space_bounds(candidate.terrain, &bounds) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE)
    {
        g0_world_instance_destroy(&candidate);
        return 0;
    }
    kernel_config = minisnn_worlds_kernel_config_default();
    kernel_config.master_seed = WF0_FISH_WORLD_SEED_V1;
    kernel_config.space_bounds = bounds;
    candidate.kernel = minisnn_worlds_kernel_create(&kernel_config, &kernel_error);
    if (candidate.kernel == NULL || kernel_error != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_terrain_materialize(candidate.terrain, candidate.kernel) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        minisnn_worlds_terrain_tile_to_world(
            candidate.terrain, config->fish_spawn_x, config->fish_spawn_y, &spawn) !=
            MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
        !create_placed_entity(candidate.kernel, spawn, &candidate.actor) ||
        !set_actor_occupancy(candidate.kernel, candidate.actor))
    {
        g0_world_instance_destroy(&candidate);
        return 0;
    }
    {
        MiniSNNWorldsKernelEntityId food_ids[G0_WORLD_CONFIG_MAX_WIDTH * G0_WORLD_CONFIG_MAX_HEIGHT];
        size_t food_count = 0U;

        for (index = 0U; index < (size_t)config->width * (size_t)config->height; ++index)
        {
            MiniSNNWorldsKernelPosition position;
            uint32_t x;
            uint32_t y;

            if (config->foods[index] == 0U)
                continue;
            x = (uint32_t)(index % (size_t)config->width);
            y = (uint32_t)(index / (size_t)config->width);
            if (food_count >= sizeof(food_ids) / sizeof(food_ids[0]) ||
                minisnn_worlds_terrain_tile_to_world(
                    candidate.terrain, x, y, &position) !=
                    MINISNN_WORLDS_TERRAIN_ERROR_NONE ||
                !create_placed_entity(candidate.kernel, position, &food_ids[food_count]))
            {
                g0_world_instance_destroy(&candidate);
                return 0;
            }
            food_count++;
        }
        if (!set_actor_occupancy(candidate.kernel, candidate.actor))
        {
            g0_world_instance_destroy(&candidate);
            return 0;
        }
        candidate.domain = minisnn_worlds_domain_create(candidate.kernel, &domain_error);
        species = wf0_fish_species_v1();
        if (candidate.domain == NULL || domain_error != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
            minisnn_worlds_domain_add_species(candidate.domain, &species) !=
                MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
            minisnn_worlds_domain_register_organism(
                candidate.domain, candidate.actor, species.species_id, G0_INITIAL_ENERGY) !=
                MINISNN_WORLDS_DOMAIN_ERROR_NONE)
        {
            g0_world_instance_destroy(&candidate);
            return 0;
        }
        for (index = 0U; index < food_count; ++index)
        {
            if (minisnn_worlds_domain_register_food(
                    candidate.domain, food_ids[index], G0_FOOD_NUTRITION) !=
                    MINISNN_WORLDS_DOMAIN_ERROR_NONE)
            {
                g0_world_instance_destroy(&candidate);
                return 0;
            }
        }
    }
    candidate.brain = wf0_fish_brain_v1_create();
    brain_config = wf0_fish_brain_bridge_config_v1();
    candidate.brain_bridge = minisnn_worlds_brain_bridge_create(
        &brain_config, &brain_error);
    if (candidate.brain == NULL || candidate.brain_bridge == NULL ||
        brain_error != MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE ||
        minisnn_worlds_brain_bridge_bind(
            candidate.brain_bridge, candidate.domain, candidate.actor,
            candidate.brain) != MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE)
    {
        g0_world_instance_destroy(&candidate);
        return 0;
    }
    *out_instance = candidate;
    return 1;
}

static int g0_world_config_legacy(G0WorldConfig *config, int blocked_first_move)
{
    static const uint32_t land_tiles[][2] =
    {
        { 1U, 3U }, { 2U, 3U }, { 3U, 3U },
        { 1U, 4U }, { 2U, 4U }, { 3U, 4U }
    };
    size_t index;

    if (config == NULL || !g0_world_config_init(config, UINT32_C(5), UINT32_C(5)) ||
        !g0_world_config_set_fish_spawn(config, 0U, 0U) ||
        !g0_world_config_set_food(config, 1U, 0U, 1))
    {
        g0_world_config_destroy(config);
        return 0;
    }
    for (index = 0U; index < sizeof(land_tiles) / sizeof(land_tiles[0]); ++index)
    {
        if (!g0_world_config_set_tile(
                config, land_tiles[index][0], land_tiles[index][1],
                MINISNN_WORLDS_TERRAIN_TILE_LAND))
        {
            g0_world_config_destroy(config);
            return 0;
        }
    }
    if (!g0_world_config_set_rock(
            config, blocked_first_move != 0 ? 1U : 2U,
            blocked_first_move != 0 ? 0U : 1U, 1))
    {
        g0_world_config_destroy(config);
        return 0;
    }
    return 1;
}

static int g0_world_config_sandbox(G0WorldConfig *config)
{
    uint32_t x;

    if (config == NULL || !g0_world_config_init(config, UINT32_C(20), UINT32_C(15)) ||
        !g0_world_config_set_fish_spawn(config, 3U, 4U) ||
        !g0_world_config_set_food(config, 6U, 4U, 1) ||
        !g0_world_config_set_food(config, 10U, 8U, 1) ||
        !g0_world_config_set_food(config, 15U, 11U, 1))
    {
        g0_world_config_destroy(config);
        return 0;
    }
    for (x = 8U; x <= 11U; ++x)
    {
        if (!g0_world_config_set_tile(
                config, x, 7U, MINISNN_WORLDS_TERRAIN_TILE_LAND))
        {
            g0_world_config_destroy(config);
            return 0;
        }
    }
    if (!g0_world_config_set_rock(config, 9U, 4U, 1) ||
        !g0_world_config_set_rock(config, 13U, 10U, 1))
    {
        g0_world_config_destroy(config);
        return 0;
    }
    return 1;
}

static G0VisualizerRuntime *runtime_create_with_config(
    const G0WorldConfig *config,
    int legacy_layout,
    int blocked_first_move)
{
    G0VisualizerRuntime *runtime;

    if (config == NULL || !g0_world_config_validate(config))
        return NULL;
    runtime = calloc(1U, sizeof(*runtime));
    if (runtime == NULL)
        return NULL;
    if (!g0_world_config_clone(config, &runtime->config))
    {
        g0_visualizer_runtime_destroy(&runtime);
        return NULL;
    }
    if (!g0_world_instance_build(config, legacy_layout, &runtime->active))
    {
        g0_visualizer_runtime_destroy(&runtime);
        return NULL;
    }
    if (!runtime_capture_state(&runtime->active, &runtime->state))
    {
        g0_visualizer_runtime_destroy(&runtime);
        return NULL;
    }
    runtime->grid_enabled = 1;
    runtime->debug_enabled = 1;
    runtime->blocked_first_move = blocked_first_move != 0;
    runtime->legacy_layout = legacy_layout != 0;
    runtime->speed_index = 2U;
    runtime->facing = G0_VISUALIZER_FACING_SOUTH;
    return runtime;
}

G0VisualizerRuntime *g0_visualizer_runtime_create(int blocked_first_move)
{
    G0WorldConfig config;
    G0VisualizerRuntime *runtime;

    memset(&config, 0, sizeof(config));
    if (!g0_world_config_legacy(&config, blocked_first_move))
        return NULL;
    runtime = runtime_create_with_config(&config, 1, blocked_first_move);
    g0_world_config_destroy(&config);
    return runtime;
}

G0VisualizerRuntime *g0_visualizer_runtime_create_sandbox(void)
{
    G0WorldConfig config;
    G0VisualizerRuntime *runtime;

    memset(&config, 0, sizeof(config));
    if (!g0_world_config_sandbox(&config))
        return NULL;
    runtime = runtime_create_with_config(&config, 0, 0);
    g0_world_config_destroy(&config);
    return runtime;
}

G0VisualizerRuntime *g0_visualizer_runtime_create_from_config(const G0WorldConfig *config)
{
    return runtime_create_with_config(config, 0, 0);
}

void g0_visualizer_runtime_destroy(G0VisualizerRuntime **runtime_ptr)
{
    G0VisualizerRuntime *runtime;

    if (runtime_ptr == NULL || *runtime_ptr == NULL)
        return;
    runtime = *runtime_ptr;
    g0_world_instance_destroy(&runtime->active);
    g0_world_config_destroy(&runtime->config);
    free(runtime);
    *runtime_ptr = NULL;
}

int g0_visualizer_runtime_apply_config(G0VisualizerRuntime *runtime)
{
    G0WorldInstance candidate;
    WF0FishWorldState candidate_state;
    int paused;

    if (runtime == NULL || !g0_world_config_validate(&runtime->config))
        return 0;
    memset(&candidate, 0, sizeof(candidate));
    if (!g0_world_instance_build(&runtime->config, runtime->legacy_layout, &candidate) ||
        !runtime_capture_state(&candidate, &candidate_state))
    {
        g0_world_instance_destroy(&candidate);
        return 0;
    }
    paused = runtime->paused;
    g0_world_instance_destroy(&runtime->active);
    runtime->active = candidate;
    runtime->state = candidate_state;
    memset(&runtime->last_record, 0, sizeof(runtime->last_record));
    runtime->has_last_record = 0;
    runtime->elapsed_units = 0U;
    runtime->episode_tick = 0U;
    runtime->facing = G0_VISUALIZER_FACING_SOUTH;
    runtime->paused = paused;
    return 1;
}

int g0_visualizer_runtime_reset(G0VisualizerRuntime *runtime)
{
    G0WorldInstance candidate;
    WF0FishWorldState state;
    int paused;

    if (runtime == NULL || !g0_world_config_validate(&runtime->config))
        return 0;
    memset(&candidate, 0, sizeof(candidate));
    if (!g0_world_instance_build(&runtime->config, runtime->legacy_layout, &candidate) ||
        !runtime_capture_state(&candidate, &state))
    {
        g0_world_instance_destroy(&candidate);
        return 0;
    }
    paused = runtime->paused;
    g0_world_instance_destroy(&runtime->active);
    runtime->active = candidate;
    if (!runtime_capture_state(&runtime->active, &runtime->state))
        return 0;
    memset(&runtime->last_record, 0, sizeof(runtime->last_record));
    runtime->has_last_record = 0;
    runtime->elapsed_units = 0U;
    runtime->episode_tick = 0U;
    runtime->facing = G0_VISUALIZER_FACING_SOUTH;
    runtime->paused = paused;
    return 1;
}

int g0_visualizer_runtime_new_world(G0VisualizerRuntime *runtime,
                                    uint32_t width,
                                    uint32_t height)
{
    G0WorldConfig candidate;

    if (runtime == NULL)
        return 0;
    memset(&candidate, 0, sizeof(candidate));
    if (!g0_world_config_init(&candidate, width, height))
        return 0;
    g0_world_config_destroy(&runtime->config);
    runtime->config = candidate;
    runtime->legacy_layout = 0;
    return 1;
}
int g0_visualizer_runtime_resize_world(G0VisualizerRuntime *runtime,
                                       uint32_t width,
                                       uint32_t height)
{
    if (runtime == NULL || !g0_world_config_resize(&runtime->config, width, height))
    {
        return 0;
    }
    runtime->legacy_layout = 0;
    return 1;
}

int g0_visualizer_runtime_prepare_edit(G0VisualizerRuntime *runtime)
{
    /*
     * The editable configuration is the persistent episode blueprint. Entering
     * edit mode must not fold transient food consumption or fish movement back
     * into that blueprint.
     */
    return runtime != NULL && runtime->active.kernel != NULL &&
           runtime->active.domain != NULL && runtime->active.terrain != NULL &&
           g0_world_config_validate(&runtime->config);
}

int g0_visualizer_runtime_edit_tile(G0VisualizerRuntime *runtime,
                                    G0WorldTool tool,
                                    uint32_t tile_x,
                                    uint32_t tile_y)
{
    if (runtime == NULL)
        return 0;
    switch (tool)
    {
        case G0_WORLD_TOOL_WATER:
            return g0_world_config_set_tile(
                &runtime->config, tile_x, tile_y,
                MINISNN_WORLDS_TERRAIN_TILE_WATER);
        case G0_WORLD_TOOL_LAND:
            return g0_world_config_set_food(&runtime->config, tile_x, tile_y, 0) &&
                   g0_world_config_set_tile(
                       &runtime->config, tile_x, tile_y,
                       MINISNN_WORLDS_TERRAIN_TILE_LAND);
        case G0_WORLD_TOOL_ROCK:
            return g0_world_config_set_food(&runtime->config, tile_x, tile_y, 0) &&
                   g0_world_config_set_rock(&runtime->config, tile_x, tile_y, 1);
        case G0_WORLD_TOOL_FOOD:
            return g0_world_config_set_food(&runtime->config, tile_x, tile_y, 1);
        case G0_WORLD_TOOL_ERASE:
            return g0_world_config_set_rock(&runtime->config, tile_x, tile_y, 0) &&
                   g0_world_config_set_food(&runtime->config, tile_x, tile_y, 0);
        case G0_WORLD_TOOL_FISH_SPAWN:
            return g0_world_config_set_fish_spawn(&runtime->config, tile_x, tile_y);
        default:
            return 0;
    }
}

const G0WorldConfig *g0_visualizer_runtime_config(const G0VisualizerRuntime *runtime)
{
    return runtime == NULL ? NULL : &runtime->config;
}

G0VisualizerFacing g0_visualizer_facing_from_move_delta(
    G0VisualizerFacing previous_facing,
    MiniSNNWorldsKernelPosition move_delta)
{
    if (move_delta.x > 0 && move_delta.y == 0)
        return G0_VISUALIZER_FACING_EAST;
    if (move_delta.x < 0 && move_delta.y == 0)
        return G0_VISUALIZER_FACING_WEST;
    if (move_delta.x == 0 && move_delta.y > 0)
        return G0_VISUALIZER_FACING_SOUTH;
    if (move_delta.x == 0 && move_delta.y < 0)
        return G0_VISUALIZER_FACING_NORTH;
    return previous_facing;
}

int g0_visualizer_runtime_step_once(G0VisualizerRuntime *runtime)
{
    WF0FishTickRecord record;
    MiniSNNWorldsDomainOrganismInfo organism;

    if (runtime == NULL || runtime->active.domain == NULL ||
        runtime->active.brain == NULL ||
        runtime->active.brain_bridge == NULL)
        return 0;
    memset(&record, 0, sizeof(record));
    record.tick_before = minisnn_worlds_domain_tick(runtime->active.domain);
    record.core_step_start = runtime->active.core_step;
    if (!organism_info(runtime->active.domain, runtime->active.actor, &organism))
    {
        return 0;
    }
    if (organism.life_state == MINISNN_WORLDS_DOMAIN_LIFE_ALIVE)
    {
        if (minisnn_worlds_brain_bridge_decide(
                runtime->active.brain_bridge, runtime->active.domain,
                runtime->active.actor, &record.decision.action,
                &record.decision) != MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE ||
            minisnn_worlds_domain_step(
                runtime->active.domain, &record.decision.action, 1U,
                &record.action_result) != MINISNN_WORLDS_DOMAIN_ERROR_NONE)
        {
            return 0;
        }
        record.decision_performed = 1;
    }
    else if (minisnn_worlds_domain_step(runtime->active.domain, NULL, 0U, NULL) !=
             MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        return 0;
    }
    if (!organism_info(runtime->active.domain, runtime->active.actor, &organism) ||
        minisnn_worlds_kernel_entity_transform(
            runtime->active.kernel, runtime->active.actor,
            &record.transform_after) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_state_hash(
            runtime->active.kernel, &record.kernel_hash) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_domain_state_hash(
            runtime->active.domain, &record.domain_hash) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        !runtime_capture_state(&runtime->active, &runtime->state))
        return 0;
    runtime->active.core_step += (int)record.decision.core_steps_executed;
    record.core_step_end = runtime->active.core_step;
    record.energy_after_tick = organism.energy;
    if (record.decision.action.type == MINISNN_WORLDS_DOMAIN_ACTION_MOVE &&
        record.action_result.status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED)
    {
        record.move_cost = record.action_result.energy_before -
                           record.action_result.energy_after;
        runtime->facing = g0_visualizer_facing_from_move_delta(
            runtime->facing, record.decision.action.move_delta);
    }
    if (record.decision.action.type == MINISNN_WORLDS_DOMAIN_ACTION_EAT &&
        record.action_result.status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED)
        record.food_nutrition = record.action_result.energy_after -
                                record.action_result.energy_before;
    record.remaining_food = runtime->state.remaining_food;
    runtime->episode_tick++;
    runtime->last_record = record;
    runtime->has_last_record = 1;
    return 1;
}

int g0_visualizer_runtime_advance(G0VisualizerRuntime *runtime)
{
    return runtime != NULL && (runtime->paused != 0 ||
                               g0_visualizer_runtime_step_once(runtime));
}

int g0_visualizer_runtime_advance_elapsed(
    G0VisualizerRuntime *runtime,
    uint32_t elapsed_milliseconds)
{
    uint64_t threshold;

    if (runtime == NULL)
        return 0;
    if (runtime->paused != 0)
        return 1;
    threshold = G0_TICK_INTERVAL_MILLISECONDS *
                g0_speed_denominators[runtime->speed_index];
    runtime->elapsed_units += (uint64_t)elapsed_milliseconds *
                              g0_speed_numerators[runtime->speed_index];
    while (runtime->elapsed_units >= threshold)
    {
        if (!g0_visualizer_runtime_step_once(runtime))
            return 0;
        runtime->elapsed_units -= threshold;
    }
    return 1;
}

void g0_visualizer_runtime_toggle_paused(G0VisualizerRuntime *runtime)
{
    if (runtime != NULL)
        runtime->paused = runtime->paused == 0;
}

void g0_visualizer_runtime_set_paused(G0VisualizerRuntime *runtime, int paused)
{
    if (runtime != NULL)
        runtime->paused = paused != 0;
}

void g0_visualizer_runtime_toggle_grid(G0VisualizerRuntime *runtime)
{
    if (runtime != NULL)
        runtime->grid_enabled = runtime->grid_enabled == 0;
}

void g0_visualizer_runtime_set_grid_enabled(G0VisualizerRuntime *runtime, int enabled)
{
    if (runtime != NULL)
        runtime->grid_enabled = enabled != 0;
}

void g0_visualizer_runtime_toggle_debug(G0VisualizerRuntime *runtime)
{
    if (runtime != NULL)
        runtime->debug_enabled = runtime->debug_enabled == 0;
}

void g0_visualizer_runtime_set_debug_enabled(G0VisualizerRuntime *runtime, int enabled)
{
    if (runtime != NULL)
        runtime->debug_enabled = enabled != 0;
}

int g0_visualizer_runtime_speed_increase(G0VisualizerRuntime *runtime)
{
    if (runtime == NULL || runtime->speed_index + 1U >= G0_VISUALIZER_SPEED_COUNT)
        return 0;
    runtime->speed_index++;
    return 1;
}

int g0_visualizer_runtime_speed_decrease(G0VisualizerRuntime *runtime)
{
    if (runtime == NULL || runtime->speed_index == 0U)
        return 0;
    runtime->speed_index--;
    return 1;
}

int g0_visualizer_runtime_set_speed_index(
    G0VisualizerRuntime *runtime,
    unsigned int speed_index)
{
    if (runtime == NULL || speed_index >= G0_VISUALIZER_SPEED_COUNT)
        return 0;
    runtime->speed_index = speed_index;
    runtime->elapsed_units = 0U;
    return 1;
}
unsigned int g0_visualizer_runtime_speed_index(const G0VisualizerRuntime *runtime)
{
    return runtime == NULL ? 0U : runtime->speed_index;
}

const char *g0_visualizer_runtime_speed_name(const G0VisualizerRuntime *runtime)
{
    return runtime == NULL ? "NA" : g0_speed_names[runtime->speed_index];
}

int g0_visualizer_runtime_is_paused(const G0VisualizerRuntime *runtime)
{
    return runtime != NULL && runtime->paused != 0;
}

int g0_visualizer_runtime_grid_enabled(const G0VisualizerRuntime *runtime)
{
    return runtime != NULL && runtime->grid_enabled != 0;
}

int g0_visualizer_runtime_debug_enabled(const G0VisualizerRuntime *runtime)
{
    return runtime != NULL && runtime->debug_enabled != 0;
}

unsigned int g0_visualizer_runtime_ticks_per_second(const G0VisualizerRuntime *runtime)
{
    return runtime == NULL ? 0U : g0_speed_ticks_per_second[runtime->speed_index];
}

uint64_t g0_visualizer_runtime_episode_tick(const G0VisualizerRuntime *runtime)
{
    return runtime == NULL ? UINT64_C(0) : runtime->episode_tick;
}

G0VisualizerFacing g0_visualizer_runtime_facing(const G0VisualizerRuntime *runtime)
{
    return runtime == NULL ? G0_VISUALIZER_FACING_SOUTH : runtime->facing;
}

const char *g0_visualizer_facing_name(G0VisualizerFacing facing)
{
    switch (facing)
    {
        case G0_VISUALIZER_FACING_NORTH: return "NORTH";
        case G0_VISUALIZER_FACING_EAST: return "EAST";
        case G0_VISUALIZER_FACING_SOUTH: return "SOUTH";
        case G0_VISUALIZER_FACING_WEST: return "WEST";
        default: return "SOUTH";
    }
}
const MiniSNNWorldsTerrain *g0_visualizer_runtime_terrain(
    const G0VisualizerRuntime *runtime)
{
    return runtime == NULL ? NULL : runtime->active.terrain;
}

const WF0FishWorldState *g0_visualizer_runtime_state(
    const G0VisualizerRuntime *runtime)
{
    return runtime == NULL ? NULL : &runtime->state;
}

const WF0FishTickRecord *g0_visualizer_runtime_last_record(
    const G0VisualizerRuntime *runtime)
{
    return runtime == NULL || runtime->has_last_record == 0 ? NULL :
        &runtime->last_record;
}

size_t g0_visualizer_runtime_food_count(const G0VisualizerRuntime *runtime)
{
    return runtime == NULL || runtime->active.domain == NULL ? 0U :
        minisnn_worlds_domain_food_count(runtime->active.domain);
}

int g0_visualizer_runtime_food_position_at(
    const G0VisualizerRuntime *runtime,
    size_t canonical_index,
    MiniSNNWorldsKernelPosition *out_position)
{
    MiniSNNWorldsDomainFoodInfo food;
    MiniSNNWorldsKernelTransform transform;

    if (runtime == NULL || out_position == NULL || runtime->active.domain == NULL ||
        runtime->active.kernel == NULL ||
        minisnn_worlds_domain_food_at(runtime->active.domain, canonical_index, &food) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_kernel_entity_transform(
            runtime->active.kernel, food.entity_id, &transform) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE)
        return 0;
    *out_position = transform.position;
    return 1;
}

int g0_visualizer_runtime_food_visible(const G0VisualizerRuntime *runtime)
{
    return g0_visualizer_runtime_food_count(runtime) > 0U;
}

int g0_visualizer_runtime_blocked_first_move(const G0VisualizerRuntime *runtime)
{
    return runtime != NULL && runtime->blocked_first_move != 0;
}

size_t g0_visualizer_layer_order(
    G0VisualizerLayer *out_layers,
    size_t capacity)
{
    static const G0VisualizerLayer order[G0_VISUALIZER_LAYER_COUNT] =
    {
        G0_VISUALIZER_LAYER_BASE_TERRAIN,
        G0_VISUALIZER_LAYER_TERRAIN_OBJECTS,
        G0_VISUALIZER_LAYER_WORLD_ENTITIES,
        G0_VISUALIZER_LAYER_WATER_OVERLAY,
        G0_VISUALIZER_LAYER_UI
    };

    if (out_layers != NULL && capacity >= G0_VISUALIZER_LAYER_COUNT)
        memcpy(out_layers, order, sizeof(order));
    return G0_VISUALIZER_LAYER_COUNT;
}

int g0_visualizer_tile_to_screen_scaled(
    uint32_t tile_x,
    uint32_t tile_y,
    int origin_x,
    int origin_y,
    int tile_pixels,
    G0VisualizerScreenPoint *out_point)
{
    int64_t screen_x;
    int64_t screen_y;

    if (out_point == NULL || tile_pixels <= 0)
        return 0;
    screen_x = (int64_t)origin_x + (int64_t)tile_x * (int64_t)tile_pixels;
    screen_y = (int64_t)origin_y + (int64_t)tile_y * (int64_t)tile_pixels;
    if (screen_x < INT_MIN || screen_x > INT_MAX ||
        screen_y < INT_MIN || screen_y > INT_MAX)
    {
        return 0;
    }
    out_point->screen_x = (int)screen_x;
    out_point->screen_y = (int)screen_y;
    return 1;
}

int g0_visualizer_tile_to_screen(
    uint32_t tile_x,
    uint32_t tile_y,
    int origin_x,
    int origin_y,
    G0VisualizerScreenPoint *out_point)
{
    return g0_visualizer_tile_to_screen_scaled(
        tile_x, tile_y, origin_x, origin_y, G0_VISUALIZER_TILE_PIXELS, out_point);
}
int g0_visualizer_screen_to_tile(
    uint32_t width,
    uint32_t height,
    int origin_x,
    int origin_y,
    int tile_pixels,
    int screen_x,
    int screen_y,
    uint32_t *out_tile_x,
    uint32_t *out_tile_y)
{
    int local_x;
    int local_y;
    uint32_t tile_x;
    uint32_t tile_y;

    if (width == 0U || height == 0U || tile_pixels <= 0 || out_tile_x == NULL ||
        out_tile_y == NULL)
        return 0;
    local_x = screen_x - origin_x;
    local_y = screen_y - origin_y;
    if (local_x < 0 || local_y < 0)
        return 0;
    tile_x = (uint32_t)(local_x / tile_pixels);
    tile_y = (uint32_t)(local_y / tile_pixels);
    if (tile_x >= width || tile_y >= height)
        return 0;
    *out_tile_x = tile_x;
    *out_tile_y = tile_y;
    return 1;
}

int g0_visualizer_world_to_screen_scaled(
    const G0VisualizerRuntime *runtime,
    MiniSNNWorldsKernelPosition world_position,
    int origin_x,
    int origin_y,
    int tile_pixels,
    G0VisualizerScreenPoint *out_point)
{
    uint32_t tile_x;
    uint32_t tile_y;

    return runtime != NULL && runtime->active.terrain != NULL &&
           minisnn_worlds_terrain_world_to_tile(
               runtime->active.terrain, world_position, &tile_x, &tile_y) ==
               MINISNN_WORLDS_TERRAIN_ERROR_NONE &&
           g0_visualizer_tile_to_screen_scaled(
               tile_x, tile_y, origin_x, origin_y, tile_pixels, out_point);
}

int g0_visualizer_world_to_screen(
    const G0VisualizerRuntime *runtime,
    MiniSNNWorldsKernelPosition world_position,
    int origin_x,
    int origin_y,
    G0VisualizerScreenPoint *out_point)
{
    return g0_visualizer_world_to_screen_scaled(
        runtime, world_position, origin_x, origin_y,
        G0_VISUALIZER_TILE_PIXELS, out_point);
}
const char *g0_visualizer_action_name(MiniSNNWorldsDomainActionType action)
{
    switch (action)
    {
        case MINISNN_WORLDS_DOMAIN_ACTION_WAIT: return "WAIT";
        case MINISNN_WORLDS_DOMAIN_ACTION_MOVE: return "MOVE";
        case MINISNN_WORLDS_DOMAIN_ACTION_EAT: return "EAT";
        default: return "NONE";
    }
}
