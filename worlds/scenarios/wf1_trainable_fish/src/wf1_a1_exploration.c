#include "wf1_a1_exploration.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define WF1_A1_PATH_MAX 480U
#define WF1_A1_CALIBRATION_SEED_COUNT 2U
#define WF1_A1_CANDIDATE_COUNT 3U

typedef struct
{
    uint64_t decisions, waits, move_pos_x, move_neg_x, move_pos_y, move_neg_y;
    uint64_t eats, move_applied, move_rejected, eat_applied, eat_rejected;
    uint64_t successful_eats, positive_rewards, terminal_rewards, starvation_count;
    uint64_t total_spikes, active_decisions, zero_scores, ties;
    double food_sum, reward_sum;
    uint32_t successes;
} WF1A1Metrics;

typedef struct
{
    uint64_t initial_signature, trained_signature, loaded_signature;
    uint64_t evaluation_before, evaluation_after;
    WF1A1Metrics untrained, training, trained_evaluation, control;
} WF1A1Seed;

static const uint64_t calibration_brain_seeds[WF1_A1_CALIBRATION_SEED_COUNT] =
{ UINT64_C(1001), UINT64_C(1002) };

static void metrics_add(WF1A1Metrics *m, const WF1TrainableFishEpisodeResult *r)
{
    m->decisions += r->decision_count; m->waits += r->wait_selected;
    m->move_pos_x += r->move_pos_x_selected; m->move_neg_x += r->move_neg_x_selected;
    m->move_pos_y += r->move_pos_y_selected; m->move_neg_y += r->move_neg_y_selected;
    m->eats += r->eat_selected; m->move_applied += r->move_applied;
    m->move_rejected += r->move_rejected; m->eat_applied += r->eat_applied;
    m->eat_rejected += r->eat_rejected; m->successful_eats += r->successful_eat_count;
    m->positive_rewards += r->positive_reward_count; m->terminal_rewards += r->terminal_reward_count;
    m->starvation_count += r->starvation_count; m->total_spikes += r->total_spikes;
    m->active_decisions += r->active_decision_count; m->zero_scores += r->zero_score_decisions;
    m->ties += r->tied_winner_decisions; m->food_sum += (double)r->foods_eaten;
    m->reward_sum += r->cumulative_reward; m->successes += r->foods_eaten != 0U ? 1U : 0U;
}

static uint64_t metrics_moves(const WF1A1Metrics *m)
{
    return m->move_pos_x + m->move_neg_x + m->move_pos_y + m->move_neg_y;
}

static uint32_t metrics_move_directions(const WF1A1Metrics *m)
{
    return (m->move_pos_x != 0U ? 1U : 0U) + (m->move_neg_x != 0U ? 1U : 0U) +
        (m->move_pos_y != 0U ? 1U : 0U) + (m->move_neg_y != 0U ? 1U : 0U);
}

static int metrics_bootstrapped(const WF1A1Metrics *m)
{
    return m->decisions != 0U && m->total_spikes != 0U && m->active_decisions != 0U &&
        metrics_moves(m) != 0U && metrics_move_directions(m) >= 2U && m->eats != 0U &&
        m->waits * UINT64_C(100) < m->decisions * UINT64_C(90);
}

static int metrics_is_better(const WF1A1Metrics *candidate, const WF1A1Metrics *current)
{
    uint64_t cn = metrics_moves(candidate) + candidate->eats;
    uint64_t on = metrics_moves(current) + current->eats;
    if (metrics_bootstrapped(candidate) != metrics_bootstrapped(current))
        return metrics_bootstrapped(candidate);
    if (metrics_move_directions(candidate) != metrics_move_directions(current))
        return metrics_move_directions(candidate) > metrics_move_directions(current);
    if (candidate->eats != current->eats) return candidate->eats > current->eats;
    if (cn != on) return cn > on;
    if (candidate->active_decisions != current->active_decisions)
        return candidate->active_decisions > current->active_decisions;
    return candidate->waits < current->waits;
}

static void configure_environment(WF1TrainableFishConfig *config)
{
    config->max_energy = UINT64_C(120); config->initial_energy = UINT64_C(96);
    config->metabolism_per_tick = UINT64_C(1); config->move_energy_cost = UINT64_C(1);
    config->eat_range = 0; config->food_nutrition = UINT64_C(20);
    config->food_count = 24U; config->food_grid_radius = INT64_C(2);
    config->world_bound = INT64_C(3000); config->max_episode_ticks = 96U;
}

static void configure_candidate(WF1TrainableFishConfig *config, uint32_t candidate)
{
    *config = wf1_trainable_fish_config_default();
    configure_environment(config);
    config->brain_config.topology = MINISNN_WORLDS_BRAIN_TOPOLOGY_RANDOM;
    config->brain_config.neuron_count = 42U;
    config->brain_config.neural_config.neuron_count = 42;
    config->brain_config.inhibitory_count = 7U;
    config->brain_config.connection_probability = 0.20;
    config->brain_config.excitatory_weight = 50.0;
    config->brain_config.inhibitory_weight = -40.0;
    config->brain_config.decision_steps_per_tick = 16U;
    config->brain_config.action_population_size = candidate + 1U;
    (void)snprintf(config->brain_config.brain_name, sizeof(config->brain_config.brain_name),
                   "wf1_a1_exploration_%u", candidate + 1U);
}

static int create_brain(const WF1TrainableFishConfig *config, uint64_t seed,
                        MiniSNNWorldsBrainMode mode, MiniSNNWorldsTrainableBrain **out_brain)
{
    MiniSNNWorldsBrainConfig bc = config->brain_config;
    MiniSNNWorldsTrainableBrainError error;
    if (out_brain == NULL) return 0;
    bc.seed = seed; bc.mode = mode;
    bc.plasticity_enabled = mode == MINISNN_WORLDS_BRAIN_MODE_TRAINING ? 1 : 0;
    *out_brain = minisnn_worlds_trainable_brain_create(&bc, &error);
    return *out_brain != NULL && error == MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE;
}

static uint64_t episode_seed(uint64_t base, uint32_t seed_index, uint32_t episode)
{
    return base + (uint64_t)seed_index * UINT64_C(1000) + (uint64_t)episode;
}
static int run_metrics_set(MiniSNNWorldsTrainableBrain *brain,
                           const WF1TrainableFishConfig *config,
                           WF1TrainableFishPhase phase, uint64_t brain_seed,
                           uint64_t base, uint32_t seed_index, uint32_t episodes,
                           WF1A1Metrics *out_metrics)
{
    uint32_t episode;
    if (out_metrics == NULL) return 0;
    for (episode = 0U; episode < episodes; ++episode)
    {
        WF1TrainableFishEpisodeResult r;
        if (!wf1_trainable_fish_run_episode(brain, config, phase, brain_seed,
            episode_seed(base, seed_index, episode), &r)) return 0;
        metrics_add(out_metrics, &r);
    }
    return 1;
}

static int write_metrics_columns(FILE *file, const WF1TrainableFishEpisodeResult *r)
{
    return fprintf(file, ",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%" PRIu64 ",%u,%u,%u,%u",
        r->decision_count, r->wait_selected, r->move_pos_x_selected,
        r->move_neg_x_selected, r->move_pos_y_selected, r->move_neg_y_selected,
        r->eat_selected, r->move_applied, r->move_rejected, r->eat_applied,
        r->eat_rejected, r->successful_eat_count, r->positive_reward_count,
        r->terminal_reward_count, r->total_spikes, r->active_decision_count,
        r->zero_score_decisions, r->tied_winner_decisions, r->starvation_count) >= 0;
}

static int write_training_header(FILE *file)
{
    return fprintf(file, "brain_seed,episode,world_seed,ticks,foods_eaten,cumulative_reward,"
        "death_cause,initial_or_start_weight_signature,end_weight_signature,decision_count,"
        "wait_selected,move_pos_x_selected,move_neg_x_selected,move_pos_y_selected,"
        "move_neg_y_selected,eat_selected,move_applied,move_rejected,eat_applied,"
        "eat_rejected,successful_eat_count,positive_reward_count,terminal_reward_count,"
        "total_spikes,active_decision_count,zero_score_decisions,tied_winner_decisions,"
        "starvation_count\n") >= 0;
}

static int write_evaluation_header(FILE *file)
{
    return fprintf(file, "phase,brain_seed,world_seed,ticks,foods_eaten,success,cumulative_reward,"
        "weight_signature_before,weight_signature_after,decision_count,wait_selected,"
        "move_pos_x_selected,move_neg_x_selected,move_pos_y_selected,move_neg_y_selected,"
        "eat_selected,move_applied,move_rejected,eat_applied,eat_rejected,successful_eat_count,"
        "positive_reward_count,terminal_reward_count,total_spikes,active_decision_count,"
        "zero_score_decisions,tied_winner_decisions,starvation_count\n") >= 0;
}

static int write_training_row(FILE *file, uint32_t episode,
                              const WF1TrainableFishEpisodeResult *r)
{
    return fprintf(file, "%" PRIu64 ",%u,%" PRIu64 ",%u,%u,%.17g,%s,%016" PRIx64 ",%016" PRIx64,
        r->brain_seed, episode, r->world_seed, r->ticks, r->foods_eaten,
        r->cumulative_reward, r->starvation_count != 0U ? "STARVATION" : "NONE",
        r->weight_signature_before, r->weight_signature_after) >= 0 &&
        write_metrics_columns(file, r) && fputc('\n', file) != EOF;
}

static int write_evaluation_row(FILE *file, const WF1TrainableFishEpisodeResult *r)
{
    return fprintf(file, "%s,%" PRIu64 ",%" PRIu64 ",%u,%u,%u,%.17g,%016" PRIx64 ",%016" PRIx64,
        wf1_trainable_fish_phase_name(r->phase), r->brain_seed, r->world_seed,
        r->ticks, r->foods_eaten, r->foods_eaten != 0U ? 1U : 0U,
        r->cumulative_reward, r->weight_signature_before, r->weight_signature_after) >= 0 &&
        write_metrics_columns(file, r) && fputc('\n', file) != EOF;
}

static int run_evaluation_set(MiniSNNWorldsTrainableBrain *brain,
                              const WF1TrainableFishConfig *config,
                              WF1TrainableFishPhase phase, uint32_t seed_index,
                              uint64_t brain_seed, FILE *file, uint32_t episodes,
                              WF1A1Metrics *out_metrics)
{
    uint32_t episode;
    for (episode = 0U; episode < episodes; ++episode)
    {
        WF1TrainableFishEpisodeResult r;
        if (!wf1_trainable_fish_run_episode(brain, config, phase, brain_seed,
            episode_seed(config->evaluation_world_seed_base, seed_index, episode), &r) ||
            !write_evaluation_row(file, &r)) return 0;
        metrics_add(out_metrics, &r);
    }
    return 1;
}

static int write_seed_header(FILE *file)
{
    return fprintf(file, "brain_seed,initial_weight_signature,trained_weight_signature,"
        "evaluation_weight_before,evaluation_weight_after,loaded_weight_signature,"
        "untrained_mean_food,trained_mean_food,control_mean_food,"
        "untrained_success_rate,trained_success_rate,control_success_rate,training_food_total,"
        "positive_rewards,starvation_count,wait_rate,move_rate,eat_rate\n") >= 0;
}

static int write_seed_row(FILE *file, uint64_t seed, const WF1A1Seed *s, uint32_t evals)
{
    double e = (double)evals, a = (double)s->training.decisions;
    return fprintf(file, "%" PRIu64 ",%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64 ","
        "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%" PRIu64 ",%" PRIu64 ",%.17g,%.17g,%.17g\n",
        seed, s->initial_signature, s->trained_signature, s->evaluation_before,
        s->evaluation_after, s->loaded_signature, s->untrained.food_sum / e,
        s->trained_evaluation.food_sum / e, s->control.food_sum / e,
        (double)s->untrained.successes / e, (double)s->trained_evaluation.successes / e,
        (double)s->control.successes / e, s->training.food_sum,
        s->training.positive_rewards, s->training.starvation_count,
        a == 0.0 ? 0.0 : (double)s->training.waits / a,
        a == 0.0 ? 0.0 : (double)metrics_moves(&s->training) / a,
        a == 0.0 ? 0.0 : (double)s->training.eats / a) >= 0;
}static int calibrate_original(WF1A1Metrics *out_metrics)
{
    WF1TrainableFishConfig config = wf1_trainable_fish_config_default();
    uint32_t index;
    for (index = 0U; index < WF1_A1_CALIBRATION_SEED_COUNT; ++index)
    {
        MiniSNNWorldsTrainableBrain *brain = NULL;
        if (!create_brain(&config, calibration_brain_seeds[index],
                          MINISNN_WORLDS_BRAIN_MODE_EVALUATION, &brain) ||
            !run_metrics_set(brain, &config, WF1_TRAINABLE_FISH_PHASE_UNTRAINED,
                calibration_brain_seeds[index], UINT64_C(700000), index, 1U, out_metrics))
        {
            minisnn_worlds_trainable_brain_destroy(&brain);
            return 0;
        }
        minisnn_worlds_trainable_brain_destroy(&brain);
    }
    return 1;
}

static int choose_bootstrap_config(WF1TrainableFishConfig *out_config,
                                   char *out_name, size_t out_name_size,
                                   WF1A1Metrics *out_metrics)
{
    WF1A1Metrics best;
    uint32_t best_candidate = 0U, candidate;
    memset(&best, 0, sizeof(best));
    for (candidate = 0U; candidate < WF1_A1_CANDIDATE_COUNT; ++candidate)
    {
        WF1TrainableFishConfig config;
        WF1A1Metrics metrics;
        uint32_t index;
        memset(&metrics, 0, sizeof(metrics));
        configure_candidate(&config, candidate);
        if (!wf1_trainable_fish_config_is_valid(&config)) return 0;
        for (index = 0U; index < WF1_A1_CALIBRATION_SEED_COUNT; ++index)
        {
            MiniSNNWorldsTrainableBrain *brain = NULL;
            if (!create_brain(&config, calibration_brain_seeds[index],
                              MINISNN_WORLDS_BRAIN_MODE_EVALUATION, &brain) ||
                !run_metrics_set(brain, &config, WF1_TRAINABLE_FISH_PHASE_UNTRAINED,
                    calibration_brain_seeds[index], UINT64_C(700000), index, 1U, &metrics))
            {
                minisnn_worlds_trainable_brain_destroy(&brain);
                return 0;
            }
            minisnn_worlds_trainable_brain_destroy(&brain);
        }
        if (candidate == 0U || metrics_is_better(&metrics, &best))
        {
            best_candidate = candidate;
            best = metrics;
        }
    }
    configure_candidate(out_config, best_candidate);
    if (snprintf(out_name, out_name_size, "random_42_output_population_%u",
                 best_candidate + 1U) < 0) return 0;
    *out_metrics = best;
    return 1;
}

static int write_summary(const char *path, const WF1TrainableFishConfig *config,
                         const WF1A1ExplorationSummary *summary,
                         const WF1A1Metrics *original, const WF1A1Metrics *bootstrap,
                         double untrained_food, double trained_food, double control_food,
                         double untrained_success, double trained_success, double control_success)
{
    FILE *file = fopen(path, "wb");
    double od = (double)original->decisions, bd = (double)bootstrap->decisions;
    int ok;
    if (file == NULL) return 0;
    ok = fprintf(file,
        "experiment=WF1-A.1 exploration bootstrap\n"
        "predecessor_result=WF1-A:NO_IMPROVEMENT\n"
        "calibration_config=%s\ncalibration_brain_seeds=1001,1002\n"
        "calibration_world_seed_base=700000\nbrain_seeds=101,202,303,404\n"
        "training_world_seed_base=200000\nevaluation_world_seed_base=950000\n"
        "training_episodes_per_seed=%u\nevaluation_episodes_per_seed=%u\n"
        "environment=bound:%" PRId64 ",grid_radius:%" PRId64 ",food_count:%u,initial_energy:%" PRIu64 ",max_energy:%" PRIu64 ",nutrition:%" PRIu64 ",max_ticks:%u\n"
        "action_population_size=%u\n"
        "original_decisions=%" PRIu64 "\noriginal_wait_rate=%.17g\noriginal_move_rate=%.17g\noriginal_eat_rate=%.17g\noriginal_total_spikes=%" PRIu64 "\n"
        "original_zero_score_decisions=%" PRIu64 "\noriginal_tied_winner_decisions=%" PRIu64 "\n"
        "bootstrap_decisions=%" PRIu64 "\nbootstrap_wait_rate=%.17g\nbootstrap_move_rate=%.17g\nbootstrap_eat_rate=%.17g\nbootstrap_total_spikes=%" PRIu64 "\n"
        "bootstrap_zero_score_decisions=%" PRIu64 "\nbootstrap_tied_winner_decisions=%" PRIu64 "\n"
        "training_successful_eats=%" PRIu64 "\npositive_rewards=%" PRIu64 "\n"
        "untrained_mean_food=%.17g\ntrained_mean_food=%.17g\ncontrol_mean_food=%.17g\n"
        "untrained_success_rate=%.17g\ntrained_success_rate=%.17g\ncontrol_success_rate=%.17g\n"
        "improved_seed_count=%u\nchanged_weight_seed_count=%u\n"
        "evaluation_plasticity=OFF\nevaluation_weight_freeze=YES\n"
        "exploration_bootstrapped=%s\npositive_reward_reached=%s\nresult=%s\n",
        summary->calibration_config, config->training_episodes, config->evaluation_episodes,
        config->world_bound, config->food_grid_radius, config->food_count,
        config->initial_energy, config->max_energy, config->food_nutrition,
        config->max_episode_ticks, config->brain_config.action_population_size,
        original->decisions, od == 0.0 ? 0.0 : (double)original->waits / od,
        od == 0.0 ? 0.0 : (double)metrics_moves(original) / od,
        od == 0.0 ? 0.0 : (double)original->eats / od, original->total_spikes,
        original->zero_scores, original->ties, bootstrap->decisions,
        bd == 0.0 ? 0.0 : (double)bootstrap->waits / bd,
        bd == 0.0 ? 0.0 : (double)metrics_moves(bootstrap) / bd,
        bd == 0.0 ? 0.0 : (double)bootstrap->eats / bd, bootstrap->total_spikes,
        bootstrap->zero_scores, bootstrap->ties, summary->training_successful_eats,
        summary->positive_rewards, untrained_food, trained_food, control_food,
        untrained_success, trained_success, control_success, summary->improved_seed_count,
        summary->changed_weight_seed_count, summary->exploration_bootstrapped ? "YES" : "NO",
        summary->positive_reward_reached ? "YES" : "NO",
        wf1_trainable_fish_result_name(summary->result)) >= 0;
    return fclose(file) == 0 && ok;
}int wf1_a1_exploration_run_experiment(const char *output_directory,
                                      WF1A1ExplorationSummary *out_summary)
{
    WF1TrainableFishConfig config;
    WF1A1ExplorationSummary summary;
    WF1A1Metrics original, bootstrap;
    WF1A1Seed seeds[WF1_TRAINABLE_FISH_MAX_BRAIN_SEEDS];
    char training_path[WF1_A1_PATH_MAX], evaluation_path[WF1_A1_PATH_MAX];
    char seeds_path[WF1_A1_PATH_MAX], summary_path[WF1_A1_PATH_MAX];
    FILE *training = NULL, *evaluation = NULL, *seed_file = NULL;
    uint32_t seed_index;
    int ok = 0;

    if (output_directory == NULL || output_directory[0] == '\0' || out_summary == NULL)
        return 0;
    memset(&summary, 0, sizeof(summary)); memset(&original, 0, sizeof(original));
    memset(&bootstrap, 0, sizeof(bootstrap)); memset(seeds, 0, sizeof(seeds));
    if (!calibrate_original(&original) || !choose_bootstrap_config(&config,
        summary.calibration_config, sizeof(summary.calibration_config), &bootstrap)) return 0;
    config.training_episodes = 256U; config.evaluation_episodes = 24U;
    config.training_world_seed_base = UINT64_C(200000);
    config.evaluation_world_seed_base = UINT64_C(950000);
    if (!wf1_trainable_fish_config_is_valid(&config) ||
        snprintf(training_path, sizeof(training_path), "%s/training.csv", output_directory) < 0 ||
        snprintf(evaluation_path, sizeof(evaluation_path), "%s/evaluation.csv", output_directory) < 0 ||
        snprintf(seeds_path, sizeof(seeds_path), "%s/seeds.csv", output_directory) < 0 ||
        snprintf(summary_path, sizeof(summary_path), "%s/summary.txt", output_directory) < 0) return 0;
    training = fopen(training_path, "wb"); evaluation = fopen(evaluation_path, "wb");
    seed_file = fopen(seeds_path, "wb");
    if (training == NULL || evaluation == NULL || seed_file == NULL ||
        !write_training_header(training) || !write_evaluation_header(evaluation) ||
        !write_seed_header(seed_file)) goto done;

    for (seed_index = 0U; seed_index < config.brain_seed_count; ++seed_index)
    {
        uint64_t brain_seed = config.brain_seeds[seed_index];
        WF1A1Seed *seed = &seeds[seed_index];
        MiniSNNWorldsTrainableBrain *baseline = NULL, *trained = NULL;
        MiniSNNWorldsTrainableBrain *control = NULL, *loaded = NULL;
        MiniSNNWorldsTrainableBrainError error;
        char checkpoint[WF1_A1_PATH_MAX];
        uint32_t episode;

        if (!create_brain(&config, brain_seed, MINISNN_WORLDS_BRAIN_MODE_EVALUATION, &baseline))
            goto per_seed_done;
        seed->initial_signature = minisnn_worlds_trainable_brain_weight_signature(baseline);
        if (seed->initial_signature == 0U || !run_evaluation_set(baseline, &config,
            WF1_TRAINABLE_FISH_PHASE_UNTRAINED, seed_index, brain_seed, evaluation,
            config.evaluation_episodes, &seed->untrained) ||
            minisnn_worlds_trainable_brain_weight_signature(baseline) != seed->initial_signature ||
            !create_brain(&config, brain_seed, MINISNN_WORLDS_BRAIN_MODE_TRAINING, &trained) ||
            minisnn_worlds_trainable_brain_weight_signature(trained) != seed->initial_signature)
            goto per_seed_done;

        for (episode = 0U; episode < config.training_episodes; ++episode)
        {
            WF1TrainableFishEpisodeResult r;
            if (!wf1_trainable_fish_run_episode(trained, &config,
                WF1_TRAINABLE_FISH_PHASE_TRAINED, brain_seed,
                episode_seed(config.training_world_seed_base, seed_index, episode), &r) ||
                !write_training_row(training, episode, &r) ||
                minisnn_worlds_trainable_brain_weight_signature(trained) != r.weight_signature_after)
                goto per_seed_done;
            metrics_add(&seed->training, &r);
        }
        seed->trained_signature = minisnn_worlds_trainable_brain_weight_signature(trained);
        if (seed->trained_signature == 0U || !minisnn_worlds_trainable_brain_set_mode(
            trained, MINISNN_WORLDS_BRAIN_MODE_EVALUATION)) goto per_seed_done;
        seed->evaluation_before = minisnn_worlds_trainable_brain_weight_signature(trained);
        if (!run_evaluation_set(trained, &config, WF1_TRAINABLE_FISH_PHASE_TRAINED,
            seed_index, brain_seed, evaluation, config.evaluation_episodes, &seed->trained_evaluation) ||
            (seed->evaluation_after = minisnn_worlds_trainable_brain_weight_signature(trained)) !=
                seed->evaluation_before) goto per_seed_done;
        if (snprintf(checkpoint, sizeof(checkpoint), "%s/brain_%" PRIu64 ".wb1",
            output_directory, brain_seed) < 0 ||
            !minisnn_worlds_trainable_brain_save(trained, checkpoint, &error)) goto per_seed_done;
        loaded = minisnn_worlds_trainable_brain_load(checkpoint, &error);
        if (loaded == NULL || minisnn_worlds_trainable_brain_weight_signature(loaded) !=
            seed->evaluation_after || !run_evaluation_set(loaded, &config,
            WF1_TRAINABLE_FISH_PHASE_LOADED, seed_index, brain_seed, evaluation, 1U,
            &(WF1A1Metrics){0})) goto per_seed_done;
        seed->loaded_signature = minisnn_worlds_trainable_brain_weight_signature(loaded);
        if (!create_brain(&config, brain_seed, MINISNN_WORLDS_BRAIN_MODE_EVALUATION, &control))
            goto per_seed_done;
        for (episode = 0U; episode < config.training_episodes; ++episode)
        {
            WF1TrainableFishEpisodeResult r;
            if (!wf1_trainable_fish_run_episode(control, &config,
                WF1_TRAINABLE_FISH_PHASE_CONTROL, brain_seed,
                episode_seed(config.training_world_seed_base, seed_index, episode), &r) ||
                minisnn_worlds_trainable_brain_weight_signature(control) != seed->initial_signature)
                goto per_seed_done;
        }
        if (!run_evaluation_set(control, &config, WF1_TRAINABLE_FISH_PHASE_CONTROL,
            seed_index, brain_seed, evaluation, config.evaluation_episodes, &seed->control) ||
            !write_seed_row(seed_file, brain_seed, seed, config.evaluation_episodes)) goto per_seed_done;
        if (seed->trained_evaluation.food_sum > seed->untrained.food_sum) summary.improved_seed_count++;
        if (seed->trained_signature != seed->initial_signature) summary.changed_weight_seed_count++;
        summary.training_successful_eats += seed->training.successful_eats;
        summary.positive_rewards += seed->training.positive_rewards;
        minisnn_worlds_trainable_brain_destroy(&baseline); minisnn_worlds_trainable_brain_destroy(&trained);
        minisnn_worlds_trainable_brain_destroy(&control); minisnn_worlds_trainable_brain_destroy(&loaded);
        continue;
per_seed_done:
        minisnn_worlds_trainable_brain_destroy(&baseline); minisnn_worlds_trainable_brain_destroy(&trained);
        minisnn_worlds_trainable_brain_destroy(&control); minisnn_worlds_trainable_brain_destroy(&loaded);
        goto done;
    }
    {
        double uf = 0.0, tf = 0.0, cf = 0.0, us = 0.0, ts = 0.0, cs = 0.0;
        double denominator = (double)(config.brain_seed_count * config.evaluation_episodes);
        uint32_t index;
        for (index = 0U; index < config.brain_seed_count; ++index)
        {
            uf += seeds[index].untrained.food_sum; tf += seeds[index].trained_evaluation.food_sum;
            cf += seeds[index].control.food_sum; us += (double)seeds[index].untrained.successes;
            ts += (double)seeds[index].trained_evaluation.successes; cs += (double)seeds[index].control.successes;
        }
        uf /= denominator; tf /= denominator; cf /= denominator;
        us /= denominator; ts /= denominator; cs /= denominator;
        summary.original_decisions = original.decisions; summary.original_waits = original.waits;
        summary.original_moves = metrics_moves(&original); summary.original_eats = original.eats;
        summary.original_total_spikes = original.total_spikes;
        summary.bootstrap_decisions = bootstrap.decisions; summary.bootstrap_waits = bootstrap.waits;
        summary.bootstrap_moves = metrics_moves(&bootstrap); summary.bootstrap_eats = bootstrap.eats;
        summary.bootstrap_total_spikes = bootstrap.total_spikes;
        summary.exploration_bootstrapped = metrics_bootstrapped(&bootstrap);
        summary.positive_reward_reached = summary.positive_rewards != 0U;
        if (tf > uf && tf > cf && summary.improved_seed_count > config.brain_seed_count / 2U)
            summary.result = WF1_TRAINABLE_FISH_RESULT_LEARNING_DETECTED;
        else if (tf <= uf) summary.result = WF1_TRAINABLE_FISH_RESULT_NO_IMPROVEMENT;
        else summary.result = WF1_TRAINABLE_FISH_RESULT_INCONCLUSIVE;
        if (!write_summary(summary_path, &config, &summary, &original, &bootstrap,
            uf, tf, cf, us, ts, cs)) goto done;
    }
    *out_summary = summary; ok = 1;
done:
    if (training != NULL) fclose(training);
    if (evaluation != NULL) fclose(evaluation);
    if (seed_file != NULL) fclose(seed_file);
    return ok;
}