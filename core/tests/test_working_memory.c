#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "neuron_model.h"
#include "scenario_config.h"
#include "scenario_runner.h"
#include "working_memory.h"

#define TEST_RUN_NAME "test_working_memory_protocol"
#define TEST_DISABLED_RUN_NAME "test_working_memory_disabled"
#define TEST_OUTPUT_DIR "results/scenarios/" TEST_RUN_NAME

static void cleanup_outputs(void)
{
    system("if exist results\\scenarios\\test_working_memory_protocol rmdir /S /Q results\\scenarios\\test_working_memory_protocol");
    system("if exist results\\scenarios\\test_working_memory_disabled rmdir /S /Q results\\scenarios\\test_working_memory_disabled");
    remove("build/test_working_memory.ini");
}

static int fail(const char *message)
{
    cleanup_outputs();
    printf("FAIL: %s\n", message);
    return 0;
}

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
    char line[1024];
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

static void configure_protocol(ScenarioConfig *config, int reset_between_trials)
{
    scenario_config_default(config);
    snprintf(config->run_name, sizeof(config->run_name), "%s", TEST_RUN_NAME);
    snprintf(config->topology, sizeof(config->topology), "working_memory");
    config->neurons = 6;
    config->inhibitory_fraction = 1.0 / 3.0;
    config->connection_probability = 1.0;
    config->delay = 1;
    config->max_synaptic_delay = 8;
    config->source_count = 1;
    config->input_current = 400.0;
    config->steps = 16;
    config->record_neuron = 0;
    config->history_enabled = 0;
    config->allow_self_connections = 1;
    config->allow_inh_to_inh = 0;
    config->excitatory_weight = 200.0;
    config->inhibitory_weight = -300.0;

    config->working_memory_enabled = 1;
    config->working_memory_trials = 20;
    config->working_memory_cue_steps = 20;
    config->working_memory_delay_steps = 40;
    config->working_memory_probe_steps = 20;
    snprintf(config->working_memory_cue_pattern,
             sizeof(config->working_memory_cue_pattern), "alternating");
    config->working_memory_cue_start = 0;
    config->working_memory_cue_group_size = 2;
    config->working_memory_readout_start = 0;
    config->working_memory_readout_count = 2;
    config->working_memory_readout_group_size = 2;
    config->working_memory_seed = 17U;
    config->working_memory_reset_between_trials = reset_between_trials;
    config->working_memory_recall_tolerance = 0.25;
    config->working_memory_recall_threshold = 0.75;
}

static int results_match(
    const WorkingMemoryResult *left,
    const WorkingMemoryResult *right)
{
    if (left->trial_count != right->trial_count ||
        left->correct_trials != right->correct_trials ||
        !nearly_equal(left->recall_accuracy, right->recall_accuracy) ||
        !nearly_equal(left->mean_recall_score, right->mean_recall_score) ||
        !nearly_equal(left->recall_score_stddev, right->recall_score_stddev) ||
        !nearly_equal(left->mean_response_latency,
                      right->mean_response_latency) ||
        !nearly_equal(left->chance_accuracy, right->chance_accuracy) ||
        !nearly_equal(left->control_accuracy, right->control_accuracy) ||
        !nearly_equal(left->retention_margin, right->retention_margin))
    {
        return 0;
    }

    for (int index = 0; index < left->trial_count; index++)
    {
        const WorkingMemoryTrial *a = &left->trials[index];
        const WorkingMemoryTrial *b = &right->trials[index];

        if (a->trial != b->trial || a->cue_pattern != b->cue_pattern ||
            a->expected_pattern != b->expected_pattern ||
            a->recalled_pattern != b->recalled_pattern ||
            !nearly_equal(a->recall_score, b->recall_score) ||
            a->recall_correct != b->recall_correct ||
            a->control_correct != b->control_correct ||
            a->delay_steps != b->delay_steps ||
            a->first_response_step != b->first_response_step ||
            !nearly_equal(a->mean_readout_activity,
                          b->mean_readout_activity) ||
            a->delay_inputs_zero != b->delay_inputs_zero ||
            a->probe_inputs_zero != b->probe_inputs_zero)
        {
            return 0;
        }
    }
    return 1;
}

static int same_readout_group(
    const ScenarioConfig *config,
    int source,
    int target)
{
    for (int group = 0; group < config->working_memory_readout_count;
         group++)
    {
        int start = config->working_memory_readout_start +
                    group * config->working_memory_readout_group_size;
        int end = start + config->working_memory_readout_group_size;

        if (source >= start && source < end && target >= start && target < end)
            return 1;
    }
    return 0;
}

static int copy_without_readout_recurrence(
    const ScenarioConfig *config,
    const ScenarioBlueprint *source,
    ScenarioBlueprint *out_blueprint)
{
    ScenarioBlueprint copy = {0};

    copy = *source;
    copy.neuron_types = calloc(
        (size_t)source->neuron_count, sizeof(*copy.neuron_types));
    copy.connections = calloc(
        source->connection_count, sizeof(*copy.connections));
    if (copy.neuron_types == NULL || copy.connections == NULL)
    {
        scenario_blueprint_destroy(&copy);
        return 0;
    }

    memcpy(copy.neuron_types, source->neuron_types,
           (size_t)source->neuron_count * sizeof(*copy.neuron_types));
    copy.connection_count = 0;
    for (size_t index = 0; index < source->connection_count; index++)
    {
        const MiniSNNConnectionInfo *connection = &source->connections[index];

        if (!same_readout_group(
                config, (int)connection->source, (int)connection->target))
        {
            copy.connections[copy.connection_count++] = *connection;
        }
    }

    *out_blueprint = copy;
    return 1;
}

static int test_parser_and_validation(void)
{
    ScenarioConfig config;
    ScenarioConfig invalid;
    FILE *file;
    char error[256] = "";

    scenario_config_default(&config);
    if (!scenario_config_validate(&config, error, sizeof(error)) ||
        config.working_memory_enabled != 0)
    {
        return fail("default de memoria de trabalho invalido");
    }

    file = fopen("build/test_working_memory.ini", "w");
    if (file == NULL)
        return fail("nao foi possivel criar INI temporario");
    fputs("[run]\nrun_name = working_memory_parser\n"
          "[working_memory]\nenabled = true\ntrials = 3\n"
          "cue_steps = 9\ndelay_steps = 7\nprobe_steps = 8\n"
          "cue_pattern = seeded\ncue_start = 0\ncue_group_size = 2\n"
          "readout_start = 0\nreadout_count = 2\nreadout_group_size = 2\n"
          "seed = 99\nreset_between_trials = false\n"
          "recall_tolerance = 0.2\nrecall_threshold = 0.8\n",
          file);
    fclose(file);

    if (!scenario_config_load_file("build/test_working_memory.ini", &config,
                                   error, sizeof(error)) ||
        !config.working_memory_enabled || config.working_memory_trials != 3 ||
        strcmp(config.working_memory_cue_pattern, "seeded") != 0 ||
        config.working_memory_cue_group_size != 2 ||
        config.working_memory_readout_group_size != 2 ||
        config.working_memory_reset_between_trials != 0 ||
        !nearly_equal(config.working_memory_recall_threshold, 0.8))
    {
        return fail("parser de memoria de trabalho invalido");
    }

    file = fopen("build/test_working_memory.ini", "w");
    if (file == NULL)
        return fail("nao foi possivel recriar INI temporario");
    fputs("[working_memory]\nenabled=true\nunknown_key=1\n", file);
    fclose(file);
    if (scenario_config_load_file("build/test_working_memory.ini", &config,
                                  error, sizeof(error)))
    {
        return fail("chave desconhecida de memoria de trabalho foi aceita");
    }

    file = fopen("build/test_working_memory.ini", "w");
    if (file == NULL)
        return fail("nao foi possivel recriar INI duplicado");
    fputs("[working_memory]\nenabled=true\nenabled=false\n", file);
    fclose(file);
    if (scenario_config_load_file("build/test_working_memory.ini", &config,
                                  error, sizeof(error)))
    {
        return fail("chave duplicada de memoria de trabalho foi aceita");
    }

    invalid = config;
    snprintf(invalid.working_memory_cue_pattern,
             sizeof(invalid.working_memory_cue_pattern), "copiado");
    if (scenario_config_validate(&invalid, error, sizeof(error)))
        return fail("padrao de cue invalido foi aceito");

    invalid = config;
    invalid.working_memory_readout_count = 1;
    if (scenario_config_validate(&invalid, error, sizeof(error)))
        return fail("readout com menos de dois canais foi aceito");

    invalid = config;
    invalid.working_memory_cue_group_size = 0;
    if (scenario_config_validate(&invalid, error, sizeof(error)))
        return fail("grupo de cue vazio foi aceito");

    invalid = config;
    invalid.working_memory_readout_group_size = 0;
    if (scenario_config_validate(&invalid, error, sizeof(error)))
        return fail("grupo de readout vazio foi aceito");

    invalid = config;
    invalid.working_memory_recall_threshold = 0.0;
    if (scenario_config_validate(&invalid, error, sizeof(error)))
        return fail("limiar de recall invalido foi aceito");

    return 1;
}

int main(void)
{
    ScenarioConfig config;
    ScenarioConfig persistent_config;
    ScenarioConfig disabled_config;
    ScenarioBlueprint blueprint = {0};
    WorkingMemoryResult first = {0};
    WorkingMemoryResult second = {0};
    WorkingMemoryResult persistent_first = {0};
    WorkingMemoryResult persistent_second = {0};
    WorkingMemoryResult without_recurrence = {0};
    WorkingMemoryResult failed = {0};
    ScenarioBlueprint ablated_blueprint = {0};
    ScenarioRunResult run_result;
    char error[256] = "";
    int spike_counts[2] = {3, 1};
    double score;
    double mean_absolute_error;
    int recalled_pattern;
    int recalled_pattern_other_expected;

    cleanup_outputs();
    if (!test_parser_and_validation())
        return 1;

    if (!working_memory_decode_readout(
            spike_counts, 2, 0, &score, &recalled_pattern,
            &mean_absolute_error) || !nearly_equal(score, 0.75) ||
        recalled_pattern != 0 || !nearly_equal(mean_absolute_error, 0.25))
    {
        return fail("calculo de score do readout incorreto");
    }

    if (!working_memory_decode_readout(
            spike_counts, 2, 1, &score, &recalled_pattern_other_expected,
            &mean_absolute_error) || recalled_pattern_other_expected != 0 ||
        !nearly_equal(score, 0.25))
    {
        return fail("readout dependeu do alvo esperado em vez dos spikes");
    }

    configure_protocol(&config, 1);
    if (!scenario_config_validate(&config, error, sizeof(error)) ||
        !scenario_runner_capture_blueprint(&config, &blueprint, error,
                                           sizeof(error)) ||
        !working_memory_execute(&config, &blueprint, &first, error,
                                sizeof(error)) ||
        !working_memory_execute(&config, &blueprint, &second, error,
                                sizeof(error)))
    {
        scenario_blueprint_destroy(&blueprint);
        working_memory_result_destroy(&first);
        working_memory_result_destroy(&second);
        return fail("execucao com reset entre trials falhou");
    }

    if (!results_match(&first, &second) || first.trial_count != 20 ||
        !nearly_equal(first.recall_accuracy,
                      (double)first.correct_trials / first.trial_count) ||
        first.trials[0].delay_steps != config.working_memory_delay_steps ||
        first.trials[0].first_response_step < 0 ||
        first.recall_accuracy < 0.8 ||
        first.mean_recall_score <= 0.6 ||
        !nearly_equal(first.chance_accuracy, 0.5) ||
        !nearly_equal(first.control_accuracy, first.chance_accuracy) ||
        first.control_accuracy >= first.recall_accuracy ||
        first.retention_margin <= 0.0)
    {
        scenario_blueprint_destroy(&blueprint);
        working_memory_result_destroy(&first);
        working_memory_result_destroy(&second);
        return fail("reset, determinismo, accuracy ou latencia incorretos");
    }

    {
        int cue_count[2] = {0, 0};
        int recalled_by_cue[2] = {-1, -1};

        for (int index = 0; index < first.trial_count; index++)
        {
            const WorkingMemoryTrial *trial = &first.trials[index];

            if (trial->cue_pattern < 0 || trial->cue_pattern >= 2 ||
                !trial->recall_correct || !trial->delay_inputs_zero ||
                !trial->probe_inputs_zero)
            {
                scenario_blueprint_destroy(&blueprint);
                working_memory_result_destroy(&first);
                working_memory_result_destroy(&second);
                return fail("cue, readout ou janelas sem estimulo invalidos");
            }
            cue_count[trial->cue_pattern]++;
            if (recalled_by_cue[trial->cue_pattern] < 0)
                recalled_by_cue[trial->cue_pattern] = trial->recalled_pattern;
            else if (recalled_by_cue[trial->cue_pattern] !=
                     trial->recalled_pattern)
            {
                scenario_blueprint_destroy(&blueprint);
                working_memory_result_destroy(&first);
                working_memory_result_destroy(&second);
                return fail("mesmo cue nao produziu readout deterministico");
            }
        }
        if (cue_count[0] != 10 || cue_count[1] != 10 ||
            recalled_by_cue[0] == recalled_by_cue[1])
        {
            scenario_blueprint_destroy(&blueprint);
            working_memory_result_destroy(&first);
            working_memory_result_destroy(&second);
            return fail("cues balanceados nao produziram estados distinguiveis");
        }
    }

    if (!copy_without_readout_recurrence(&config, &blueprint,
                                         &ablated_blueprint) ||
        !working_memory_execute(&config, &ablated_blueprint,
                                &without_recurrence, error, sizeof(error)) ||
        first.recall_accuracy <= without_recurrence.recall_accuracy + 0.30)
    {
        scenario_blueprint_destroy(&ablated_blueprint);
        scenario_blueprint_destroy(&blueprint);
        working_memory_result_destroy(&first);
        working_memory_result_destroy(&second);
        working_memory_result_destroy(&without_recurrence);
        return fail("remover recorrencia nao reduziu retencao");
    }
    scenario_blueprint_destroy(&ablated_blueprint);
    working_memory_result_destroy(&without_recurrence);

    persistent_config = config;
    persistent_config.working_memory_reset_between_trials = 0;
    if (!working_memory_execute(&persistent_config, &blueprint,
                                &persistent_first, error, sizeof(error)) ||
        !working_memory_execute(&persistent_config, &blueprint,
                                &persistent_second, error, sizeof(error)) ||
        !results_match(&persistent_first, &persistent_second))
    {
        scenario_blueprint_destroy(&blueprint);
        working_memory_result_destroy(&first);
        working_memory_result_destroy(&second);
        working_memory_result_destroy(&persistent_first);
        working_memory_result_destroy(&persistent_second);
        return fail("modo sem reset entre trials nao e deterministico");
    }

    neuron_model_test_fail_after_calls(0);
    if (working_memory_execute(&config, &blueprint, &failed, error,
                               sizeof(error)) || failed.trials != NULL)
    {
        neuron_model_test_fail_after_calls(-1);
        scenario_blueprint_destroy(&blueprint);
        working_memory_result_destroy(&first);
        working_memory_result_destroy(&second);
        working_memory_result_destroy(&persistent_first);
        working_memory_result_destroy(&persistent_second);
        working_memory_result_destroy(&failed);
        return fail("erro neuronal nao interrompeu memoria de trabalho");
    }
    neuron_model_test_fail_after_calls(-1);

    scenario_blueprint_destroy(&blueprint);
    working_memory_result_destroy(&first);
    working_memory_result_destroy(&second);
    working_memory_result_destroy(&persistent_first);
    working_memory_result_destroy(&persistent_second);

    if (!scenario_runner_execute(&config, NULL, &run_result, error,
                                 sizeof(error)) ||
        !run_result.working_memory_enabled ||
        run_result.working_memory_trial_count != config.working_memory_trials ||
        !file_exists(TEST_OUTPUT_DIR "/working_memory_trials.csv") ||
        !file_exists(TEST_OUTPUT_DIR "/working_memory_summary.txt") ||
        !file_exists(TEST_OUTPUT_DIR "/working_memory_report.html") ||
        !file_contains(TEST_OUTPUT_DIR "/working_memory_trials.csv",
                       "trial,cue_pattern,expected_pattern,recalled_pattern") ||
        !file_contains(TEST_OUTPUT_DIR "/working_memory_summary.txt",
                       "recall_accuracy=") ||
        !file_contains(TEST_OUTPUT_DIR "/working_memory_report.html",
                       "O cue nao e copiado para a resposta"))
    {
        return fail("runner nao gerou as saidas de memoria de trabalho");
    }

    disabled_config = config;
    snprintf(disabled_config.run_name, sizeof(disabled_config.run_name), "%s",
             TEST_DISABLED_RUN_NAME);
    disabled_config.working_memory_enabled = 0;
    if (!scenario_runner_execute(&disabled_config, NULL, &run_result, error,
                                 sizeof(error)) ||
        run_result.working_memory_enabled ||
        file_exists("results/scenarios/" TEST_DISABLED_RUN_NAME
                    "/working_memory_trials.csv"))
    {
        return fail("cenario desativado alterou o comportamento historico");
    }

    cleanup_outputs();
    printf("Working memory validation OK\n");
    return 0;
}
