#include <math.h>
#include <stdio.h>
#include <string.h>

#include "associative_memory.h"
#include "scenario_runner.h"
#include "sequence_prediction.h"

static int nearly_equal(double left, double right)
{
    return fabs(left - right) < 1e-12;
}

static int blueprints_match(
    const ScenarioBlueprint *left,
    const ScenarioBlueprint *right)
{
    if (left == NULL || right == NULL ||
        left->neuron_count != right->neuron_count ||
        left->inhibitory_count != right->inhibitory_count ||
        left->connection_count != right->connection_count ||
        left->topology_signature != right->topology_signature ||
        left->neuron_model != right->neuron_model ||
        left->neuron_model_config_signature !=
            right->neuron_model_config_signature)
    {
        return 0;
    }
    for (int neuron_id = 0; neuron_id < left->neuron_count; neuron_id++)
    {
        if (left->neuron_types[neuron_id] != right->neuron_types[neuron_id])
            return 0;
    }
    for (size_t connection_id = 0;
         connection_id < left->connection_count;
         connection_id++)
    {
        const MiniSNNConnectionInfo *a = &left->connections[connection_id];
        const MiniSNNConnectionInfo *b = &right->connections[connection_id];

        if (a->source != b->source || a->target != b->target ||
            a->delay != b->delay || !nearly_equal(a->weight, b->weight))
        {
            return 0;
        }
    }
    return 1;
}

static int test_associative_checkpoint(void)
{
    ScenarioConfig config;
    ScenarioConfig reloaded_config;
    ScenarioConfig incompatible;
    ScenarioBlueprint base = {0};
    ScenarioBlueprint loaded = {0};
    AssociativeMemoryResult result = {0};
    char error[256] = {0};
    double recall_accuracy;
    int ok = 0;

    if (!scenario_config_load_file("configs/associative_memory_demo.ini",
                                   &config, error, sizeof(error)) ||
        !scenario_config_save_file(
            "build/c6_associative_config_used.ini", &config, error,
            sizeof(error)) ||
        !scenario_runner_capture_blueprint(&config, &base, error,
                                           sizeof(error)) ||
        !associative_memory_execute(&config, &base, &result, error,
                                    sizeof(error)) ||
        !scenario_blueprint_write_checkpoint(
            &result.learned_blueprint, "build/c6_associative_checkpoint.txt",
            error, sizeof(error)) ||
        !scenario_blueprint_load_checkpoint(
            "build/c6_associative_checkpoint.txt", &loaded, error,
            sizeof(error)) ||
        !blueprints_match(&result.learned_blueprint, &loaded))
    {
        fprintf(stderr, "associative checkpoint: %s\n", error);
        goto done;
    }

    associative_memory_result_destroy(&result);
    scenario_blueprint_destroy(&base);
    memset(&config, 0, sizeof(config));
    if (!scenario_config_load_file("build/c6_associative_config_used.ini",
                                   &reloaded_config, error, sizeof(error)))
    {
        fprintf(stderr, "associative effective config reload: %s\n", error);
        goto done;
    }
    if (!associative_memory_test_recall_accuracy(
            &reloaded_config, &loaded, &recall_accuracy, error, sizeof(error)) ||
        recall_accuracy < 0.80)
    {
        fprintf(stderr, "associative checkpoint recall: %s\n", error);
        goto done;
    }
    incompatible = reloaded_config;
    incompatible.dt *= 2.0;
    if (associative_memory_test_recall_accuracy(
            &incompatible, &loaded, &recall_accuracy, error, sizeof(error)))
    {
        fprintf(stderr, "associative incompatible checkpoint accepted\n");
        goto done;
    }
    ok = 1;

done:
    associative_memory_result_destroy(&result);
    scenario_blueprint_destroy(&loaded);
    scenario_blueprint_destroy(&base);
    remove("build/c6_associative_checkpoint.txt");
    remove("build/c6_associative_config_used.ini");
    return ok;
}

static int test_sequence_checkpoint(void)
{
    ScenarioConfig config;
    ScenarioConfig reloaded_config;
    ScenarioConfig incompatible;
    ScenarioBlueprint base = {0};
    ScenarioBlueprint loaded = {0};
    SequencePredictionResult result = {0};
    char error[256] = {0};
    double prediction_accuracy;
    int ok = 0;

    if (!scenario_config_load_file("configs/sequence_prediction_context_demo.ini",
                                   &config, error, sizeof(error)) ||
        !scenario_config_save_file(
            "build/c6_sequence_config_used.ini", &config, error,
            sizeof(error)) ||
        !scenario_runner_capture_blueprint(&config, &base, error,
                                           sizeof(error)) ||
        !sequence_prediction_execute(&config, &base, &result, error,
                                     sizeof(error)) ||
        !scenario_blueprint_write_checkpoint(
            &result.learned_blueprint, "build/c6_sequence_checkpoint.txt",
            error, sizeof(error)) ||
        !scenario_blueprint_load_checkpoint(
            "build/c6_sequence_checkpoint.txt", &loaded, error,
            sizeof(error)) ||
        !blueprints_match(&result.learned_blueprint, &loaded))
    {
        fprintf(stderr, "sequence checkpoint: %s\n", error);
        goto done;
    }

    sequence_prediction_result_destroy(&result);
    scenario_blueprint_destroy(&base);
    memset(&config, 0, sizeof(config));
    if (!scenario_config_load_file("build/c6_sequence_config_used.ini",
                                   &reloaded_config, error, sizeof(error)))
    {
        fprintf(stderr, "sequence effective config reload: %s\n", error);
        goto done;
    }
    if (!sequence_prediction_test_accuracy(
            &reloaded_config, &loaded, &prediction_accuracy, error,
            sizeof(error)) ||
        prediction_accuracy < 0.80)
    {
        fprintf(stderr, "sequence checkpoint recall: %s\n", error);
        goto done;
    }
    incompatible = reloaded_config;
    incompatible.dt *= 2.0;
    if (sequence_prediction_test_accuracy(
            &incompatible, &loaded, &prediction_accuracy, error, sizeof(error)))
    {
        fprintf(stderr, "sequence incompatible checkpoint accepted\n");
        goto done;
    }
    ok = 1;

done:
    sequence_prediction_result_destroy(&result);
    scenario_blueprint_destroy(&loaded);
    scenario_blueprint_destroy(&base);
    remove("build/c6_sequence_checkpoint.txt");
    remove("build/c6_sequence_config_used.ini");
    return ok;
}

int main(void)
{
    if (!test_associative_checkpoint() || !test_sequence_checkpoint())
    {
        fprintf(stderr, "C6 checkpoint validation failed\n");
        return 1;
    }
    printf("C6 checkpoint validation OK\n");
    return 0;
}
