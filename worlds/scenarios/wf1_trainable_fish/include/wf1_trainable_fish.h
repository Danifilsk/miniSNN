#ifndef WF1_TRAINABLE_FISH_H
#define WF1_TRAINABLE_FISH_H

#include <stdint.h>

#include "minisnn_worlds_brain_bridge.h"

#define WF1_TRAINABLE_FISH_MAX_BRAIN_SEEDS 8U
#define WF1_TRAINABLE_FISH_MAX_FOOD_COUNT 64U

typedef enum
{
    WF1_TRAINABLE_FISH_PHASE_UNTRAINED = 0,
    WF1_TRAINABLE_FISH_PHASE_TRAINED,
    WF1_TRAINABLE_FISH_PHASE_CONTROL,
    WF1_TRAINABLE_FISH_PHASE_LOADED
} WF1TrainableFishPhase;

typedef enum
{
    WF1_TRAINABLE_FISH_RESULT_LEARNING_DETECTED = 0,
    WF1_TRAINABLE_FISH_RESULT_NO_IMPROVEMENT,
    WF1_TRAINABLE_FISH_RESULT_INCONCLUSIVE
} WF1TrainableFishResult;

typedef struct
{
    MiniSNNWorldsBrainConfig brain_config;
    uint64_t brain_seeds[WF1_TRAINABLE_FISH_MAX_BRAIN_SEEDS];
    uint32_t brain_seed_count;
    uint32_t training_episodes;
    uint32_t evaluation_episodes;
    uint32_t max_episode_ticks;
    uint64_t training_world_seed_base;
    uint64_t evaluation_world_seed_base;
    double starvation_terminal_reward;

    /* Open-water environment values. Defaults retain WF1-A exactly; WF1-A.1
     * may change only these generic world-density and lifetime parameters. */
    uint64_t max_energy;
    uint64_t initial_energy;
    uint64_t metabolism_per_tick;
    uint64_t move_energy_cost;
    int64_t eat_range;
    uint64_t food_nutrition;
    uint32_t food_count;
    int64_t food_grid_radius;
    int64_t world_bound;
} WF1TrainableFishConfig;

typedef struct
{
    WF1TrainableFishPhase phase;
    uint64_t brain_seed;
    uint64_t world_seed;
    uint32_t ticks;
    uint32_t foods_eaten;
    uint32_t rejected_actions;
    uint32_t starvation_count;
    double cumulative_reward;
    uint64_t weight_signature_before;
    uint64_t weight_signature_after;

    /* Direct decision telemetry. These fields observe the generic WB1 output;
     * they never choose, replace, or reinterpret an action. */
    uint32_t decision_count;
    uint32_t wait_selected;
    uint32_t move_pos_x_selected;
    uint32_t move_neg_x_selected;
    uint32_t move_pos_y_selected;
    uint32_t move_neg_y_selected;
    uint32_t eat_selected;
    uint32_t move_applied;
    uint32_t move_rejected;
    uint32_t eat_applied;
    uint32_t eat_rejected;
    uint32_t successful_eat_count;
    uint32_t positive_reward_count;
    uint32_t terminal_reward_count;
    uint64_t total_spikes;
    uint32_t active_decision_count;
    uint32_t zero_score_decisions;
    uint32_t tied_winner_decisions;
} WF1TrainableFishEpisodeResult;

/* Observation is emitted only after the queued positive EAT reward is
 * actually delivered by the existing AgentCycle on the following Core tick. */
typedef struct
{
    uint64_t brain_seed;
    uint64_t world_seed;
    uint32_t episode_tick;
    uint64_t core_tick;
    MiniSNNWorldsBrainActionChannel action;
    double reward;
    double energy_gain;
    uint64_t weight_signature_before;
    uint64_t weight_signature_after;
    MiniSNNRewardStats stats_before;
    MiniSNNRewardStats stats_after;
} WF1TrainableFishRewardObservation;

typedef int (*WF1TrainableFishRewardObserver)(
    const WF1TrainableFishRewardObservation *observation,
    void *user_data);

typedef struct
{
    uint64_t topology_signature;
    uint64_t brain_config_signature;
    uint64_t initial_weight_signature;
    uint64_t trained_weight_signature;
    uint64_t loaded_weight_signature;
    uint64_t evaluation_weight_signature_before;
    uint64_t evaluation_weight_signature_after;
    double untrained_mean_food;
    double trained_mean_food;
    double control_mean_food;
    double untrained_success_rate;
    double trained_success_rate;
    double control_success_rate;
    WF1TrainableFishResult result;
} WF1TrainableFishExperimentSummary;

WF1TrainableFishConfig wf1_trainable_fish_config_default(void);
int wf1_trainable_fish_config_is_valid(const WF1TrainableFishConfig *config);
const char *wf1_trainable_fish_phase_name(WF1TrainableFishPhase phase);
const char *wf1_trainable_fish_result_name(WF1TrainableFishResult result);

/* Creates a fresh deterministic Domain/Kernel episode, binds the supplied
 * persistent WB1 brain, consumes every action consequence exactly once, and
 * resets only transient neural state at the boundary. */
int wf1_trainable_fish_run_episode(
    MiniSNNWorldsTrainableBrain *brain,
    const WF1TrainableFishConfig *config,
    WF1TrainableFishPhase phase,
    uint64_t brain_seed,
    uint64_t world_seed,
    WF1TrainableFishEpisodeResult *out_result);

/* Scenario-level diagnostic hook. It observes real positive feedback delivery
 * without choosing actions, changing rewards, or exposing Core internals. */
int wf1_trainable_fish_run_episode_observed(
    MiniSNNWorldsTrainableBrain *brain,
    const WF1TrainableFishConfig *config,
    WF1TrainableFishPhase phase,
    uint64_t brain_seed,
    uint64_t world_seed,
    WF1TrainableFishRewardObserver observer,
    void *user_data,
    WF1TrainableFishEpisodeResult *out_result);

/* Runs the WF1-A baseline/training/trained-evaluation/control/load protocol
 * and writes summary.txt, training.csv, evaluation.csv, and seeds.csv. */
int wf1_trainable_fish_run_experiment(
    const WF1TrainableFishConfig *config,
    const char *output_directory,
    WF1TrainableFishExperimentSummary *out_summary);

#endif