#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_filesystem.h"
#include "c7_audit_common.h"
#include "d1_b_robustness_config.h"
#include "minisnn.h"

#define D1_B_OUTPUT_DIRECTORY "results/d1_b_robustness"
#define D1_B_PATH_MAX 512U
#define D1_B_AUDIT_FORMAT "d1_b_v2"

#ifdef _WIN32
#define D1_B_POPEN _popen
#define D1_B_PCLOSE _pclose
#define D1_B_NULL_REDIRECT "2>NUL"
#else
#define D1_B_POPEN popen
#define D1_B_PCLOSE pclose
#define D1_B_NULL_REDIRECT "2>/dev/null"
#endif

typedef struct
{
    const char *name;
    const char *model_name;
    uint64_t declared_steps;
    uint64_t completed_steps;
    uint64_t spikes;
    uint64_t signature;
    int passed;
} D1BRun;

static int join_path(char *out_path, size_t out_size, const char *name)
{
    int written;

    if (out_path == NULL || out_size == 0U || name == NULL)
        return 0;
    written = snprintf(out_path, out_size, "%s/%s", D1_B_OUTPUT_DIRECTORY, name);
    return written >= 0 && (size_t)written < out_size;
}

static uint64_t fnv_mix(uint64_t hash, uint64_t value)
{
    for (unsigned int index = 0U; index < 8U; index++)
    {
        hash ^= (value >> (index * 8U)) & UINT64_C(0xff);
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t double_bits(double value)
{
    uint64_t bits = 0U;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void git_value(char *out_value, size_t out_size, const char *command)
{
    FILE *pipe;

    if (out_value == NULL || out_size == 0U || command == NULL)
        return;
    snprintf(out_value, out_size, "NA");
    pipe = D1_B_POPEN(command, "r");
    if (pipe != NULL)
    {
        if (fgets(out_value, (int)out_size, pipe) != NULL)
        {
            out_value[strcspn(out_value, "\r\n")] = '\0';
            if (D1_B_PCLOSE(pipe) == 0 && out_value[0] != '\0')
                return;
        }
        else
        {
            (void)D1_B_PCLOSE(pipe);
        }
    }
    snprintf(out_value, out_size, "NA");
}

static void git_status_value(char *out_value, size_t out_size)
{
    FILE *pipe;
    char line[8];

    if (out_value == NULL || out_size == 0U)
        return;
    snprintf(out_value, out_size, "NA");
    pipe = D1_B_POPEN("git status --porcelain " D1_B_NULL_REDIRECT, "r");
    if (pipe == NULL)
        return;
    if (fgets(line, sizeof(line), pipe) != NULL)
    {
        (void)D1_B_PCLOSE(pipe);
        snprintf(out_value, out_size, "dirty");
    }
    else if (D1_B_PCLOSE(pipe) == 0)
        snprintf(out_value, out_size, "clean");
}

static int network_is_finite_and_valid(const MiniSNN *network)
{
    const int neurons = minisnn_neuron_count(network);
    const size_t connections = minisnn_connection_count(network);

    if (network == NULL || neurons <= 0)
        return 0;
    for (int neuron = 0; neuron < neurons; neuron++)
    {
        double voltage;

        if (!minisnn_get_voltage(network, neuron, &voltage) || !isfinite(voltage))
            return 0;
    }
    for (size_t index = 0U; index < connections; index++)
    {
        MiniSNNConnectionInfo connection;

        if (!minisnn_get_connection(network, index, &connection) ||
            !isfinite(connection.weight) || connection.delay == 0U ||
            connection.source >= (size_t)neurons || connection.target >= (size_t)neurons)
            return 0;
    }
    return 1;
}

static int configure_structural(MiniSNN *network, uint32_t neurons)
{
    MiniSNNStructuralPlasticityConfig structural =
        minisnn_default_structural_plasticity_config();

    structural.enabled = 1;
    structural.maintenance_interval_steps = 32U;
    structural.grace_period_steps = 32U;
    structural.min_connections = 1U;
    structural.max_connections = (size_t)neurons * (size_t)(neurons - 1U);
    return minisnn_set_structural_plasticity_config(network, &structural);
}

static int run_network(const D1BRobustnessConfig *config,
                       MiniSNNNeuronModel model, uint64_t steps,
                       int structural_enabled, D1BRun *out_run)
{
    MiniSNNConfig network_config = minisnn_default_config();
    MiniSNN *network;
    uint64_t signature = UINT64_C(14695981039346656037);
    uint64_t spikes = 0U;

    if (config == NULL || out_run == NULL || steps == 0U)
        return 0;
    network_config.neuron_count = (int)config->neuron_count;
    network_config.max_synaptic_delay = 4;
    network_config.neuron_model = model;
    network_config.adex = minisnn_adex_config_default();
    network_config.hodgkin_huxley = minisnn_hodgkin_huxley_config_default();
    network_config.dt = model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY ? 0.01 : 0.1;
    network = minisnn_create_with_config(&network_config);
    if (network == NULL)
        return 0;
    for (uint32_t source = 0U; source < config->neuron_count; source++)
    {
        if (!minisnn_connect_delayed(network, (int)source,
                                     (int)((source + 1U) % config->neuron_count),
                                     12.0, 1 + source % 4U))
        {
            minisnn_destroy(&network);
            return 0;
        }
    }
    if (structural_enabled && !configure_structural(network, config->neuron_count))
    {
        minisnn_destroy(&network);
        return 0;
    }
    signature = fnv_mix(signature, minisnn_neuron_model_config_signature(network));
    for (uint64_t step = 0U; step < steps; step++)
    {
        const double drive = (step + config->seed) % 9U < 5U ?
            (model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY ? 12.0 : 1000.0) : 0.0;
        int step_spikes;

        minisnn_clear_inputs(network);
        if (!minisnn_set_input(network, 0, drive) ||
            (step_spikes = minisnn_step(network)) < 0 ||
            !network_is_finite_and_valid(network))
        {
            minisnn_destroy(&network);
            return 0;
        }
        spikes += (uint64_t)step_spikes;
        signature = fnv_mix(signature, (uint64_t)(unsigned int)step_spikes);
        if ((step & UINT64_C(1023)) == 0U)
        {
            for (int neuron = 0; neuron < network_config.neuron_count; neuron++)
            {
                double voltage;

                if (!minisnn_get_voltage(network, neuron, &voltage))
                {
                    minisnn_destroy(&network);
                    return 0;
                }
                signature = fnv_mix(signature, double_bits(voltage));
            }
        }
        out_run->completed_steps = step + 1U;
    }
    for (size_t index = 0U; index < minisnn_connection_count(network); index++)
    {
        MiniSNNConnectionInfo connection;

        if (!minisnn_get_connection(network, index, &connection))
        {
            minisnn_destroy(&network);
            return 0;
        }
        signature = fnv_mix(signature, double_bits(connection.weight));
        signature = fnv_mix(signature, connection.delay);
    }
    out_run->spikes = spikes;
    out_run->signature = signature;
    out_run->passed = 1;
    minisnn_destroy(&network);
    return 1;
}

static int run_c7(const D1BRobustnessConfig *config, MiniSNNNeuronModel model,
                  D1BRun *out_run)
{
    C7AuditFixture fixture = {0};
    C7AuditFixture restored = {0};
    C7AuditFingerprint fingerprint;
    uint64_t spikes = 0U;
    char checkpoint[D1_B_PATH_MAX];
    MiniSNNAgentCycleError cycle_error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    const int homeostasis = model == MINISNN_NEURON_MODEL_LIF;

    if (config == NULL || out_run == NULL ||
        !join_path(checkpoint, sizeof(checkpoint), "c7_checkpoint"))
        return 0;
    c7_audit_remove_checkpoint_directory(checkpoint);
    if (!c7_audit_fixture_create(&fixture, model, C7_AUDIT_MIN_NEURONS,
                                 config->brain_steps_per_tick, 1, 1, homeostasis, 1))
        return 0;
    c7_audit_fingerprint_init(&fingerprint, &fixture, config->seed);
    for (uint64_t tick = 0U; tick < config->c7_ticks; tick++)
    {
        const double input[C7_AUDIT_SENSOR_COUNT] =
        {
            (tick + config->seed) % 3U == 0U ? 1.0 : 0.5,
            (tick + config->seed) % 3U == 1U ? -0.5 : 0.25,
            ((tick ^ config->seed) & UINT64_C(1)) == 0U ? 1.0 : 0.0
        };
        double action[C7_AUDIT_ACTION_COUNT] = {0};
        MiniSNNAgentCycleDiagnostics diagnostics = {0};

        if (!c7_audit_run_tick(&fixture, tick, input, action, &diagnostics) ||
            !c7_audit_fixture_all_finite(&fixture))
            goto failure;
        spikes += diagnostics.total_spikes;
        c7_audit_fingerprint_tick(&fingerprint, tick, input, action,
                                  &diagnostics, &fixture);
        if (tick + 1U == config->c7_ticks / 2U)
        {
            if (!minisnn_agent_cycle_save_checkpoint(fixture.cycle, checkpoint,
                                                     &cycle_error) ||
                !c7_audit_fixture_create(&restored, model, C7_AUDIT_MIN_NEURONS,
                                          config->brain_steps_per_tick, 1, 1,
                                          homeostasis, 1) ||
                !minisnn_agent_cycle_load_checkpoint(restored.cycle, checkpoint,
                                                     &cycle_error))
                goto failure;
            c7_audit_fixture_destroy(&fixture);
            fixture = restored;
            memset(&restored, 0, sizeof(restored));
        }
        if ((tick + 1U) % 1000U == 0U && !minisnn_agent_cycle_reset_episode(fixture.cycle))
            goto failure;
        out_run->completed_steps = tick + 1U;
    }
    out_run->spikes = spikes;
    out_run->signature = c7_audit_fingerprint_value(&fingerprint);
    out_run->passed = 1;
    c7_audit_fixture_destroy(&fixture);
    c7_audit_remove_checkpoint_directory(checkpoint);
    return 1;

failure:
    c7_audit_fixture_destroy(&fixture);
    c7_audit_fixture_destroy(&restored);
    c7_audit_remove_checkpoint_directory(checkpoint);
    return 0;
}

static int write_outputs(const D1BRobustnessConfig *config, const D1BRun *runs,
                         size_t run_count, const char *source_path)
{
    char source_copy[D1_B_PATH_MAX];
    char used_path[D1_B_PATH_MAX];
    char csv_path[D1_B_PATH_MAX];
    char summary_path[D1_B_PATH_MAX];
    char manifest_path[D1_B_PATH_MAX];
    char error[160];
    char git_commit[64];
    char git_status[16];
    FILE *csv;
    FILE *summary;
    FILE *manifest;

    if (!app_filesystem_ensure_directory_tree(D1_B_OUTPUT_DIRECTORY) ||
        !join_path(source_copy, sizeof(source_copy), "config_source.ini") ||
        !join_path(used_path, sizeof(used_path), "config_used.ini") ||
        !join_path(csv_path, sizeof(csv_path), "d1_b_long_runs.csv") ||
        !join_path(summary_path, sizeof(summary_path), "d1_b_summary.txt") ||
        !join_path(manifest_path, sizeof(manifest_path), "d1_b_manifest.txt") ||
        !app_filesystem_copy_file(source_path, source_copy) ||
        !d1_b_robustness_config_write_file(used_path, config, error, sizeof(error)))
        return 0;
    csv = fopen(csv_path, "wb");
    summary = fopen(summary_path, "wb");
    manifest = fopen(manifest_path, "wb");
    if (csv == NULL || summary == NULL || manifest == NULL)
    {
        if (csv != NULL)
            fclose(csv);
        if (summary != NULL)
            fclose(summary);
        if (manifest != NULL)
            fclose(manifest);
        return 0;
    }
    if (fprintf(csv, "audit_format_version,case,model,declared_steps,completed_steps,spikes,signature,status\n") < 0)
        goto output_failure;
    for (size_t index = 0U; index < run_count; index++)
        if (fprintf(csv, "%s,%s,%s,%llu,%llu,%llu,%llu,%s\n", D1_B_AUDIT_FORMAT,
                    runs[index].name,
                    runs[index].model_name,
                    (unsigned long long)runs[index].declared_steps,
                    (unsigned long long)runs[index].completed_steps,
                    (unsigned long long)runs[index].spikes,
                    (unsigned long long)runs[index].signature,
                    runs[index].passed ? "PASS" : "FAIL") < 0)
            goto output_failure;
    git_value(git_commit, sizeof(git_commit),
              "git rev-parse --short HEAD " D1_B_NULL_REDIRECT);
    git_status_value(git_status, sizeof(git_status));
    if (fprintf(summary, "D1-B robustness long-run audit\naudit_format_version=%s\nseed=%llu\nrun_count=%llu\n",
                D1_B_AUDIT_FORMAT,
                (unsigned long long)config->seed, (unsigned long long)run_count) < 0 ||
        fprintf(manifest,
                "audit=d1_b\naudit_format_version=" D1_B_AUDIT_FORMAT "\n"
                "git_commit=%s\ngit_status=%s\nconfig_signature=%llu\n"
                "config_source=config_source.ini\nconfig_used=config_used.ini\n"
                "long_runs=d1_b_long_runs.csv\nsource_api=candidate_v1\n"
                "studio_dependency=none\ncheckpoint_resume=c7_mid_run\n"
                "optimization_matrix_status=PENDING\nposix_headless_status=PENDING\n"
                "sanitizer_status=PENDING\nartifact_completion=PENDING\n",
                git_commit, git_status,
                (unsigned long long)d1_b_robustness_config_signature(config)) < 0)
        goto output_failure;
    return fclose(csv) == 0 && fclose(summary) == 0 && fclose(manifest) == 0;

output_failure:
    fclose(csv);
    fclose(summary);
    fclose(manifest);
    return 0;
}

int main(int argc, char **argv)
{
    D1BRobustnessConfig config;
    D1BRun runs[] =
    {
        {"neural", "lif", 0U, 0U, 0U, 0U, 0},
        {"neural", "adex", 0U, 0U, 0U, 0U, 0},
        {"neural", "hodgkin_huxley", 0U, 0U, 0U, 0U, 0},
        {"structural", "lif", 0U, 0U, 0U, 0U, 0},
        {"c7", "lif", 0U, 0U, 0U, 0U, 0},
        {"c7", "adex", 0U, 0U, 0U, 0U, 0},
        {"c7", "hodgkin_huxley", 0U, 0U, 0U, 0U, 0}
    };
    char error[160];
    const char *source_path = argc == 2 ? argv[1] : "configs/d1_b_robustness_audit.ini";

    if (argc > 2 || !d1_b_robustness_config_load_file(source_path, &config, error,
                                                       sizeof(error)))
    {
        fprintf(stderr, "D1-B audit configuration error: %s\n", error);
        return 1;
    }
    runs[0].declared_steps = config.lif_steps;
    runs[1].declared_steps = config.adex_steps;
    runs[2].declared_steps = config.hodgkin_huxley_steps;
    runs[3].declared_steps = config.structural_steps;
    runs[4].declared_steps = config.c7_ticks;
    runs[5].declared_steps = config.c7_ticks;
    runs[6].declared_steps = config.c7_ticks;
    if (!run_network(&config, MINISNN_NEURON_MODEL_LIF, config.lif_steps, 0, &runs[0]) ||
        !run_network(&config, MINISNN_NEURON_MODEL_ADEX, config.adex_steps, 0, &runs[1]) ||
        !run_network(&config, MINISNN_NEURON_MODEL_HODGKIN_HUXLEY,
                     config.hodgkin_huxley_steps, 0, &runs[2]) ||
        !run_network(&config, MINISNN_NEURON_MODEL_LIF, config.structural_steps, 1,
                     &runs[3]) ||
        !run_c7(&config, MINISNN_NEURON_MODEL_LIF, &runs[4]) ||
        !run_c7(&config, MINISNN_NEURON_MODEL_ADEX, &runs[5]) ||
        !run_c7(&config, MINISNN_NEURON_MODEL_HODGKIN_HUXLEY, &runs[6]) ||
        !write_outputs(&config, runs, sizeof(runs) / sizeof(runs[0]), source_path))
    {
        fprintf(stderr, "D1-B long-run audit FAILED\n");
        return 1;
    }
    printf("D1-B long-run audit OK: LIF=%llu AdEx=%llu HH=%llu structural=%llu C7=%llu/model\n",
           (unsigned long long)config.lif_steps, (unsigned long long)config.adex_steps,
           (unsigned long long)config.hodgkin_huxley_steps,
           (unsigned long long)config.structural_steps,
           (unsigned long long)config.c7_ticks);
    return 0;
}
