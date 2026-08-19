#include "wf1_trainable_fish.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "WF1-A test failed: %s (%s:%d)\n", #condition, __FILE__, __LINE__); \
        return 0; \
    } \
} while (0)

static int ensure_directory(const char *path)
{
#ifdef _WIN32
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0777) == 0 || errno == EEXIST;
#endif
}

static int test_config_and_episode_reset(void)
{
    WF1TrainableFishConfig config = wf1_trainable_fish_config_default();
    WF1TrainableFishConfig invalid;
    MiniSNNWorldsTrainableBrainError error;
    MiniSNNWorldsTrainableBrain *brain;
    WF1TrainableFishEpisodeResult result;
    uint64_t before;

    CHECK(wf1_trainable_fish_config_is_valid(&config));
    invalid = config;
    invalid.starvation_terminal_reward = 1.0;
    CHECK(!wf1_trainable_fish_config_is_valid(&invalid));
    brain = minisnn_worlds_trainable_brain_create(&config.brain_config, &error);
    CHECK(brain != NULL && error == MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE);
    before = minisnn_worlds_trainable_brain_weight_signature(brain);
    CHECK(wf1_trainable_fish_run_episode(
        brain, &config, WF1_TRAINABLE_FISH_PHASE_TRAINED,
        config.brain_seeds[0], config.training_world_seed_base, &result));
    CHECK(result.starvation_count == 1U);
    CHECK(result.ticks > 0U);
    CHECK(result.cumulative_reward <= config.starvation_terminal_reward);
    CHECK(result.weight_signature_before == before);
    CHECK(minisnn_worlds_trainable_brain_last_error(brain) ==
          MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE);
    /* The episode reset left the same brain ready for a fresh Domain binding. */
    CHECK(wf1_trainable_fish_run_episode(
        brain, &config, WF1_TRAINABLE_FISH_PHASE_TRAINED,
        config.brain_seeds[0], config.training_world_seed_base + UINT64_C(1),
        &result));
    minisnn_worlds_trainable_brain_destroy(&brain);
    return 1;
}

static int test_evaluation_freezes_weights(void)
{
    WF1TrainableFishConfig config = wf1_trainable_fish_config_default();
    MiniSNNWorldsBrainConfig brain_config = config.brain_config;
    MiniSNNWorldsTrainableBrainError error;
    MiniSNNWorldsTrainableBrain *brain;
    WF1TrainableFishEpisodeResult result;
    uint64_t before;

    brain_config.mode = MINISNN_WORLDS_BRAIN_MODE_EVALUATION;
    brain_config.plasticity_enabled = 0;
    brain = minisnn_worlds_trainable_brain_create(&brain_config, &error);
    CHECK(brain != NULL && error == MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE);
    before = minisnn_worlds_trainable_brain_weight_signature(brain);
    CHECK(wf1_trainable_fish_run_episode(
        brain, &config, WF1_TRAINABLE_FISH_PHASE_UNTRAINED,
        config.brain_seeds[0], config.evaluation_world_seed_base, &result));
    CHECK(result.weight_signature_before == before);
    CHECK(result.weight_signature_after == before);
    CHECK(minisnn_worlds_trainable_brain_weight_signature(brain) == before);
    minisnn_worlds_trainable_brain_destroy(&brain);
    return 1;
}

static int test_small_experiment_save_load(void)
{
    WF1TrainableFishConfig config = wf1_trainable_fish_config_default();
    WF1TrainableFishExperimentSummary summary;
    FILE *summary_file;

    config.brain_seed_count = 1U;
    config.training_episodes = 3U;
    config.evaluation_episodes = 2U;
    CHECK(ensure_directory("build"));
    CHECK(ensure_directory("build/wf1_test_results"));
    CHECK(wf1_trainable_fish_run_experiment(
        &config, "build/wf1_test_results", &summary));
    CHECK(summary.evaluation_weight_signature_before ==
          summary.evaluation_weight_signature_after);
    CHECK(summary.trained_weight_signature == summary.loaded_weight_signature);
    summary_file = fopen("build/wf1_test_results/summary.txt", "rb");
    CHECK(summary_file != NULL);
    fclose(summary_file);
    summary_file = fopen("build/wf1_test_results/training.csv", "rb");
    CHECK(summary_file != NULL);
    fclose(summary_file);
    summary_file = fopen("build/wf1_test_results/evaluation.csv", "rb");
    CHECK(summary_file != NULL);
    fclose(summary_file);
    summary_file = fopen("build/wf1_test_results/seeds.csv", "rb");
    CHECK(summary_file != NULL);
    fclose(summary_file);
    return 1;
}

static int test_exploration_metrics_contract(void)
{
    WF1TrainableFishConfig config = wf1_trainable_fish_config_default();
    MiniSNNWorldsTrainableBrainError error;
    MiniSNNWorldsTrainableBrain *brain;
    WF1TrainableFishEpisodeResult result;
    uint32_t selected;
    uint32_t moves;

    brain = minisnn_worlds_trainable_brain_create(&config.brain_config, &error);
    CHECK(brain != NULL && error == MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE);
    CHECK(wf1_trainable_fish_run_episode(
        brain, &config, WF1_TRAINABLE_FISH_PHASE_TRAINED,
        config.brain_seeds[0], config.training_world_seed_base + UINT64_C(77), &result));
    selected = result.wait_selected + result.move_pos_x_selected +
        result.move_neg_x_selected + result.move_pos_y_selected +
        result.move_neg_y_selected + result.eat_selected;
    moves = result.move_pos_x_selected + result.move_neg_x_selected +
        result.move_pos_y_selected + result.move_neg_y_selected;
    CHECK(result.decision_count == result.ticks);
    CHECK(selected == result.decision_count);
    CHECK(result.move_applied + result.move_rejected <= moves);
    CHECK(result.eat_applied + result.eat_rejected <= result.eat_selected);
    CHECK(result.successful_eat_count <= result.eat_applied);
    CHECK(result.positive_reward_count == result.successful_eat_count);
    CHECK(result.terminal_reward_count == result.starvation_count);
    CHECK(result.active_decision_count <= result.decision_count);
    CHECK(result.zero_score_decisions <= result.decision_count);
    CHECK(result.tied_winner_decisions <= result.decision_count);
    minisnn_worlds_trainable_brain_destroy(&brain);
    return 1;
}

static int test_zero_terminal_reward_preserves_boundary(void)
{
    WF1TrainableFishConfig config = wf1_trainable_fish_config_default();
    MiniSNNWorldsTrainableBrainError error;
    MiniSNNWorldsTrainableBrain *brain;
    WF1TrainableFishEpisodeResult result;

    config.starvation_terminal_reward = 0.0;
    CHECK(wf1_trainable_fish_config_is_valid(&config));
    brain = minisnn_worlds_trainable_brain_create(&config.brain_config, &error);
    CHECK(brain != NULL && error == MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE);
    CHECK(wf1_trainable_fish_run_episode(brain, &config, WF1_TRAINABLE_FISH_PHASE_TRAINED,
        config.brain_seeds[0], config.training_world_seed_base + UINT64_C(99), &result));
    CHECK(result.starvation_count == 1U);
    CHECK(result.terminal_reward_count == 1U);
    minisnn_worlds_trainable_brain_destroy(&brain);
    return 1;
}

int main(void)
{
    if (!test_config_and_episode_reset() ||
        !test_evaluation_freezes_weights() ||
        !test_small_experiment_save_load() ||
        !test_exploration_metrics_contract() ||
        !test_zero_terminal_reward_preserves_boundary())
    {
        return 1;
    }
    puts("WF1-A episode, exploration metrics, terminal boundary, evaluation freeze, and save/load OK");
    return 0;
}