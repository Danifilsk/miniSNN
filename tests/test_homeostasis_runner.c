#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scenario_config.h"
#include "scenario_runner.h"

#define ON_DIR "results/scenarios/test_homeostasis_runner_on"
#define OFF_DIR "results/scenarios/test_homeostasis_runner_off"
#define SILENT_DIR "results/scenarios/test_homeostasis_runner_silent"

static void cleanup(void)
{
    system("if exist results\\scenarios\\test_homeostasis_runner_on rmdir /S /Q results\\scenarios\\test_homeostasis_runner_on");
    system("if exist results\\scenarios\\test_homeostasis_runner_off rmdir /S /Q results\\scenarios\\test_homeostasis_runner_off");
    system("if exist results\\scenarios\\test_homeostasis_runner_silent rmdir /S /Q results\\scenarios\\test_homeostasis_runner_silent");
}

static void require(int condition, const char *message)
{
    if (!condition)
    {
        cleanup();
        fprintf(stderr, "Homeostasis runner test failed: %s\n", message);
        exit(1);
    }
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
    FILE *file = fopen(path, "r");
    char line[4096];

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

static int file_has_nonfinite_text(const char *path)
{
    FILE *file = fopen(path, "r");
    char line[4096];

    if (file == NULL)
        return 1;
    while (fgets(line, sizeof(line), file) != NULL)
    {
        for (char *p = line; *p != '\0'; p++)
        {
            if ((p == line || p[-1] == ',' || p[-1] == '=') &&
                (strncmp(p, "nan", 3) == 0 ||
                 strncmp(p, "inf", 3) == 0 ||
                 strncmp(p, "-inf", 4) == 0))
            {
                fclose(file);
                return 1;
            }
        }
    }
    fclose(file);
    return 0;
}

static int csv_last_integer(const char *path, int *out_value)
{
    FILE *file;
    char line[8192];
    char data[8192] = "";
    char *last_comma;

    if (out_value == NULL)
        return 0;
    file = fopen(path, "r");
    if (file == NULL)
        return 0;
    while (fgets(line, sizeof(line), file) != NULL)
        snprintf(data, sizeof(data), "%s", line);
    fclose(file);
    last_comma = strrchr(data, ',');
    return last_comma != NULL && sscanf(last_comma + 1, "%d", out_value) == 1;
}

static ScenarioConfig base_config(const char *run_name)
{
    ScenarioConfig config;
    scenario_config_default(&config);
    snprintf(config.run_name, sizeof(config.run_name), "%s", run_name);
    snprintf(config.topology, sizeof(config.topology), "chain");
    config.neurons = 2;
    config.inhibitory_fraction = 0.0;
    config.source_count = 1;
    config.record_neuron = 0;
    config.steps = 25;
    config.dt = 1.0;
    config.tau = 1.0;
    config.input_current = 20.0;
    config.diagnostics_level[0] = 'o';
    config.diagnostics_level[1] = 'f';
    config.diagnostics_level[2] = 'f';
    config.diagnostics_level[3] = '\0';
    config.history_enabled = 0;
    return config;
}

int main(void)
{
    ScenarioConfig config;
    ScenarioRunResult result;
    char error[256];

    cleanup();

    config = base_config("test_homeostasis_runner_on");
    config.homeostasis_enabled = 1;
    config.homeostasis_intrinsic_enabled = 1;
    config.homeostasis_target_rate = 0.1;
    config.homeostasis_rate_tau = 5.0;
    config.homeostasis_update_interval_steps = 5;
    config.homeostasis_threshold_eta = 0.5;
    config.homeostasis_record_history = 1;
    config.homeostasis_record_interval_steps = 6;
    config.homeostasis_record_neuron_limit = 1;
    snprintf(config.diagnostics_level, sizeof(config.diagnostics_level), "basic");

    require(scenario_runner_execute(
        &config, NULL, &result, error, sizeof(error)), error);
    require(file_exists(ON_DIR "/homeostasis_metrics.csv"), "metrics missing");
    require(file_exists(ON_DIR "/homeostasis_history.csv"), "history missing");
    require(file_exists(ON_DIR "/threshold_history.csv"), "threshold history missing");
    require(file_exists(ON_DIR "/homeostasis_neurons.csv"), "neurons missing");
    require(file_exists(ON_DIR "/homeostasis_report.txt"), "text report missing");
    require(file_exists(ON_DIR "/homeostasis_report.html"), "HTML report missing");
    require(file_contains(ON_DIR "/metrics.csv", "homeostasis_population_rate_final"),
        "general metrics do not include stable homeostasis fields");
    require(file_contains(ON_DIR "/homeostasis_history.csv", "25,"),
        "final non-multiple history step missing");
    require(file_contains(ON_DIR "/threshold_history.csv", ",0,"),
        "deterministic sampled neuron missing");
    require(!file_has_nonfinite_text(ON_DIR "/homeostasis_metrics.csv"),
        "non-finite homeostasis metric");

    config = base_config("test_homeostasis_runner_off");
    require(scenario_runner_execute(
        &config, NULL, &result, error, sizeof(error)), error);
    require(!file_exists(OFF_DIR "/homeostasis_metrics.csv"),
        "homeostasis metrics created while disabled");
    require(!file_exists(OFF_DIR "/homeostasis_history.csv"),
        "heavy history created while disabled");
    require(!file_exists(OFF_DIR "/threshold_history.csv"),
        "threshold history created while disabled");

    config = base_config("test_homeostasis_runner_silent");
    config.input_current = 0.0;
    config.homeostasis_enabled = 1;
    config.homeostasis_intrinsic_enabled = 0;
    config.homeostasis_inhibitory_gain_enabled = 1;
    config.homeostasis_record_history = 0;
    require(scenario_runner_execute(
        &config, NULL, &result, error, sizeof(error)), error);
    require(result.spikes_total == 0, "silent homeostasis run produced spikes");
    require(file_exists(SILENT_DIR "/homeostasis_history.csv"),
        "active run without full history lacks compact history");
    require(file_exists(SILENT_DIR "/threshold_history.csv"),
        "active run without full history lacks compact thresholds");
    require(file_contains(SILENT_DIR "/homeostasis_history.csv", "0,"),
        "compact history lacks initial state");
    require(file_contains(SILENT_DIR "/homeostasis_history.csv", "25,"),
        "compact history lacks final state");
    {
        int inhibitory_connections = -1;
        require(csv_last_integer(
            SILENT_DIR "/homeostasis_metrics.csv", &inhibitory_connections),
            "could not read inhibitory connection metric");
        require(inhibitory_connections == 0,
            "run without inhibitory connections not registered");
    }

    cleanup();
    printf("Homeostasis runner validation OK\n");
    return 0;
}
