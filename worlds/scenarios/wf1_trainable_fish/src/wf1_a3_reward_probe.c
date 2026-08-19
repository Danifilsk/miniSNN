#include "wf1_a3_reward_probe.h"

#include "wf1_a2_terminal_ablation.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define WF1_A3_PATH_MAX 480U
#define WF1_A3_BRAIN_SEED UINT64_C(101)
#define WF1_A3_WORLD_SEED_BASE UINT64_C(300000)
#define WF1_A3_MAX_REWARDS 2U

typedef struct
{
    FILE *file;
    WF1A3RewardProbeSummary summary;
    uint32_t reward_index;
    uint32_t episode;
    int failed;
} WF1A3ProbeWriter;

static uint64_t episode_seed(uint32_t episode)
{
    return WF1_A3_WORLD_SEED_BASE + (uint64_t)episode;
}

static int create_training_brain(const WF1TrainableFishConfig *config,
                                 MiniSNNWorldsTrainableBrain **out_brain)
{
    MiniSNNWorldsBrainConfig brain_config;
    MiniSNNWorldsTrainableBrainError error;

    if (config == NULL || out_brain == NULL)
        return 0;
    brain_config = config->brain_config;
    brain_config.seed = WF1_A3_BRAIN_SEED;
    brain_config.mode = MINISNN_WORLDS_BRAIN_MODE_TRAINING;
    brain_config.plasticity_enabled = 1;
    *out_brain = minisnn_worlds_trainable_brain_create(&brain_config, &error);
    return *out_brain != NULL && error == MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE;
}

static int write_header(FILE *file)
{
    return fprintf(file,
        "brain_seed,episode,world_seed,episode_tick,core_tick,action,reward,energy_gain,"
        "weight_signature_before,weight_signature_after,active_eligibility_count,"
        "modified_connection_count,weight_signed_change,weight_absolute_change,"
        "max_absolute_weight_change_observed,max_absolute_weight_change_delta,"
        "eligibility_mean,eligibility_mean_absolute,eligibility_min,eligibility_max,"
        "eligibility_max_absolute,weight_clamp_min_events_delta,"
        "weight_clamp_max_events_delta,reward_event_count_delta\n") >= 0;
}

static int observe_positive_reward(
    const WF1TrainableFishRewardObservation *observation,
    void *user_data)
{
    WF1A3ProbeWriter *writer = user_data;
    const MiniSNNRewardStats *before;
    const MiniSNNRewardStats *after;
    double max_delta;
    unsigned long long clamp_min_delta;
    unsigned long long clamp_max_delta;

    if (writer == NULL || observation == NULL || writer->file == NULL ||
        observation->action != MINISNN_WORLDS_BRAIN_ACTION_EAT ||
        observation->reward != 1.0 || observation->energy_gain <= 0.0)
    {
        return 0;
    }
    before = &observation->stats_before;
    after = &observation->stats_after;
    if (after->reward_event_count != before->reward_event_count + UINT64_C(1) ||
        after->positive_reward_event_count != before->positive_reward_event_count + UINT64_C(1) ||
        after->last_applied_reward != observation->reward ||
        !isfinite(after->last_weight_signed_change) ||
        !isfinite(after->last_weight_absolute_change) ||
        !isfinite(after->eligibility_final_mean) ||
        !isfinite(after->eligibility_final_mean_absolute) ||
        !isfinite(after->eligibility_final_min) ||
        !isfinite(after->eligibility_final_max) ||
        !isfinite(after->eligibility_max_absolute_observed) ||
        after->weight_clamp_min_events < before->weight_clamp_min_events ||
        after->weight_clamp_max_events < before->weight_clamp_max_events ||
        observation->weight_signature_before == 0U ||
        observation->weight_signature_after == 0U)
    {
        return 0;
    }
    max_delta = after->max_absolute_weight_change - before->max_absolute_weight_change;
    clamp_min_delta = after->weight_clamp_min_events - before->weight_clamp_min_events;
    clamp_max_delta = after->weight_clamp_max_events - before->weight_clamp_max_events;
    if (fprintf(writer->file,
        "%" PRIu64 ",%u,%" PRIu64 ",%u,%" PRIu64 ",EAT,%.17g,%.17g,%016" PRIx64 ",%016" PRIx64 ","
        "%zu,%zu,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%llu,%llu,%llu\n",
        observation->brain_seed, writer->episode, observation->world_seed,
        observation->episode_tick, observation->core_tick, observation->reward,
        observation->energy_gain, observation->weight_signature_before,
        observation->weight_signature_after, after->last_active_eligibility_count,
        after->last_modified_connection_count, after->last_weight_signed_change,
        after->last_weight_absolute_change, after->max_absolute_weight_change, max_delta,
        after->eligibility_final_mean, after->eligibility_final_mean_absolute,
        after->eligibility_final_min, after->eligibility_final_max,
        after->eligibility_max_absolute_observed, clamp_min_delta, clamp_max_delta,
        after->reward_event_count - before->reward_event_count) < 0)
    {
        writer->failed = 1;
        return 0;
    }
    writer->summary.rewards_observed++;
    writer->summary.mean_active_eligibility_count +=
        (double)after->last_active_eligibility_count;
    writer->summary.mean_modified_connection_count +=
        (double)after->last_modified_connection_count;
    writer->summary.mean_weight_absolute_change += after->last_weight_absolute_change;
    writer->summary.eligibility_mean_absolute += after->eligibility_final_mean_absolute;
    if (after->max_absolute_weight_change >
        writer->summary.max_weight_absolute_change_observed)
    {
        writer->summary.max_weight_absolute_change_observed =
            after->max_absolute_weight_change;
    }
    if (after->eligibility_max_absolute_observed >
        writer->summary.eligibility_max_absolute_observed)
    {
        writer->summary.eligibility_max_absolute_observed =
            after->eligibility_max_absolute_observed;
    }
    writer->summary.clamp_min_events += clamp_min_delta;
    writer->summary.clamp_max_events += clamp_max_delta;
    if (observation->weight_signature_before != observation->weight_signature_after)
        writer->summary.weight_changed_reward_count++;
    writer->reward_index++;
    return 1;
}

static const char *diagnosis(const WF1A3RewardProbeSummary *summary)
{
    int global_credit;
    int saturation;

    if (summary->mean_active_eligibility_count == 0.0 ||
        summary->mean_modified_connection_count == 0.0)
    {
        return "TEMPORAL_CREDIT_SUSPECT";
    }
    global_credit = summary->mean_modified_connection_count >= 100.0;
    saturation = summary->clamp_min_events != 0U || summary->clamp_max_events != 0U;
    if (global_credit && saturation)
        return "GLOBAL_CREDIT_SUSPECT,WEIGHT_SATURATION_SUSPECT";
    if (global_credit)
        return "GLOBAL_CREDIT_SUSPECT";
    if (saturation)
        return "WEIGHT_SATURATION_SUSPECT";
    return "CONFIGURATION_SUSPECT";
}

static int write_summary(const char *path, const WF1A3RewardProbeSummary *summary)
{
    FILE *file;

    file = fopen(path, "wb");
    if (file == NULL)
        return 0;
    return fprintf(file,
        "experiment=WF1-A.3\nsource_protocol=WF1-A.2\nbehavior_tuning=NONE\n"
        "terminal_reward=0\nterminal_boundary=STARVATION_DEATH\n"
        "positive_reward=EAT_APPLIED_ENERGY_GAIN\nbrain_seed=101\n"
        "training_world_seed_base=300000\nrewards_observed=%u\nepisodes_run=%u\n"
        "mean_active_eligibility_count=%.17g\nmean_modified_connection_count=%.17g\n"
        "mean_weight_absolute_change=%.17g\n"
        "max_absolute_weight_change_observed=%.17g\n"
        "eligibility_mean_absolute=%.17g\n"
        "eligibility_max_absolute_observed=%.17g\n"
        "clamp_min_events=%" PRIu64 "\nclamp_max_events=%" PRIu64 "\n"
        "weight_changed_reward_count=%u\n"
        "max_absolute_weight_change_note=public_reward_stats_high_watermark_not_per_reward_connection_maximum\n"
        "DIAGNOSIS=%s\n",
        summary->rewards_observed, summary->episodes_run,
        summary->mean_active_eligibility_count,
        summary->mean_modified_connection_count,
        summary->mean_weight_absolute_change,
        summary->max_weight_absolute_change_observed,
        summary->eligibility_mean_absolute,
        summary->eligibility_max_absolute_observed,
        summary->clamp_min_events, summary->clamp_max_events,
        summary->weight_changed_reward_count, diagnosis(summary)) >= 0 && fclose(file) == 0;
}

int wf1_a3_reward_probe_run(const char *output_directory,
                            WF1A3RewardProbeSummary *out_summary)
{
    WF1TrainableFishConfig config;
    WF1A3ProbeWriter writer;
    MiniSNNWorldsTrainableBrain *brain = NULL;
    char csv_path[WF1_A3_PATH_MAX];
    char summary_path[WF1_A3_PATH_MAX];
    uint32_t episode;
    int ok = 0;

    if (output_directory == NULL || output_directory[0] == '\0' || out_summary == NULL ||
        snprintf(csv_path, sizeof(csv_path), "%s/reward_probe.csv", output_directory) < 0 ||
        snprintf(summary_path, sizeof(summary_path), "%s/summary.txt", output_directory) < 0)
    {
        return 0;
    }
    wf1_a2_terminal_ablation_config(&config);
    if (!wf1_trainable_fish_config_is_valid(&config) ||
        !create_training_brain(&config, &brain))
    {
        return 0;
    }
    memset(&writer, 0, sizeof(writer));
    writer.file = fopen(csv_path, "wb");
    if (writer.file == NULL || !write_header(writer.file))
        goto done;
    for (episode = 0U; episode < config.training_episodes &&
         writer.summary.rewards_observed < WF1_A3_MAX_REWARDS; ++episode)
    {
        WF1TrainableFishEpisodeResult result;

        writer.episode = episode;
        if (!wf1_trainable_fish_run_episode_observed(
                brain, &config, WF1_TRAINABLE_FISH_PHASE_TRAINED,
                WF1_A3_BRAIN_SEED, episode_seed(episode), observe_positive_reward,
                &writer, &result))
        {
            goto done;
        }
        writer.summary.episodes_run++;
    }
    if (writer.failed || writer.summary.rewards_observed < 2U ||
        writer.summary.rewards_observed > WF1_A3_MAX_REWARDS)
    {
        goto done;
    }
    writer.summary.mean_active_eligibility_count /= writer.summary.rewards_observed;
    writer.summary.mean_modified_connection_count /= writer.summary.rewards_observed;
    writer.summary.mean_weight_absolute_change /= writer.summary.rewards_observed;
    writer.summary.eligibility_mean_absolute /= writer.summary.rewards_observed;
    if (fclose(writer.file) != 0)
    {
        writer.file = NULL;
        goto done;
    }
    writer.file = NULL;
    if (!write_summary(summary_path, &writer.summary))
        goto done;
    *out_summary = writer.summary;
    ok = 1;
done:
    if (writer.file != NULL)
        fclose(writer.file);
    minisnn_worlds_trainable_brain_destroy(&brain);
    return ok;
}