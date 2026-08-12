#include <stdio.h>
#include <string.h>

#include "k0_scenario_artifacts.h"

static int corrupt_file(const char *filename)
{
    FILE *file = fopen(filename, "r+b");
    int value;

    if (file == NULL || fseek(file, 0L, SEEK_END) != 0 || ftell(file) < 4L ||
        fseek(file, -2L, SEEK_END) != 0)
    {
        if (file != NULL) (void)fclose(file);
        return 0;
    }
    value = fgetc(file);
    if (value == EOF || fseek(file, -1L, SEEK_CUR) != 0 || fputc(value == '0' ? '1' : '0', file) == EOF)
    {
        (void)fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

int main(void)
{
    const char *directory = "../../build/worlds/kernel/tests/k0_d_artifacts";
    const char *manifest = "../../build/worlds/kernel/tests/k0_d_artifacts/manifest.ini";
    const char *not_directory = "../../build/worlds/kernel/tests/k0_d_not_directory";
    K0ScenarioConfig config;
    K0ScenarioRunResult result;
    char error[256];

    memset(&result, 0, sizeof(result));
    {
        FILE *file = fopen(not_directory, "wb");
        int file_ok = file != NULL && fputs("not a directory\n", file) != EOF && fclose(file) == 0;

        if (!file_ok || k0_scenario_artifacts_check_output(not_directory, 0, error, sizeof(error)) ||
            k0_scenario_artifacts_check_output("worlds/kernel/src/forbidden", 0,
                                              error, sizeof(error)))
        {
            return 1;
        }
    }
    (void)remove(not_directory);
    if (!k0_scenario_config_load_file("configs/k0_integrated_demo.ini", &config,
                                      error, sizeof(error)) ||
        !k0_scenario_execute(&config, &result, error, sizeof(error)) ||
        !k0_scenario_artifacts_write(directory, &result, 1, error, sizeof(error)) ||
        !k0_scenario_artifacts_validate_directory(directory, error, sizeof(error)))
    {
        k0_scenario_run_result_destroy(&result);
        return 1;
    }
    if (k0_scenario_artifacts_write(directory, &result, 0, error, sizeof(error)) ||
        !corrupt_file(manifest) ||
        k0_scenario_artifacts_validate_directory(directory, error, sizeof(error)))
    {
        k0_scenario_run_result_destroy(&result);
        return 1;
    }
    if (!k0_scenario_artifacts_write(directory, &result, 1, error, sizeof(error)) ||
        !k0_scenario_artifacts_validate_directory(directory, error, sizeof(error)) ||
        result.trace_row_count == 0U || result.event_row_count == 0U)
    {
        k0_scenario_run_result_destroy(&result);
        return 1;
    }
    k0_scenario_run_result_destroy(&result);
    printf("K0-D artifact validation OK\n");
    return 0;
}
