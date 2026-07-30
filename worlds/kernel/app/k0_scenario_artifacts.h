#ifndef K0_SCENARIO_ARTIFACTS_H
#define K0_SCENARIO_ARTIFACTS_H

#include <stddef.h>

#include "k0_scenario_engine.h"

#define K0_SCENARIO_ARTIFACT_PATH_MAX 1024U

int k0_scenario_artifacts_check_output(
    const char *output_directory,
    int overwrite,
    char *error_message,
    size_t error_message_size);

int k0_scenario_artifacts_write(
    const char *output_directory,
    const K0ScenarioRunResult *result,
    int overwrite,
    char *error_message,
    size_t error_message_size);

int k0_scenario_artifacts_validate_directory(
    const char *output_directory,
    char *error_message,
    size_t error_message_size);

#endif
