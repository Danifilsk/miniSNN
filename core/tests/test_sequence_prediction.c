#include <math.h>
#include <stdio.h>
#include <string.h>

#include "neuron_model.h"
#include "scenario_runner.h"
#include "sequence_prediction.h"

#define TEST_RUN_NAME "test_sequence_prediction_protocol"

static int nearly_equal(double left, double right)
{
    return fabs(left - right) < 1e-12;
}

static int file_exists(const char *path)
{
    FILE *file = fopen(path, "r");

    if (file == NULL)
        return 0;
    fclose(file);
    return 1;
}

static int file_contains(const char *path, const char *needle)
{
    char line[8192];
    FILE *file = fopen(path, "r");

    if (file == NULL)
        return 0;
    while (fgets(line, sizeof(line), file) != NULL)
    {
        if (strstr(line, needle) != NULL)
        {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

static void configure_demo(ScenarioConfig *config)
{
    scenario_config_default(config);
    snprintf(config->run_name, sizeof(config->run_name), "%s", TEST_RUN_NAME);
    snprintf(config->topology, sizeof(config->topology), "sequence_prediction");
    config->neurons = 16;
    config->inhibitory_fraction = 0.0;
    config->allow_self_connections = 0;
    config->excitatory_weight = 200.0;
    config->input_current = 4000.0;
    config->synaptic_decay = 0.95;
    config->plasticity_enabled = 1;
    config->plasticity_a_plus = 0.5;
    config->plasticity_a_minus = 0.525;
    config->plasticity_tau_plus = 1.0;
    config->plasticity_tau_minus = 1.0;
    config->plasticity_weight_min = 0.0;
    config->plasticity_weight_max = 200.0;
    config->sequence_prediction_enabled = 1;
    config->sequence_prediction_sequence_count = 2;
    config->sequence_prediction_sequence_length = 4;
    config->sequence_prediction_training_epochs = 30;
    config->sequence_prediction_pattern_steps = 5;
    config->sequence_prediction_inter_pattern_gap_steps = 20;
    config->sequence_prediction_initial_weight = 1.0;
    config->sequence_prediction_prefix_length = 3;
    config->sequence_prediction_prediction_delay_steps = 0;
    config->sequence_prediction_prediction_probe_steps = 16;
    config->sequence_prediction_trial_count = 20;
    config->sequence_prediction_freeze_plasticity_during_evaluation = 1;
    config->sequence_prediction_reset_between_sequences = 0;
    config->sequence_prediction_seed = 1234U;
    config->sequence_prediction_prediction_threshold = 0.75;
    snprintf(config->sequence_prediction_pattern_mode,
             sizeof(config->sequence_prediction_pattern_mode), "fixed");
    config->sequence_prediction_input_start = 0;
    config->sequence_prediction_input_group_size = 1;
    config->sequence_prediction_prediction_start = 8;
    config->sequence_prediction_prediction_group_size = 1;
}

static void configure_contextual_demo(ScenarioConfig *config)
{
    configure_demo(config);
    snprintf(config->run_name, sizeof(config->run_name),
             "test_sequence_prediction_context");
    config->neurons = 24;
    config->sequence_prediction_inter_pattern_gap_steps = 4;
    config->sequence_prediction_prediction_delay_steps = 3;
    config->sequence_prediction_reset_between_sequences = 1;
    config->sequence_prediction_prediction_threshold = 0.30;
    snprintf(config->sequence_prediction_pattern_mode,
             sizeof(config->sequence_prediction_pattern_mode),
             "contextual");
    config->sequence_prediction_input_group_size = 2;
    config->sequence_prediction_prediction_start = 16;
}

static int test_parser_and_validation(void)
{
    ScenarioConfig config;
    ScenarioConfig invalid;
    ScenarioConfig different_seed;
    char error[256] = {0};
    FILE *file;
    int first_pattern;

    scenario_config_default(&config);
    if (!scenario_config_validate(&config, error, sizeof(error)) ||
        config.sequence_prediction_enabled != 0)
    {
        return 0;
    }
    file = fopen("build/test_sequence_prediction.ini", "w");
    if (file == NULL)
        return 0;
    fputs("[network]\ntopology = sequence_prediction\nneurons = 16\n"
          "[plasticity]\nenabled = true\n"
          "[sequence_prediction]\nenabled = true\n"
          "sequence_count = 2\nsequence_length = 4\n"
          "prefix_length = 3\npattern_mode = seeded\n"
          "trial_count = 4\n", file);
    fclose(file);
    if (!scenario_config_load_file("build/test_sequence_prediction.ini", &config,
                                   error, sizeof(error)) ||
        !config.sequence_prediction_enabled ||
        strcmp(config.sequence_prediction_pattern_mode, "seeded") != 0)
    {
        remove("build/test_sequence_prediction.ini");
        return 0;
    }
    remove("build/test_sequence_prediction.ini");
    first_pattern = sequence_prediction_pattern_id(&config, 0, 0);
    different_seed = config;
    different_seed.sequence_prediction_seed++;
    if (first_pattern != sequence_prediction_pattern_id(&config, 0, 0) ||
        first_pattern == sequence_prediction_pattern_id(&different_seed, 0, 0))
    {
        return 0;
    }
    invalid = config;
    invalid.sequence_prediction_sequence_length = 2;
    if (scenario_config_validate(&invalid, error, sizeof(error)))
        return 0;
    invalid = config;
    invalid.sequence_prediction_prefix_length =
        invalid.sequence_prediction_sequence_length;
    if (scenario_config_validate(&invalid, error, sizeof(error)))
        return 0;
    invalid = config;
    snprintf(invalid.sequence_prediction_pattern_mode,
             sizeof(invalid.sequence_prediction_pattern_mode), "unknown");
    return !scenario_config_validate(&invalid, error, sizeof(error));
}

static int test_full_probe_decoding(void)
{
    static const int probe_frames[] = {
        1, 0,
        0, 2,
        0, 2
    };
    int predicted_pattern = -1;
    double similarity = 0.0;

    return sequence_prediction_test_decode_probe(
               probe_frames, 3, 2, &predicted_pattern, &similarity) &&
           predicted_pattern == 1 && nearly_equal(similarity, 0.8);
}

static int test_contextual_protocol(void)
{
    ScenarioConfig config;
    ScenarioBlueprint blueprint = {0};
    SequencePredictionResult result = {0};
    char error[256] = {0};
    const char *last_symbol_zero;
    const char *last_symbol_one;
    int ok = 0;

    configure_contextual_demo(&config);
    if (!scenario_config_validate(&config, error, sizeof(error)) ||
        !scenario_runner_capture_blueprint(&config, &blueprint, error,
                                           sizeof(error)) ||
        !sequence_prediction_execute(&config, &blueprint, &result, error,
                                     sizeof(error)))
    {
        fprintf(stderr, "contextual setup: %s\n", error);
        goto done;
    }

    last_symbol_zero = strrchr(result.trials[0].prefix, '>');
    last_symbol_one = strrchr(result.trials[1].prefix, '>');
    if (result.context_accuracy < 0.80 ||
        result.last_symbol_only_control_accuracy > 0.60 ||
        result.context_margin < 0.25 ||
        result.shuffled_order_control_accuracy >= result.context_accuracy ||
        result.trials[0].expected_next_pattern ==
            result.trials[1].expected_next_pattern ||
        result.trials[0].predicted_pattern !=
            result.trials[0].expected_next_pattern ||
        result.trials[1].predicted_pattern !=
            result.trials[1].expected_next_pattern ||
        last_symbol_zero == NULL || last_symbol_one == NULL ||
        strcmp(last_symbol_zero, last_symbol_one) != 0 ||
        !result.trials[0].delay_probe_inputs_zero ||
        !result.trials[1].delay_probe_inputs_zero)
    {
        fprintf(stderr,
                "context metrics accuracy=%.6f last=%.6f margin=%.6f shuffled=%.6f prefix=%s/%s expected=%d/%d predicted=%d/%d\n",
                result.context_accuracy,
                result.last_symbol_only_control_accuracy,
                result.context_margin,
                result.shuffled_order_control_accuracy,
                result.trials[0].prefix, result.trials[1].prefix,
                result.trials[0].expected_next_pattern,
                result.trials[1].expected_next_pattern,
                result.trials[0].predicted_pattern,
                result.trials[1].predicted_pattern);
        goto done;
    }
    ok = 1;

done:
    sequence_prediction_result_destroy(&result);
    scenario_blueprint_destroy(&blueprint);
    return ok;
}

static int test_protocol(void)
{
    ScenarioConfig config;
    ScenarioBlueprint blueprint = {0};
    SequencePredictionResult first = {0};
    SequencePredictionResult second = {0};
    SequencePredictionResult failed = {0};
    ScenarioRunResult run_result;
    char error[256] = {0};
    int ok = 0;

    configure_demo(&config);
    if (!scenario_config_validate(&config, error, sizeof(error)) ||
        !scenario_runner_capture_blueprint(&config, &blueprint, error,
                                           sizeof(error)) ||
        !sequence_prediction_execute(&config, &blueprint, &first, error,
                                     sizeof(error)) ||
        !sequence_prediction_execute(&config, &blueprint, &second, error,
                                     sizeof(error)))
    {
        fprintf(stderr, "sequence setup: %s\n", error);
        goto done;
    }
    if (first.trial_count != 20 || first.correct_predictions != 20 ||
        first.next_pattern_accuracy < 0.80 ||
        first.mean_prediction_similarity < 0.75 ||
        first.prediction_margin < 0.25 ||
        first.untrained_control_accuracy >= first.next_pattern_accuracy ||
        first.shuffled_order_control_accuracy >= first.next_pattern_accuracy ||
        first.frozen_training_control_accuracy >= first.next_pattern_accuracy ||
        first.permuted_labels_control_accuracy >= first.next_pattern_accuracy ||
        first.evaluation_weights_changed ||
        !first.evaluation_reconstructed_from_blueprint ||
        first.training_weight_absolute_change <= 0.0 ||
        !first.trials[0].prefix_is_incomplete ||
        !first.trials[0].delay_probe_inputs_zero ||
        !first.trials[1].delay_probe_inputs_zero ||
        strcmp(first.trials[0].prefix, first.trials[1].prefix) == 0 ||
        first.trials[0].expected_next_pattern ==
            first.trials[1].expected_next_pattern ||
        first.trials[0].predicted_pattern ==
            first.trials[1].predicted_pattern ||
        !nearly_equal(first.next_pattern_accuracy,
                      second.next_pattern_accuracy) ||
        !nearly_equal(first.mean_prediction_similarity,
                      second.mean_prediction_similarity) ||
        first.trials[0].predicted_pattern !=
            second.trials[0].predicted_pattern)
    {
        fprintf(stderr,
                "sequence metrics accuracy=%.6f similarity=%.6f margin=%.6f controls=%.6f/%.6f/%.6f/%.6f changed=%d reconstructed=%d weight=%.6f prefix=%s/%s predicted=%d/%d\n",
                first.next_pattern_accuracy,
                first.mean_prediction_similarity, first.prediction_margin,
                first.untrained_control_accuracy,
                first.shuffled_order_control_accuracy,
                first.frozen_training_control_accuracy,
                first.permuted_labels_control_accuracy,
                first.evaluation_weights_changed,
                first.evaluation_reconstructed_from_blueprint,
                first.training_weight_absolute_change,
                first.trials[0].prefix, first.trials[1].prefix,
                first.trials[0].predicted_pattern,
                first.trials[1].predicted_pattern);
        goto done;
    }
    neuron_model_test_fail_after_calls(0);
    if (sequence_prediction_execute(&config, &blueprint, &failed, error,
                                    sizeof(error)))
    {
        neuron_model_test_fail_after_calls(-1);
        fprintf(stderr, "sequence neuronal error accepted\n");
        goto done;
    }
    neuron_model_test_fail_after_calls(-1);
    if (!scenario_runner_execute(&config, NULL, &run_result, error,
                                 sizeof(error)) ||
        !run_result.sequence_prediction_enabled ||
        run_result.sequence_prediction_next_pattern_accuracy < 0.80 ||
        !file_exists("results/scenarios/" TEST_RUN_NAME
                     "/sequence_prediction_training.csv") ||
        !file_exists("results/scenarios/" TEST_RUN_NAME
                     "/sequence_prediction_trials.csv") ||
        !file_exists("results/scenarios/" TEST_RUN_NAME
                     "/sequence_prediction_summary.txt") ||
        !file_exists("results/scenarios/" TEST_RUN_NAME
                     "/sequence_prediction_report.html") ||
        !file_contains("results/scenarios/" TEST_RUN_NAME
                       "/sequence_prediction_trials.csv",
                       "expected_next_pattern") ||
        !file_contains("results/scenarios/" TEST_RUN_NAME
                       "/sequence_prediction_report.html",
                       "Sequencias e prefixos") ||
        !file_contains("results/scenarios/" TEST_RUN_NAME
                       "/sequence_prediction_report.html",
                       "teacher pulse supervisionado"))
    {
        fprintf(stderr, "sequence outputs: %s\n", error);
        goto done;
    }
    config.sequence_prediction_enabled = 0;
    if (!scenario_runner_execute(&config, NULL, &run_result, error,
                                 sizeof(error)) ||
        run_result.sequence_prediction_enabled)
    {
        fprintf(stderr, "sequence disabled: %s\n", error);
        goto done;
    }
    ok = 1;

done:
    neuron_model_test_fail_after_calls(-1);
    sequence_prediction_result_destroy(&failed);
    sequence_prediction_result_destroy(&first);
    sequence_prediction_result_destroy(&second);
    scenario_blueprint_destroy(&blueprint);
    return ok;
}

int main(void)
{
    if (!test_parser_and_validation() || !test_full_probe_decoding() ||
        !test_protocol() || !test_contextual_protocol())
    {
        fprintf(stderr, "Sequence prediction validation failed\n");
        return 1;
    }

    printf("Sequence prediction validation OK\n");
    return 0;
}
