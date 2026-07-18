#ifndef ASSOCIATIVE_MEMORY_H
#define ASSOCIATIVE_MEMORY_H

#include <stddef.h>

#include "scenario_config.h"
#include "scenario_runtime.h"

typedef struct
{
    int epoch;
    int pair_id;
    double mean_cue_target_weight;
} AssociativeMemoryTrainingRecord;

typedef struct
{
    int trial;
    int pair_id;
    double cue_corruption;
    unsigned int cue_mask_signature;
    int cue_mask_consistent;
    int delay_probe_inputs_zero;
    int expected_pattern;
    int recalled_pattern;
    double pattern_similarity;
    double pattern_completion_score;
    int recall_correct;
    int first_response_step;
    double mean_target_activity;
} AssociativeMemoryTrial;

typedef struct
{
    int trial_count;
    int correct_trials;
    double recall_accuracy;
    double mean_pattern_similarity;
    double mean_completion_score;
    double mean_response_latency;
    double chance_accuracy;
    double control_accuracy;
    double association_margin;
    double untrained_accuracy;
    double shuffled_target_accuracy;
    double frozen_training_accuracy;
    double training_weight_absolute_change;
    int recall_weights_changed;
    int recall_reconstructed_from_blueprint;
    ScenarioBlueprint learned_blueprint;
    int training_record_count;
    AssociativeMemoryTrainingRecord *training_records;
    int connection_count;
    MiniSNNConnectionInfo *initial_connections;
    double *final_weights;
    AssociativeMemoryTrial *trials;
} AssociativeMemoryResult;

void associative_memory_result_destroy(AssociativeMemoryResult *result);

int associative_memory_decode_target(
    const int *target_group_spikes,
    const int *target_unit_active,
    int pair_count,
    int target_group_size,
    int expected_pattern,
    double *out_similarity,
    double *out_completion_score,
    int *out_recalled_pattern);

int associative_memory_execute(
    const ScenarioConfig *config,
    const ScenarioBlueprint *blueprint,
    AssociativeMemoryResult *out_result,
    char *error_message,
    size_t error_message_size);

int associative_memory_write_outputs(
    const ScenarioConfig *config,
    const AssociativeMemoryResult *result,
    const char *output_directory,
    char *error_message,
    size_t error_message_size);

#ifdef MINISNN_TESTING
int associative_memory_test_recall_accuracy(
    const ScenarioConfig *config,
    const ScenarioBlueprint *blueprint,
    double *out_accuracy,
    char *error_message,
    size_t error_message_size);
#endif

#endif
