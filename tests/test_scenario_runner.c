#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scenario_config.h"
#include "scenario_runner.h"

#define TEST_RUN_NAME "test_scenario_runner_temp"
#define TEST_OUTPUT_DIR "results/scenarios/test_scenario_runner_temp"
#define TEST_RANDOM_RUN_NAME "test_scenario_runner_random"
#define TEST_SMALL_WORLD_RUN_NAME "test_scenario_runner_small_world"
#define TEST_FEEDFORWARD_RUN_NAME "test_scenario_runner_feedforward"
#define TEST_ALL_TO_ALL_NO_SELF_RUN_NAME "test_scenario_runner_all_to_all_no_self"
#define TEST_ALL_TO_ALL_SELF_RUN_NAME "test_scenario_runner_all_to_all_self"
#define TEST_RANDOM_SELF_RUN_NAME "test_scenario_runner_random_self"
#define TEST_UNIQUE_RUN_NAME "test_scenario_runner_unique"

static int fail(const char *message)
{
    system("if exist results\\scenarios\\test_scenario_runner_temp rmdir /S /Q results\\scenarios\\test_scenario_runner_temp");
    system("if exist results\\scenarios\\test_scenario_runner_random rmdir /S /Q results\\scenarios\\test_scenario_runner_random");
    system("if exist results\\scenarios\\test_scenario_runner_small_world rmdir /S /Q results\\scenarios\\test_scenario_runner_small_world");
    system("if exist results\\scenarios\\test_scenario_runner_feedforward rmdir /S /Q results\\scenarios\\test_scenario_runner_feedforward");
    system("if exist results\\scenarios\\test_scenario_runner_all_to_all_no_self rmdir /S /Q results\\scenarios\\test_scenario_runner_all_to_all_no_self");
    system("if exist results\\scenarios\\test_scenario_runner_all_to_all_self rmdir /S /Q results\\scenarios\\test_scenario_runner_all_to_all_self");
    system("if exist results\\scenarios\\test_scenario_runner_random_self rmdir /S /Q results\\scenarios\\test_scenario_runner_random_self");
    system("for /D %D in (results\\scenarios\\test_scenario_runner_unique*) do @rmdir /S /Q \"%D\"");
    printf("FAIL: %s\n", message);
    return 0;
}

static int file_exists(const char *path)
{
    FILE *file = fopen(path, "r");

    if (file == NULL)
        return 0;

    fclose(file);
    return 1;
}

static int summary_contains_expected_data(void)
{
    FILE *file = fopen(TEST_OUTPUT_DIR "/summary.txt", "r");
    char line[256];
    int saw_run_name = 0;
    int saw_connections = 0;
    int saw_spikes = 0;

    if (file == NULL)
        return 0;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        if (strstr(line, "run_name=" TEST_RUN_NAME) != NULL)
            saw_run_name = 1;
        else if (strstr(line, "connection_count=2") != NULL)
            saw_connections = 1;
        else if (strstr(line, "spikes_total=") != NULL)
            saw_spikes = 1;
    }

    fclose(file);
    return saw_run_name && saw_connections && saw_spikes;
}

static int neuron_csv_contains_expected_data(void)
{
    FILE *file = fopen(TEST_OUTPUT_DIR "/neuron_0.csv", "r");
    char line[256];
    int data_rows = 0;

    if (file == NULL)
        return 0;

    if (fgets(line, sizeof(line), file) == NULL)
    {
        fclose(file);
        return 0;
    }

    if (strcmp(line, "tempo,V,spike,corrente_externa,corrente_sinaptica\n") != 0)
    {
        fclose(file);
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL)
        data_rows++;

    fclose(file);
    return data_rows > 0;
}

static void cleanup_extra_outputs(void)
{
    system("if exist results\\scenarios\\test_scenario_runner_random rmdir /S /Q results\\scenarios\\test_scenario_runner_random");
    system("if exist results\\scenarios\\test_scenario_runner_small_world rmdir /S /Q results\\scenarios\\test_scenario_runner_small_world");
    system("if exist results\\scenarios\\test_scenario_runner_feedforward rmdir /S /Q results\\scenarios\\test_scenario_runner_feedforward");
    system("if exist results\\scenarios\\test_scenario_runner_all_to_all_no_self rmdir /S /Q results\\scenarios\\test_scenario_runner_all_to_all_no_self");
    system("if exist results\\scenarios\\test_scenario_runner_all_to_all_self rmdir /S /Q results\\scenarios\\test_scenario_runner_all_to_all_self");
    system("if exist results\\scenarios\\test_scenario_runner_random_self rmdir /S /Q results\\scenarios\\test_scenario_runner_random_self");
}

static int summary_has_connection_count(
    const char *run_name,
    int expected_connection_count)
{
    char path[256];
    char expected_line[64];
    char line[256];
    FILE *file;
    int found = 0;

    snprintf(
        path,
        sizeof(path),
        "results/scenarios/%s/summary.txt",
        run_name);
    snprintf(
        expected_line,
        sizeof(expected_line),
        "connection_count=%d",
        expected_connection_count);

    file = fopen(path, "r");
    if (file == NULL)
        return 0;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        if (strstr(line, expected_line) != NULL)
        {
            found = 1;
            break;
        }
    }

    fclose(file);
    return found;
}

static int run_topology_smoke(
    const char *topology,
    const char *run_name)
{
    ScenarioConfig config;
    ScenarioRunResult result;
    char error[256];

    scenario_config_default(&config);
    snprintf(config.run_name, sizeof(config.run_name), "%s", run_name);
    snprintf(config.topology, sizeof(config.topology), "%s", topology);
    config.neurons = 8;
    config.inhibitory_fraction = 0.25;
    config.connection_probability = 1.0;
    config.source_count = 2;
    config.steps = 30;
    config.record_neuron = 0;
    config.small_world_neighbors = 2;
    config.small_world_rewire_probability = 0.25;
    config.feedforward_layers = 4;
    config.history_enabled = 0;

    if (!scenario_runner_execute(
            &config,
            NULL,
            &result,
            error,
            sizeof(error)))
    {
        printf("Unexpected runner error for %s: %s\n", topology, error);
        return 0;
    }

    return result.connection_count > 0;
}

static int run_connection_count_case(
    const char *topology,
    const char *run_name,
    int allow_self_connections,
    int expected_connection_count)
{
    ScenarioConfig config;
    ScenarioRunResult result;
    char error[256];

    scenario_config_default(&config);
    snprintf(config.run_name, sizeof(config.run_name), "%s", run_name);
    snprintf(config.topology, sizeof(config.topology), "%s", topology);
    config.neurons = 4;
    config.inhibitory_fraction = 0.0;
    config.allow_inh_to_inh = 1;
    config.allow_self_connections = allow_self_connections;
    config.connection_probability = 1.0;
    config.source_count = 1;
    config.steps = 20;
    config.record_neuron = 0;
    config.small_world_neighbors = 2;
    config.feedforward_layers = 2;
    config.history_enabled = 0;

    if (!scenario_runner_execute(
            &config,
            NULL,
            &result,
            error,
            sizeof(error)))
    {
        printf("Unexpected runner error for %s: %s\n", run_name, error);
        return 0;
    }

    if (result.connection_count != expected_connection_count)
        return 0;

    return summary_has_connection_count(run_name, expected_connection_count);
}

static int history_entries_for_run(const char *run_name)
{
    FILE *file = fopen("results/scenarios/index.csv", "r");
    char line[1024];
    int count = 0;

    if (file == NULL)
        return 0;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        if (strstr(line, run_name) != NULL)
            count++;
    }

    fclose(file);
    return count;
}

static int check_auto_unique_history(void)
{
    ScenarioConfig config;
    ScenarioRunResult first;
    ScenarioRunResult second;
    char error[256];
    int ok = 1;

    system("for /D %D in (results\\scenarios\\test_scenario_runner_unique*) do @rmdir /S /Q \"%D\"");
    system("if exist build\\test_scenario_runner_index_backup.csv del /Q build\\test_scenario_runner_index_backup.csv");
    system("if exist results\\scenarios\\index.csv move /Y results\\scenarios\\index.csv build\\test_scenario_runner_index_backup.csv >NUL");

    scenario_config_default(&config);
    snprintf(config.run_name, sizeof(config.run_name), TEST_UNIQUE_RUN_NAME);
    snprintf(config.topology, sizeof(config.topology), "chain");
    config.neurons = 2;
    config.inhibitory_fraction = 0.0;
    config.source_count = 1;
    config.steps = 10;
    config.record_neuron = 0;
    config.auto_unique_run = 1;
    config.history_enabled = 1;

    if (!scenario_runner_execute(&config, NULL, &first, error, sizeof(error)))
    {
        printf("Unexpected runner error: %s\n", error);
        ok = 0;
        goto cleanup;
    }

    if (!scenario_runner_execute(&config, NULL, &second, error, sizeof(error)))
    {
        printf("Unexpected runner error: %s\n", error);
        ok = 0;
        goto cleanup;
    }

    if (strcmp(first.output_directory, second.output_directory) == 0)
        ok = 0;

    if (ok && !file_exists("results/scenarios/test_scenario_runner_unique/summary.txt"))
        ok = 0;

    if (ok && !file_exists("results/scenarios/index.csv"))
        ok = 0;

    if (ok && history_entries_for_run(TEST_UNIQUE_RUN_NAME) < 2)
        ok = 0;

cleanup:
    system("for /D %D in (results\\scenarios\\test_scenario_runner_unique*) do @rmdir /S /Q \"%D\"");
    system("if exist results\\scenarios\\index.csv del /Q results\\scenarios\\index.csv");
    system("if exist build\\test_scenario_runner_index_backup.csv move /Y build\\test_scenario_runner_index_backup.csv results\\scenarios\\index.csv >NUL");
    return ok;
}

static int csv_header_contains(const char *path, const char *column)
{
    FILE *file = fopen(path, "r");
    char header[8192];

    if (file == NULL)
        return 0;

    if (fgets(header, sizeof(header), file) == NULL)
    {
        fclose(file);
        return 0;
    }

    fclose(file);
    return strstr(header, column) != NULL;
}

static int check_diagnostics_modes(void)
{
    ScenarioConfig config;
    ScenarioRunResult result;
    char error[256];
    int ok = 1;

    system("if exist results\\scenarios\\test_runner_off rmdir /S /Q results\\scenarios\\test_runner_off");
    system("if exist results\\scenarios\\test_runner_silent rmdir /S /Q results\\scenarios\\test_runner_silent");

    scenario_config_default(&config);
    snprintf(config.run_name, sizeof(config.run_name), "test_runner_off");
    snprintf(config.topology, sizeof(config.topology), "chain");
    snprintf(config.diagnostics_level, sizeof(config.diagnostics_level), "off");
    config.neurons = 2;
    config.inhibitory_fraction = 0.0;
    config.source_count = 1;
    config.steps = 5;
    config.record_neuron = 0;
    config.history_enabled = 0;

    if (!scenario_runner_execute(&config, NULL, &result, error, sizeof(error)) ||
        !file_exists("results/scenarios/test_runner_off/run_manifest.txt") ||
        file_exists("results/scenarios/test_runner_off/metrics.csv"))
    {
        ok = 0;
        goto cleanup;
    }

    snprintf(config.run_name, sizeof(config.run_name), "test_runner_silent");
    snprintf(config.diagnostics_level, sizeof(config.diagnostics_level), "basic");
    config.input_current = 0.0;

    if (!scenario_runner_execute(&config, NULL, &result, error, sizeof(error)) ||
        result.spikes_total != 0 ||
        !csv_header_contains(
            "results/scenarios/test_runner_silent/metrics.csv",
            "activity_total_spikes") ||
        !file_exists("results/scenarios/test_runner_silent/run_manifest.txt"))
    {
        ok = 0;
    }

cleanup:
    system("if exist results\\scenarios\\test_runner_off rmdir /S /Q results\\scenarios\\test_runner_off");
    system("if exist results\\scenarios\\test_runner_silent rmdir /S /Q results\\scenarios\\test_runner_silent");
    return ok;
}

int main(void)
{
    ScenarioConfig config;
    ScenarioRunResult result;
    char error[256];

    system("if exist results\\scenarios\\test_scenario_runner_temp rmdir /S /Q results\\scenarios\\test_scenario_runner_temp");

    scenario_config_default(&config);
    snprintf(config.run_name, sizeof(config.run_name), TEST_RUN_NAME);
    snprintf(config.topology, sizeof(config.topology), "chain");
    config.neurons = 3;
    config.inhibitory_fraction = 0.0;
    config.connection_probability = 1.0;
    config.source_count = 1;
    config.steps = 80;
    config.record_neuron = 0;
    config.history_enabled = 0;

    if (!scenario_runner_execute(
            &config,
            NULL,
            &result,
            error,
            sizeof(error)))
    {
        printf("Unexpected runner error: %s\n", error);
        return fail("valid scenario did not run");
    }

    if (strcmp(result.output_directory, TEST_OUTPUT_DIR) != 0)
        return fail("unexpected output directory");

    if (result.connection_count != 2 || result.inhibitory_count != 0)
        return fail("unexpected runner metrics");

    if (!file_exists(TEST_OUTPUT_DIR "/config_used.ini") ||
        !file_exists(TEST_OUTPUT_DIR "/summary.txt") ||
        !file_exists(TEST_OUTPUT_DIR "/population.csv") ||
        !file_exists(TEST_OUTPUT_DIR "/raster.csv") ||
        !file_exists(TEST_OUTPUT_DIR "/neuron_0.csv") ||
        !file_exists(TEST_OUTPUT_DIR "/metrics.csv") ||
        !file_exists(TEST_OUTPUT_DIR "/run_manifest.txt"))
    {
        return fail("required output file missing");
    }

    if (!summary_contains_expected_data())
        return fail("summary does not contain expected data");

    if (!neuron_csv_contains_expected_data())
        return fail("neuron CSV does not contain expected data");

    cleanup_extra_outputs();

    if (!run_topology_smoke("random", TEST_RANDOM_RUN_NAME))
        return fail("random scenario smoke test failed");

    if (!run_topology_smoke("small_world", TEST_SMALL_WORLD_RUN_NAME))
        return fail("small_world scenario smoke test failed");

    if (!run_topology_smoke("feedforward", TEST_FEEDFORWARD_RUN_NAME))
        return fail("feedforward scenario smoke test failed");

    cleanup_extra_outputs();

    if (!run_connection_count_case(
            "all_to_all",
            TEST_ALL_TO_ALL_NO_SELF_RUN_NAME,
            0,
            12))
    {
        return fail("all_to_all without self-loop had wrong connection count");
    }

    if (!run_connection_count_case(
            "all_to_all",
            TEST_ALL_TO_ALL_SELF_RUN_NAME,
            1,
            16))
    {
        return fail("all_to_all with self-loop had wrong connection count");
    }

    if (!run_connection_count_case(
            "random",
            TEST_RANDOM_SELF_RUN_NAME,
            1,
            16))
    {
        return fail("random with self-loop did not produce coherent count");
    }

    cleanup_extra_outputs();

    if (!check_auto_unique_history())
        return fail("auto_unique_run did not preserve repeated outputs and history");

    if (!check_diagnostics_modes())
        return fail("diagnostics off/basic or silent execution failed");

    system("if exist results\\scenarios\\test_scenario_runner_temp rmdir /S /Q results\\scenarios\\test_scenario_runner_temp");
    printf("Scenario runner validation OK\n");
    return 0;
}
