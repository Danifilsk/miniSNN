#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "k0_scenario_artifacts.h"
#include "minisnn_worlds_kernel.h"

static void print_usage(const char *program)
{
    printf("Usage: %s --config <file.ini> --output <directory> [--overwrite]\n", program);
    printf("       %s --config <file.ini> --validate-only\n", program);
}

static int results_match(const K0ScenarioRunResult *left, const K0ScenarioRunResult *right)
{
    size_t index;

    if (left->scenario_config_signature != right->scenario_config_signature ||
        left->initial_state_hash != right->initial_state_hash ||
        left->final_state_hash != right->final_state_hash ||
        left->trace_row_count != right->trace_row_count ||
        left->event_row_count != right->event_row_count)
    {
        return 0;
    }
    for (index = 0U; index < left->trace_row_count; ++index)
    {
        const K0ScenarioTraceRow *a = &left->trace_rows[index];
        const K0ScenarioTraceRow *b = &right->trace_rows[index];

        if (a->tick != b->tick || a->state_hash != b->state_hash ||
            a->alive_entities != b->alive_entities || a->pending_commands != b->pending_commands ||
            a->last_tick_events != b->last_tick_events ||
            a->total_entities_created != b->total_entities_created ||
            a->total_entities_destroyed != b->total_entities_destroyed ||
            a->total_commands_submitted != b->total_commands_submitted ||
            a->total_commands_applied != b->total_commands_applied ||
            a->total_commands_rejected != b->total_commands_rejected ||
            a->total_events_emitted != b->total_events_emitted ||
            a->random_streams != b->random_streams ||
            a->total_random_u32_generated != b->total_random_u32_generated ||
            a->state_hash_version != b->state_hash_version || a->prng_version != b->prng_version)
        {
            return 0;
        }
    }
    for (index = 0U; index < left->event_row_count; ++index)
    {
        const K0ScenarioEventRow *a = &left->event_rows[index];
        const K0ScenarioEventRow *b = &right->event_rows[index];

        if (a->event_id != b->event_id || a->tick != b->tick ||
            a->event_type != b->event_type || a->command_id != b->command_id ||
            a->issuer_entity_id != b->issuer_entity_id ||
            a->subject_entity_id != b->subject_entity_id ||
            a->rejection_reason != b->rejection_reason)
        {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    const char *config_path = NULL;
    const char *output_directory = NULL;
    int overwrite = 0;
    int validate_only = 0;
    int index;
    char error[256];
    K0ScenarioConfig config;
    K0ScenarioRunResult result;

    memset(&result, 0, sizeof(result));
    for (index = 1; index < argc; ++index)
    {
        if (strcmp(argv[index], "--help") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[index], "--overwrite") == 0)
        {
            overwrite = 1;
            continue;
        }
        if (strcmp(argv[index], "--validate-only") == 0)
        {
            validate_only = 1;
            continue;
        }
        if ((strcmp(argv[index], "--config") == 0 || strcmp(argv[index], "--output") == 0) &&
            index + 1 < argc)
        {
            const char *value = argv[++index];

            if (strcmp(argv[index - 1], "--config") == 0 && config_path == NULL)
            {
                config_path = value;
                continue;
            }
            if (strcmp(argv[index - 1], "--output") == 0 && output_directory == NULL)
            {
                output_directory = value;
                continue;
            }
        }
        fprintf(stderr, "error: unknown or duplicate option\n");
        print_usage(argv[0]);
        return 2;
    }
    if (config_path == NULL || (!validate_only && output_directory == NULL))
    {
        fprintf(stderr, "error: --config and --output are required for execution\n");
        print_usage(argv[0]);
        return 2;
    }
    if (!k0_scenario_config_load_file(config_path, &config, error, sizeof(error)))
    {
        fprintf(stderr, "error: config: %s\n", error);
        return 1;
    }
    if (validate_only)
    {
        printf("status=VALID\nscenario_id=%s\nscenario_config_signature=%llu\n",
               config.scenario_id, (unsigned long long)k0_scenario_config_signature(&config));
        return 0;
    }
    if (!k0_scenario_artifacts_check_output(output_directory, overwrite, error, sizeof(error)))
    {
        fprintf(stderr, "error: output: %s\n", error);
        return 1;
    }
    if (!k0_scenario_execute(&config, &result, error, sizeof(error)))
    {
        fprintf(stderr, "error: execution: %s\n", error);
        return 1;
    }
    if (!k0_scenario_artifacts_write(output_directory, &result, overwrite, error, sizeof(error)))
    {
        fprintf(stderr, "error: artifacts: %s\n", error);
        k0_scenario_run_result_destroy(&result);
        return 1;
    }
    printf("status=OK\nscenario_id=%s\nscenario_config_signature=%llu\n"
           "initial_state_hash=0x%016llX\nfinal_state_hash=0x%016llX\n"
           "ticks_completed=%llu\ntrace_rows=%llu\nevent_rows=%llu\n",
           config.scenario_id, (unsigned long long)result.scenario_config_signature,
           (unsigned long long)result.initial_state_hash,
           (unsigned long long)result.final_state_hash,
           (unsigned long long)result.ticks_completed,
           (unsigned long long)result.trace_row_count,
           (unsigned long long)result.event_row_count);
    if (strcmp(config.scenario_id, "k0_integrated_demo") == 0)
    {
        K0ScenarioRunResult repeat;
        K0ScenarioRunResult alternate;
        K0ScenarioConfig alternate_config = config;
        int repeat_ok;
        int diverged;

        memset(&repeat, 0, sizeof(repeat));
        memset(&alternate, 0, sizeof(alternate));
        repeat_ok = k0_scenario_execute(&config, &repeat, error, sizeof(error)) &&
                    results_match(&result, &repeat);
        alternate_config.master_seed += UINT64_C(1);
        diverged = k0_scenario_execute(&alternate_config, &alternate, error, sizeof(error)) &&
                   alternate.final_state_hash != result.final_state_hash;
        printf("same_seed_repeat_match=%s\ndifferent_seed_diverged=%s\n",
               repeat_ok ? "yes" : "no", diverged ? "yes" : "no");
        k0_scenario_run_result_destroy(&repeat);
        k0_scenario_run_result_destroy(&alternate);
        if (!repeat_ok || !diverged)
        {
            k0_scenario_run_result_destroy(&result);
            return 1;
        }
    }
    else
    {
        printf("same_seed_repeat_match=not_run\ndifferent_seed_diverged=not_run\n");
    }
    k0_scenario_run_result_destroy(&result);
    return 0;
}
