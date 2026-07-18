#include <math.h>
#include <stdio.h>

#include "associative_memory.h"
#include "scenario_runner.h"
#include "sequence_prediction.h"
#include "working_memory.h"

static int run_working_memory(void)
{
    ScenarioConfig config;
    ScenarioBlueprint blueprint = {0};
    WorkingMemoryResult result = {0};
    char error[256] = {0};
    int ok;

    ok = scenario_config_load_file("configs/working_memory_demo.ini", &config,
                                   error, sizeof(error));
    config.working_memory_trials = 40;
    ok = ok && scenario_runner_capture_blueprint(&config, &blueprint, error,
                                                  sizeof(error)) &&
         working_memory_execute(&config, &blueprint, &result, error,
                                sizeof(error)) &&
         isfinite(result.recall_accuracy) &&
         isfinite(result.mean_recall_score);
    if (!ok)
        fprintf(stderr, "C6 working-memory long: %s\n", error);
    working_memory_result_destroy(&result);
    scenario_blueprint_destroy(&blueprint);
    return ok;
}

static int run_associative_memory(void)
{
    ScenarioConfig config;
    ScenarioBlueprint blueprint = {0};
    AssociativeMemoryResult result = {0};
    char error[256] = {0};
    int ok;

    ok = scenario_config_load_file("configs/associative_memory_demo.ini",
                                   &config, error, sizeof(error));
    config.associative_memory_trial_count = 40;
    config.associative_memory_training_epochs = 40;
    ok = ok && scenario_runner_capture_blueprint(&config, &blueprint, error,
                                                  sizeof(error)) &&
         associative_memory_execute(&config, &blueprint, &result, error,
                                    sizeof(error)) &&
         isfinite(result.recall_accuracy) && !result.recall_weights_changed;
    if (!ok)
        fprintf(stderr, "C6 associative long: %s\n", error);
    associative_memory_result_destroy(&result);
    scenario_blueprint_destroy(&blueprint);
    return ok;
}

static int run_sequence_prediction(void)
{
    ScenarioConfig config;
    ScenarioBlueprint blueprint = {0};
    SequencePredictionResult result = {0};
    char error[256] = {0};
    int ok;

    ok = scenario_config_load_file("configs/sequence_prediction_context_demo.ini",
                                   &config, error, sizeof(error));
    config.sequence_prediction_trial_count = 40;
    config.sequence_prediction_training_epochs = 40;
    ok = ok && scenario_runner_capture_blueprint(&config, &blueprint, error,
                                                  sizeof(error)) &&
         sequence_prediction_execute(&config, &blueprint, &result, error,
                                     sizeof(error)) &&
         isfinite(result.context_accuracy) &&
         !result.evaluation_weights_changed;
    if (!ok)
        fprintf(stderr, "C6 sequence long: %s\n", error);
    sequence_prediction_result_destroy(&result);
    scenario_blueprint_destroy(&blueprint);
    return ok;
}

int main(void)
{
    if (!run_working_memory() || !run_associative_memory() ||
        !run_sequence_prediction())
    {
        fprintf(stderr, "C6 long validation failed\n");
        return 1;
    }
    printf("C6 long validation OK\n");
    return 0;
}
