#include "wf1_a5_tau_probe.h"

#include "wf1_a2_terminal_ablation.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define WF1_A5_PATH_MAX 480U
#define WF1_A5_BRAIN_SEED UINT64_C(101)
#define WF1_A5_WORLD_SEED_BASE UINT64_C(300000)
#define WF1_A5_VARIANT_COUNT 3U
#define WF1_A5_LEARNING_RATE 0.01

typedef struct
{
    double eligibility_tau;
    uint32_t episode;
    int captured;
    WF1TrainableFishRewardObservation observation;
} WF1A5Variant;

static uint64_t episode_seed(uint32_t episode)
{
    return WF1_A5_WORLD_SEED_BASE + (uint64_t)episode;
}

static int create_training_brain(const WF1TrainableFishConfig *config,
                                 MiniSNNWorldsTrainableBrain **out_brain)
{
    MiniSNNWorldsBrainConfig brain_config;
    MiniSNNWorldsTrainableBrainError error;

    if (config == NULL || out_brain == NULL)
        return 0;
    brain_config = config->brain_config;
    brain_config.seed = WF1_A5_BRAIN_SEED;
    brain_config.mode = MINISNN_WORLDS_BRAIN_MODE_TRAINING;
    brain_config.plasticity_enabled = 1;
    *out_brain = minisnn_worlds_trainable_brain_create(&brain_config, &error);
    return *out_brain != NULL && error == MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE;
}

static int capture_first_reward(const WF1TrainableFishRewardObservation *observation,
                                void *user_data)
{
    WF1A5Variant *variant = user_data;

    if (variant == NULL || observation == NULL || variant->captured ||
        observation->action != MINISNN_WORLDS_BRAIN_ACTION_EAT ||
        observation->reward != 1.0 || observation->energy_gain != 20.0 ||
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

static int run_variant(const WF1TrainableFishConfig *config, WF1A5Variant *variant)
{
    MiniSNNWorldsTrainableBrain *brain = NULL;
    uint32_t episode;
    int ok = 0;

    if (!create_training_brain(config, &brain) ||
        !minisnn_worlds_trainable_brain_set_reward_learning_rate(
            brain, WF1_A5_LEARNING_RATE) ||
        !minisnn_worlds_trainable_brain_set_reward_eligibility_tau(
            brain, variant->eligibility_tau))
    {
        goto done;
    }
    for (episode = 0U; episode <= 1U; ++episode)
    {
        WF1TrainableFishEpisodeResult result;

        variant->episode = episode;
        if (!wf1_trainable_fish_run_episode_observed(
                brain, config, WF1_TRAINABLE_FISH_PHASE_TRAINED,
                WF1_A5_BRAIN_SEED, episode_seed(episode), capture_first_reward,
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

static int same_pre_reward(const WF1A5Variant *first, const WF1A5Variant *other)
{
    return first->observation.brain_seed == other->observation.brain_seed &&
        first->observation.world_seed == other->observation.world_seed &&
        first->episode == other->episode &&
        first->observation.episode_tick == other->observation.episode_tick &&
        first->observation.core_tick == other->observation.core_tick &&
        first->observation.action == other->observation.action &&
        first->observation.reward == other->observation.reward &&
        first->observation.energy_gain == other->observation.energy_gain &&
        first->observation.weight_signature_before ==
            other->observation.weight_signature_before;
}

static int write_csv(const char *path, const WF1A5Variant variants[WF1_A5_VARIANT_COUNT])
{
    FILE *file;
    uint32_t index;

    file = fopen(path, "wb");
    if (file == NULL)
        return 0;
    if (fprintf(file,
        "eligibility_tau,brain_seed,world_seed,episode,episode_tick,core_tick,action,reward,energy_gain,"
        "active_eligibility_count,modified_connection_count,eligibility_mean,"
        "eligibility_mean_absolute,eligibility_min,eligibility_max,eligibility_max_absolute,"
        "weight_absolute_change,weight_signed_change,clamp_min_delta,clamp_max_delta,"
        "weight_signature_before,weight_signature_after\n") < 0)
    {
        fclose(file);
        return 0;
    }
    for (index = 0U; index < WF1_A5_VARIANT_COUNT; ++index)
    {
        const WF1TrainableFishRewardObservation *observation = &variants[index].observation;
        const MiniSNNRewardStats *before = &observation->stats_before;
        const MiniSNNRewardStats *after = &observation->stats_after;

        if (fprintf(file,
            "%.17g,%" PRIu64 ",%" PRIu64 ",%u,%u,%" PRIu64 ",EAT,%.17g,%.17g,%zu,%zu,"
            "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%llu,%llu,%016" PRIx64 ",%016" PRIx64 "\n",
            variants[index].eligibility_tau, observation->brain_seed,
            observation->world_seed, variants[index].episode,
            observation->episode_tick, observation->core_tick, observation->reward,
            observation->energy_gain, after->last_active_eligibility_count,
            after->last_modified_connection_count, after->eligibility_final_mean,
            after->eligibility_final_mean_absolute, after->eligibility_final_min,
            after->eligibility_final_max, after->eligibility_max_absolute_observed,
            after->last_weight_absolute_change, after->last_weight_signed_change,
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

const char *wf1_a5_tau_probe_diagnosis_name(WF1A5TauProbeDiagnosis diagnosis)
{
    switch (diagnosis)
    {
        case WF1_A5_DIAGNOSIS_TEMPORAL_CREDIT_SCOPE_SUPPORTED:
            return "TEMPORAL_CREDIT_SCOPE_SUPPORTED";
        case WF1_A5_DIAGNOSIS_EFFECTIVE_TEMPORAL_CREDIT_REDUCED:
            return "EFFECTIVE_TEMPORAL_CREDIT_REDUCED";
        case WF1_A5_DIAGNOSIS_TAU_EFFECT_WEAK_AT_OBSERVED_REWARD:
            return "TAU_EFFECT_WEAK_AT_OBSERVED_REWARD";
        default:
            return "INCONCLUSIVE";
    }
}

static WF1A5TauProbeDiagnosis diagnose(
    const WF1A5Variant variants[WF1_A5_VARIANT_COUNT])
{
    const MiniSNNRewardStats *high = &variants[0].observation.stats_after;
    const MiniSNNRewardStats *mid = &variants[1].observation.stats_after;
    const MiniSNNRewardStats *low = &variants[2].observation.stats_after;
    int magnitude_decreases =
        high->eligibility_final_mean_absolute > mid->eligibility_final_mean_absolute &&
        mid->eligibility_final_mean_absolute > low->eligibility_final_mean_absolute &&
        high->last_weight_absolute_change > mid->last_weight_absolute_change &&
        mid->last_weight_absolute_change > low->last_weight_absolute_change;
    int counts_stable = low->last_active_eligibility_count * 10U >=
            high->last_active_eligibility_count * 9U &&
        low->last_modified_connection_count * 10U >=
            high->last_modified_connection_count * 9U;
    int effect_weak = low->eligibility_final_mean_absolute * 10.0 >=
            high->eligibility_final_mean_absolute * 9.0 &&
        low->last_weight_absolute_change * 10.0 >=
            high->last_weight_absolute_change * 9.0;

    if (magnitude_decreases)
    {
        return counts_stable ? WF1_A5_DIAGNOSIS_EFFECTIVE_TEMPORAL_CREDIT_REDUCED :
            WF1_A5_DIAGNOSIS_TEMPORAL_CREDIT_SCOPE_SUPPORTED;
    }
    if (effect_weak)
        return WF1_A5_DIAGNOSIS_TAU_EFFECT_WEAK_AT_OBSERVED_REWARD;
    return WF1_A5_DIAGNOSIS_INCONCLUSIVE;
}

static int write_summary(const char *path, const WF1A5TauProbeSummary *summary,
                         const WF1A5Variant variants[WF1_A5_VARIANT_COUNT])
{
    const MiniSNNRewardStats *high = &variants[0].observation.stats_after;
    const MiniSNNRewardStats *mid = &variants[1].observation.stats_after;
    const MiniSNNRewardStats *low = &variants[2].observation.stats_after;
    unsigned long long clamp_count =
        high->last_weight_clamp_min_count + high->last_weight_clamp_max_count +
        mid->last_weight_clamp_min_count + mid->last_weight_clamp_max_count +
        low->last_weight_clamp_min_count + low->last_weight_clamp_max_count;
    FILE *file = fopen(path, "wb");

    if (file == NULL)
        return 0;
    return fprintf(file,
        "experiment=WF1-A.5\nsource_protocol=WF1-A.4\nbehavior_tuning=NONE\n"
        "neural_variable_changed=eligibility_tau_only\n"
        "reward_learning_rate=0.01\nterminal_reward=0\n"
        "brain_seed=101\nworld_seed=300001\nepisode=1\n"
        "pre_reward_equivalence=%s\neligibility_taus=100,50,20\n"
        "tau_100_eligibility_mean_absolute=%.17g\n"
        "tau_50_eligibility_mean_absolute=%.17g\n"
        "tau_20_eligibility_mean_absolute=%.17g\n"
        "tau_100_weight_absolute_change=%.17g\n"
        "tau_50_weight_absolute_change=%.17g\n"
        "tau_20_weight_absolute_change=%.17g\n"
        "CLAMPS=%s\nDIAGNOSIS=%s\nPILOT=NOT_EXECUTED_QUOTA\n",
        summary->pre_reward_equivalent ? "PASS" : "FAIL",
        high->eligibility_final_mean_absolute, mid->eligibility_final_mean_absolute,
        low->eligibility_final_mean_absolute, high->last_weight_absolute_change,
        mid->last_weight_absolute_change, low->last_weight_absolute_change,
        summary->clamps_zero && clamp_count == 0U ? "ZERO" : "OBSERVED",
        wf1_a5_tau_probe_diagnosis_name(summary->diagnosis)) >= 0 && fclose(file) == 0;
}

int wf1_a5_tau_probe_run(const char *output_directory,
                         WF1A5TauProbeSummary *out_summary)
{
    static const double eligibility_taus[WF1_A5_VARIANT_COUNT] = { 100.0, 50.0, 20.0 };
    WF1TrainableFishConfig config;
    WF1A5Variant variants[WF1_A5_VARIANT_COUNT];
    WF1A5TauProbeSummary summary;
    char csv_path[WF1_A5_PATH_MAX];
    char summary_path[WF1_A5_PATH_MAX];
    uint32_t index;

    if (output_directory == NULL || output_directory[0] == '\0' || out_summary == NULL ||
        snprintf(csv_path, sizeof(csv_path), "%s/tau_probe.csv", output_directory) < 0 ||
        snprintf(summary_path, sizeof(summary_path), "%s/summary.txt", output_directory) < 0)
    {
        return 0;
    }
    wf1_a2_terminal_ablation_config(&config);
    if (!wf1_trainable_fish_config_is_valid(&config))
        return 0;
    memset(&variants, 0, sizeof(variants));
    memset(&summary, 0, sizeof(summary));
    for (index = 0U; index < WF1_A5_VARIANT_COUNT; ++index)
    {
        variants[index].eligibility_tau = eligibility_taus[index];
        if (!run_variant(&config, &variants[index]))
            return 0;
        summary.variants_completed++;
    }
    summary.pre_reward_equivalent = same_pre_reward(&variants[0], &variants[1]) &&
        same_pre_reward(&variants[0], &variants[2]);
    if (!summary.pre_reward_equivalent)
        return 0;
    summary.clamps_zero =
        variants[0].observation.stats_after.last_weight_clamp_min_count == 0U &&
        variants[0].observation.stats_after.last_weight_clamp_max_count == 0U &&
        variants[1].observation.stats_after.last_weight_clamp_min_count == 0U &&
        variants[1].observation.stats_after.last_weight_clamp_max_count == 0U &&
        variants[2].observation.stats_after.last_weight_clamp_min_count == 0U &&
        variants[2].observation.stats_after.last_weight_clamp_max_count == 0U;
    summary.diagnosis = diagnose(variants);
    if (!write_csv(csv_path, variants) || !write_summary(summary_path, &summary, variants))
        return 0;
    *out_summary = summary;
    return 1;
}