#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scenario_config.h"
#include "scenario_runner.h"

#define TEST_RUN_NAME "test_scenario_runner_temp"
#define TEST_OUTPUT_DIR "results/scenarios/test_scenario_runner_temp"

static int fail(const char *message)
{
    system("if exist results\\scenarios\\test_scenario_runner_temp rmdir /S /Q results\\scenarios\\test_scenario_runner_temp");
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
        !file_exists(TEST_OUTPUT_DIR "/neuron_0.csv"))
    {
        return fail("required output file missing");
    }

    if (!summary_contains_expected_data())
        return fail("summary does not contain expected data");

    system("if exist results\\scenarios\\test_scenario_runner_temp rmdir /S /Q results\\scenarios\\test_scenario_runner_temp");
    printf("Scenario runner validation OK\n");
    return 0;
}
