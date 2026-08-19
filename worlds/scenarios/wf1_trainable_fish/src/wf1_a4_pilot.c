#include "wf1_a4_pilot.h"

#include "wf1_a2_terminal_ablation.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define WF1_A4P_PATH_MAX 480U
#define WF1_A4P_TRAIN_EPISODES 64U
#define WF1_A4P_EVAL_EPISODES 12U
#define WF1_A4P_TRAIN_BASE UINT64_C(400000)
#define WF1_A4P_EVAL_BASE UINT64_C(990000)
#define WF1_A4P_LR 0.01

typedef struct
{
    uint64_t food, positive, starvation, decisions, waits, moves, eats;
    uint32_t success;
} Metrics;

typedef struct
{
    Metrics untrained, training, trained, control;
    uint64_t initial_signature, trained_signature, eval_before, eval_after;
    uint64_t clamp_events;
} SeedData;

static uint64_t world_seed(uint64_t base, uint32_t seed_index, uint32_t episode)
{ return base + (uint64_t)seed_index * UINT64_C(1000) + (uint64_t)episode; }

static void metrics_add(Metrics *metrics, const WF1TrainableFishEpisodeResult *result)
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

static double rate(uint64_t value, uint64_t count)
{ return count == 0U ? 0.0 : (double)value / (double)count; }

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
    return fprintf(file, "phase,brain_seed,episode,world_seed,foods_eaten,success,"
        "weight_signature_before,weight_signature_after,wait_rate,move_rate,eat_rate\n") >= 0;
}

static int run_evaluation(MiniSNNWorldsTrainableBrain *brain,
                          const WF1TrainableFishConfig *config, WF1TrainableFishPhase phase,
                          const char *name, uint32_t seed_index, uint64_t brain_seed,
                          FILE *file, Metrics *metrics)
{
    uint32_t episode;
    for (episode = 0U; episode < WF1_A4P_EVAL_EPISODES; ++episode)
    {
        WF1TrainableFishEpisodeResult result;
        if (!wf1_trainable_fish_run_episode(brain, config, phase, brain_seed,
            world_seed(WF1_A4P_EVAL_BASE, seed_index, episode), &result) ||
            fprintf(file, "%s,%" PRIu64 ",%u,%" PRIu64 ",%u,%u,%016" PRIx64 ",%016" PRIx64 ",%.17g,%.17g,%.17g\n",
            name, brain_seed, episode, result.world_seed, result.foods_eaten,
            result.foods_eaten != 0U ? 1U : 0U, result.weight_signature_before,
            result.weight_signature_after, rate(result.wait_selected, result.decision_count),
            rate((uint64_t)result.move_pos_x_selected + result.move_neg_x_selected +
                 result.move_pos_y_selected + result.move_neg_y_selected, result.decision_count),
            rate(result.eat_selected, result.decision_count)) < 0)
            return 0;
        metrics_add(metrics, &result);
    }
    return 1;
}

static int write_summary(const char *path, const WF1A4PilotSummary *summary,
                         const SeedData seeds[WF1_TRAINABLE_FISH_MAX_BRAIN_SEEDS],
                         const WF1TrainableFishConfig *config)
{
    FILE *file;
    uint32_t index;

    file = fopen(path, "wb");
    if (file == NULL)
        return 0;
    if (fprintf(file, "experiment=WF1-A.4P\nsource_protocol=WF1-A.2\n"
        "behavior_tuning=NONE\nreward_learning_rate=0.01\neligibility_tau=UNCHANGED\n"
        "training_world_seed_base=400000\nevaluation_world_seed_base=990000\n"
        "training_episodes_per_seed=64\nevaluation_episodes_per_seed=12\n"
        "positive_rewards=%" PRIu64 "\nclamp_events=%" PRIu64 "\n"
        "untrained_mean_food=%.17g\ntrained_mean_food=%.17g\ncontrol_mean_food=%.17g\n"
        "untrained_success=%.17g\ntrained_success=%.17g\ncontrol_success=%.17g\n"
        "improved_seed_count=%u\nRESULT=%s\n",
        summary->positive_rewards, summary->clamp_events,
        summary->untrained_mean_food, summary->trained_mean_food, summary->control_mean_food,
        summary->untrained_success, summary->trained_success, summary->control_success,
        summary->improved_seed_count,
        summary->improvement ? "PILOT_IMPROVEMENT" : "PILOT_NO_IMPROVEMENT") < 0)
    {
        fclose(file);
        return 0;
    }
    for (index = 0U; index < config->brain_seed_count; ++index)
    {
        const SeedData *seed = &seeds[index];
        if (fprintf(file, "seed_%" PRIu64 "=training_food:%" PRIu64 ",positive:%" PRIu64 ",starvation:%" PRIu64 ","
            "untrained:%.17g,trained:%.17g,control:%.17g,wait:%.17g,move:%.17g,eat:%.17g,"
            "initial:%016" PRIx64 ",trained:%016" PRIx64 ",clamps:%" PRIu64 "\n",
            config->brain_seeds[index], seed->training.food, seed->training.positive,
            seed->training.starvation, (double)seed->untrained.food / WF1_A4P_EVAL_EPISODES,
            (double)seed->trained.food / WF1_A4P_EVAL_EPISODES,
            (double)seed->control.food / WF1_A4P_EVAL_EPISODES,
            rate(seed->training.waits, seed->training.decisions),
            rate(seed->training.moves, seed->training.decisions),
            rate(seed->training.eats, seed->training.decisions), seed->initial_signature,
            seed->trained_signature, seed->clamp_events) < 0)
        {
            fclose(file);
            return 0;
        }
    }
    return fclose(file) == 0;
}

int wf1_a4_pilot_run(const char *output_directory, WF1A4PilotSummary *out_summary)
{
    WF1TrainableFishConfig config;
    SeedData seeds[WF1_TRAINABLE_FISH_MAX_BRAIN_SEEDS];
    WF1A4PilotSummary summary;
    char eval_path[WF1_A4P_PATH_MAX], summary_path[WF1_A4P_PATH_MAX];
    FILE *evaluation = NULL;
    uint32_t index;
    int ok = 0;

    if (output_directory == NULL || output_directory[0] == '\0' || out_summary == NULL ||
        snprintf(eval_path, sizeof(eval_path), "%s/evaluation.csv", output_directory) < 0 ||
        snprintf(summary_path, sizeof(summary_path), "%s/summary.txt", output_directory) < 0)
        return 0;
    wf1_a2_terminal_ablation_config(&config);
    config.training_episodes = WF1_A4P_TRAIN_EPISODES;
    config.evaluation_episodes = WF1_A4P_EVAL_EPISODES;
    config.training_world_seed_base = WF1_A4P_TRAIN_BASE;
    config.evaluation_world_seed_base = WF1_A4P_EVAL_BASE;
    if (!wf1_trainable_fish_config_is_valid(&config))
        return 0;
    memset(seeds, 0, sizeof(seeds));
    memset(&summary, 0, sizeof(summary));
    evaluation = fopen(eval_path, "wb");
    if (evaluation == NULL || !write_header(evaluation))
        goto done;
    for (index = 0U; index < config.brain_seed_count; ++index)
    {
        MiniSNNWorldsTrainableBrain *untrained = NULL, *trained = NULL, *control = NULL;
        SeedData *seed = &seeds[index];
        uint64_t brain_seed = config.brain_seeds[index];
        uint32_t episode;
        MiniSNNRewardStats stats;
        if (!create_brain(&config, brain_seed, MINISNN_WORLDS_BRAIN_MODE_EVALUATION, &untrained) ||
            !run_evaluation(untrained, &config, WF1_TRAINABLE_FISH_PHASE_UNTRAINED,
                "UNTRAINED", index, brain_seed, evaluation, &seed->untrained) ||
            !create_brain(&config, brain_seed, MINISNN_WORLDS_BRAIN_MODE_TRAINING, &trained) ||
            !minisnn_worlds_trainable_brain_set_reward_learning_rate(trained, WF1_A4P_LR))
            goto seed_done;
        seed->initial_signature = minisnn_worlds_trainable_brain_weight_signature(trained);
        for (episode = 0U; episode < WF1_A4P_TRAIN_EPISODES; ++episode)
        {
            WF1TrainableFishEpisodeResult result;
            if (!wf1_trainable_fish_run_episode(trained, &config, WF1_TRAINABLE_FISH_PHASE_TRAINED,
                brain_seed, world_seed(WF1_A4P_TRAIN_BASE, index, episode), &result))
                goto seed_done;
            metrics_add(&seed->training, &result);
        }
        if (!minisnn_worlds_trainable_brain_get_reward_stats(trained, &stats))
            goto seed_done;
        seed->clamp_events = stats.weight_clamp_min_events + stats.weight_clamp_max_events;
        seed->trained_signature = minisnn_worlds_trainable_brain_weight_signature(trained);
        if (!minisnn_worlds_trainable_brain_set_mode(trained, MINISNN_WORLDS_BRAIN_MODE_EVALUATION))
            goto seed_done;
        seed->eval_before = minisnn_worlds_trainable_brain_weight_signature(trained);
        if (!run_evaluation(trained, &config, WF1_TRAINABLE_FISH_PHASE_TRAINED,
            "TRAINED", index, brain_seed, evaluation, &seed->trained) ||
            (seed->eval_after = minisnn_worlds_trainable_brain_weight_signature(trained)) != seed->eval_before ||
            !create_brain(&config, brain_seed, MINISNN_WORLDS_BRAIN_MODE_EVALUATION, &control))
            goto seed_done;
        for (episode = 0U; episode < WF1_A4P_TRAIN_EPISODES; ++episode)
        {
            WF1TrainableFishEpisodeResult result;
            if (!wf1_trainable_fish_run_episode(control, &config, WF1_TRAINABLE_FISH_PHASE_CONTROL,
                brain_seed, world_seed(WF1_A4P_TRAIN_BASE, index, episode), &result))
                goto seed_done;
        }
        if (!run_evaluation(control, &config, WF1_TRAINABLE_FISH_PHASE_CONTROL,
            "CONTROL", index, brain_seed, evaluation, &seed->control))
            goto seed_done;
        summary.positive_rewards += seed->training.positive;
        summary.clamp_events += seed->clamp_events;
        summary.improved_seed_count += seed->trained.food > seed->untrained.food ? 1U : 0U;
        minisnn_worlds_trainable_brain_destroy(&untrained);
        minisnn_worlds_trainable_brain_destroy(&trained);
        minisnn_worlds_trainable_brain_destroy(&control);
        continue;
seed_done:
        minisnn_worlds_trainable_brain_destroy(&untrained);
        minisnn_worlds_trainable_brain_destroy(&trained);
        minisnn_worlds_trainable_brain_destroy(&control);
        goto done;
    }
    for (index = 0U; index < config.brain_seed_count; ++index)
    {
        summary.untrained_mean_food += (double)seeds[index].untrained.food / WF1_A4P_EVAL_EPISODES;
        summary.trained_mean_food += (double)seeds[index].trained.food / WF1_A4P_EVAL_EPISODES;
        summary.control_mean_food += (double)seeds[index].control.food / WF1_A4P_EVAL_EPISODES;
        summary.untrained_success += rate(seeds[index].untrained.success, WF1_A4P_EVAL_EPISODES);
        summary.trained_success += rate(seeds[index].trained.success, WF1_A4P_EVAL_EPISODES);
        summary.control_success += rate(seeds[index].control.success, WF1_A4P_EVAL_EPISODES);
    }
    summary.untrained_mean_food /= config.brain_seed_count;
    summary.trained_mean_food /= config.brain_seed_count;
    summary.control_mean_food /= config.brain_seed_count;
    summary.untrained_success /= config.brain_seed_count;
    summary.trained_success /= config.brain_seed_count;
    summary.control_success /= config.brain_seed_count;
    summary.improvement = summary.trained_mean_food > summary.untrained_mean_food;
    if (fclose(evaluation) != 0)
    { evaluation = NULL; goto done; }
    evaluation = NULL;
    if (!write_summary(summary_path, &summary, seeds, &config))
        goto done;
    *out_summary = summary;
    ok = 1;
done:
    if (evaluation != NULL) fclose(evaluation);
    return ok;
}