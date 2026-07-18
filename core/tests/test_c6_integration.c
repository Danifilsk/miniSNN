#include <math.h>
#include <stdio.h>
#include <string.h>

#include "c6_suite.h"
#include "neuron_model.h"
#include "associative_memory.h"
#include "scenario_runner.h"
#include "scenario_runtime.h"
#include "sequence_prediction.h"
#include "working_memory.h"

static int nearly_equal(double left, double right)
{
    return fabs(left - right) < 1e-12;
}

static int file_contains(const char *path, const char *needle)
{
    char buffer[4096];
    FILE *file = fopen(path, "r");

    if (file == NULL)
        return 0;
    while (fgets(buffer, sizeof(buffer), file) != NULL)
    {
        if (strstr(buffer, needle) != NULL)
        {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

static int run_config(const char *path, ScenarioConfig *out_config,
                      ScenarioRunResult *out_result, char *error,
                      size_t error_size)
{
    return scenario_config_load_file(path, out_config, error, error_size) &&
           scenario_runner_execute(out_config, path, out_result, error,
                                   error_size);
}

static int results_are_semantically_equal(
    const ScenarioRunResult *left,
    const ScenarioRunResult *right)
{
    return left != NULL && right != NULL &&
        left->connection_count == right->connection_count &&
        left->topology_signature == right->topology_signature &&
        left->working_memory_trial_count == right->working_memory_trial_count &&
        left->associative_memory_trial_count ==
            right->associative_memory_trial_count &&
        left->sequence_prediction_trial_count ==
            right->sequence_prediction_trial_count &&
        nearly_equal(left->working_memory_recall_accuracy,
                     right->working_memory_recall_accuracy) &&
        nearly_equal(left->working_memory_control_accuracy,
                     right->working_memory_control_accuracy) &&
        nearly_equal(left->working_memory_retention_margin,
                     right->working_memory_retention_margin) &&
        nearly_equal(left->associative_memory_recall_accuracy,
                     right->associative_memory_recall_accuracy) &&
        nearly_equal(left->associative_memory_control_accuracy,
                     right->associative_memory_control_accuracy) &&
        nearly_equal(left->associative_memory_association_margin,
                     right->associative_memory_association_margin) &&
        nearly_equal(left->sequence_prediction_next_pattern_accuracy,
                     right->sequence_prediction_next_pattern_accuracy) &&
        nearly_equal(left->sequence_prediction_last_symbol_only_control_accuracy,
                     right->sequence_prediction_last_symbol_only_control_accuracy) &&
        nearly_equal(left->sequence_prediction_context_margin,
                     right->sequence_prediction_context_margin);
}

static int test_protocol_order(char *error, size_t error_size)
{
    static const char *const paths[] = {
        "configs/working_memory_demo.ini",
        "configs/associative_memory_demo.ini",
        "configs/sequence_prediction_context_demo.ini"
    };
    ScenarioConfig forward_configs[3];
    ScenarioConfig reverse_configs[3];
    ScenarioRunResult forward_results[3];
    ScenarioRunResult reverse_results[3];

    for (size_t index = 0; index < 3; index++)
    {
        if (!run_config(paths[index], &forward_configs[index],
                        &forward_results[index], error, error_size))
        {
            return 0;
        }
    }
    for (int index = 2; index >= 0; index--)
    {
        if (!run_config(paths[index], &reverse_configs[index],
                        &reverse_results[index], error, error_size))
        {
            return 0;
        }
    }
    for (size_t index = 0; index < 3; index++)
    {
        if (!results_are_semantically_equal(&forward_results[index],
                                             &reverse_results[index]))
        {
            snprintf(error, error_size,
                     "resultado mudou ao inverter a ordem do protocolo %zu",
                     index);
            return 0;
        }
    }
    return 1;
}

static int test_seed_changes_associative_mask(char *error, size_t error_size)
{
    ScenarioConfig first_config;
    ScenarioConfig second_config;
    ScenarioBlueprint blueprint = {0};
    AssociativeMemoryResult first = {0};
    AssociativeMemoryResult second = {0};
    int changed = 0;
    int ok;

    ok = scenario_config_load_file("configs/associative_memory_demo.ini",
                                   &first_config, error, error_size);
    if (ok)
    {
        first_config.associative_memory_trial_count = 2;
        second_config = first_config;
        second_config.associative_memory_seed++;
        ok = scenario_runner_capture_blueprint(&first_config, &blueprint,
                                               error, error_size) &&
             associative_memory_execute(&first_config, &blueprint, &first,
                                        error, error_size) &&
             associative_memory_execute(&second_config, &blueprint, &second,
                                        error, error_size);
    }
    for (int index = 0; ok && index < first.trial_count; index++)
    {
        if (first.trials[index].cue_mask_signature !=
            second.trials[index].cue_mask_signature)
        {
            changed = 1;
        }
    }
    associative_memory_result_destroy(&first);
    associative_memory_result_destroy(&second);
    scenario_blueprint_destroy(&blueprint);
    if (!ok || !changed)
        snprintf(error, error_size, "seed nao alterou a mascara associativa");
    return ok && changed;
}

static int test_independent_networks(char *error, size_t error_size)
{
    ScenarioConfig config;
    ScenarioBlueprint blueprint = {0};
    MiniSNN *first = NULL;
    MiniSNN *second = NULL;
    ScenarioRuntimeStep first_step;
    ScenarioRuntimeStep second_step;
    double inputs[SCENARIO_RUNTIME_MAX_NEURONS] = {0.0};
    double second_before;
    double second_after_first_step;
    double first_after_first_step;
    double second_after_own_step;
    int ok;

    ok = scenario_config_load_file("configs/working_memory_demo.ini", &config,
                                   error, error_size) &&
         scenario_runner_capture_blueprint(&config, &blueprint, error,
                                           error_size) &&
         scenario_runtime_create_from_blueprint(&config, &blueprint, &first,
                                                error, error_size) &&
         scenario_runtime_create_from_blueprint(&config, &blueprint, &second,
                                                error, error_size) &&
         minisnn_get_voltage(second, 0, &second_before);
    if (ok)
    {
        inputs[0] = config.input_current;
        ok = scenario_runtime_step_with_inputs(
                 first, &config, blueprint.inhibitory_count, 0, inputs,
                 config.neurons, &first_step, error, error_size) &&
             minisnn_get_voltage(first, 0, &first_after_first_step) &&
             minisnn_get_voltage(second, 0, &second_after_first_step) &&
             nearly_equal(second_before, second_after_first_step) &&
             scenario_runtime_step_with_inputs(
                 second, &config, blueprint.inhibitory_count, 0, inputs,
                 config.neurons, &second_step, error, error_size) &&
             minisnn_get_voltage(second, 0, &second_after_own_step) &&
             nearly_equal(first_after_first_step, second_after_own_step);
    }
    minisnn_destroy(&first);
    minisnn_destroy(&second);
    scenario_blueprint_destroy(&blueprint);
    if (!ok)
        snprintf(error, error_size, "duas redes compartilharam estado");
    return ok;
}

static int test_suite_and_isolation(void)
{
    ScenarioConfig working;
    ScenarioConfig associative;
    ScenarioRunResult failed_result;
    ScenarioRunResult associative_result;
    char error[256] = {0};

    if (!c6_suite_execute(error, sizeof(error)) ||
        !file_contains("results/scenarios/c6_suite/c6_suite_summary.csv",
                       "sequence_context") ||
        !file_contains("results/scenarios/c6_suite/c6_suite_summary.csv",
                       ",true,PASSOU,") ||
        !file_contains("results/scenarios/c6_suite/c6_suite_report.html",
                       "Suite integrada C6") ||
        !test_protocol_order(error, sizeof(error)) ||
        !test_seed_changes_associative_mask(error, sizeof(error)) ||
        !test_independent_networks(error, sizeof(error)))
    {
        fprintf(stderr, "C6 suite/determinism: %s\n", error);
        return 0;
    }

    if (!scenario_config_load_file("configs/working_memory_demo.ini", &working,
                                   error, sizeof(error)))
    {
        return 0;
    }
    neuron_model_test_fail_after_calls(0);
    if (scenario_runner_execute(&working, NULL, &failed_result, error,
                                sizeof(error)))
    {
        neuron_model_test_fail_after_calls(-1);
        fprintf(stderr, "C6 neuronal error was accepted\n");
        return 0;
    }
    neuron_model_test_fail_after_calls(-1);
    error[0] = '\0';
    if (!run_config("configs/associative_memory_demo.ini", &associative,
                    &associative_result, error, sizeof(error)) ||
        associative_result.associative_memory_recall_accuracy < 0.80)
    {
        fprintf(stderr, "C6 error isolation: %s\n", error);
        return 0;
    }
    return 1;
}

static void configure_model_smoke(
    ScenarioConfig *config,
    MiniSNNNeuronModel model,
    const char *protocol)
{
    config->neuron_model = model;
    config->dt = model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY ? 0.01 : 0.1;
    config->input_current = model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY ?
        10.0 : model == MINISNN_NEURON_MODEL_ADEX ? 500.0 :
        config->input_current;
    config->excitatory_weight =
        model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY ? 2.0 :
        config->excitatory_weight;
    config->inhibitory_weight =
        model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY ? -2.0 :
        config->inhibitory_weight;
    config->steps = 8;
    config->history_enabled = 0;
    config->auto_unique_run = 0;
    snprintf(config->diagnostics_level, sizeof(config->diagnostics_level),
             "off");
    snprintf(config->run_name, sizeof(config->run_name), "c6_smoke_%s_%s",
             protocol, minisnn_neuron_model_name(model));

    if (strcmp(protocol, "working") == 0)
    {
        config->working_memory_trials = 1;
        config->working_memory_cue_steps = 2;
        config->working_memory_delay_steps = 1;
        config->working_memory_probe_steps = 2;
    }
    else if (strcmp(protocol, "associative") == 0)
    {
        config->associative_memory_training_epochs = 1;
        config->associative_memory_training_cue_steps = 2;
        config->associative_memory_training_gap_steps = 1;
        config->associative_memory_recall_cue_steps = 1;
        config->associative_memory_recall_delay_steps = 1;
        config->associative_memory_recall_probe_steps = 2;
        config->associative_memory_trial_count = 1;
    }
    else
    {
        config->sequence_prediction_training_epochs = 1;
        config->sequence_prediction_pattern_steps = 2;
        config->sequence_prediction_inter_pattern_gap_steps = 1;
        config->sequence_prediction_prediction_delay_steps = 1;
        config->sequence_prediction_prediction_probe_steps = 2;
        config->sequence_prediction_trial_count = 1;
    }
}

static int smoke_result_is_finite(
    const ScenarioRunResult *result,
    const char *protocol)
{
    if (strcmp(protocol, "working") == 0)
        return isfinite(result->working_memory_recall_accuracy) &&
               isfinite(result->working_memory_mean_recall_score);
    if (strcmp(protocol, "associative") == 0)
        return isfinite(result->associative_memory_recall_accuracy) &&
               isfinite(result->associative_memory_mean_pattern_similarity);
    return isfinite(result->sequence_prediction_next_pattern_accuracy) &&
           isfinite(result->sequence_prediction_mean_similarity);
}

static int test_model_smoke(void)
{
    const MiniSNNNeuronModel models[] = {
        MINISNN_NEURON_MODEL_LIF,
        MINISNN_NEURON_MODEL_ADEX,
        MINISNN_NEURON_MODEL_HODGKIN_HUXLEY
    };
    static const char *const protocol_names[] = {
        "working", "associative", "sequence"
    };
    static const char *const config_paths[] = {
        "configs/working_memory_demo.ini",
        "configs/associative_memory_demo.ini",
        "configs/sequence_prediction_context_demo.ini"
    };
    char error[256] = {0};

    for (size_t model_index = 0;
         model_index < sizeof(models) / sizeof(models[0]); model_index++)
    {
        for (size_t protocol_index = 0; protocol_index < 3; protocol_index++)
        {
            ScenarioConfig config;
            ScenarioBlueprint blueprint = {0};
            ScenarioRunResult result;
            ScenarioRunResult failed_result;
            MiniSNN *snn = NULL;
            char manifest_path[SCENARIO_OUTPUT_PATH_MAX + 32];
            char expected_model[64];
            int ok;

            error[0] = '\0';
            ok = scenario_config_load_file(config_paths[protocol_index], &config,
                                           error, sizeof(error));
            if (ok)
                configure_model_smoke(&config, models[model_index],
                                      protocol_names[protocol_index]);
            ok = ok && scenario_config_validate(&config, error, sizeof(error)) &&
                 scenario_runner_capture_blueprint(&config, &blueprint, error,
                                                   sizeof(error)) &&
                 scenario_runtime_create_from_blueprint(&config, &blueprint,
                                                        &snn, error, sizeof(error)) &&
                 minisnn_neuron_model(snn) == models[model_index];
            minisnn_destroy(&snn);
            scenario_blueprint_destroy(&blueprint);
            ok = ok && scenario_runner_execute(&config, NULL, &result, error,
                                                sizeof(error)) &&
                 smoke_result_is_finite(&result, protocol_names[protocol_index]) &&
                 snprintf(manifest_path, sizeof(manifest_path), "%s/run_manifest.txt",
                          result.output_directory) < (int)sizeof(manifest_path) &&
                 snprintf(expected_model, sizeof(expected_model), "neuron_model=%s",
                          minisnn_neuron_model_name(models[model_index])) <
                     (int)sizeof(expected_model) &&
                 file_contains(manifest_path, expected_model);
            neuron_model_test_fail_after_calls(0);
            if (scenario_runner_execute(&config, NULL, &failed_result, error,
                                        sizeof(error)) || error[0] == '\0')
            {
                ok = 0;
            }
            neuron_model_test_fail_after_calls(-1);
            if (!ok)
            {
                fprintf(stderr, "C6 model smoke %s/%s: %s\n",
                        protocol_names[protocol_index],
                        minisnn_neuron_model_name(models[model_index]), error);
                return 0;
            }
        }
    }
    return 1;
}

int main(void)
{
    if (!test_suite_and_isolation() || !test_model_smoke())
    {
        fprintf(stderr, "C6 integration validation failed\n");
        return 1;
    }
    printf("C6 integration validation OK\n");
    return 0;
}
