#include "wf1_trainable_fish.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WF1_SPECIES_ID UINT64_C(101)
#define WF1_MAX_ENERGY UINT64_C(60)
#define WF1_INITIAL_ENERGY UINT64_C(36)
#define WF1_METABOLISM_PER_TICK UINT64_C(1)
#define WF1_MOVE_ENERGY_COST UINT64_C(1)
#define WF1_EAT_RANGE 0
#define WF1_FOOD_NUTRITION UINT64_C(12)
#define WF1_FOOD_COUNT 10U
#define WF1_FOOD_GRID_RADIUS INT64_C(5)
#define WF1_GRID_STEP INT64_C(1000)
#define WF1_WORLD_BOUND INT64_C(8000)
#define WF1_PATH_MAX 480U

typedef struct
{
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsDomain *domain;
    MiniSNNWorldsKernelEntityId actor;
} WF1EpisodeWorld;

typedef struct
{
    double untrained_food_sum;
    double trained_food_sum;
    double control_food_sum;
    uint32_t untrained_successes;
    uint32_t trained_successes;
    uint32_t control_successes;
    uint64_t initial_signature;
    uint64_t trained_signature;
    uint64_t loaded_signature;
    uint64_t evaluation_before;
    uint64_t evaluation_after;
} WF1SeedAggregate;

static MiniSNNWorldsKernelEntityId no_entity(void)
{
    MiniSNNWorldsKernelEntityId id = { UINT64_C(0) };
    return id;
}

static MiniSNNWorldsKernelEntityId entity_id(uint64_t value)
{
    MiniSNNWorldsKernelEntityId id = { value };
    return id;
}

static MiniSNNWorldsKernelTransform transform_at(
    MiniSNNWorldsKernelScalar x,
    MiniSNNWorldsKernelScalar y)
{
    MiniSNNWorldsKernelTransform transform;

    transform.position.x = x;
    transform.position.y = y;
    transform.orientation = 0U;
    return transform;
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

static uint64_t world_random_next(uint64_t *state)
{
    uint64_t value;

    value = *state;
    value ^= value >> 12U;
    value ^= value << 25U;
    value ^= value >> 27U;
    *state = value;
    return value * UINT64_C(2685821657736338717);
}

static void food_position(uint64_t *state, uint32_t index,
                          int64_t grid_radius,
                          MiniSNNWorldsKernelScalar *out_x,
                          MiniSNNWorldsKernelScalar *out_y)
{
    uint64_t span = (uint64_t)(grid_radius * INT64_C(2) + INT64_C(1));
    int64_t x = (int64_t)(world_random_next(state) % span) - grid_radius;
    int64_t y = (int64_t)(world_random_next(state) % span) - grid_radius;

    if (x == 0 && y == 0)
    {
        x = (index & 1U) == 0U ? 1 : -1;
    }
    *out_x = x * WF1_GRID_STEP;
    *out_y = y * WF1_GRID_STEP;
}

static int actor_info(const MiniSNNWorldsDomain *domain,
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

static int episode_world_create(WF1EpisodeWorld *world,
                                const WF1TrainableFishConfig *config,
                                uint64_t world_seed)
{
    MiniSNNWorldsKernelConfig kernel_config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError kernel_error;
    MiniSNNWorldsDomainError domain_error;
    MiniSNNWorldsDomainSpeciesConfig species;
    uint64_t random_state = world_seed == 0U ? UINT64_C(1) : world_seed;
    uint32_t index;

    if (world == NULL || config == NULL)
        return 0;
    memset(world, 0, sizeof(*world));
    kernel_config.master_seed = world_seed;
    kernel_config.space_bounds.min_x = -config->world_bound;
    kernel_config.space_bounds.min_y = -config->world_bound;
    kernel_config.space_bounds.max_x = config->world_bound;
    kernel_config.space_bounds.max_y = config->world_bound;
    world->kernel = minisnn_worlds_kernel_create(&kernel_config, &kernel_error);
    if (world->kernel == NULL || kernel_error != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        !create_placed_entity(world->kernel, 0, 0, &world->actor))
    {
        return 0;
    }
    for (index = 0U; index < config->food_count; ++index)
    {
        MiniSNNWorldsKernelEntityId food;
        MiniSNNWorldsKernelScalar x;
        MiniSNNWorldsKernelScalar y;

        food_position(&random_state, index, config->food_grid_radius, &x, &y);
        if (!create_placed_entity(world->kernel, x, y, &food))
            return 0;
    }
    world->domain = minisnn_worlds_domain_create(world->kernel, &domain_error);
    if (world->domain == NULL || domain_error != MINISNN_WORLDS_DOMAIN_ERROR_NONE)
        return 0;
    species.species_id = WF1_SPECIES_ID;
    species.max_energy = config->max_energy;
    species.metabolism_per_tick = config->metabolism_per_tick;
    species.move_energy_cost = config->move_energy_cost;
    species.eat_range = config->eat_range;
    if (minisnn_worlds_domain_add_species(world->domain, &species) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_register_organism(
            world->domain, world->actor, species.species_id,
            config->initial_energy) != MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        return 0;
    }
    for (index = 1U; index <= config->food_count; ++index)
    {
        if (minisnn_worlds_domain_register_food(
                world->domain, entity_id((uint64_t)index + UINT64_C(1)),
                config->food_nutrition) != MINISNN_WORLDS_DOMAIN_ERROR_NONE)
        {
            return 0;
        }
    }
    return 1;
}

static void episode_world_destroy(WF1EpisodeWorld *world)
{
    if (world == NULL)
        return;
    minisnn_worlds_domain_destroy(world->domain);
    minisnn_worlds_kernel_destroy(world->kernel);
    memset(world, 0, sizeof(*world));
}

WF1TrainableFishConfig wf1_trainable_fish_config_default(void)
{
    WF1TrainableFishConfig config;

    memset(&config, 0, sizeof(config));
    config.brain_config = minisnn_worlds_trainable_brain_config_default();
    snprintf(config.brain_config.brain_name, sizeof(config.brain_config.brain_name),
             "%s", "wf1_trainable_fish");
    config.brain_config.topology = MINISNN_WORLDS_BRAIN_TOPOLOGY_RANDOM;
    config.brain_config.neuron_count = 24U;
    config.brain_config.neural_config.neuron_count = 24;
    config.brain_config.neural_config.max_synaptic_delay = 1;
    config.brain_config.inhibitory_count = 4U;
    config.brain_config.connection_probability = 0.30;
    config.brain_config.excitatory_weight = 200.0;
    config.brain_config.inhibitory_weight = -180.0;
    config.brain_config.decision_steps_per_tick = 16U;
    config.brain_config.mode = MINISNN_WORLDS_BRAIN_MODE_TRAINING;
    config.brain_config.plasticity_enabled = 1;
    config.brain_config.reward_profile.eat_applied_reward = 1.0;
    /* WF1 rewards only real EAT gain and terminal scenario feedback. */
    config.brain_config.reward_profile.rejected_action_penalty = 0.0;
    config.brain_seeds[0] = UINT64_C(101);
    config.brain_seeds[1] = UINT64_C(202);
    config.brain_seeds[2] = UINT64_C(303);
    config.brain_seeds[3] = UINT64_C(404);
    config.brain_seed_count = 4U;
    config.training_episodes = 32U;
    config.evaluation_episodes = 12U;
    config.max_episode_ticks = 72U;
    config.training_world_seed_base = UINT64_C(100000);
    config.evaluation_world_seed_base = UINT64_C(900000);
    config.starvation_terminal_reward = -1.0;
    config.max_energy = WF1_MAX_ENERGY;
    config.initial_energy = WF1_INITIAL_ENERGY;
    config.metabolism_per_tick = WF1_METABOLISM_PER_TICK;
    config.move_energy_cost = WF1_MOVE_ENERGY_COST;
    config.eat_range = WF1_EAT_RANGE;
    config.food_nutrition = WF1_FOOD_NUTRITION;
    config.food_count = WF1_FOOD_COUNT;
    config.food_grid_radius = WF1_FOOD_GRID_RADIUS;
    config.world_bound = WF1_WORLD_BOUND;
    return config;
}

int wf1_trainable_fish_config_is_valid(const WF1TrainableFishConfig *config)
{
    uint32_t index;

    if (config == NULL || config->brain_seed_count == 0U ||
        config->brain_seed_count > WF1_TRAINABLE_FISH_MAX_BRAIN_SEEDS ||
        config->training_episodes == 0U || config->evaluation_episodes == 0U ||
        config->max_episode_ticks == 0U || !isfinite(config->starvation_terminal_reward) ||
        config->starvation_terminal_reward > 0.0 || config->max_energy == 0U ||
        config->initial_energy == 0U || config->initial_energy > config->max_energy ||
        config->metabolism_per_tick == 0U || config->food_nutrition == 0U ||
        config->food_count == 0U || config->food_count > WF1_TRAINABLE_FISH_MAX_FOOD_COUNT ||
        config->food_grid_radius <= 0 || config->food_grid_radius > INT64_C(1000) ||
        config->world_bound <= 0 ||
        config->world_bound < config->food_grid_radius * WF1_GRID_STEP ||
        config->eat_range < 0 ||
        !minisnn_worlds_trainable_brain_config_is_valid(&config->brain_config))
    {
        return 0;
    }
    for (index = 0U; index < config->brain_seed_count; ++index)
    {
        if (config->brain_seeds[index] == 0U)
            return 0;
    }
    return 1;
}

const char *wf1_trainable_fish_phase_name(WF1TrainableFishPhase phase)
{
    switch (phase)
    {
        case WF1_TRAINABLE_FISH_PHASE_UNTRAINED: return "UNTRAINED";
        case WF1_TRAINABLE_FISH_PHASE_TRAINED: return "TRAINED";
        case WF1_TRAINABLE_FISH_PHASE_CONTROL: return "CONTROL";
        case WF1_TRAINABLE_FISH_PHASE_LOADED: return "LOADED";
        default: return "UNKNOWN";
    }
}

const char *wf1_trainable_fish_result_name(WF1TrainableFishResult result)
{
    switch (result)
    {
        case WF1_TRAINABLE_FISH_RESULT_LEARNING_DETECTED: return "LEARNING_DETECTED";
        case WF1_TRAINABLE_FISH_RESULT_NO_IMPROVEMENT: return "NO_IMPROVEMENT";
        default: return "INCONCLUSIVE";
    }
}

static void record_exploration(
    WF1TrainableFishEpisodeResult *result,
    const MiniSNNWorldsTrainableBrainReport *report,
    const MiniSNNWorldsDomainAction *action,
    const MiniSNNWorldsDomainActionResult *action_result)
{
    uint32_t index;
    uint32_t score_sum = 0U;

    result->decision_count++;
    result->total_spikes += report->total_spikes;
    if (report->total_spikes != 0U)
        result->active_decision_count++;
    for (index = 0U; index < MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1; ++index)
        score_sum += report->output_scores[index];
    if (score_sum == 0U)
        result->zero_score_decisions++;
    if (report->tie_break_used != 0U)
        result->tied_winner_decisions++;

    switch (report->selected_channel)
    {
        case MINISNN_WORLDS_BRAIN_ACTION_WAIT:
            result->wait_selected++;
            break;
        case MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_X:
            result->move_pos_x_selected++;
            break;
        case MINISNN_WORLDS_BRAIN_ACTION_MOVE_NEG_X:
            result->move_neg_x_selected++;
            break;
        case MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_Y:
            result->move_pos_y_selected++;
            break;
        case MINISNN_WORLDS_BRAIN_ACTION_MOVE_NEG_Y:
            result->move_neg_y_selected++;
            break;
        case MINISNN_WORLDS_BRAIN_ACTION_EAT:
            result->eat_selected++;
            break;
        default:
            break;
    }

    if (action->type == MINISNN_WORLDS_DOMAIN_ACTION_MOVE)
    {
        if (action_result->status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED)
            result->move_applied++;
        else if (action_result->status == MINISNN_WORLDS_DOMAIN_ACTION_REJECTED)
            result->move_rejected++;
    }
    if (action->type == MINISNN_WORLDS_DOMAIN_ACTION_EAT)
    {
        if (action_result->status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED)
            result->eat_applied++;
        else if (action_result->status == MINISNN_WORLDS_DOMAIN_ACTION_REJECTED)
            result->eat_rejected++;
        if (action_result->status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED &&
            action_result->energy_after > action_result->energy_before)
        {
            result->successful_eat_count++;
            result->positive_reward_count++;
        }
    }
}
int wf1_trainable_fish_run_episode_observed(
    MiniSNNWorldsTrainableBrain *brain,
    const WF1TrainableFishConfig *config,
    WF1TrainableFishPhase phase,
    uint64_t brain_seed,
    uint64_t world_seed,
    WF1TrainableFishRewardObserver observer,
    void *user_data,
    WF1TrainableFishEpisodeResult *out_result)
{
    WF1EpisodeWorld world;
    WF1TrainableFishEpisodeResult result;
    MiniSNNWorldsBrainConfig brain_config;
    MiniSNNWorldsDomainOrganismInfo organism;
    MiniSNNWorldsDomainAction action;
    MiniSNNWorldsTrainableBrainReport report;
    MiniSNNWorldsDomainActionResult action_result;
    MiniSNNWorldsDomainDiagnostics diagnostics;
    WF1TrainableFishRewardObservation pending_observation;
    uint32_t tick;
    int positive_reward_pending = 0;
    int alive = 1;
    int reset_needed = 0;
    int ok = 0;

    if (brain == NULL || out_result == NULL || !wf1_trainable_fish_config_is_valid(config) ||
        phase > WF1_TRAINABLE_FISH_PHASE_LOADED)
    {
        return 0;
    }
    memset(&world, 0, sizeof(world));
    memset(&result, 0, sizeof(result));
    result.phase = phase;
    result.brain_seed = brain_seed;
    result.world_seed = world_seed;
    result.weight_signature_before = minisnn_worlds_trainable_brain_weight_signature(brain);
    if (result.weight_signature_before == 0U ||
        !minisnn_worlds_trainable_brain_get_config(brain, &brain_config) ||
        !episode_world_create(&world, config, world_seed) ||
        !minisnn_worlds_trainable_brain_bind(brain, world.domain, world.actor))
    {
        goto done;
    }
    reset_needed = 1;
    for (tick = 0U; tick < config->max_episode_ticks; ++tick)
    {
        if (!minisnn_worlds_trainable_brain_decide(
                brain, world.domain, world.actor, &action, &report))
        {
            goto done;
        }
        if (positive_reward_pending)
        {
            MiniSNNRewardStats after_stats;

            if (!minisnn_worlds_trainable_brain_get_reward_stats(brain, &after_stats) ||
                after_stats.last_applied_reward != pending_observation.reward ||
                after_stats.reward_event_count !=
                    pending_observation.stats_before.reward_event_count + UINT64_C(1) ||
                after_stats.positive_reward_event_count !=
                    pending_observation.stats_before.positive_reward_event_count + UINT64_C(1) ||
                (pending_observation.weight_signature_after =
                    minisnn_worlds_trainable_brain_weight_signature(brain)) == 0U)
            {
                goto done;
            }
            pending_observation.core_tick = report.core_tick;
            pending_observation.stats_after = after_stats;
            if (!observer(&pending_observation, user_data))
                goto done;
            positive_reward_pending = 0;
        }
        if (minisnn_worlds_domain_step(
                world.domain, &action, 1U, &action_result) !=
                MINISNN_WORLDS_DOMAIN_ERROR_NONE)
        {
            goto done;
        }
        if (observer != NULL && action.type == MINISNN_WORLDS_DOMAIN_ACTION_EAT &&
            action_result.status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED &&
            action_result.energy_after > action_result.energy_before)
        {
            memset(&pending_observation, 0, sizeof(pending_observation));
            if (!minisnn_worlds_trainable_brain_get_reward_stats(
                    brain, &pending_observation.stats_before) ||
                (pending_observation.weight_signature_before =
                    minisnn_worlds_trainable_brain_weight_signature(brain)) == 0U)
            {
                goto done;
            }
            pending_observation.brain_seed = brain_seed;
            pending_observation.world_seed = world_seed;
            pending_observation.episode_tick = tick;
            pending_observation.action = MINISNN_WORLDS_BRAIN_ACTION_EAT;
            pending_observation.reward = brain_config.reward_profile.eat_applied_reward;
            pending_observation.energy_gain = (double)(action_result.energy_after -
                                                        action_result.energy_before);
        }
        if (!minisnn_worlds_trainable_brain_apply_action_result(
                brain, world.domain, world.actor, report.domain_tick,
                &action, &action_result))
        {
            goto done;
        }
        if (observer != NULL && action.type == MINISNN_WORLDS_DOMAIN_ACTION_EAT &&
            action_result.status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED &&
            action_result.energy_after > action_result.energy_before)
        {
            positive_reward_pending = 1;
        }
        result.ticks++;
        record_exploration(&result, &report, &action, &action_result);
        if (action_result.status == MINISNN_WORLDS_DOMAIN_ACTION_REJECTED)
            result.rejected_actions++;
        result.cumulative_reward += minisnn_worlds_trainable_brain_last_reward(brain);
        if (!actor_info(world.domain, world.actor, &organism))
            goto done;
        alive = organism.life_state == MINISNN_WORLDS_DOMAIN_LIFE_ALIVE;
        if (!alive)
        {
            if (organism.death_cause != MINISNN_WORLDS_DOMAIN_DEATH_CAUSE_STARVATION)
                goto done;
            result.starvation_count = 1U;
            if (brain_config.mode == MINISNN_WORLDS_BRAIN_MODE_TRAINING &&
                !minisnn_worlds_trainable_brain_apply_terminal_feedback(
                    brain, config->starvation_terminal_reward))
            {
                goto done;
            }
            if (brain_config.mode == MINISNN_WORLDS_BRAIN_MODE_TRAINING)
            {
                result.terminal_reward_count++;
                result.cumulative_reward += config->starvation_terminal_reward;
            }
            break;
        }
    }
    if (positive_reward_pending)
        goto done;
    /* A live timeout is truncation. Training still closes the AgentCycle at a
     * safe terminal boundary with zero scenario reward, preserving any due
     * real EAT feedback without declaring a death. */
    if (alive && brain_config.mode == MINISNN_WORLDS_BRAIN_MODE_TRAINING &&
        !minisnn_worlds_trainable_brain_apply_terminal_feedback(brain, 0.0))
    {
        goto done;
    }
    if (minisnn_worlds_domain_get_diagnostics(world.domain, &diagnostics) !=
        MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        goto done;
    }
    result.foods_eaten = (uint32_t)diagnostics.total_food_consumed;
    result.weight_signature_after = minisnn_worlds_trainable_brain_weight_signature(brain);
    if (result.weight_signature_after == 0U ||
        !minisnn_worlds_trainable_brain_reset_episode(brain))
    {
        goto done;
    }
    reset_needed = 0;
    *out_result = result;
    ok = 1;

done:
    if (reset_needed)
        (void)minisnn_worlds_trainable_brain_reset_episode(brain);
    episode_world_destroy(&world);
    return ok;
}

int wf1_trainable_fish_run_episode(
    MiniSNNWorldsTrainableBrain *brain,
    const WF1TrainableFishConfig *config,
    WF1TrainableFishPhase phase,
    uint64_t brain_seed,
    uint64_t world_seed,
    WF1TrainableFishEpisodeResult *out_result)
{
    return wf1_trainable_fish_run_episode_observed(
        brain, config, phase, brain_seed, world_seed, NULL, NULL, out_result);
}

static int write_training_header(FILE *file)
{
    return fprintf(file,
        "brain_seed,episode,world_seed,ticks,foods_eaten,cumulative_reward,"
        "death_cause,initial_or_start_weight_signature,end_weight_signature\n") >= 0;
}

static int write_evaluation_header(FILE *file)
{
    return fprintf(file,
        "phase,brain_seed,world_seed,ticks,foods_eaten,success,cumulative_reward,"
        "weight_signature_before,weight_signature_after\n") >= 0;
}

static int write_training_row(FILE *file, uint32_t episode,
                              const WF1TrainableFishEpisodeResult *result)
{
    return fprintf(file, "%" PRIu64 ",%u,%" PRIu64 ",%u,%u,%.17g,%s,%016" PRIx64 ",%016" PRIx64 "\n",
        result->brain_seed, episode, result->world_seed, result->ticks,
        result->foods_eaten, result->cumulative_reward,
        result->starvation_count != 0U ? "STARVATION" : "NONE",
        result->weight_signature_before, result->weight_signature_after) >= 0;
}

static int write_evaluation_row(FILE *file,
                                const WF1TrainableFishEpisodeResult *result)
{
    return fprintf(file, "%s,%" PRIu64 ",%" PRIu64 ",%u,%u,%u,%.17g,%016" PRIx64 ",%016" PRIx64 "\n",
        wf1_trainable_fish_phase_name(result->phase), result->brain_seed,
        result->world_seed, result->ticks, result->foods_eaten,
        result->foods_eaten != 0U ? 1U : 0U, result->cumulative_reward,
        result->weight_signature_before, result->weight_signature_after) >= 0;
}

static int create_brain(const WF1TrainableFishConfig *config, uint64_t seed,
                        MiniSNNWorldsBrainMode mode,
                        MiniSNNWorldsTrainableBrain **out_brain)
{
    MiniSNNWorldsBrainConfig brain_config = config->brain_config;
    MiniSNNWorldsTrainableBrainError error;

    brain_config.seed = seed;
    brain_config.mode = mode;
    brain_config.plasticity_enabled = mode == MINISNN_WORLDS_BRAIN_MODE_TRAINING ? 1 : 0;
    *out_brain = minisnn_worlds_trainable_brain_create(&brain_config, &error);
    return *out_brain != NULL && error == MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE;
}

static uint64_t world_seed_for(uint64_t base, uint32_t seed_index, uint32_t episode)
{
    return base + (uint64_t)seed_index * UINT64_C(1000) + (uint64_t)episode;
}

static int run_evaluation_set(MiniSNNWorldsTrainableBrain *brain,
                              const WF1TrainableFishConfig *config,
                              WF1TrainableFishPhase phase,
                              uint32_t seed_index,
                              uint64_t brain_seed,
                              FILE *evaluation,
                              double *out_food_sum,
                              uint32_t *out_successes)
{
    uint32_t episode;

    for (episode = 0U; episode < config->evaluation_episodes; ++episode)
    {
        WF1TrainableFishEpisodeResult result;
        if (!wf1_trainable_fish_run_episode(
                brain, config, phase, brain_seed,
                world_seed_for(config->evaluation_world_seed_base, seed_index, episode),
                &result) || !write_evaluation_row(evaluation, &result))
        {
            return 0;
        }
        *out_food_sum += (double)result.foods_eaten;
        *out_successes += result.foods_eaten != 0U ? 1U : 0U;
    }
    return 1;
}

static int write_seed_header(FILE *file)
{
    return fprintf(file,
        "brain_seed,initial_weight_signature,trained_weight_signature,"
        "loaded_weight_signature,untrained_mean_food,trained_mean_food,"
        "control_mean_food,untrained_success_rate,trained_success_rate,"
        "control_success_rate\n") >= 0;
}

static int write_seed_row(FILE *file, uint64_t seed, const WF1SeedAggregate *aggregate,
                          uint32_t evaluation_episodes)
{
    double denominator = (double)evaluation_episodes;

    return fprintf(file,
        "%" PRIu64 ",%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64 ","
        "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n",
        seed, aggregate->initial_signature, aggregate->trained_signature,
        aggregate->loaded_signature,
        aggregate->untrained_food_sum / denominator,
        aggregate->trained_food_sum / denominator,
        aggregate->control_food_sum / denominator,
        (double)aggregate->untrained_successes / denominator,
        (double)aggregate->trained_successes / denominator,
        (double)aggregate->control_successes / denominator) >= 0;
}

static int write_summary(const char *path, const WF1TrainableFishConfig *config,
                         const WF1TrainableFishExperimentSummary *summary)
{
    FILE *file = fopen(path, "wb");
    int ok;

    if (file == NULL)
        return 0;
    ok = fprintf(file,
        "scenario=WF1-A first trainable fish experiment\n"
        "brain_topology=%s\ntopology_signature=%016" PRIx64 "\n"
        "brain_config_signature=%016" PRIx64 "\nneuron_count=%u\ndecision_steps_per_tick=%u\n"
        "brain_seed_count=%u\ntraining_episode_count=%u\nevaluation_episode_count=%u\n"
        "training_world_seed_base=%" PRIu64 "\nevaluation_world_seed_base=%" PRIu64 "\n"
        "initial_weight_signature=%016" PRIx64 "\ntrained_weight_signature=%016" PRIx64 "\n"
        "loaded_weight_signature=%016" PRIx64 "\n"
        "evaluation_weight_before=%016" PRIx64 "\nevaluation_weight_after=%016" PRIx64 "\n"
        "untrained_mean_food=%.17g\ntrained_mean_food=%.17g\ncontrol_mean_food=%.17g\n"
        "untrained_success_rate=%.17g\ntrained_success_rate=%.17g\ncontrol_success_rate=%.17g\n"
        "positive_reward=EAT_APPLIED_ENERGY_GAIN\nstarvation_terminal_reward=%.17g\n"
        "evaluation_plasticity=OFF\nresult=%s\n",
        minisnn_worlds_brain_topology_name(config->brain_config.topology),
        summary->topology_signature, summary->brain_config_signature,
        config->brain_config.neuron_count, config->brain_config.decision_steps_per_tick,
        config->brain_seed_count, config->training_episodes, config->evaluation_episodes,
        config->training_world_seed_base, config->evaluation_world_seed_base,
        summary->initial_weight_signature, summary->trained_weight_signature,
        summary->loaded_weight_signature, summary->evaluation_weight_signature_before,
        summary->evaluation_weight_signature_after, summary->untrained_mean_food,
        summary->trained_mean_food, summary->control_mean_food,
        summary->untrained_success_rate, summary->trained_success_rate,
        summary->control_success_rate, config->starvation_terminal_reward,
        wf1_trainable_fish_result_name(summary->result)) >= 0;
    return fclose(file) == 0 && ok;
}

int wf1_trainable_fish_run_experiment(
    const WF1TrainableFishConfig *config,
    const char *output_directory,
    WF1TrainableFishExperimentSummary *out_summary)
{
    WF1TrainableFishExperimentSummary summary;
    WF1SeedAggregate aggregates[WF1_TRAINABLE_FISH_MAX_BRAIN_SEEDS];
    char training_path[WF1_PATH_MAX];
    char evaluation_path[WF1_PATH_MAX];
    char seeds_path[WF1_PATH_MAX];
    char summary_path[WF1_PATH_MAX];
    FILE *training = NULL;
    FILE *evaluation = NULL;
    FILE *seeds = NULL;
    uint32_t seed_index;
    uint32_t improved_seed_count = 0U;
    const char *stage = "opening artifacts";
    int ok = 0;

    if (out_summary == NULL || output_directory == NULL || output_directory[0] == '\0' ||
        !wf1_trainable_fish_config_is_valid(config) ||
        snprintf(training_path, sizeof(training_path), "%s/training.csv", output_directory) < 0 ||
        snprintf(evaluation_path, sizeof(evaluation_path), "%s/evaluation.csv", output_directory) < 0 ||
        snprintf(seeds_path, sizeof(seeds_path), "%s/seeds.csv", output_directory) < 0 ||
        snprintf(summary_path, sizeof(summary_path), "%s/summary.txt", output_directory) < 0)
    {
        return 0;
    }
    memset(&summary, 0, sizeof(summary));
    memset(aggregates, 0, sizeof(aggregates));
    training = fopen(training_path, "wb");
    evaluation = fopen(evaluation_path, "wb");
    seeds = fopen(seeds_path, "wb");
    if (training == NULL || evaluation == NULL || seeds == NULL ||
        !write_training_header(training) || !write_evaluation_header(evaluation) ||
        !write_seed_header(seeds))
    {
        goto done;
    }
    for (seed_index = 0U; seed_index < config->brain_seed_count; ++seed_index)
    {
        MiniSNNWorldsTrainableBrain *baseline = NULL;
        MiniSNNWorldsTrainableBrain *trained = NULL;
        MiniSNNWorldsTrainableBrain *control = NULL;
        MiniSNNWorldsTrainableBrain *loaded = NULL;
        MiniSNNWorldsTrainableBrainError error;
        uint64_t brain_seed = config->brain_seeds[seed_index];
        char checkpoint[WF1_PATH_MAX];
        uint32_t episode;
        WF1SeedAggregate *aggregate = &aggregates[seed_index];

        stage = "creating untrained brain";
        if (!create_brain(config, brain_seed, MINISNN_WORLDS_BRAIN_MODE_EVALUATION,
                          &baseline))
        {
            goto per_seed_done;
        }
        aggregate->initial_signature =
            minisnn_worlds_trainable_brain_weight_signature(baseline);
        stage = "running untrained evaluation";
        if (!run_evaluation_set(baseline, config, WF1_TRAINABLE_FISH_PHASE_UNTRAINED,
                                seed_index, brain_seed, evaluation,
                                &aggregate->untrained_food_sum,
                                &aggregate->untrained_successes) ||
            minisnn_worlds_trainable_brain_weight_signature(baseline) !=
                aggregate->initial_signature ||
            !create_brain(config, brain_seed, MINISNN_WORLDS_BRAIN_MODE_TRAINING,
                          &trained))
        {
            goto per_seed_done;
        }
        if (seed_index == 0U)
        {
            summary.topology_signature =
                minisnn_worlds_trainable_brain_topology_signature(trained);
            summary.brain_config_signature =
                minisnn_worlds_trainable_brain_config_signature(trained);
        }
        if (minisnn_worlds_trainable_brain_weight_signature(trained) !=
            aggregate->initial_signature)
        {
            goto per_seed_done;
        }
        stage = "running training";
        for (episode = 0U; episode < config->training_episodes; ++episode)
        {
            WF1TrainableFishEpisodeResult result;
            if (!wf1_trainable_fish_run_episode(
                    trained, config, WF1_TRAINABLE_FISH_PHASE_TRAINED, brain_seed,
                    world_seed_for(config->training_world_seed_base, seed_index, episode),
                    &result) || !write_training_row(training, episode, &result))
            {
                goto per_seed_done;
            }
        }
        stage = "freezing trained evaluation";
        aggregate->trained_signature =
            minisnn_worlds_trainable_brain_weight_signature(trained);
        stage = "setting trained evaluation mode";
        if (!minisnn_worlds_trainable_brain_set_mode(
                trained, MINISNN_WORLDS_BRAIN_MODE_EVALUATION))
        {
            goto per_seed_done;
        }
        aggregate->evaluation_before =
            minisnn_worlds_trainable_brain_weight_signature(trained);
        stage = "running trained evaluation";
        if (!run_evaluation_set(trained, config, WF1_TRAINABLE_FISH_PHASE_TRAINED,
                                seed_index, brain_seed, evaluation,
                                &aggregate->trained_food_sum,
                                &aggregate->trained_successes))
        {
            goto per_seed_done;
        }
        aggregate->evaluation_after =
            minisnn_worlds_trainable_brain_weight_signature(trained);
        stage = "saving trained brain";
        if (aggregate->evaluation_before != aggregate->evaluation_after)
        {
            fprintf(stderr, "WF1-A frozen evaluation changed weights: %016" PRIx64 " -> %016" PRIx64 "\n",
                    aggregate->evaluation_before, aggregate->evaluation_after);
            goto per_seed_done;
        }
        if (snprintf(checkpoint, sizeof(checkpoint), "%s/brain_%" PRIu64 ".wb1",
                     output_directory, brain_seed) < 0 ||
            !minisnn_worlds_trainable_brain_save(trained, checkpoint, &error))
        {
            fprintf(stderr, "WF1-A save error: %s\n",
                    minisnn_worlds_trainable_brain_error_string(error));
            goto per_seed_done;
        }
        stage = "loading trained brain";
        loaded = minisnn_worlds_trainable_brain_load(checkpoint, &error);
        if (loaded == NULL ||
            minisnn_worlds_trainable_brain_weight_signature(loaded) !=
                aggregate->evaluation_after ||
            !run_evaluation_set(loaded, config, WF1_TRAINABLE_FISH_PHASE_LOADED,
                                seed_index, brain_seed, evaluation,
                                &(double){ 0.0 }, &(uint32_t){ 0U }))
        {
            goto per_seed_done;
        }
        aggregate->loaded_signature =
            minisnn_worlds_trainable_brain_weight_signature(loaded);
        if (!create_brain(config, brain_seed, MINISNN_WORLDS_BRAIN_MODE_EVALUATION,
                          &control))
        {
            goto per_seed_done;
        }
        for (episode = 0U; episode < config->training_episodes; ++episode)
        {
            WF1TrainableFishEpisodeResult result;
            if (!wf1_trainable_fish_run_episode(
                    control, config, WF1_TRAINABLE_FISH_PHASE_CONTROL, brain_seed,
                    world_seed_for(config->training_world_seed_base, seed_index, episode),
                    &result))
            {
                goto per_seed_done;
            }
        }
        if (minisnn_worlds_trainable_brain_weight_signature(control) !=
            aggregate->initial_signature ||
            !run_evaluation_set(control, config, WF1_TRAINABLE_FISH_PHASE_CONTROL,
                                seed_index, brain_seed, evaluation,
                                &aggregate->control_food_sum,
                                &aggregate->control_successes) ||
            !write_seed_row(seeds, brain_seed, aggregate, config->evaluation_episodes))
        {
            goto per_seed_done;
        }
        if (aggregate->trained_food_sum > aggregate->untrained_food_sum)
            improved_seed_count++;
        summary.untrained_mean_food += aggregate->untrained_food_sum;
        summary.trained_mean_food += aggregate->trained_food_sum;
        summary.control_mean_food += aggregate->control_food_sum;
        summary.untrained_success_rate += (double)aggregate->untrained_successes;
        summary.trained_success_rate += (double)aggregate->trained_successes;
        summary.control_success_rate += (double)aggregate->control_successes;
        if (seed_index == 0U)
        {
            summary.initial_weight_signature = aggregate->initial_signature;
            summary.trained_weight_signature = aggregate->trained_signature;
            summary.loaded_weight_signature = aggregate->loaded_signature;
            summary.evaluation_weight_signature_before = aggregate->evaluation_before;
            summary.evaluation_weight_signature_after = aggregate->evaluation_after;
        }
        minisnn_worlds_trainable_brain_destroy(&baseline);
        minisnn_worlds_trainable_brain_destroy(&trained);
        minisnn_worlds_trainable_brain_destroy(&control);
        minisnn_worlds_trainable_brain_destroy(&loaded);
        continue;

per_seed_done:
        fprintf(stderr, "WF1-A failed during %s for brain seed %" PRIu64 "\n", stage, brain_seed);
        minisnn_worlds_trainable_brain_destroy(&baseline);
        minisnn_worlds_trainable_brain_destroy(&trained);
        minisnn_worlds_trainable_brain_destroy(&control);
        minisnn_worlds_trainable_brain_destroy(&loaded);
        goto done;
    }
    {
        double denominator = (double)(config->brain_seed_count * config->evaluation_episodes);
        summary.untrained_mean_food /= denominator;
        summary.trained_mean_food /= denominator;
        summary.control_mean_food /= denominator;
        summary.untrained_success_rate /= denominator;
        summary.trained_success_rate /= denominator;
        summary.control_success_rate /= denominator;
    }
    if (summary.trained_mean_food > summary.untrained_mean_food &&
        summary.trained_mean_food > summary.control_mean_food &&
        improved_seed_count > config->brain_seed_count / 2U)
    {
        summary.result = WF1_TRAINABLE_FISH_RESULT_LEARNING_DETECTED;
    }
    else if (summary.trained_mean_food <= summary.untrained_mean_food)
    {
        summary.result = WF1_TRAINABLE_FISH_RESULT_NO_IMPROVEMENT;
    }
    else
    {
        summary.result = WF1_TRAINABLE_FISH_RESULT_INCONCLUSIVE;
    }
    if (!write_summary(summary_path, config, &summary))
        goto done;
    *out_summary = summary;
    ok = 1;

done:
    if (training != NULL)
        fclose(training);
    if (evaluation != NULL)
        fclose(evaluation);
    if (seeds != NULL)
        fclose(seeds);
    return ok;
}