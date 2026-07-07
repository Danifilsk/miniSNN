#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "network.h"
#include "recorder.h"

#define POPULATION_SIZE 20
#define INHIBITORY_COUNT 4
#define EXPERIMENT_STEPS 1000
#define TEST_W_INH -400.0
#define INPUT_SOURCE_COUNT 3
#define ARCHITECTURE_COUNT 2
#define PROBABILITY_COUNT 4
#define SEED_COUNT 10
#define LAST_WINDOW_START 900
#define TEMP_FILENAME_LENGTH 160

typedef enum
{
    ARCH_RANDOM_ALL_CONNECTIONS = 0,
    ARCH_RANDOM_NO_INH_TO_INH = 1
} ArchitectureMode;

typedef struct
{
    const char *name;
    ArchitectureMode mode;
} ArchitectureConfig;

typedef struct
{
    int total;
    int exc_exc;
    int exc_inh;
    int inh_exc;
    int inh_inh;
} ConnectionCounts;

typedef struct
{
    int spikes_exc_total;
    int spikes_inh_total;
    int spikes_exc_last_100;
    int spikes_inh_last_100;
    int active_exc_timesteps_last_100;
    int active_inh_timesteps_last_100;
    int last_exc_step;
    int last_inh_step;
} ActivityMetrics;

typedef struct
{
    const char *architecture;
    double probability;
    unsigned int seed;
    ConnectionCounts connections;
    ActivityMetrics activity;
} RunResult;

typedef struct
{
    const char *architecture;
    double probability;
    int executions;
    double exc_inh_connection_sum;
    double inh_exc_connection_sum;
    double inh_inh_connection_sum;
    double exc_spike_total_sum;
    double inh_spike_total_sum;
    double exc_spike_last_100_sum;
    double inh_spike_last_100_sum;
    double exc_active_timestep_last_100_sum;
    double inh_active_timestep_last_100_sum;
    int cases_with_exc_last_100;
    int cases_with_inh_last_100;
} SummaryResult;

static int is_inhibitory_neuron(int neuron_id)
{
    return neuron_id >= POPULATION_SIZE - INHIBITORY_COUNT;
}

static int is_valid_probability(double probability)
{
    return probability >= 0.0 && probability <= 1.0;
}

static void reset_connection_counts(ConnectionCounts *counts)
{
    counts->total = 0;
    counts->exc_exc = 0;
    counts->exc_inh = 0;
    counts->inh_exc = 0;
    counts->inh_inh = 0;
}

static void reset_activity_metrics(ActivityMetrics *metrics)
{
    metrics->spikes_exc_total = 0;
    metrics->spikes_inh_total = 0;
    metrics->spikes_exc_last_100 = 0;
    metrics->spikes_inh_last_100 = 0;
    metrics->active_exc_timesteps_last_100 = 0;
    metrics->active_inh_timesteps_last_100 = 0;
    metrics->last_exc_step = -1;
    metrics->last_inh_step = -1;
}

static void reset_run_result(
    RunResult *result,
    const ArchitectureConfig *architecture,
    double probability,
    unsigned int seed)
{
    result->architecture = architecture->name;
    result->probability = probability;
    result->seed = seed;
    reset_connection_counts(&result->connections);
    reset_activity_metrics(&result->activity);
}

static void init_source_recorders(NeuronRecorder recorders[INPUT_SOURCE_COUNT])
{
    for (int i = 0; i < INPUT_SOURCE_COUNT; i++)
    {
        recorders[i].file = NULL;
        recorders[i].neuron_id = -1;
    }
}

static void close_source_recorders(
    NeuronRecorder recorders[INPUT_SOURCE_COUNT])
{
    for (int i = 0; i < INPUT_SOURCE_COUNT; i++)
        neuron_recorder_close(&recorders[i]);
}

static void remove_temp_files(
    const char *population_filename,
    char source_filenames[INPUT_SOURCE_COUNT][TEMP_FILENAME_LENGTH])
{
    if (population_filename != NULL && population_filename[0] != '\0')
        remove(population_filename);

    for (int i = 0; i < INPUT_SOURCE_COUNT; i++)
    {
        if (source_filenames[i][0] != '\0')
            remove(source_filenames[i]);
    }
}

static int write_source_filename(
    char *buffer,
    int size,
    int architecture_index,
    int probability_index,
    unsigned int seed,
    int neuron_id)
{
    int written = snprintf(
        buffer,
        (size_t)size,
        "tmp_sparse_ei_balance_a%d_p%d_s%u_source%d.csv",
        architecture_index,
        probability_index,
        seed,
        neuron_id);

    return written > 0 && written < size;
}

static int make_temp_filenames(
    int architecture_index,
    int probability_index,
    unsigned int seed,
    char *population_filename,
    char source_filenames[INPUT_SOURCE_COUNT][TEMP_FILENAME_LENGTH])
{
    int written = snprintf(
        population_filename,
        TEMP_FILENAME_LENGTH,
        "tmp_sparse_ei_balance_a%d_p%d_s%u_population.csv",
        architecture_index,
        probability_index,
        seed);

    if (written <= 0 || written >= TEMP_FILENAME_LENGTH)
        return 0;

    for (int i = 0; i < INPUT_SOURCE_COUNT; i++)
    {
        if (!write_source_filename(
                source_filenames[i],
                TEMP_FILENAME_LENGTH,
                architecture_index,
                probability_index,
                seed,
                i))
        {
            return 0;
        }
    }

    return 1;
}

static int open_source_recorders(
    NeuronRecorder recorders[INPUT_SOURCE_COUNT],
    char source_filenames[INPUT_SOURCE_COUNT][TEMP_FILENAME_LENGTH])
{
    init_source_recorders(recorders);

    for (int neuron_id = 0; neuron_id < INPUT_SOURCE_COUNT; neuron_id++)
    {
        if (!neuron_recorder_open(
                &recorders[neuron_id],
                source_filenames[neuron_id],
                neuron_id))
        {
            close_source_recorders(recorders);
            return 0;
        }

        if (!neuron_recorder_write_header(&recorders[neuron_id]))
        {
            close_source_recorders(recorders);
            return 0;
        }
    }

    return 1;
}

static int source_file_has_spike(const char *filename, int *has_spike)
{
    FILE *file = fopen(filename, "r");
    char line[256];

    *has_spike = 0;

    if (file == NULL)
        return 0;

    if (fgets(line, sizeof(line), file) == NULL)
    {
        fclose(file);
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL)
    {
        int step;
        int spike;
        double potential;
        double external_current;
        double synaptic_current;

        if (sscanf(
                line,
                "%d,%lf,%d,%lf,%lf",
                &step,
                &potential,
                &spike,
                &external_current,
                &synaptic_current) != 5)
        {
            fclose(file);
            return 0;
        }

        if (spike != 0 && spike != 1)
        {
            fclose(file);
            return 0;
        }

        if (spike)
        {
            *has_spike = 1;
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 1;
}

static int validate_source_spikes(
    const ArchitectureConfig *architecture,
    double probability,
    unsigned int seed,
    char source_filenames[INPUT_SOURCE_COUNT][TEMP_FILENAME_LENGTH])
{
    for (int neuron_id = 0; neuron_id < INPUT_SOURCE_COUNT; neuron_id++)
    {
        int has_spike = 0;

        if (!source_file_has_spike(source_filenames[neuron_id], &has_spike))
        {
            printf(
                "Erro: falha ao validar fonte %d em %s p=%.2f seed=%u.\n",
                neuron_id,
                architecture->name,
                probability,
                seed);
            return 0;
        }

        if (!has_spike)
        {
            printf(
                "Erro: fonte %d nunca disparou em %s p=%.2f seed=%u.\n",
                neuron_id,
                architecture->name,
                probability,
                seed);
            return 0;
        }
    }

    return 1;
}

static int parse_population_metrics(
    const char *filename,
    ActivityMetrics *metrics)
{
    FILE *file = fopen(filename, "r");
    char line[256];
    int rows = 0;

    reset_activity_metrics(metrics);

    if (file == NULL)
        return 0;

    if (fgets(line, sizeof(line), file) == NULL)
    {
        fclose(file);
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL)
    {
        int step;
        int spikes_total;
        int spikes_exc;
        int spikes_inh;
        double mean_potential;
        double mean_syn_current;

        if (sscanf(
                line,
                "%d,%d,%d,%d,%lf,%lf",
                &step,
                &spikes_total,
                &spikes_exc,
                &spikes_inh,
                &mean_potential,
                &mean_syn_current) != 6)
        {
            fclose(file);
            return 0;
        }

        if (step < 0 ||
            step >= EXPERIMENT_STEPS ||
            spikes_total < 0 ||
            spikes_exc < 0 ||
            spikes_inh < 0 ||
            spikes_total != spikes_exc + spikes_inh)
        {
            fclose(file);
            return 0;
        }

        metrics->spikes_exc_total += spikes_exc;
        metrics->spikes_inh_total += spikes_inh;

        if (spikes_exc > 0)
            metrics->last_exc_step = step;

        if (spikes_inh > 0)
            metrics->last_inh_step = step;

        if (step >= LAST_WINDOW_START)
        {
            metrics->spikes_exc_last_100 += spikes_exc;
            metrics->spikes_inh_last_100 += spikes_inh;

            if (spikes_exc > 0)
                metrics->active_exc_timesteps_last_100++;

            if (spikes_inh > 0)
                metrics->active_inh_timesteps_last_100++;
        }

        rows++;
    }

    fclose(file);
    return rows == EXPERIMENT_STEPS;
}

static void close_run_resources(
    Network *net,
    int net_initialized,
    PopulationRecorder *population_recorder,
    NeuronRecorder source_recorders[INPUT_SOURCE_COUNT])
{
    population_recorder_close(population_recorder);
    close_source_recorders(source_recorders);

    if (net_initialized)
        network_destroy(net);
}

static int configure_neuron_types(Network *net)
{
    for (int neuron_id = 0; neuron_id < POPULATION_SIZE; neuron_id++)
    {
        NeuronType type = NEURON_EXCITATORY;

        if (is_inhibitory_neuron(neuron_id))
            type = NEURON_INHIBITORY;

        if (!network_set_neuron_type(net, neuron_id, type))
            return 0;
    }

    return 1;
}

static int should_add_selected_connection(
    const ArchitectureConfig *architecture,
    int source,
    int target)
{
    if (architecture->mode == ARCH_RANDOM_NO_INH_TO_INH &&
        is_inhibitory_neuron(source) &&
        is_inhibitory_neuron(target))
    {
        return 0;
    }

    return 1;
}

static int draw_connection(double probability)
{
    double value = (double)rand() / ((double)RAND_MAX + 1.0);

    return value < probability;
}

static void count_created_connection(
    ConnectionCounts *counts,
    int source,
    int target)
{
    int source_inh = is_inhibitory_neuron(source);
    int target_inh = is_inhibitory_neuron(target);

    counts->total++;

    if (!source_inh && !target_inh)
        counts->exc_exc++;
    else if (!source_inh && target_inh)
        counts->exc_inh++;
    else if (source_inh && !target_inh)
        counts->inh_exc++;
    else
        counts->inh_inh++;
}

static int connect_sparse_network(
    Network *net,
    const ArchitectureConfig *architecture,
    double probability,
    unsigned int seed,
    ConnectionCounts *counts)
{
    reset_connection_counts(counts);
    srand(seed);

    for (int source = 0; source < POPULATION_SIZE; source++)
    {
        double weight = W_EXC;

        if (is_inhibitory_neuron(source))
            weight = TEST_W_INH;

        for (int target = 0; target < POPULATION_SIZE; target++)
        {
            if (source == target)
                continue;

            if (!draw_connection(probability))
                continue;

            if (!should_add_selected_connection(architecture, source, target))
                continue;

            if (!network_connect_delayed(net, source, target, weight, 1))
                return 0;

            count_created_connection(counts, source, target);
        }
    }

    return 1;
}

static int apply_input_currents(Network *net)
{
    network_clear_external_currents(net);

    for (int neuron_id = 0; neuron_id < INPUT_SOURCE_COUNT; neuron_id++)
    {
        if (!network_set_external_current(net, neuron_id, I_EXT))
            return 0;
    }

    return 1;
}

static int run_case(
    const ArchitectureConfig *architecture,
    int architecture_index,
    double probability,
    int probability_index,
    unsigned int seed,
    RunResult *result)
{
    Network net;
    PopulationRecorder population_recorder = {NULL};
    NeuronRecorder source_recorders[INPUT_SOURCE_COUNT];
    char population_filename[TEMP_FILENAME_LENGTH] = "";
    char source_filenames[INPUT_SOURCE_COUNT][TEMP_FILENAME_LENGTH] = {{0}};
    int net_initialized = 0;

    init_source_recorders(source_recorders);
    reset_run_result(result, architecture, probability, seed);

    if (!is_valid_probability(probability))
        return 0;

    if (!make_temp_filenames(
            architecture_index,
            probability_index,
            seed,
            population_filename,
            source_filenames))
    {
        printf("Erro: falha ao criar nomes temporarios.\n");
        return 0;
    }

    if (!network_init(&net, POPULATION_SIZE))
    {
        printf(
            "Erro: falha ao criar rede em %s p=%.2f seed=%u.\n",
            architecture->name,
            probability,
            seed);
        remove_temp_files(population_filename, source_filenames);
        return 0;
    }

    net_initialized = 1;

    if (!configure_neuron_types(&net))
    {
        printf(
            "Erro: falha ao configurar tipos em %s p=%.2f seed=%u.\n",
            architecture->name,
            probability,
            seed);
        close_run_resources(
            &net,
            net_initialized,
            &population_recorder,
            source_recorders);
        remove_temp_files(population_filename, source_filenames);
        return 0;
    }

    if (!connect_sparse_network(
            &net,
            architecture,
            probability,
            seed,
            &result->connections))
    {
        printf(
            "Erro: falha ao criar conexoes em %s p=%.2f seed=%u.\n",
            architecture->name,
            probability,
            seed);
        close_run_resources(
            &net,
            net_initialized,
            &population_recorder,
            source_recorders);
        remove_temp_files(population_filename, source_filenames);
        return 0;
    }

    if (!population_recorder_open(&population_recorder, population_filename))
    {
        printf(
            "Erro: falha ao criar recorder de populacao em %s p=%.2f seed=%u.\n",
            architecture->name,
            probability,
            seed);
        close_run_resources(
            &net,
            net_initialized,
            &population_recorder,
            source_recorders);
        remove_temp_files(population_filename, source_filenames);
        return 0;
    }

    if (!population_recorder_write_header(&population_recorder))
    {
        printf(
            "Erro: falha ao escrever cabecalho de populacao em %s p=%.2f seed=%u.\n",
            architecture->name,
            probability,
            seed);
        close_run_resources(
            &net,
            net_initialized,
            &population_recorder,
            source_recorders);
        remove_temp_files(population_filename, source_filenames);
        return 0;
    }

    if (!open_source_recorders(source_recorders, source_filenames))
    {
        printf(
            "Erro: falha ao criar recorders de fonte em %s p=%.2f seed=%u.\n",
            architecture->name,
            probability,
            seed);
        close_run_resources(
            &net,
            net_initialized,
            &population_recorder,
            source_recorders);
        remove_temp_files(population_filename, source_filenames);
        return 0;
    }

    for (int step = 0; step < EXPERIMENT_STEPS; step++)
    {
        int step_spikes;

        if (!apply_input_currents(&net))
        {
            printf(
                "Erro: falha ao aplicar estimulo em %s p=%.2f seed=%u.\n",
                architecture->name,
                probability,
                seed);
            close_run_resources(
                &net,
                net_initialized,
                &population_recorder,
                source_recorders);
            remove_temp_files(population_filename, source_filenames);
            return 0;
        }

        step_spikes = network_update(&net);

        if (step_spikes < 0)
        {
            printf(
                "Erro: atividade invalida em %s p=%.2f seed=%u.\n",
                architecture->name,
                probability,
                seed);
            close_run_resources(
                &net,
                net_initialized,
                &population_recorder,
                source_recorders);
            remove_temp_files(population_filename, source_filenames);
            return 0;
        }

        if (!population_recorder_record(&population_recorder, &net, step))
        {
            printf(
                "Erro: falha ao registrar populacao em %s p=%.2f seed=%u.\n",
                architecture->name,
                probability,
                seed);
            close_run_resources(
                &net,
                net_initialized,
                &population_recorder,
                source_recorders);
            remove_temp_files(population_filename, source_filenames);
            return 0;
        }

        for (int neuron_id = 0; neuron_id < INPUT_SOURCE_COUNT; neuron_id++)
        {
            if (!neuron_recorder_record(
                    &source_recorders[neuron_id],
                    &net,
                    step))
            {
                printf(
                    "Erro: falha ao registrar fonte %d em %s p=%.2f seed=%u.\n",
                    neuron_id,
                    architecture->name,
                    probability,
                    seed);
                close_run_resources(
                    &net,
                    net_initialized,
                    &population_recorder,
                    source_recorders);
                remove_temp_files(population_filename, source_filenames);
                return 0;
            }
        }
    }

    close_run_resources(
        &net,
        net_initialized,
        &population_recorder,
        source_recorders);

    if (!parse_population_metrics(population_filename, &result->activity))
    {
        printf(
            "Erro: falha ao ler metricas de populacao em %s p=%.2f seed=%u.\n",
            architecture->name,
            probability,
            seed);
        remove_temp_files(population_filename, source_filenames);
        return 0;
    }

    if (!validate_source_spikes(
            architecture,
            probability,
            seed,
            source_filenames))
    {
        remove_temp_files(population_filename, source_filenames);
        return 0;
    }

    remove_temp_files(population_filename, source_filenames);
    return 1;
}

static int write_run_row(FILE *csv, const RunResult *result)
{
    if (fprintf(
            csv,
            "%s,%.2f,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
            result->architecture,
            result->probability,
            result->seed,
            result->connections.total,
            result->connections.exc_exc,
            result->connections.exc_inh,
            result->connections.inh_exc,
            result->connections.inh_inh,
            result->activity.spikes_exc_total,
            result->activity.spikes_inh_total,
            result->activity.spikes_exc_last_100,
            result->activity.spikes_inh_last_100,
            result->activity.active_exc_timesteps_last_100,
            result->activity.active_inh_timesteps_last_100,
            result->activity.last_exc_step,
            result->activity.last_inh_step) < 0)
    {
        return 0;
    }

    return 1;
}

static void reset_summary(
    SummaryResult *summary,
    const ArchitectureConfig *architecture,
    double probability)
{
    summary->architecture = architecture->name;
    summary->probability = probability;
    summary->executions = 0;
    summary->exc_inh_connection_sum = 0.0;
    summary->inh_exc_connection_sum = 0.0;
    summary->inh_inh_connection_sum = 0.0;
    summary->exc_spike_total_sum = 0.0;
    summary->inh_spike_total_sum = 0.0;
    summary->exc_spike_last_100_sum = 0.0;
    summary->inh_spike_last_100_sum = 0.0;
    summary->exc_active_timestep_last_100_sum = 0.0;
    summary->inh_active_timestep_last_100_sum = 0.0;
    summary->cases_with_exc_last_100 = 0;
    summary->cases_with_inh_last_100 = 0;
}

static void update_summary(SummaryResult *summary, const RunResult *result)
{
    summary->executions++;
    summary->exc_inh_connection_sum += result->connections.exc_inh;
    summary->inh_exc_connection_sum += result->connections.inh_exc;
    summary->inh_inh_connection_sum += result->connections.inh_inh;
    summary->exc_spike_total_sum += result->activity.spikes_exc_total;
    summary->inh_spike_total_sum += result->activity.spikes_inh_total;
    summary->exc_spike_last_100_sum += result->activity.spikes_exc_last_100;
    summary->inh_spike_last_100_sum += result->activity.spikes_inh_last_100;
    summary->exc_active_timestep_last_100_sum +=
        result->activity.active_exc_timesteps_last_100;
    summary->inh_active_timestep_last_100_sum +=
        result->activity.active_inh_timesteps_last_100;

    if (result->activity.spikes_exc_last_100 > 0)
        summary->cases_with_exc_last_100++;

    if (result->activity.spikes_inh_last_100 > 0)
        summary->cases_with_inh_last_100++;
}

static double average(double value_sum, int count)
{
    if (count <= 0)
        return 0.0;

    return value_sum / (double)count;
}

static int write_summary_row(FILE *csv, const SummaryResult *summary)
{
    if (fprintf(
            csv,
            "%s,%.2f,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d\n",
            summary->architecture,
            summary->probability,
            summary->executions,
            average(summary->exc_inh_connection_sum, summary->executions),
            average(summary->inh_exc_connection_sum, summary->executions),
            average(summary->inh_inh_connection_sum, summary->executions),
            average(summary->exc_spike_total_sum, summary->executions),
            average(summary->inh_spike_total_sum, summary->executions),
            average(summary->exc_spike_last_100_sum, summary->executions),
            average(summary->inh_spike_last_100_sum, summary->executions),
            average(
                summary->exc_active_timestep_last_100_sum,
                summary->executions),
            average(
                summary->inh_active_timestep_last_100_sum,
                summary->executions),
            summary->cases_with_exc_last_100,
            summary->cases_with_inh_last_100) < 0)
    {
        return 0;
    }

    return 1;
}

static void print_summary_row(const SummaryResult *summary)
{
    printf(
        "%22s | %.2f | %9.2f | %9.2f | %15.2f | %15.2f | %16d | %16d\n",
        summary->architecture,
        summary->probability,
        average(summary->exc_spike_total_sum, summary->executions),
        average(summary->inh_spike_total_sum, summary->executions),
        average(summary->exc_spike_last_100_sum, summary->executions),
        average(summary->inh_spike_last_100_sum, summary->executions),
        summary->cases_with_exc_last_100,
        summary->cases_with_inh_last_100);
}

static void print_p_comparison(
    SummaryResult summaries[ARCHITECTURE_COUNT][PROBABILITY_COUNT])
{
    printf("\n");
    printf("Descricao baseada nas metricas:\n");

    for (int architecture_index = 0;
         architecture_index < ARCHITECTURE_COUNT;
         architecture_index++)
    {
        const SummaryResult *p025 = &summaries[architecture_index][0];
        const SummaryResult *p050 = &summaries[architecture_index][1];

        printf("- %s: p=0.25 teve media EXC total %.2f e INH total %.2f; "
               "p=0.50 teve media EXC total %.2f e INH total %.2f.\n",
               p025->architecture,
               average(p025->exc_spike_total_sum, p025->executions),
               average(p025->inh_spike_total_sum, p025->executions),
               average(p050->exc_spike_total_sum, p050->executions),
               average(p050->inh_spike_total_sum, p050->executions));
        printf("  Nos ultimos 100 passos, EXC permaneceu ativo em %d/%d "
               "casos para p=0.25 e %d/%d para p=0.50; INH permaneceu "
               "ativo em %d/%d e %d/%d casos.\n",
               p025->cases_with_exc_last_100,
               p025->executions,
               p050->cases_with_exc_last_100,
               p050->executions,
               p025->cases_with_inh_last_100,
               p025->executions,
               p050->cases_with_inh_last_100,
               p050->executions);
    }

    printf("- Esses numeros descrevem os regimes observados nestas execucoes; "
           "nao estabelecem causalidade definitiva.\n");
}

int main(void)
{
    ArchitectureConfig architectures[ARCHITECTURE_COUNT] =
    {
        {"random_all_connections", ARCH_RANDOM_ALL_CONNECTIONS},
        {"random_no_inh_to_inh", ARCH_RANDOM_NO_INH_TO_INH}
    };
    double probabilities[PROBABILITY_COUNT] = {0.25, 0.50, 0.75, 1.00};
    unsigned int seeds[SEED_COUNT] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    SummaryResult summaries[ARCHITECTURE_COUNT][PROBABILITY_COUNT];
    FILE *runs_csv = fopen("results/experiments/sparse_ei/sparse_ei_balance_runs.csv", "w");
    FILE *summary_csv = NULL;

    if (runs_csv == NULL)
    {
        printf("Erro: nao foi possivel criar results/experiments/sparse_ei/sparse_ei_balance_runs.csv.\n");
        return 1;
    }

    summary_csv = fopen("results/experiments/sparse_ei/sparse_ei_balance_summary.csv", "w");

    if (summary_csv == NULL)
    {
        printf("Erro: nao foi possivel criar results/experiments/sparse_ei/sparse_ei_balance_summary.csv.\n");
        fclose(runs_csv);
        return 1;
    }

    if (fprintf(
            runs_csv,
            "arquitetura,probabilidade,seed,conexoes_total,"
            "conexoes_exc_exc,conexoes_exc_inh,conexoes_inh_exc,"
            "conexoes_inh_inh,spikes_exc_total,spikes_inh_total,"
            "spikes_exc_ultimos_100,spikes_inh_ultimos_100,"
            "timesteps_exc_ativos_ultimos_100,"
            "timesteps_inh_ativos_ultimos_100,ultimo_timestep_exc,"
            "ultimo_timestep_inh\n") < 0)
    {
        printf("Erro: nao foi possivel escrever cabecalho do CSV detalhado.\n");
        fclose(runs_csv);
        fclose(summary_csv);
        return 1;
    }

    if (fprintf(
            summary_csv,
            "arquitetura,probabilidade,execucoes,media_conexoes_exc_inh,"
            "media_conexoes_inh_exc,media_conexoes_inh_inh,"
            "media_spikes_exc_total,media_spikes_inh_total,"
            "media_spikes_exc_ultimos_100,media_spikes_inh_ultimos_100,"
            "media_timesteps_exc_ativos_ultimos_100,"
            "media_timesteps_inh_ativos_ultimos_100,"
            "casos_com_exc_ativos_ultimos_100,"
            "casos_com_inh_ativos_ultimos_100\n") < 0)
    {
        printf("Erro: nao foi possivel escrever cabecalho do CSV resumo.\n");
        fclose(runs_csv);
        fclose(summary_csv);
        return 1;
    }

    printf("=== Atividade EXC/INH em redes esparsas ===\n\n");
    printf(
        "arquitetura | p | EXC total | INH total | EXC ultimos 100 | "
        "INH ultimos 100 | casos EXC tardios | casos INH tardios\n");

    for (int architecture_index = 0;
         architecture_index < ARCHITECTURE_COUNT;
         architecture_index++)
    {
        for (int probability_index = 0;
             probability_index < PROBABILITY_COUNT;
             probability_index++)
        {
            SummaryResult *summary =
                &summaries[architecture_index][probability_index];

            reset_summary(
                summary,
                &architectures[architecture_index],
                probabilities[probability_index]);

            for (int seed_index = 0; seed_index < SEED_COUNT; seed_index++)
            {
                RunResult result;

                if (!run_case(
                        &architectures[architecture_index],
                        architecture_index,
                        probabilities[probability_index],
                        probability_index,
                        seeds[seed_index],
                        &result))
                {
                    fclose(runs_csv);
                    fclose(summary_csv);
                    return 1;
                }

                if (!write_run_row(runs_csv, &result))
                {
                    printf("Erro: nao foi possivel escrever linha detalhada.\n");
                    fclose(runs_csv);
                    fclose(summary_csv);
                    return 1;
                }

                update_summary(summary, &result);
            }

            if (!write_summary_row(summary_csv, summary))
            {
                printf("Erro: nao foi possivel escrever linha de resumo.\n");
                fclose(runs_csv);
                fclose(summary_csv);
                return 1;
            }

            print_summary_row(summary);
        }
    }

    fclose(runs_csv);
    fclose(summary_csv);

    print_p_comparison(summaries);
    return 0;
}
