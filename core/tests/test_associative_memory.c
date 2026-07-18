#include <math.h>
#include <stdio.h>
#include <string.h>

#include "associative_memory.h"
#include "scenario_runner.h"

#define TEST_RUN_NAME "test_associative_memory_protocol"

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
    char line[512];
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

static int report_sections_are_ordered(const char *path)
{
    char contents[32768];
    char *weights;
    char *weights_close;
    char *pairs;
    char *trials;
    FILE *file = fopen(path, "r");
    size_t count;

    if (file == NULL)
        return 0;
    count = fread(contents, 1, sizeof(contents) - 1, file);
    fclose(file);
    contents[count] = '\0';
    weights = strstr(contents, "<h2>Pesos antes/depois</h2>");
    pairs = strstr(contents, "<h2>Pares e cue parcial</h2>");
    trials = strstr(contents, "<h2>Trials</h2>");
    if (weights == NULL || pairs == NULL || trials == NULL ||
        !(weights < pairs && pairs < trials))
    {
        return 0;
    }
    weights_close = strstr(weights, "</tbody></table>");
    return weights_close != NULL && weights_close < pairs;
}

static void configure_demo(ScenarioConfig *config)
{
    scenario_config_default(config);
    snprintf(config->run_name, sizeof(config->run_name), "%s", TEST_RUN_NAME);
    snprintf(config->topology, sizeof(config->topology), "associative_memory");
    config->neurons = 8;
    config->inhibitory_fraction = 0.0;
    config->allow_self_connections = 1;
    config->excitatory_weight = 200.0;
    config->input_current = 4000.0;
    config->synaptic_decay = 0.98;
    config->plasticity_enabled = 1;
    config->plasticity_a_plus = 0.5;
    config->plasticity_a_minus = 0.525;
    config->plasticity_weight_min = 0.0;
    config->plasticity_weight_max = 200.0;
    config->associative_memory_enabled = 1;
    config->associative_memory_pair_count = 2;
    config->associative_memory_training_epochs = 20;
    config->associative_memory_training_cue_steps = 5;
    config->associative_memory_training_gap_steps = 20;
    config->associative_memory_initial_weight = 1.0;
    config->associative_memory_recall_cue_steps = 10;
    config->associative_memory_recall_delay_steps = 20;
    config->associative_memory_recall_probe_steps = 20;
    config->associative_memory_cue_corruption = 0.50;
    config->associative_memory_reset_between_pairs = 1;
    config->associative_memory_trial_count = 20;
    config->associative_memory_freeze_plasticity_during_recall = 1;
    config->associative_memory_seed = 1234U;
    config->associative_memory_recall_threshold = 0.75;
    snprintf(config->associative_memory_pattern_mode,
             sizeof(config->associative_memory_pattern_mode), "fixed");
    config->associative_memory_cue_start = 0;
    config->associative_memory_cue_group_size = 2;
    config->associative_memory_target_start = 4;
    config->associative_memory_target_group_size = 2;
}

static int test_decode(void)
{
    const int groups[] = {1, 4};
    const int units[] = {0, 0, 1, 1};
    double similarity;
    double completion;
    int recalled;

    return associative_memory_decode_target(
               groups, units, 2, 2, 1, &similarity, &completion,
               &recalled) &&
           recalled == 1 && nearly_equal(similarity, 0.8) &&
           nearly_equal(completion, 1.0);
}

static int test_parser_and_validation(void)
{
    ScenarioConfig config;
    ScenarioConfig invalid;
    char error[256] = {0};
    FILE *file;

    scenario_config_default(&config);
    if (!scenario_config_validate(&config, error, sizeof(error)) ||
        config.associative_memory_enabled != 0)
    {
        return 0;
    }
    file = fopen("build/test_associative_memory.ini", "w");
    if (file == NULL)
        return 0;
    fputs("[run]\nrun_name = associative_parser\n"
          "[network]\ntopology = associative_memory\nneurons = 8\n"
          "[plasticity]\nenabled = true\n"
          "[associative_memory]\nenabled = true\npair_count = 2\n"
          "initial_association_weight = 1\ntrial_count = 4\n"
          "cue_corruption = 0.5\n", file);
    fclose(file);
    if (!scenario_config_load_file("build/test_associative_memory.ini", &config,
                                   error, sizeof(error)) ||
        !config.associative_memory_enabled ||
        !nearly_equal(config.associative_memory_initial_weight, 1.0))
    {
        remove("build/test_associative_memory.ini");
        return 0;
    }
    remove("build/test_associative_memory.ini");
    invalid = config;
    invalid.associative_memory_pair_count = 1;
    if (scenario_config_validate(&invalid, error, sizeof(error)))
        return 0;
    invalid = config;
    invalid.associative_memory_cue_corruption = 1.0;
    return !scenario_config_validate(&invalid, error, sizeof(error));
}

static int test_protocol(void)
{
    ScenarioConfig config;
    ScenarioBlueprint blueprint = {0};
    AssociativeMemoryResult first = {0};
    AssociativeMemoryResult second = {0};
    AssociativeMemoryResult reset_between_pairs = {0};
    AssociativeMemoryResult failed = {0};
    ScenarioRunResult run_result;
    char error[256] = {0};
    int ok = 0;

    configure_demo(&config);
    if (!scenario_config_validate(&config, error, sizeof(error)) ||
        !scenario_runner_capture_blueprint(&config, &blueprint, error,
                                           sizeof(error)) ||
        !associative_memory_execute(&config, &blueprint, &first, error,
                                    sizeof(error)) ||
        !associative_memory_execute(&config, &blueprint, &second, error,
                                    sizeof(error)))
    {
        fprintf(stderr, "protocol setup: %s\n", error);
        goto done;
    }
    if (first.trial_count != 20 || first.correct_trials != 20 ||
        first.recall_accuracy < 0.80 ||
        first.mean_pattern_similarity < 0.75 ||
        first.association_margin < 0.25 ||
        first.untrained_accuracy >= first.recall_accuracy ||
        first.frozen_training_accuracy >= first.recall_accuracy ||
        first.recall_weights_changed ||
        !first.recall_reconstructed_from_blueprint ||
        first.training_weight_absolute_change <= 0.0 ||
        first.trials[0].pair_id != 0 || first.trials[1].pair_id != 1 ||
        first.trials[0].cue_corruption <= 0.0 ||
        !first.trials[0].cue_mask_consistent ||
        !first.trials[0].delay_probe_inputs_zero ||
        !first.trials[1].cue_mask_consistent ||
        !first.trials[1].delay_probe_inputs_zero ||
        first.trials[0].cue_mask_signature !=
            second.trials[0].cue_mask_signature ||
        first.trials[1].cue_mask_signature !=
            second.trials[1].cue_mask_signature ||
        first.trials[0].expected_pattern == first.trials[1].expected_pattern ||
        !nearly_equal(first.recall_accuracy, second.recall_accuracy) ||
        !nearly_equal(first.mean_pattern_similarity,
                      second.mean_pattern_similarity) ||
        first.trials[0].recalled_pattern != second.trials[0].recalled_pattern)
    {
        fprintf(stderr,
                "protocol metrics accuracy=%.6f similarity=%.6f margin=%.6f untrained=%.6f frozen=%.6f changed=%d weight=%.6f pairs=%d/%d corruption=%.6f expected=%d/%d same=%.6f/%.6f recalled=%d/%d\n",
                first.recall_accuracy, first.mean_pattern_similarity,
                first.association_margin, first.untrained_accuracy,
                first.frozen_training_accuracy, first.recall_weights_changed,
                first.training_weight_absolute_change, first.trials[0].pair_id,
                first.trials[1].pair_id, first.trials[0].cue_corruption,
                first.trials[0].expected_pattern,
                first.trials[1].expected_pattern, second.recall_accuracy,
                second.mean_pattern_similarity,
                first.trials[0].recalled_pattern,
                second.trials[0].recalled_pattern);
        goto done;
    }
    config.associative_memory_reset_between_pairs = 1;
    if (!associative_memory_execute(&config, &blueprint, &reset_between_pairs,
                                    error, sizeof(error)) ||
        reset_between_pairs.training_weight_absolute_change <= 0.0 ||
        reset_between_pairs.recall_weights_changed)
    {
        fprintf(stderr, "protocol reset between pairs: %s\n", error);
        goto done;
    }
    blueprint.neuron_count = 7;
    if (associative_memory_execute(&config, &blueprint, &failed, error,
                                   sizeof(error)))
    {
        fprintf(stderr, "protocol invalid blueprint accepted\n");
        goto done;
    }
    blueprint.neuron_count = config.neurons;
    if (!scenario_runner_execute(&config, NULL, &run_result, error,
                                 sizeof(error)) ||
        !run_result.associative_memory_enabled ||
        run_result.associative_memory_recall_accuracy < 0.80 ||
        !file_exists("results/scenarios/" TEST_RUN_NAME
                     "/associative_memory_training.csv") ||
        !file_exists("results/scenarios/" TEST_RUN_NAME
                     "/associative_memory_trials.csv") ||
        !file_exists("results/scenarios/" TEST_RUN_NAME
                     "/associative_memory_summary.txt") ||
        !file_exists("results/scenarios/" TEST_RUN_NAME
                     "/associative_memory_report.html") ||
        !file_contains("results/scenarios/" TEST_RUN_NAME
                       "/associative_memory_trials.csv",
                       "pattern_completion_score") ||
        !file_contains("results/scenarios/" TEST_RUN_NAME
                       "/associative_memory_report.html",
                       "Controles") ||
        !report_sections_are_ordered("results/scenarios/" TEST_RUN_NAME
                                     "/associative_memory_report.html"))
    {
        fprintf(stderr, "protocol outputs: %s\n", error);
        goto done;
    }
    config.associative_memory_enabled = 0;
    if (!scenario_runner_execute(&config, NULL, &run_result, error,
                                 sizeof(error)) ||
        run_result.associative_memory_enabled)
    {
        fprintf(stderr, "protocol disabled: %s\n", error);
        goto done;
    }
    ok = 1;

done:
    associative_memory_result_destroy(&failed);
    associative_memory_result_destroy(&reset_between_pairs);
    associative_memory_result_destroy(&first);
    associative_memory_result_destroy(&second);
    scenario_blueprint_destroy(&blueprint);
    return ok;
}

int main(void)
{
    if (!test_decode() || !test_parser_and_validation() || !test_protocol())
    {
        fprintf(stderr, "Associative memory validation failed\n");
        return 1;
    }

    printf("Associative memory validation OK\n");
    return 0;
}
