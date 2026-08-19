#include "wf1_a2_terminal_ablation.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define WF1_A2_PATH_MAX 480U
#define WF1_A2_TRAINING_EPISODES 256U
#define WF1_A2_EVALUATION_EPISODES 24U
#define WF1_A2_WINDOW 32U

typedef struct
{
    uint64_t food, positive, starvation, decisions, waits, moves, eats;
    uint32_t success;
} WF1A2Metrics;

typedef struct
{
    WF1A2Metrics untrained, training, trained, control;
    uint64_t initial_signature, trained_signature, evaluation_before, evaluation_after;
    uint64_t first_wait, first_decisions, last_wait, last_decisions;
} WF1A2Seed;

static void metrics_add(WF1A2Metrics *metrics, const WF1TrainableFishEpisodeResult *result)
{
    metrics->food += result->foods_eaten;
    metrics->positive += result->positive_reward_count;
    metrics->starvation += result->starvation_count;
    metrics->decisions += result->decision_count;
    metrics->waits += result->wait_selected;
    metrics->moves += (uint64_t)result->move_pos_x_selected + result->move_neg_x_selected +
        result->move_pos_y_selected + result->move_neg_y_selected;
    metrics->eats += result->eat_selected;
    metrics->success += result->foods_eaten != 0U ? 1U : 0U;
}

static uint64_t episode_seed(uint64_t base, uint32_t seed_index, uint32_t episode)
{
    return base + (uint64_t)seed_index * UINT64_C(1000) + (uint64_t)episode;
}

void wf1_a2_terminal_ablation_config(WF1TrainableFishConfig *config)
{
    *config = wf1_trainable_fish_config_default();
    config->max_energy = UINT64_C(120);
    config->initial_energy = UINT64_C(96);
    config->metabolism_per_tick = UINT64_C(1);
    config->move_energy_cost = UINT64_C(1);
    config->eat_range = 0;
    config->food_nutrition = UINT64_C(20);
    config->food_count = 24U;
    config->food_grid_radius = INT64_C(2);
    config->world_bound = INT64_C(3000);
    config->max_episode_ticks = 96U;
    config->brain_config.topology = MINISNN_WORLDS_BRAIN_TOPOLOGY_RANDOM;
    config->brain_config.neuron_count = 42U;
    config->brain_config.neural_config.neuron_count = 42;
    config->brain_config.inhibitory_count = 7U;
    config->brain_config.connection_probability = 0.20;
    config->brain_config.excitatory_weight = 50.0;
    config->brain_config.inhibitory_weight = -40.0;
    config->brain_config.decision_steps_per_tick = 16U;
    config->brain_config.action_population_size = 3U;
    config->training_episodes = WF1_A2_TRAINING_EPISODES;
    config->evaluation_episodes = WF1_A2_EVALUATION_EPISODES;
    config->training_world_seed_base = UINT64_C(300000);
    config->evaluation_world_seed_base = UINT64_C(970000);
    config->starvation_terminal_reward = 0.0;
    (void)snprintf(config->brain_config.brain_name, sizeof(config->brain_config.brain_name),
                   "%s", "wf1_a2_terminal_reward_zero");
}

static int create_brain(const WF1TrainableFishConfig *config, uint64_t seed,
                        MiniSNNWorldsBrainMode mode, MiniSNNWorldsTrainableBrain **out_brain)
{
    MiniSNNWorldsBrainConfig brain_config = config->brain_config;
    MiniSNNWorldsTrainableBrainError error;
    brain_config.seed = seed;
    brain_config.mode = mode;
    brain_config.plasticity_enabled = mode == MINISNN_WORLDS_BRAIN_MODE_TRAINING ? 1 : 0;
    *out_brain = minisnn_worlds_trainable_brain_create(&brain_config, &error);
    return *out_brain != NULL && error == MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE;
}

static int write_header(FILE *file)
{
    return fprintf(file, "phase,brain_seed,episode,world_seed,foods_eaten,cumulative_reward,"
        "weight_signature_before,weight_signature_after,decision_count,wait_selected,"
        "move_selected,eat_selected,successful_eat_count,positive_reward_count,"
        "terminal_reward_count,starvation_count,total_spikes\n") >= 0;
}

static int write_row(FILE *file, const char *phase, uint32_t episode,
                     const WF1TrainableFishEpisodeResult *result)
{
    uint32_t moves = result->move_pos_x_selected + result->move_neg_x_selected +
        result->move_pos_y_selected + result->move_neg_y_selected;
    return fprintf(file, "%s,%" PRIu64 ",%u,%" PRIu64 ",%u,%.17g,%016" PRIx64 ",%016" PRIx64 ","
        "%u,%u,%u,%u,%u,%u,%u,%u,%" PRIu64 "\n", phase, result->brain_seed,
        episode, result->world_seed, result->foods_eaten, result->cumulative_reward,
        result->weight_signature_before, result->weight_signature_after, result->decision_count,
        result->wait_selected, moves, result->eat_selected, result->successful_eat_count,
        result->positive_reward_count, result->terminal_reward_count, result->starvation_count,
        result->total_spikes) >= 0;
}

static int run_evaluation(MiniSNNWorldsTrainableBrain *brain,
                          const WF1TrainableFishConfig *config, const char *phase,
                          uint32_t seed_index, uint64_t brain_seed, FILE *file,
                          WF1A2Metrics *metrics)
{
    uint32_t episode;
    for (episode = 0U; episode < config->evaluation_episodes; ++episode)
    {
        WF1TrainableFishEpisodeResult result;
        if (!wf1_trainable_fish_run_episode(brain, config,
            strcmp(phase, "UNTRAINED") == 0 ? WF1_TRAINABLE_FISH_PHASE_UNTRAINED :
            (strcmp(phase, "CONTROL") == 0 ? WF1_TRAINABLE_FISH_PHASE_CONTROL :
             WF1_TRAINABLE_FISH_PHASE_TRAINED), brain_seed,
            episode_seed(config->evaluation_world_seed_base, seed_index, episode), &result) ||
            !write_row(file, phase, episode, &result)) return 0;
        metrics_add(metrics, &result);
    }
    return 1;
}

static double rate(uint64_t numerator, uint64_t denominator)
{
    return denominator == 0U ? 0.0 : (double)numerator / (double)denominator;
}

static int write_seed_row(FILE *file, uint64_t seed, const WF1A2Seed *data,
                          const WF1TrainableFishConfig *config)
{
    return fprintf(file, "%" PRIu64 ",%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64 ","
        "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%" PRIu64 ",%" PRIu64 ","
        "%.17g,%.17g,%.17g,%.17g,%.17g\n", seed, data->initial_signature,
        data->trained_signature, data->evaluation_before, data->evaluation_after,
        (double)data->untrained.food / config->evaluation_episodes,
        (double)data->trained.food / config->evaluation_episodes,
        (double)data->control.food / config->evaluation_episodes,
        rate(data->untrained.success, config->evaluation_episodes),
        rate(data->trained.success, config->evaluation_episodes),
        rate(data->control.success, config->evaluation_episodes), (double)data->training.food,
        data->training.positive, data->training.starvation,
        rate(data->training.waits, data->training.decisions),
        rate(data->training.moves, data->training.decisions),
        rate(data->training.eats, data->training.decisions),
        rate(data->first_wait, data->first_decisions), rate(data->last_wait, data->last_decisions)) >= 0;
}

int wf1_a2_terminal_ablation_run_experiment(const char *directory,
                                             WF1A2TerminalAblationSummary *out_summary)
{
    WF1TrainableFishConfig config;
    WF1A2TerminalAblationSummary summary;
    WF1A2Seed seeds[WF1_TRAINABLE_FISH_MAX_BRAIN_SEEDS];
    char training_path[WF1_A2_PATH_MAX], evaluation_path[WF1_A2_PATH_MAX], seeds_path[WF1_A2_PATH_MAX], summary_path[WF1_A2_PATH_MAX];
    FILE *training = NULL, *evaluation = NULL, *seed_file = NULL;
    uint32_t index;
    int ok = 0;

    if (directory == NULL || directory[0] == '\0' || out_summary == NULL) return 0;
    wf1_a2_terminal_ablation_config(&config);
    if (!wf1_trainable_fish_config_is_valid(&config) ||
        snprintf(training_path, sizeof(training_path), "%s/training.csv", directory) < 0 ||
        snprintf(evaluation_path, sizeof(evaluation_path), "%s/evaluation.csv", directory) < 0 ||
        snprintf(seeds_path, sizeof(seeds_path), "%s/seeds.csv", directory) < 0 ||
        snprintf(summary_path, sizeof(summary_path), "%s/summary.txt", directory) < 0) return 0;
    memset(&summary, 0, sizeof(summary));
    memset(seeds, 0, sizeof(seeds));
    training = fopen(training_path, "wb"); evaluation = fopen(evaluation_path, "wb"); seed_file = fopen(seeds_path, "wb");
    if (training == NULL || evaluation == NULL || seed_file == NULL || !write_header(training) || !write_header(evaluation) ||
        fprintf(seed_file, "brain_seed,initial_weight_signature,trained_weight_signature,evaluation_weight_before,evaluation_weight_after,untrained_eval_mean_food,trained_eval_mean_food,control_eval_mean_food,untrained_success,trained_success,control_success,training_food_total,positive_rewards,training_starvations,training_wait_rate,training_move_rate,training_eat_rate,first_32_wait_rate,last_32_wait_rate\n") < 0) goto done;
    for (index = 0U; index < config.brain_seed_count; ++index)
    {
        MiniSNNWorldsTrainableBrain *untrained = NULL, *trained = NULL, *control = NULL;
        WF1A2Seed *seed = &seeds[index]; uint32_t episode; uint64_t brain_seed = config.brain_seeds[index];
        if (!create_brain(&config, brain_seed, MINISNN_WORLDS_BRAIN_MODE_EVALUATION, &untrained) ||
            !run_evaluation(untrained, &config, "UNTRAINED", index, brain_seed, evaluation, &seed->untrained) ||
            !create_brain(&config, brain_seed, MINISNN_WORLDS_BRAIN_MODE_TRAINING, &trained)) goto seed_done;
        seed->initial_signature = minisnn_worlds_trainable_brain_weight_signature(trained);
        for (episode = 0U; episode < config.training_episodes; ++episode)
        {
            WF1TrainableFishEpisodeResult result; uint64_t before;
            before = minisnn_worlds_trainable_brain_weight_signature(trained);
            if (!wf1_trainable_fish_run_episode(trained, &config, WF1_TRAINABLE_FISH_PHASE_TRAINED,
                brain_seed, episode_seed(config.training_world_seed_base, index, episode), &result) ||
                !write_row(training, "TRAINING", episode, &result)) goto seed_done;
            metrics_add(&seed->training, &result);
            if (episode < WF1_A2_WINDOW) { seed->first_wait += result.wait_selected; seed->first_decisions += result.decision_count; }
            if (episode >= config.training_episodes - WF1_A2_WINDOW) { seed->last_wait += result.wait_selected; seed->last_decisions += result.decision_count; }
            (void)before;
        }
        seed->trained_signature = minisnn_worlds_trainable_brain_weight_signature(trained);
        if (!minisnn_worlds_trainable_brain_set_mode(trained, MINISNN_WORLDS_BRAIN_MODE_EVALUATION)) goto seed_done;
        seed->evaluation_before = minisnn_worlds_trainable_brain_weight_signature(trained);
        if (!run_evaluation(trained, &config, "TRAINED", index, brain_seed, evaluation, &seed->trained) ||
            (seed->evaluation_after = minisnn_worlds_trainable_brain_weight_signature(trained)) != seed->evaluation_before ||
            !create_brain(&config, brain_seed, MINISNN_WORLDS_BRAIN_MODE_EVALUATION, &control)) goto seed_done;
        for (episode = 0U; episode < config.training_episodes; ++episode)
        {
            WF1TrainableFishEpisodeResult result;
            if (!wf1_trainable_fish_run_episode(control, &config, WF1_TRAINABLE_FISH_PHASE_CONTROL, brain_seed,
                episode_seed(config.training_world_seed_base, index, episode), &result)) goto seed_done;
        }
        if (!run_evaluation(control, &config, "CONTROL", index, brain_seed, evaluation, &seed->control) ||
            !write_seed_row(seed_file, brain_seed, seed, &config)) goto seed_done;
        summary.positive_rewards += seed->training.positive;
        summary.training_starvations += seed->training.starvation;
        if (seed->trained_signature != seed->initial_signature) summary.changed_weight_seed_count++;
        if (seed->trained.food > seed->untrained.food) summary.improved_seed_count++;
        minisnn_worlds_trainable_brain_destroy(&untrained); minisnn_worlds_trainable_brain_destroy(&trained); minisnn_worlds_trainable_brain_destroy(&control); continue;
seed_done:
        minisnn_worlds_trainable_brain_destroy(&untrained); minisnn_worlds_trainable_brain_destroy(&trained); minisnn_worlds_trainable_brain_destroy(&control); goto done;
    }
    for (index = 0U; index < config.brain_seed_count; ++index)
    {
        summary.untrained_eval_mean_food += (double)seeds[index].untrained.food / config.evaluation_episodes;
        summary.trained_eval_mean_food += (double)seeds[index].trained.food / config.evaluation_episodes;
        summary.control_eval_mean_food += (double)seeds[index].control.food / config.evaluation_episodes;
        summary.untrained_success_rate += rate(seeds[index].untrained.success, config.evaluation_episodes);
        summary.trained_success_rate += rate(seeds[index].trained.success, config.evaluation_episodes);
        summary.control_success_rate += rate(seeds[index].control.success, config.evaluation_episodes);
        summary.first_32_wait_rate += rate(seeds[index].first_wait, seeds[index].first_decisions);
        summary.last_32_wait_rate += rate(seeds[index].last_wait, seeds[index].last_decisions);
    }
    summary.untrained_eval_mean_food /= config.brain_seed_count; summary.trained_eval_mean_food /= config.brain_seed_count; summary.control_eval_mean_food /= config.brain_seed_count;
    summary.untrained_success_rate /= config.brain_seed_count; summary.trained_success_rate /= config.brain_seed_count; summary.control_success_rate /= config.brain_seed_count;
    summary.first_32_wait_rate /= config.brain_seed_count; summary.last_32_wait_rate /= config.brain_seed_count;
    summary.policy_collapse = summary.last_32_wait_rate >= 0.90 && summary.last_32_wait_rate > summary.first_32_wait_rate + 0.10;
    summary.result = summary.trained_eval_mean_food > summary.untrained_eval_mean_food && summary.trained_eval_mean_food > summary.control_eval_mean_food && summary.improved_seed_count > config.brain_seed_count / 2U ? WF1_TRAINABLE_FISH_RESULT_LEARNING_DETECTED : WF1_TRAINABLE_FISH_RESULT_NO_IMPROVEMENT;
    { FILE *file = fopen(summary_path, "wb"); if (file == NULL) goto done;
      ok = fprintf(file, "experiment=WF1-A.2 terminal reward ablation\npredecessor_result=WF1-A.1:NO_IMPROVEMENT\nterminal_reward=0\nterminal_boundary=STARVATION_DEATH\npositive_reward=EAT_APPLIED_ENERGY_GAIN\ntraining_world_seed_base=300000\nevaluation_world_seed_base=970000\nbrain_seeds=101,202,303,404\ntraining_episodes_per_seed=256\nevaluation_episodes_per_seed=24\npositive_rewards=%" PRIu64 "\ntraining_starvations=%" PRIu64 "\nuntrained_eval_mean_food=%.17g\ntrained_eval_mean_food=%.17g\ncontrol_eval_mean_food=%.17g\nuntrained_success=%.17g\ntrained_success=%.17g\ncontrol_success=%.17g\nfirst_32_wait_rate=%.17g\nlast_32_wait_rate=%.17g\nchanged_weight_seed_count=%u\nimproved_seed_count=%u\nPOLICY_COLLAPSE=%s\nresult=%s\n", summary.positive_rewards, summary.training_starvations, summary.untrained_eval_mean_food, summary.trained_eval_mean_food, summary.control_eval_mean_food, summary.untrained_success_rate, summary.trained_success_rate, summary.control_success_rate, summary.first_32_wait_rate, summary.last_32_wait_rate, summary.changed_weight_seed_count, summary.improved_seed_count, summary.policy_collapse ? "YES" : "NO", wf1_trainable_fish_result_name(summary.result)) >= 0 && fclose(file) == 0; }
    if (!ok) goto done;
    *out_summary = summary; ok = 1;
done:
    if (training != NULL) fclose(training);
    if (evaluation != NULL) fclose(evaluation);
    if (seed_file != NULL) fclose(seed_file);
    return ok;
}