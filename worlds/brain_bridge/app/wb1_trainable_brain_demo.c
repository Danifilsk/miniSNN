#include "minisnn_worlds_brain_bridge.h"

#include <stdio.h>
#include <string.h>

#define DEMO_MAX_SEED UINT64_C(256)

typedef struct
{
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsDomain *domain;
    MiniSNNWorldsKernelEntityId actor;
    MiniSNNWorldsKernelEntityId food;
} DemoWorld;

static MiniSNNWorldsKernelEntityId demo_id(uint64_t value)
{
    MiniSNNWorldsKernelEntityId result = { value };
    return result;
}

static MiniSNNWorldsKernelTransform demo_at(
    MiniSNNWorldsKernelScalar x,
    MiniSNNWorldsKernelScalar y)
{
    MiniSNNWorldsKernelTransform result;

    result.position.x = x;
    result.position.y = y;
    result.orientation = 0U;
    return result;
}

static int demo_create_entity(
    MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelScalar x,
    MiniSNNWorldsKernelScalar y,
    MiniSNNWorldsKernelEntityId *out_id)
{
    MiniSNNWorldsKernelCommandId command;
    size_t count;

    if (minisnn_worlds_kernel_queue_create_entity(
            kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
            demo_id(0U), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
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
            demo_id(0U), *out_id, demo_at(x, y), &command) !=
            MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    return 1;
}

static int demo_world_create(DemoWorld *world)
{
    MiniSNNWorldsKernelConfig kernel_config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError kernel_error;
    MiniSNNWorldsDomainError domain_error;
    MiniSNNWorldsDomainSpeciesConfig species;

    if (world == NULL)
        return 0;
    memset(world, 0, sizeof(*world));
    kernel_config.space_bounds.min_x = -1000000;
    kernel_config.space_bounds.min_y = -1000000;
    kernel_config.space_bounds.max_x = 1000000;
    kernel_config.space_bounds.max_y = 1000000;
    world->kernel = minisnn_worlds_kernel_create(&kernel_config, &kernel_error);
    if (world->kernel == NULL ||
        !demo_create_entity(world->kernel, 0, 0, &world->actor) ||
        !demo_create_entity(world->kernel, 0, 0, &world->food))
    {
        return 0;
    }
    world->domain = minisnn_worlds_domain_create(world->kernel, &domain_error);
    if (world->domain == NULL || domain_error != MINISNN_WORLDS_DOMAIN_ERROR_NONE)
        return 0;
    species.species_id = UINT64_C(1);
    species.max_energy = UINT64_C(100);
    species.metabolism_per_tick = UINT64_C(1);
    species.move_energy_cost = UINT64_C(3);
    species.eat_range = 2000;
    return minisnn_worlds_domain_add_species(world->domain, &species) ==
               MINISNN_WORLDS_DOMAIN_ERROR_NONE &&
           minisnn_worlds_domain_register_organism(
               world->domain, world->actor, UINT64_C(1), UINT64_C(50)) ==
               MINISNN_WORLDS_DOMAIN_ERROR_NONE &&
           minisnn_worlds_domain_register_food(
               world->domain, world->food, UINT64_C(15)) ==
               MINISNN_WORLDS_DOMAIN_ERROR_NONE;
}

static void demo_world_destroy(DemoWorld *world)
{
    if (world == NULL)
        return;
    minisnn_worlds_domain_destroy(world->domain);
    minisnn_worlds_kernel_destroy(world->kernel);
    memset(world, 0, sizeof(*world));
}

static int demo_apply_action(
    MiniSNNWorldsTrainableBrain *brain,
    DemoWorld *world,
    const MiniSNNWorldsTrainableBrainReport *report,
    const MiniSNNWorldsDomainAction *action)
{
    MiniSNNWorldsDomainActionResult result;

    return minisnn_worlds_domain_step(world->domain, action, 1U, &result) ==
               MINISNN_WORLDS_DOMAIN_ERROR_NONE &&
           minisnn_worlds_trainable_brain_apply_action_result(
               brain, world->domain, world->actor, report->domain_tick,
               action, &result);
}

static int demo_find_eat_case(
    MiniSNNWorldsTrainableBrain **out_brain,
    DemoWorld *out_world,
    MiniSNNWorldsBrainConfig *out_config,
    MiniSNNWorldsDomainAction *out_action,
    MiniSNNWorldsTrainableBrainReport *out_report)
{
    uint64_t seed;

    if (out_brain == NULL || out_world == NULL || out_config == NULL ||
        out_action == NULL || out_report == NULL)
    {
        return 0;
    }
    *out_brain = NULL;
    memset(out_world, 0, sizeof(*out_world));
    for (seed = UINT64_C(1); seed <= DEMO_MAX_SEED; ++seed)
    {
        MiniSNNWorldsBrainConfig config =
            minisnn_worlds_trainable_brain_config_default();
        MiniSNNWorldsTrainableBrainError error;
        MiniSNNWorldsTrainableBrain *brain;
        MiniSNNWorldsDomainAction action;
        MiniSNNWorldsTrainableBrainReport report;
        DemoWorld world;

        config.topology = MINISNN_WORLDS_BRAIN_TOPOLOGY_RANDOM;
        config.connection_probability = 0.40;
        config.seed = seed;
        config.decision_steps_per_tick = 16U;
        config.excitatory_weight = 200.0;
        config.inhibitory_weight = -100.0;
        if (!demo_world_create(&world))
            return 0;
        brain = minisnn_worlds_trainable_brain_create(&config, &error);
        if (brain != NULL &&
            minisnn_worlds_trainable_brain_bind(brain, world.domain, world.actor) &&
            minisnn_worlds_trainable_brain_decide(
                brain, world.domain, world.actor, &action, &report) &&
            action.type == MINISNN_WORLDS_DOMAIN_ACTION_EAT)
        {
            *out_brain = brain;
            *out_world = world;
            *out_config = config;
            *out_action = action;
            *out_report = report;
            return 1;
        }
        minisnn_worlds_trainable_brain_destroy(&brain);
        demo_world_destroy(&world);
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *directory = argc > 1 ? argv[1] : "build/worlds/brain_bridge/results/wb1_demo";
    MiniSNNWorldsTrainableBrain *brain = NULL;
    MiniSNNWorldsTrainableBrain *loaded = NULL;
    MiniSNNWorldsBrainConfig config;
    MiniSNNWorldsBrainConfig loaded_config;
    MiniSNNWorldsTrainableBrainError error;
    MiniSNNWorldsDomainAction action;
    MiniSNNWorldsTrainableBrainReport report;
    DemoWorld world;
    uint64_t topology_signature;
    uint64_t config_signature;
    uint64_t initial_signature;
    uint64_t learned_signature;
    uint64_t reset_signature;
    uint64_t evaluation_signature;
    uint64_t loaded_signature;
    char checkpoint[512];
    char summary_path[512];
    FILE *summary = NULL;
    int ok = 0;
    const char *stage = "initialization";

    memset(&world, 0, sizeof(world));
    stage = "creating generic training case";
    if (snprintf(checkpoint, sizeof(checkpoint), "%s/brain.wb1", directory) < 0 ||
        snprintf(summary_path, sizeof(summary_path), "%s/summary.txt", directory) < 0 ||
        !demo_find_eat_case(&brain, &world, &config, &action, &report))
    {
        goto done;
    }
    topology_signature = minisnn_worlds_trainable_brain_topology_signature(brain);

    initial_signature = minisnn_worlds_trainable_brain_weight_signature(brain);
    stage = "delivering initial training reward";
    if (!demo_apply_action(brain, &world, &report, &action) ||
        !minisnn_worlds_trainable_brain_set_mode(
            brain, MINISNN_WORLDS_BRAIN_MODE_EVALUATION))
    {
        goto done;
    }
    learned_signature = minisnn_worlds_trainable_brain_weight_signature(brain);
    stage = "frozen evaluation and episode reset";
    if (learned_signature == initial_signature ||
        !minisnn_worlds_trainable_brain_decide(
            brain, world.domain, world.actor, &action, &report) ||
        !demo_apply_action(brain, &world, &report, &action) ||
        minisnn_worlds_trainable_brain_weight_signature(brain) != learned_signature ||
        !minisnn_worlds_trainable_brain_set_mode(
            brain, MINISNN_WORLDS_BRAIN_MODE_TRAINING) ||
        !minisnn_worlds_trainable_brain_reset_episode(brain))
    {
        goto done;
    }
    reset_signature = minisnn_worlds_trainable_brain_weight_signature(brain);
    if (reset_signature != learned_signature)
        goto done;
    demo_world_destroy(&world);
    stage = "resumed training";
    if (!demo_world_create(&world) ||
        !minisnn_worlds_trainable_brain_bind(brain, world.domain, world.actor) ||
        !minisnn_worlds_trainable_brain_decide(
            brain, world.domain, world.actor, &action, &report) ||
        action.type != MINISNN_WORLDS_DOMAIN_ACTION_EAT ||
        !demo_apply_action(brain, &world, &report, &action) ||
        !minisnn_worlds_trainable_brain_set_mode(
            brain, MINISNN_WORLDS_BRAIN_MODE_EVALUATION))
    {
        goto done;
    }
    evaluation_signature = learned_signature;
    stage = "resumed reward was not applied";
    if (minisnn_worlds_trainable_brain_weight_signature(brain) == evaluation_signature)
        goto done;
    config_signature = minisnn_worlds_trainable_brain_config_signature(brain);
    stage = "safe save after learning";
    if (!minisnn_worlds_trainable_brain_save(brain, checkpoint, &error))
    {
        fprintf(stderr, "WB1 save error: %s\n",
                minisnn_worlds_trainable_brain_error_string(error));
        goto done;
    }
    stage = "loading persisted brain";
    loaded = minisnn_worlds_trainable_brain_load(checkpoint, &error);
    if (loaded == NULL)
    {
        fprintf(stderr, "WB1 load error: %s\n",
                minisnn_worlds_trainable_brain_error_string(error));
        goto done;
    }
    stage = "verifying persisted brain";
    if (!minisnn_worlds_trainable_brain_get_config(loaded, &loaded_config) ||
        minisnn_worlds_trainable_brain_config_signature(loaded) != config_signature)
        goto done;
    loaded_signature = minisnn_worlds_trainable_brain_weight_signature(loaded);
    stage = "final episode reset";
    if (loaded_signature != minisnn_worlds_trainable_brain_weight_signature(brain) ||
        !minisnn_worlds_trainable_brain_reset_episode(brain))
    {
        goto done;
    }    stage = "writing summary";
    summary = fopen(summary_path, "wb");
    if (summary == NULL)
        goto done;
    fprintf(summary,
        "brain=%s\nneuron_model=%s\ntopology=%s\nneurons=%u\n"
        "topology_signature=%016llx\nconfig_signature=%016llx\n"
        "initial_weight_signature=%016llx\nlearned_weight_signature=%016llx\n"
        "post_episode_reset_weight_signature=%016llx\n"
        "evaluation_weight_signature=%016llx\nloaded_weight_signature=%016llx\n"
        "core_tick=%d\nlearning_resumed=true\n",
        loaded_config.brain_name,
        minisnn_neuron_model_name(loaded_config.neural_config.neuron_model),
        minisnn_worlds_brain_topology_name(loaded_config.topology),
        loaded_config.neuron_count,
        (unsigned long long)topology_signature,
        (unsigned long long)config_signature,
        (unsigned long long)initial_signature,
        (unsigned long long)learned_signature,
        (unsigned long long)reset_signature,
        (unsigned long long)evaluation_signature,
        (unsigned long long)loaded_signature,
        minisnn_worlds_trainable_brain_core_step(brain));
    if (fclose(summary) != 0)
    {
        summary = NULL;
        goto done;
    }
    summary = NULL;
    ok = 1;

done:
    if (summary != NULL)
        fclose(summary);
    minisnn_worlds_trainable_brain_destroy(&loaded);
    minisnn_worlds_trainable_brain_destroy(&brain);
    demo_world_destroy(&world);
    if (!ok)
    {
        fprintf(stderr, "WB1 trainable brain demo failed at %s\n", stage);
        return 1;
    }
    puts("WB1 trainable brain demo OK");
    return 0;
}