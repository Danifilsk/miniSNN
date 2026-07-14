#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
#define TEST_COLLISION_RUN_NAME "test_scenario_runner_file_collision"
#define TEST_CORRUPT_HISTORY_RUN_NAME "test_scenario_runner_corrupt_history"
#define TEST_STDP_RUN_NAME "test_scenario_runner_stdp"
#define TEST_STDP_OUTPUT_DIR "results/scenarios/test_scenario_runner_stdp"
#define TEST_STDP_LTD_RUN_NAME "test_scenario_runner_stdp_ltd"
#define TEST_STDP_MIXED_RUN_NAME "test_scenario_runner_stdp_mixed"
#define TEST_STDP_SAMPLE_RUN_NAME "test_scenario_runner_stdp_sample"
#define TEST_STDP_EMPTY_RUN_NAME "test_scenario_runner_stdp_empty"
#define TEST_REWARD_RUN_NAME "test_scenario_runner_reward"
#define TEST_PUNISHMENT_RUN_NAME "test_scenario_runner_punishment"
#define TEST_DELAYED_RUN_NAME "test_scenario_runner_reward_delayed"
#define TEST_REWARD_SAME_STEP_RUN_NAME "test_scenario_runner_reward_same_step"
#define TEST_REWARD_SAMPLE_RUN_NAME "test_scenario_runner_reward_sample"

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
    remove("results/scenarios/test_scenario_runner_file_collision");
    system("if exist results\\scenarios\\test_scenario_runner_corrupt_history rmdir /S /Q results\\scenarios\\test_scenario_runner_corrupt_history");
    system("if exist results\\scenarios\\test_scenario_runner_stdp rmdir /S /Q results\\scenarios\\test_scenario_runner_stdp");
    system("if exist results\\scenarios\\test_scenario_runner_stdp_ltd rmdir /S /Q results\\scenarios\\test_scenario_runner_stdp_ltd");
    system("if exist results\\scenarios\\test_scenario_runner_stdp_mixed rmdir /S /Q results\\scenarios\\test_scenario_runner_stdp_mixed");
    system("if exist results\\scenarios\\test_scenario_runner_stdp_sample rmdir /S /Q results\\scenarios\\test_scenario_runner_stdp_sample");
    system("if exist results\\scenarios\\test_scenario_runner_stdp_empty rmdir /S /Q results\\scenarios\\test_scenario_runner_stdp_empty");
    system("if exist results\\scenarios\\test_scenario_runner_reward rmdir /S /Q results\\scenarios\\test_scenario_runner_reward");
    system("if exist results\\scenarios\\test_scenario_runner_punishment rmdir /S /Q results\\scenarios\\test_scenario_runner_punishment");
    system("if exist results\\scenarios\\test_scenario_runner_reward_delayed rmdir /S /Q results\\scenarios\\test_scenario_runner_reward_delayed");
    system("if exist results\\scenarios\\test_scenario_runner_reward_same_step rmdir /S /Q results\\scenarios\\test_scenario_runner_reward_same_step");
    system("if exist results\\scenarios\\test_scenario_runner_reward_sample rmdir /S /Q results\\scenarios\\test_scenario_runner_reward_sample");
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

static int check_filesystem_failures(void)
{
    ScenarioConfig config;
    ScenarioRunResult result;
    char error[256];
    FILE *file;
    int ok = 1;

    remove("results/scenarios/test_scenario_runner_file_collision");
    file = fopen("results/scenarios/test_scenario_runner_file_collision", "w");
    if (file == NULL)
        return 0;
    fputs("not a directory\n", file);
    fclose(file);

    scenario_config_default(&config);
    snprintf(config.run_name, sizeof(config.run_name), TEST_COLLISION_RUN_NAME);
    snprintf(config.topology, sizeof(config.topology), "chain");
    config.neurons = 2;
    config.inhibitory_fraction = 0.0;
    config.source_count = 1;
    config.steps = 2;
    config.record_neuron = 0;
    config.history_enabled = 0;

    if (scenario_runner_execute(&config, NULL, &result, error, sizeof(error)))
        ok = 0;
    remove("results/scenarios/test_scenario_runner_file_collision");
    return ok;
}

static int check_corrupt_history_is_rejected(void)
{
    ScenarioConfig config;
    ScenarioRunResult result;
    char error[256];
    FILE *file;
    int ok = 1;

    system("if exist build\\test_corrupt_history_backup.csv del /Q build\\test_corrupt_history_backup.csv");
    system("if exist results\\scenarios\\index.csv move /Y results\\scenarios\\index.csv build\\test_corrupt_history_backup.csv >NUL");
    file = fopen("results/scenarios/index.csv", "w");
    if (file == NULL)
        return 0;
    fputs("corrupted history without expected header\n", file);
    fclose(file);

    scenario_config_default(&config);
    snprintf(config.run_name, sizeof(config.run_name), TEST_CORRUPT_HISTORY_RUN_NAME);
    snprintf(config.topology, sizeof(config.topology), "chain");
    config.neurons = 2;
    config.inhibitory_fraction = 0.0;
    config.source_count = 1;
    config.steps = 2;
    config.record_neuron = 0;
    config.history_enabled = 1;

    if (scenario_runner_execute(&config, NULL, &result, error, sizeof(error)) ||
        strstr(error, "historico") == NULL)
    {
        ok = 0;
    }

    system("if exist results\\scenarios\\test_scenario_runner_corrupt_history rmdir /S /Q results\\scenarios\\test_scenario_runner_corrupt_history");
    remove("results/scenarios/index.csv");
    system("if exist build\\test_corrupt_history_backup.csv move /Y build\\test_corrupt_history_backup.csv results\\scenarios\\index.csv >NUL");
    return ok;
}

static int read_first_signed_change(const char *path, double *out_change)
{
    FILE *file = fopen(path, "r");
    char line[1024];
    char *token;
    int column = 0;

    if (file == NULL || fgets(line, sizeof(line), file) == NULL ||
        fgets(line, sizeof(line), file) == NULL)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }

    fclose(file);
    token = strtok(line, ",");

    while (token != NULL)
    {
        if (column == 11)
        {
            *out_change = strtod(token, NULL);
            return 1;
        }

        column++;
        token = strtok(NULL, ",");
    }

    return 0;
}

static int history_ends_at_step(const char *path, int expected_step)
{
    FILE *file = fopen(path, "r");
    char line[512];
    char last[512] = "";
    int step;

    if (file == NULL)
        return 0;

    while (fgets(line, sizeof(line), file) != NULL)
        snprintf(last, sizeof(last), "%s", line);

    fclose(file);
    return sscanf(last, "%d,", &step) == 1 && step == expected_step;
}

static int check_plasticity_runner_outputs(void)
{
    ScenarioConfig config;
    ScenarioRunResult result;
    char error[256];
    double signed_change = 0.0;
    int ok = 1;

    system("if exist results\\scenarios\\test_scenario_runner_stdp rmdir /S /Q results\\scenarios\\test_scenario_runner_stdp");
    scenario_config_default(&config);
    snprintf(config.run_name, sizeof(config.run_name), TEST_STDP_RUN_NAME);
    snprintf(config.topology, sizeof(config.topology), "chain");
    config.neurons = 2;
    config.inhibitory_fraction = 0.0;
    config.excitatory_weight = 150.0;
    config.source_count = 1;
    config.input_current = 20.0;
    config.steps = 2000;
    config.record_neuron = 1;
    config.history_enabled = 0;
    config.plasticity_enabled = 1;
    config.plasticity_a_plus = 1.0;
    config.plasticity_a_minus = 1.05;
    config.plasticity_weight_max = 300.0;
    config.plasticity_record_interval_steps = 333;
    config.plasticity_record_connection_limit = 1;

    if (!scenario_runner_execute(&config, NULL, &result, error, sizeof(error)))
    {
        printf("Unexpected STDP runner error: %s\n", error);
        ok = 0;
        goto cleanup;
    }

    if (!file_exists(TEST_STDP_OUTPUT_DIR "/weights_initial.csv") ||
        !file_exists(TEST_STDP_OUTPUT_DIR "/weights_final.csv") ||
        !file_exists(TEST_STDP_OUTPUT_DIR "/weight_history.csv") ||
        !file_exists(TEST_STDP_OUTPUT_DIR "/plasticity_metrics.csv") ||
        !file_exists(TEST_STDP_OUTPUT_DIR "/stdp_report.txt") ||
        !read_first_signed_change(
            TEST_STDP_OUTPUT_DIR "/weights_final.csv",
            &signed_change) ||
        signed_change <= 0.0 ||
        !history_ends_at_step(
            TEST_STDP_OUTPUT_DIR "/weight_history.csv",
            config.steps) ||
        !csv_header_contains(
            TEST_STDP_OUTPUT_DIR "/metrics.csv",
            "plasticity_modified_connection_fraction"))
    {
        ok = 0;
    }

cleanup:
    system("if exist results\\scenarios\\test_scenario_runner_stdp rmdir /S /Q results\\scenarios\\test_scenario_runner_stdp");
    return ok;
}

static int report_has_non_finite_token(const char *path)
{
    FILE *file = fopen(path, "r");
    char line[1024];

    if (file == NULL)
        return 1;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        for (char *cursor = line; *cursor != '\0'; cursor++)
        {
            if (*cursor >= 'A' && *cursor <= 'Z')
                *cursor = (char)(*cursor - 'A' + 'a');
        }

        if (strstr(line, "nan") != NULL ||
            strstr(line, "=inf") != NULL ||
            strstr(line, "=-inf") != NULL)
        {
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
}

static int validate_final_weight_rows(
    const char *path,
    double weight_min,
    double weight_max,
    int require_inhibitory,
    double *out_total_change)
{
    FILE *file = fopen(path, "r");
    char line[2048];
    int inhibitory_rows = 0;
    double total_change = 0.0;

    if (file == NULL || fgets(line, sizeof(line), file) == NULL)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *fields[13];
        char *token = strtok(line, ",");
        int count = 0;

        while (token != NULL && count < 13)
        {
            fields[count++] = token;
            token = strtok(NULL, ",");
        }

        if (count != 13)
        {
            fclose(file);
            return 0;
        }

        {
            double current = strtod(fields[10], NULL);
            double initial = strtod(fields[9], NULL);
            double change = strtod(fields[11], NULL);
            int eligible = atoi(fields[7]);

            if (!isfinite(current) || !isfinite(initial) || !isfinite(change))
            {
                fclose(file);
                return 0;
            }

            if (strcmp(fields[3], "INH") == 0)
            {
                inhibitory_rows++;
                if (eligible != 0 || current != initial || change != 0.0)
                {
                    fclose(file);
                    return 0;
                }
            }
            else if (eligible && (current < weight_min || current > weight_max))
            {
                fclose(file);
                return 0;
            }

            total_change += change;
        }
    }

    fclose(file);
    if (require_inhibitory && inhibitory_rows == 0)
        return 0;
    *out_total_change = total_change;
    return 1;
}

static int run_stdp_demo_case(
    const char *config_path,
    const char *run_name,
    int expected_change_sign,
    int require_inhibitory)
{
    ScenarioConfig config;
    ScenarioRunResult result;
    char error[256];
    char final_path[256];
    char report_path[256];
    double total_change = 0.0;

    if (!scenario_config_load_file(
            config_path,
            &config,
            error,
            sizeof(error)))
    {
        printf("Unexpected scenario load error: %s\n", error);
        return 0;
    }

    snprintf(config.run_name, sizeof(config.run_name), "%s", run_name);
    config.auto_unique_run = 0;
    config.history_enabled = 0;

    if (!scenario_runner_execute(&config, NULL, &result, error, sizeof(error)))
    {
        printf("Unexpected STDP demo runner error: %s\n", error);
        return 0;
    }

    snprintf(
        final_path,
        sizeof(final_path),
        "results/scenarios/%s/weights_final.csv",
        run_name);
    snprintf(
        report_path,
        sizeof(report_path),
        "results/scenarios/%s/stdp_report.txt",
        run_name);

    if (!validate_final_weight_rows(
            final_path,
            config.plasticity_weight_min,
            config.plasticity_weight_max,
            require_inhibitory,
            &total_change) ||
        report_has_non_finite_token(report_path))
    {
        return 0;
    }

    if (expected_change_sign < 0 && total_change >= 0.0)
        return 0;
    if (expected_change_sign > 0 && total_change <= 0.0)
        return 0;

    return 1;
}

static int read_sample_ids(const char *path, size_t *ids, size_t capacity)
{
    FILE *file = fopen(path, "r");
    char line[1024];
    size_t count = 0;

    if (file == NULL || fgets(line, sizeof(line), file) == NULL)
    {
        if (file != NULL)
            fclose(file);
        return -1;
    }

    while (count < capacity && fgets(line, sizeof(line), file) != NULL)
    {
        unsigned long long id;

        if (sscanf(line, "%llu,", &id) != 1)
        {
            fclose(file);
            return -1;
        }
        ids[count++] = (size_t)id;
    }

    if (fgets(line, sizeof(line), file) != NULL)
    {
        fclose(file);
        return -1;
    }

    fclose(file);
    return (int)count;
}

static int check_deterministic_plasticity_sampling(void)
{
    ScenarioConfig config;
    ScenarioRunResult result;
    char error[256];
    size_t first[5];
    size_t second[5];
    const size_t expected[5] = {0U, 94U, 189U, 284U, 379U};
    const char *path =
        "results/scenarios/test_scenario_runner_stdp_sample/weights_initial.csv";

    scenario_config_default(&config);
    snprintf(config.run_name, sizeof(config.run_name), TEST_STDP_SAMPLE_RUN_NAME);
    snprintf(config.topology, sizeof(config.topology), "all_to_all");
    config.neurons = 20;
    config.inhibitory_fraction = 0.0;
    config.allow_self_connections = 0;
    config.source_count = 1;
    config.input_current = 0.0;
    config.steps = 1;
    config.record_neuron = 0;
    config.auto_unique_run = 0;
    config.history_enabled = 0;
    config.plasticity_enabled = 1;
    config.plasticity_weight_max = 300.0;
    config.plasticity_record_history = 0;
    config.plasticity_record_connection_limit = 5;

    system("if exist results\\scenarios\\test_scenario_runner_stdp_sample rmdir /S /Q results\\scenarios\\test_scenario_runner_stdp_sample");
    if (!scenario_runner_execute(&config, NULL, &result, error, sizeof(error)) ||
        result.connection_count != 380 ||
        read_sample_ids(path, first, 5) != 5)
    {
        return 0;
    }

    system("if exist results\\scenarios\\test_scenario_runner_stdp_sample rmdir /S /Q results\\scenarios\\test_scenario_runner_stdp_sample");
    if (!scenario_runner_execute(&config, NULL, &result, error, sizeof(error)) ||
        read_sample_ids(path, second, 5) != 5)
    {
        return 0;
    }

    for (size_t i = 0; i < 5; i++)
    {
        if (first[i] != expected[i] || second[i] != expected[i])
            return 0;
    }

    return 1;
}

static int read_reward_history_step_zero_ids(
    const char *path,
    size_t *ids,
    size_t capacity)
{
    FILE *file = fopen(path, "r");
    char line[1024];
    size_t count = 0;

    if (file == NULL || fgets(line, sizeof(line), file) == NULL)
    {
        if (file != NULL)
            fclose(file);
        return -1;
    }

    while (fgets(line, sizeof(line), file) != NULL)
    {
        int step;
        unsigned long long connection_id;
        unsigned long long source;
        unsigned long long target;
        double eligibility;
        int sampled;

        if (sscanf(
                line,
                "%d,%llu,%llu,%llu,%lf,%d",
                &step,
                &connection_id,
                &source,
                &target,
                &eligibility,
                &sampled) != 6)
        {
            fclose(file);
            return -1;
        }

        if (step == 0)
        {
            if (count >= capacity || sampled != 1)
            {
                fclose(file);
                return -1;
            }
            ids[count++] = (size_t)connection_id;
        }
    }

    fclose(file);
    return (int)count;
}

static int sampled_reward_connections_are_excitatory(
    const char *path,
    const size_t *expected,
    size_t expected_count)
{
    FILE *file = fopen(path, "r");
    char line[2048];
    size_t found = 0;

    if (file == NULL || fgets(line, sizeof(line), file) == NULL)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL)
    {
        unsigned long long connection_id;
        unsigned long long source;
        unsigned long long target;
        char source_type[8];
        char target_type[8];
        int eligible;
        int sampled;

        if (sscanf(
                line,
                "%llu,%llu,%llu,%7[^,],%7[^,],%d,%d,",
                &connection_id,
                &source,
                &target,
                source_type,
                target_type,
                &eligible,
                &sampled) != 7)
        {
            fclose(file);
            return 0;
        }

        if (sampled)
        {
            if (found >= expected_count ||
                (size_t)connection_id != expected[found] ||
                strcmp(source_type, "EXC") != 0 ||
                eligible != 1)
            {
                fclose(file);
                return 0;
            }
            found++;
        }
    }

    fclose(file);
    return found == expected_count;
}

static int check_distributed_reward_sampling(void)
{
    ScenarioConfig config;
    ScenarioRunResult result;
    char error[256];
    size_t first[5];
    size_t second[5];
    const size_t expected[5] = {0U, 4U, 9U, 14U, 19U};
    const char *history_path =
        "results/scenarios/test_scenario_runner_reward_sample/eligibility_history.csv";
    const char *connections_path =
        "results/scenarios/test_scenario_runner_reward_sample/reward_connections.csv";

    scenario_config_default(&config);
    snprintf(config.run_name, sizeof(config.run_name), TEST_REWARD_SAMPLE_RUN_NAME);
    snprintf(config.topology, sizeof(config.topology), "all_to_all");
    config.neurons = 5;
    config.inhibitory_fraction = 0.0;
    config.allow_self_connections = 0;
    config.source_count = 1;
    config.input_current = 0.0;
    config.steps = 1;
    config.record_neuron = 0;
    config.auto_unique_run = 0;
    config.history_enabled = 0;
    config.plasticity_enabled = 1;
    snprintf(
        config.plasticity_learning_mode,
        sizeof(config.plasticity_learning_mode),
        "reward_modulated_stdp");
    config.plasticity_weight_max = 300.0;
    config.reward_enabled = 1;
    config.reward_record_history = 1;
    config.reward_record_connection_limit = 5;

    system("if exist results\\scenarios\\test_scenario_runner_reward_sample rmdir /S /Q results\\scenarios\\test_scenario_runner_reward_sample");
    if (!scenario_runner_execute(&config, NULL, &result, error, sizeof(error)) ||
        result.connection_count != 20 ||
        read_reward_history_step_zero_ids(history_path, first, 5) != 5 ||
        !sampled_reward_connections_are_excitatory(
            connections_path, expected, 5))
    {
        return 0;
    }

    system("if exist results\\scenarios\\test_scenario_runner_reward_sample rmdir /S /Q results\\scenarios\\test_scenario_runner_reward_sample");
    if (!scenario_runner_execute(&config, NULL, &result, error, sizeof(error)) ||
        read_reward_history_step_zero_ids(history_path, second, 5) != 5)
    {
        return 0;
    }

    for (size_t i = 0; i < 5; i++)
    {
        if (first[i] != expected[i] || second[i] != expected[i] ||
            (i > 0 && (first[i] <= first[i - 1] || second[i] <= second[i - 1])))
        {
            return 0;
        }
    }

    return 1;
}

static int check_empty_plasticity_run(void)
{
    ScenarioConfig config;
    ScenarioRunResult result;
    char error[256];
    FILE *file;
    char header[4096];
    char row[4096];
    int eligible = -1;

    scenario_config_default(&config);
    snprintf(config.run_name, sizeof(config.run_name), TEST_STDP_EMPTY_RUN_NAME);
    snprintf(config.topology, sizeof(config.topology), "chain");
    config.neurons = 1;
    config.inhibitory_fraction = 0.0;
    config.source_count = 1;
    config.input_current = 0.0;
    config.steps = 20;
    config.record_neuron = 0;
    config.auto_unique_run = 0;
    config.history_enabled = 0;
    config.plasticity_enabled = 1;
    config.plasticity_weight_max = 300.0;

    system("if exist results\\scenarios\\test_scenario_runner_stdp_empty rmdir /S /Q results\\scenarios\\test_scenario_runner_stdp_empty");
    if (!scenario_runner_execute(&config, NULL, &result, error, sizeof(error)) ||
        result.spikes_total != 0 || result.connection_count != 0 ||
        report_has_non_finite_token(
            "results/scenarios/test_scenario_runner_stdp_empty/stdp_report.txt"))
    {
        return 0;
    }

    file = fopen(
        "results/scenarios/test_scenario_runner_stdp_empty/plasticity_metrics.csv",
        "r");
    if (file == NULL || fgets(header, sizeof(header), file) == NULL ||
        fgets(row, sizeof(row), file) == NULL)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    fclose(file);

    {
        char *token = strtok(row, ",");
        int column = 0;

        while (token != NULL)
        {
            if (column == 2)
            {
                eligible = atoi(token);
                break;
            }
            column++;
            token = strtok(NULL, ",");
        }
    }

    return eligible == 0;
}

static int read_first_double_column(
    const char *path,
    int requested_column,
    double *out_value)
{
    FILE *file = fopen(path, "r");
    char line[4096];
    char *token;
    int column = 0;

    if (file == NULL || fgets(line, sizeof(line), file) == NULL ||
        fgets(line, sizeof(line), file) == NULL)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    fclose(file);
    token = strtok(line, ",");
    while (token != NULL)
    {
        if (column == requested_column)
        {
            *out_value = strtod(token, NULL);
            return isfinite(*out_value);
        }
        column++;
        token = strtok(NULL, ",");
    }
    return 0;
}

static int check_reward_runner_outputs(void)
{
    const char *configs[] = {
        "configs/reward_positive_demo.ini",
        "configs/punishment_negative_demo.ini",
        "configs/reward_delayed_demo.ini"
    };
    const char *names[] = {
        TEST_REWARD_RUN_NAME,
        TEST_PUNISHMENT_RUN_NAME,
        TEST_DELAYED_RUN_NAME
    };
    double changes[3];
    int case_index;

    for (case_index = 0; case_index < 3; case_index++)
    {
        ScenarioConfig config;
        ScenarioRunResult result;
        char error[256];
        char directory[256];
        char path[320];

        if (!scenario_config_load_file(
                configs[case_index], &config, error, sizeof(error)))
            return 0;
        snprintf(config.run_name, sizeof(config.run_name), "%s", names[case_index]);
        config.history_enabled = 0;
        config.auto_unique_run = 0;
        if (!scenario_runner_execute(&config, NULL, &result, error, sizeof(error)))
            return 0;

        snprintf(directory, sizeof(directory), "results/scenarios/%s", names[case_index]);
        snprintf(path, sizeof(path), "%s/reward_connections.csv", directory);
        if (!file_exists(path) || !read_first_double_column(path, 9, &changes[case_index]))
            return 0;
        snprintf(path, sizeof(path), "%s/reward_metrics.csv", directory);
        if (!file_exists(path) || !csv_header_contains(path, "reward_event_count"))
            return 0;
        snprintf(path, sizeof(path), "%s/reward_events.csv", directory);
        if (!file_exists(path))
            return 0;
        snprintf(path, sizeof(path), "%s/reward_history.csv", directory);
        if (!history_ends_at_step(path, config.steps))
            return 0;
        snprintf(path, sizeof(path), "%s/eligibility_history.csv", directory);
        if (!file_exists(path))
            return 0;
        snprintf(path, sizeof(path), "%s/reward_report.txt", directory);
        if (report_has_non_finite_token(path))
            return 0;
        snprintf(path, sizeof(path), "%s/reward_report.html", directory);
        if (!file_exists(path))
            return 0;
    }

    if (!(changes[0] > 0.0 && changes[1] < 0.0 && changes[2] > 0.0 &&
          changes[2] < changes[0]))
    {
        return 0;
    }

    {
        ScenarioConfig config;
        ScenarioRunResult result;
        char error[256];
        double raw_reward;
        double component_count;

        if (!scenario_config_load_file(
                "configs/reward_positive_demo.ini", &config, error, sizeof(error)))
            return 0;
        snprintf(config.run_name, sizeof(config.run_name),
            TEST_REWARD_SAME_STEP_RUN_NAME);
        config.history_enabled = 0;
        config.reward_event_count = 2;
        config.reward_events[0].index = 0;
        config.reward_events[0].step = 900;
        config.reward_events[0].value = 0.6;
        config.reward_events[1].index = 1;
        config.reward_events[1].step = 900;
        config.reward_events[1].value = 0.4;
        if (!scenario_runner_execute(&config, NULL, &result, error, sizeof(error)) ||
            !read_first_double_column(
                "results/scenarios/test_scenario_runner_reward_same_step/reward_events.csv",
                1, &raw_reward) ||
            !read_first_double_column(
                "results/scenarios/test_scenario_runner_reward_same_step/reward_events.csv",
                3, &component_count) ||
            fabs(raw_reward - 1.0) > 1e-12 || component_count != 2.0)
        {
            return 0;
        }
    }

    return 1;
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

    if (file_exists(TEST_OUTPUT_DIR "/weights_initial.csv") ||
        file_exists(TEST_OUTPUT_DIR "/plasticity_metrics.csv") ||
        file_exists(TEST_OUTPUT_DIR "/reward_metrics.csv"))
    {
        return fail("plasticity outputs were created while STDP was off");
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

    if (!check_filesystem_failures())
        return fail("filesystem collision was not handled safely");

    if (!check_corrupt_history_is_rejected())
        return fail("corrupt history was not rejected safely");

    if (!check_plasticity_runner_outputs())
        return fail("plasticity runner outputs or LTP validation failed");

    if (!run_stdp_demo_case(
            "configs/stdp_ltd_demo.ini",
            TEST_STDP_LTD_RUN_NAME,
            -1,
            0))
    {
        return fail("LTD runner demo did not depress eligible weights");
    }

    if (!run_stdp_demo_case(
            "configs/stdp_mixed_demo.ini",
            TEST_STDP_MIXED_RUN_NAME,
            0,
            1))
    {
        return fail("mixed runner demo changed inhibitory weights or left bounds");
    }

    if (!check_deterministic_plasticity_sampling())
        return fail("plasticity connection sampling was not deterministic and distributed");

    if (!check_empty_plasticity_run())
        return fail("silent plasticity run with zero eligible connections failed");

    if (!check_reward_runner_outputs())
        return fail("reward runner outputs, aggregation, or delayed reward failed");

    if (!check_distributed_reward_sampling())
        return fail("reward history sample IDs differ from 0,4,9,14,19");

    system("if exist results\\scenarios\\test_scenario_runner_temp rmdir /S /Q results\\scenarios\\test_scenario_runner_temp");
    system("if exist results\\scenarios\\test_scenario_runner_stdp_ltd rmdir /S /Q results\\scenarios\\test_scenario_runner_stdp_ltd");
    system("if exist results\\scenarios\\test_scenario_runner_stdp_mixed rmdir /S /Q results\\scenarios\\test_scenario_runner_stdp_mixed");
    system("if exist results\\scenarios\\test_scenario_runner_stdp_sample rmdir /S /Q results\\scenarios\\test_scenario_runner_stdp_sample");
    system("if exist results\\scenarios\\test_scenario_runner_stdp_empty rmdir /S /Q results\\scenarios\\test_scenario_runner_stdp_empty");
    system("if exist results\\scenarios\\test_scenario_runner_reward rmdir /S /Q results\\scenarios\\test_scenario_runner_reward");
    system("if exist results\\scenarios\\test_scenario_runner_punishment rmdir /S /Q results\\scenarios\\test_scenario_runner_punishment");
    system("if exist results\\scenarios\\test_scenario_runner_reward_delayed rmdir /S /Q results\\scenarios\\test_scenario_runner_reward_delayed");
    system("if exist results\\scenarios\\test_scenario_runner_reward_same_step rmdir /S /Q results\\scenarios\\test_scenario_runner_reward_same_step");
    system("if exist results\\scenarios\\test_scenario_runner_reward_sample rmdir /S /Q results\\scenarios\\test_scenario_runner_reward_sample");
    printf("Scenario runner validation OK\n");
    return 0;
}
