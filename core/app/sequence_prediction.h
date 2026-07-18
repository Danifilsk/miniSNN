#ifndef SEQUENCE_PREDICTION_H
#define SEQUENCE_PREDICTION_H

#include <stddef.h>

#include "scenario_config.h"
#include "scenario_runtime.h"

#define SEQUENCE_PREDICTION_PREFIX_TEXT_MAX 512

typedef struct
{
    int epoch;
    int sequence_id;
    int position;
    double mean_transition_weight;
} SequencePredictionTrainingRecord;

typedef struct
{
    int trial;
    int sequence_id;
    char prefix[SEQUENCE_PREDICTION_PREFIX_TEXT_MAX];
    int expected_next_pattern;
    int predicted_pattern;
    double prediction_similarity;
    int prediction_correct;
    double prediction_error;
    int first_prediction_step;
    double mean_prediction_activity;
    int prefix_is_incomplete;
    int delay_probe_inputs_zero;
} SequencePredictionTrial;

typedef struct
{
    int trial_count;
    int correct_predictions;
    double next_pattern_accuracy;
    double mean_prediction_similarity;
    double mean_prediction_error;
    double mean_prediction_latency;
    double chance_accuracy;
    double untrained_control_accuracy;
    double shuffled_order_control_accuracy;
    double frozen_training_control_accuracy;
    double permuted_labels_control_accuracy;
    double context_accuracy;
    double last_symbol_only_control_accuracy;
    double context_margin;
    double control_accuracy;
    double prediction_margin;
    double training_weight_absolute_change;
    int evaluation_weights_changed;
    int evaluation_reconstructed_from_blueprint;
    ScenarioBlueprint learned_blueprint;
    int training_record_count;
    SequencePredictionTrainingRecord *training_records;
    int connection_count;
    MiniSNNConnectionInfo *initial_connections;
    double *final_weights;
    SequencePredictionTrial *trials;
} SequencePredictionResult;

int sequence_prediction_pattern_id(
    const ScenarioConfig *config,
    int sequence_id,
    int position);

#ifdef MINISNN_TESTING
int sequence_prediction_test_decode_probe(
    const int *probe_frames,
    int frame_count,
    int pattern_count,
    int *out_predicted_pattern,
    double *out_similarity);
#endif

void sequence_prediction_result_destroy(SequencePredictionResult *result);

int sequence_prediction_execute(
    const ScenarioConfig *config,
    const ScenarioBlueprint *blueprint,
    SequencePredictionResult *out_result,
    char *error_message,
    size_t error_message_size);

int sequence_prediction_write_outputs(
    const ScenarioConfig *config,
    const SequencePredictionResult *result,
    const char *output_directory,
    char *error_message,
    size_t error_message_size);

#ifdef MINISNN_TESTING
int sequence_prediction_test_accuracy(
    const ScenarioConfig *config,
    const ScenarioBlueprint *blueprint,
    double *out_accuracy,
    char *error_message,
    size_t error_message_size);
#endif

#endif
