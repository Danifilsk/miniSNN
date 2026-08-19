#include "wf1_a4_saturation_probe.h"

#include "wf1_a2_terminal_ablation.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define WF1_A4_PATH_MAX 480U
#define WF1_A4_BRAIN_SEED UINT64_C(101)
#define WF1_A4_WORLD_SEED_BASE UINT64_C(300000)
#define WF1_A4_VARIANT_COUNT 3U

typedef struct
{
    double learning_rate;
    uint32_t episode;
    int captured;
    WF1TrainableFishRewardObservation observation;
} WF1A4Variant;

static uint64_t episode_seed(uint32_t episode)
{
    return WF1_A4_WORLD_SEED_BASE + (uint64_t)episode;
}

static int create_training_brain(const WF1TrainableFishConfig *config,
                                 MiniSNNWorldsTrainableBrain **out_brain)
{
    MiniSNNWorldsBrainConfig brain_config;
    MiniSNNWorldsTrainableBrainError error;

    if (config == NULL || out_brain == NULL)
        return 0;
    brain_config = config->brain_config;
    brain_config.seed = WF1_A4_BRAIN_SEED;
    brain_config.mode = MINISNN_WORLDS_BRAIN_MODE_TRAINING;
    brain_config.plasticity_enabled = 1;
    *out_brain = minisnn_worlds_trainable_brain_create(&brain_config, &error);
    return *out_brain != NULL && error == MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE;
}

static int capture_first_reward(const WF1TrainableFishRewardObservation *observation,
                                void *user_data)
{
    WF1A4Variant *variant = user_data;

    if (variant == NULL || observation == NULL || variant->captured ||
        observation->action != MINISNN_WORLDS_BRAIN_ACTION_EAT ||
        observation->reward != 1.0 || observation->energy_gain <= 0.0 ||
        observation->stats_after.reward_event_count !=
            observation->stats_before.reward_event_count + UINT64_C(1) ||
        observation->stats_after.last_applied_reward != 1.0)
    {
        return 0;
    }
    variant->observation = *observation;
    variant->captured = 1;
    return 1;
}

static int run_variant(const WF1TrainableFishConfig *config, WF1A4Variant *variant)
{
    MiniSNNWorldsTrainableBrain *brain = NULL;
    uint32_t episode;
    int ok = 0;

    if (!create_training_brain(config, &brain) ||
        !minisnn_worlds_trainable_brain_set_reward_learning_rate(
            brain, variant->learning_rate))
    {
        goto done;
    }
    for (episode = 0U; episode <= 1U; ++episode)
    {
        WF1TrainableFishEpisodeResult result;

        variant->episode = episode;
        if (!wf1_trainable_fish_run_episode_observed(
                brain, config, WF1_TRAINABLE_FISH_PHASE_TRAINED,
                WF1_A4_BRAIN_SEED, episode_seed(episode), capture_first_reward,
                variant, &result))
        {
            goto done;
        }
    }
    ok = variant->captured;
done:
    minisnn_worlds_trainable_brain_destroy(&brain);
    return ok;
}

static int same_pre_reward(const WF1A4Variant *first, const WF1A4Variant *other)
{
    return first->observation.brain_seed == other->observation.brain_seed &&
        first->observation.world_seed == other->observation.world_seed &&
        first->observation.episode_tick == other->observation.episode_tick &&
        first->observation.action == other->observation.action &&
        first->observation.reward == other->observation.reward &&
        first->observation.energy_gain == other->observation.energy_gain &&
        first->observation.weight_signature_before ==
            other->observation.weight_signature_before &&
        first->observation.stats_before.eligibility_final_mean_absolute ==
            other->observation.stats_before.eligibility_final_mean_absolute;
}

static int write_csv(const char *path, const WF1A4Variant variants[WF1_A4_VARIANT_COUNT])
{
    FILE *file;
    uint32_t index;

    file = fopen(path, "wb");
    if (file == NULL)
        return 0;
    if (fprintf(file,
        "learning_rate,brain_seed,world_seed,episode,episode_tick,action,reward,energy_gain,"
        "active_eligibility_count,modified_connection_count,weight_absolute_change,"
        "weight_signed_change,eligibility_mean_absolute,eligibility_min,eligibility_max,"
        "eligibility_max_absolute,clamp_min_delta,clamp_max_delta,"
        "weight_signature_before,weight_signature_after\n") < 0)
    {
        fclose(file);
        return 0;
    }
    for (index = 0U; index < WF1_A4_VARIANT_COUNT; ++index)
    {
        const WF1TrainableFishRewardObservation *observation = &variants[index].observation;
        const MiniSNNRewardStats *before = &observation->stats_before;
        const MiniSNNRewardStats *after = &observation->stats_after;

        if (fprintf(file,
            "%.17g,%" PRIu64 ",%" PRIu64 ",%u,%u,EAT,%.17g,%.17g,%zu,%zu,"
            "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%llu,%llu,%016" PRIx64 ",%016" PRIx64 "\n",
            variants[index].learning_rate, observation->brain_seed,
            observation->world_seed, variants[index].episode,
            observation->episode_tick, observation->reward, observation->energy_gain,
            after->last_active_eligibility_count, after->last_modified_connection_count,
            after->last_weight_absolute_change, after->last_weight_signed_change,
            after->eligibility_final_mean_absolute, after->eligibility_final_min,
            after->eligibility_final_max, after->eligibility_max_absolute_observed,
            after->weight_clamp_min_events - before->weight_clamp_min_events,
            after->weight_clamp_max_events - before->weight_clamp_max_events,
            observation->weight_signature_before, observation->weight_signature_after) < 0)
        {
            fclose(file);
            return 0;
        }
    }
    return fclose(file) == 0;
}

static int write_summary(const char *path, const WF1A4SaturationProbeSummary *summary,
                         const WF1A4Variant variants[WF1_A4_VARIANT_COUNT])
{
    FILE *file = fopen(path, "wb");
    const MiniSNNRewardStats *high = &variants[0].observation.stats_after;
    const MiniSNNRewardStats *low = &variants[2].observation.stats_after;
    unsigned long long high_clamps = high->last_weight_clamp_min_count +
        high->last_weight_clamp_max_count;
    unsigned long long low_clamps = low->last_weight_clamp_min_count +
        low->last_weight_clamp_max_count;

    if (file == NULL)
        return 0;
    return fprintf(file,
        "experiment=WF1-A.4\nsource_protocol=WF1-A.2\nbehavior_tuning=NONE\n"
        "neural_variable_changed=learning_rate_only\n"
        "terminal_reward=0\nterminal_boundary=STARVATION_DEATH\n"
        "brain_seed=101\nworld_seed=300001\nepisode=1\n"
        "pre_reward_equivalence=%s\n"
        "learning_rates=1,0.1,0.01\n"
        "lr_1_weight_absolute_change=%.17g\n"
        "lr_0_1_weight_absolute_change=%.17g\n"
        "lr_0_01_weight_absolute_change=%.17g\n"
        "lr_1_clamps=%llu\nlr_0_01_clamps=%llu\n"
        "SATURATION=%s\nGLOBAL_CREDIT=%s\nPILOT=NOT_EXECUTED_QUOTA\n",
        summary->pre_reward_equivalent ? "PASS" : "FAIL",
        variants[0].observation.stats_after.last_weight_absolute_change,
        variants[1].observation.stats_after.last_weight_absolute_change,
        variants[2].observation.stats_after.last_weight_absolute_change,
        high_clamps, low_clamps,
        summary->saturation_supported ? "SUPPORTED" : "INCONCLUSIVE",
        summary->global_credit_remains_suspect ? "REMAINS_SUSPECT" : "INCONCLUSIVE") >= 0 &&
        fclose(file) == 0;
}

int wf1_a4_saturation_probe_run(const char *output_directory,
                                WF1A4SaturationProbeSummary *out_summary)
{
    static const double learning_rates[WF1_A4_VARIANT_COUNT] = { 1.0, 0.1, 0.01 };
    WF1TrainableFishConfig config;
    WF1A4Variant variants[WF1_A4_VARIANT_COUNT];
    WF1A4SaturationProbeSummary summary;
    char csv_path[WF1_A4_PATH_MAX];
    char summary_path[WF1_A4_PATH_MAX];
    uint32_t index;

    if (output_directory == NULL || output_directory[0] == '\0' || out_summary == NULL ||
        snprintf(csv_path, sizeof(csv_path), "%s/saturation_probe.csv", output_directory) < 0 ||
        snprintf(summary_path, sizeof(summary_path), "%s/summary.txt", output_directory) < 0)
    {
        return 0;
    }
    wf1_a2_terminal_ablation_config(&config);
    if (!wf1_trainable_fish_config_is_valid(&config))
        return 0;
    memset(&variants, 0, sizeof(variants));
    memset(&summary, 0, sizeof(summary));
    for (index = 0U; index < WF1_A4_VARIANT_COUNT; ++index)
    {
        variants[index].learning_rate = learning_rates[index];
        if (!run_variant(&config, &variants[index]))
            return 0;
        summary.variants_completed++;
    }
    summary.pre_reward_equivalent = same_pre_reward(&variants[0], &variants[1]) &&
        same_pre_reward(&variants[0], &variants[2]);
    if (!summary.pre_reward_equivalent)
        return 0;
    summary.saturation_supported =
        variants[2].observation.stats_after.last_weight_absolute_change <
            variants[0].observation.stats_after.last_weight_absolute_change &&
        variants[2].observation.stats_after.last_weight_clamp_min_count +
            variants[2].observation.stats_after.last_weight_clamp_max_count <
        variants[0].observation.stats_after.last_weight_clamp_min_count +
            variants[0].observation.stats_after.last_weight_clamp_max_count;
    summary.global_credit_remains_suspect =
        variants[2].observation.stats_after.last_modified_connection_count * 10U >=
        variants[0].observation.stats_after.last_modified_connection_count * 9U;
    if (!write_csv(csv_path, variants) || !write_summary(summary_path, &summary, variants))
        return 0;
    *out_summary = summary;
    return 1;
}