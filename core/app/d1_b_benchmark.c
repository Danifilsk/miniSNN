#include <stdio.h>
#include <string.h>

#include "app_filesystem.h"
#include "c7_audit_common.h"
#include "minisnn.h"

#define D1_B_BENCHMARK_OUTPUT "results/d1_b_robustness/d1_b_performance.csv"
#define D1_B_BENCHMARK_CHECKPOINT "results/d1_b_robustness/benchmark_checkpoint"
#define D1_B_REPEATS 3U
#define D1_B_NEURONS 32U
#define D1_B_STEPS 20000U
#define D1_B_C7_TICKS 300U
#define D1_B_AUDIT_FORMAT "d1_b_v2"

typedef struct
{
    double seconds;
    double throughput;
    size_t connections;
} BenchmarkResult;

static int create_ring(MiniSNN **out_network, int structural_enabled)
{
    MiniSNNConfig config = minisnn_default_config();
    MiniSNN *network;

    if (out_network == NULL)
        return 0;
    *out_network = NULL;
    config.neuron_count = (int)D1_B_NEURONS;
    config.max_synaptic_delay = 4;
    network = minisnn_create_with_config(&config);
    if (network == NULL)
        return 0;
    for (uint32_t source = 0U; source < D1_B_NEURONS; source++)
        if (!minisnn_connect_delayed(network, (int)source,
                                     (int)((source + 1U) % D1_B_NEURONS), 12.0,
                                     1 + source % 4U))
        {
            minisnn_destroy(&network);
            return 0;
        }
    if (structural_enabled)
    {
        MiniSNNStructuralPlasticityConfig structural =
            minisnn_default_structural_plasticity_config();

        structural.enabled = 1;
        structural.maintenance_interval_steps = 32U;
        structural.grace_period_steps = 32U;
        structural.min_connections = 1U;
        structural.max_connections = (size_t)D1_B_NEURONS * (D1_B_NEURONS - 1U);
        if (!minisnn_set_structural_plasticity_config(network, &structural))
        {
            minisnn_destroy(&network);
            return 0;
        }
    }
    *out_network = network;
    return 1;
}

static int benchmark_network(int structural_enabled, BenchmarkResult *out_result)
{
    MiniSNN *network = NULL;
    double start;
    double elapsed;

    if (out_result == NULL || !create_ring(&network, structural_enabled))
        return 0;
    start = app_filesystem_monotonic_seconds();
    for (uint32_t step = 0U; step < D1_B_STEPS; step++)
    {
        minisnn_clear_inputs(network);
        if (!minisnn_set_input(network, 0, step % 9U < 5U ? 1000.0 : 0.0) ||
            minisnn_step(network) < 0)
        {
            minisnn_destroy(&network);
            return 0;
        }
    }
    elapsed = app_filesystem_monotonic_seconds() - start;
    out_result->seconds = elapsed;
    out_result->throughput = elapsed > 0.0 ? (double)D1_B_STEPS / elapsed : 0.0;
    out_result->connections = minisnn_connection_count(network);
    minisnn_destroy(&network);
    return out_result->throughput > 0.0;
}

static int benchmark_c7(BenchmarkResult *out_result)
{
    C7AuditFixture fixture = {0};
    double start;
    double elapsed;

    if (out_result == NULL ||
        !c7_audit_fixture_create(&fixture, MINISNN_NEURON_MODEL_LIF,
                                 C7_AUDIT_MIN_NEURONS, 3U, 1, 1, 1, 1))
        return 0;
    start = app_filesystem_monotonic_seconds();
    for (uint64_t tick = 0U; tick < D1_B_C7_TICKS; tick++)
    {
        const double input[C7_AUDIT_SENSOR_COUNT] =
        {
            tick % 3U == 0U ? 1.0 : 0.5,
            tick % 3U == 1U ? -0.5 : 0.25,
            (tick & UINT64_C(1)) == 0U ? 1.0 : 0.0
        };
        double action[C7_AUDIT_ACTION_COUNT] = {0};

        if (!c7_audit_run_tick(&fixture, tick, input, action, NULL))
        {
            c7_audit_fixture_destroy(&fixture);
            return 0;
        }
    }
    elapsed = app_filesystem_monotonic_seconds() - start;
    out_result->seconds = elapsed;
    out_result->throughput = elapsed > 0.0 ? (double)D1_B_C7_TICKS / elapsed : 0.0;
    out_result->connections = minisnn_connection_count(fixture.network);
    c7_audit_fixture_destroy(&fixture);
    return out_result->throughput > 0.0;
}

static int benchmark_checkpoint(BenchmarkResult *out_save, BenchmarkResult *out_load)
{
    C7AuditFixture first = {0};
    C7AuditFixture restored = {0};
    MiniSNNAgentCycleError error = MINISNN_AGENT_CYCLE_ERROR_NONE;
    const double input[C7_AUDIT_SENSOR_COUNT] = {1.0, 0.0, 1.0};
    double action[C7_AUDIT_ACTION_COUNT] = {0};
    double start;

    if (out_save == NULL || out_load == NULL ||
        !c7_audit_fixture_create(&first, MINISNN_NEURON_MODEL_LIF,
                                 C7_AUDIT_MIN_NEURONS, 3U, 1, 1, 1, 1) ||
        !c7_audit_run_tick(&first, 0U, input, action, NULL))
        goto failure;
    c7_audit_remove_checkpoint_directory(D1_B_BENCHMARK_CHECKPOINT);
    start = app_filesystem_monotonic_seconds();
    if (!minisnn_agent_cycle_save_checkpoint(first.cycle, D1_B_BENCHMARK_CHECKPOINT, &error))
        goto failure;
    out_save->seconds = app_filesystem_monotonic_seconds() - start;
    out_save->throughput = out_save->seconds > 0.0 ? 1.0 / out_save->seconds : 0.0;
    out_save->connections = minisnn_connection_count(first.network);
    if (!c7_audit_fixture_create(&restored, MINISNN_NEURON_MODEL_LIF,
                                 C7_AUDIT_MIN_NEURONS, 3U, 1, 1, 1, 1))
        goto failure;
    start = app_filesystem_monotonic_seconds();
    if (!minisnn_agent_cycle_load_checkpoint(restored.cycle, D1_B_BENCHMARK_CHECKPOINT,
                                              &error))
        goto failure;
    out_load->seconds = app_filesystem_monotonic_seconds() - start;
    out_load->throughput = out_load->seconds > 0.0 ? 1.0 / out_load->seconds : 0.0;
    out_load->connections = minisnn_connection_count(restored.network);
    c7_audit_fixture_destroy(&first);
    c7_audit_fixture_destroy(&restored);
    c7_audit_remove_checkpoint_directory(D1_B_BENCHMARK_CHECKPOINT);
    return out_save->throughput > 0.0 && out_load->throughput > 0.0;

failure:
    c7_audit_fixture_destroy(&first);
    c7_audit_fixture_destroy(&restored);
    c7_audit_remove_checkpoint_directory(D1_B_BENCHMARK_CHECKPOINT);
    return 0;
}

static int write_row(FILE *file, const char *metric, unsigned int repeat,
                     const BenchmarkResult *result, uint32_t steps)
{
    return fprintf(file, "%s,%s,%u,%.9f,%.3f,lif,%u,%llu,%u,gcc\n", D1_B_AUDIT_FORMAT,
                   metric, repeat,
                   result->seconds, result->throughput, D1_B_NEURONS,
                   (unsigned long long)result->connections, steps) >= 0;
}

int main(void)
{
    FILE *file;

    if (!app_filesystem_ensure_directory_tree("results/d1_b_robustness"))
        return 1;
    file = fopen(D1_B_BENCHMARK_OUTPUT, "wb");
    if (file == NULL || fprintf(file,
                                "audit_format_version,metric,repeat,seconds,throughput,model,neuron_count,synapse_count,steps,toolchain\n") < 0)
    {
        if (file != NULL)
            fclose(file);
        return 1;
    }
    for (unsigned int repeat = 1U; repeat <= D1_B_REPEATS; repeat++)
    {
        BenchmarkResult neural;
        BenchmarkResult structural;
        BenchmarkResult c7;
        BenchmarkResult save;
        BenchmarkResult load;

        if (!benchmark_network(0, &neural) || !benchmark_network(1, &structural) ||
            !benchmark_c7(&c7) || !benchmark_checkpoint(&save, &load) ||
            !write_row(file, "neural_steps", repeat, &neural, D1_B_STEPS) ||
            !write_row(file, "structural_steps", repeat, &structural, D1_B_STEPS) ||
            !write_row(file, "c7_ticks", repeat, &c7, D1_B_C7_TICKS) ||
            !write_row(file, "checkpoint_save", repeat, &save, 1U) ||
            !write_row(file, "checkpoint_load", repeat, &load, 1U))
        {
            fclose(file);
            return 1;
        }
    }
    if (fclose(file) != 0)
        return 1;
    printf("D1-B benchmark OK: 3 repetitions per metric\n");
    return 0;
}
