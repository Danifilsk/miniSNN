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
    const char *architecture;
    double probability;
    unsigned int seed;
    int connections;
    int total_spikes;
    int peak_spikes;
    int active_timesteps;
    int last_active_step;
    int first_full_sync_step;
    int sustained_activity;
} RunResult;

typedef struct
{
    const char *architecture;
    double probability;
    int executions;
    double connection_sum;
    double total_spike_sum;
    double peak_spike_sum;
    double active_timestep_sum;
    int sustained_cases;
    int full_sync_cases;
} SummaryResult;

static int is_inhibitory_neuron(int neuron_id)
{
    return neuron_id >= POPULATION_SIZE - INHIBITORY_COUNT;
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

static int open_source_recorders(NeuronRecorder recorders[INPUT_SOURCE_COUNT])
{
    init_source_recorders(recorders);

    for (int neuron_id = 0; neuron_id < INPUT_SOURCE_COUNT; neuron_id++)
    {
        recorders[neuron_id].file = tmpfile();
        recorders[neuron_id].neuron_id = neuron_id;

        if (recorders[neuron_id].file == NULL)
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

static int source_recorder_has_spike(NeuronRecorder *recorder, int *has_spike)
{
    char line[256];

    *has_spike = 0;

    if (recorder == NULL || recorder->file == NULL)
        return 0;

    if (fflush(recorder->file) != 0)
        return 0;

    rewind(recorder->file);

    if (fgets(line, sizeof(line), recorder->file) == NULL)
        return 0;

    while (fgets(line, sizeof(line), recorder->file) != NULL)
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
            return 0;
        }

        if (spike != 0 && spike != 1)
            return 0;

        if (spike)
        {
            *has_spike = 1;
            return 1;
        }
    }

    return 1;
}

static int validate_sources_spiked(
    const ArchitectureConfig *architecture,
    double probability,
    unsigned int seed,
    NeuronRecorder recorders[INPUT_SOURCE_COUNT])
{
    for (int neuron_id = 0; neuron_id < INPUT_SOURCE_COUNT; neuron_id++)
    {
        int has_spike = 0;

        if (!source_recorder_has_spike(&recorders[neuron_id], &has_spike))
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

static void close_run_resources(
    Network *net,
    int net_initialized,
    NeuronRecorder recorders[INPUT_SOURCE_COUNT])
{
    close_source_recorders(recorders);

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

static int connect_sparse_network(
    Network *net,
    const ArchitectureConfig *architecture,
    double probability,
    unsigned int seed,
    int *connection_count)
{
    *connection_count = 0;
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

            (*connection_count)++;
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

static void reset_run_result(
    RunResult *result,
    const ArchitectureConfig *architecture,
    double probability,
    unsigned int seed)
{
    result->architecture = architecture->name;
    result->probability = probability;
    result->seed = seed;
    result->connections = 0;
    result->total_spikes = 0;
    result->peak_spikes = 0;
    result->active_timesteps = 0;
    result->last_active_step = -1;
    result->first_full_sync_step = -1;
    result->sustained_activity = 0;
}

static void update_run_metrics(
    RunResult *result,
    int step,
    int step_spikes)
{
    result->total_spikes += step_spikes;

    if (step_spikes > result->peak_spikes)
        result->peak_spikes = step_spikes;

    if (step_spikes > 0)
    {
        result->active_timesteps++;
        result->last_active_step = step;
    }

    if (result->first_full_sync_step < 0 &&
        step_spikes == POPULATION_SIZE)
    {
        result->first_full_sync_step = step;
    }
}

static int run_case(
    const ArchitectureConfig *architecture,
    double probability,
    unsigned int seed,
    RunResult *result)
{
    Network net;
    NeuronRecorder source_recorders[INPUT_SOURCE_COUNT];
    int net_initialized = 0;

    init_source_recorders(source_recorders);
    reset_run_result(result, architecture, probability, seed);

    if (probability < 0.0 || probability > 1.0)
        return 0;

    if (!network_init(&net, POPULATION_SIZE))
    {
        printf(
            "Erro: falha ao criar rede em %s p=%.2f seed=%u.\n",
            architecture->name,
            probability,
            seed);
        return 0;
    }

    net_initialized = 1;

    if (!open_source_recorders(source_recorders))
    {
        printf(
            "Erro: falha ao criar recorders temporarios em %s p=%.2f seed=%u.\n",
            architecture->name,
            probability,
            seed);
        close_run_resources(&net, net_initialized, source_recorders);
        return 0;
    }

    if (!configure_neuron_types(&net))
    {
        printf(
            "Erro: falha ao configurar tipos em %s p=%.2f seed=%u.\n",
            architecture->name,
            probability,
            seed);
        close_run_resources(&net, net_initialized, source_recorders);
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
        close_run_resources(&net, net_initialized, source_recorders);
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
            close_run_resources(&net, net_initialized, source_recorders);
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
            close_run_resources(&net, net_initialized, source_recorders);
            return 0;
        }

        update_run_metrics(result, step, step_spikes);

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
                close_run_resources(&net, net_initialized, source_recorders);
                return 0;
            }
        }
    }

    result->sustained_activity = result->last_active_step >= 900 ? 1 : 0;

    if (!validate_sources_spiked(
            architecture,
            probability,
            seed,
            source_recorders))
    {
        close_run_resources(&net, net_initialized, source_recorders);
        return 0;
    }

    close_run_resources(&net, net_initialized, source_recorders);
    return 1;
}

static int write_raw_row(FILE *csv, const RunResult *result)
{
    if (fprintf(
            csv,
            "%s,%.2f,%u,%d,%d,%d,%d,%d,%d,%d\n",
            result->architecture,
            result->probability,
            result->seed,
            result->connections,
            result->total_spikes,
            result->peak_spikes,
            result->active_timesteps,
            result->last_active_step,
            result->first_full_sync_step,
            result->sustained_activity) < 0)
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
    summary->connection_sum = 0.0;
    summary->total_spike_sum = 0.0;
    summary->peak_spike_sum = 0.0;
    summary->active_timestep_sum = 0.0;
    summary->sustained_cases = 0;
    summary->full_sync_cases = 0;
}

static void update_summary(SummaryResult *summary, const RunResult *result)
{
    summary->executions++;
    summary->connection_sum += result->connections;
    summary->total_spike_sum += result->total_spikes;
    summary->peak_spike_sum += result->peak_spikes;
    summary->active_timestep_sum += result->active_timesteps;

    if (result->sustained_activity)
        summary->sustained_cases++;

    if (result->first_full_sync_step >= 0)
        summary->full_sync_cases++;
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
            "%s,%.2f,%d,%.2f,%.2f,%.2f,%.2f,%d,%d\n",
            summary->architecture,
            summary->probability,
            summary->executions,
            average(summary->connection_sum, summary->executions),
            average(summary->total_spike_sum, summary->executions),
            average(summary->peak_spike_sum, summary->executions),
            average(summary->active_timestep_sum, summary->executions),
            summary->sustained_cases,
            summary->full_sync_cases) < 0)
    {
        return 0;
    }

    return 1;
}

static void print_summary_row(const SummaryResult *summary)
{
    printf(
        "%22s | %.2f | %13.2f | %11d | %19d\n",
        summary->architecture,
        summary->probability,
        average(summary->total_spike_sum, summary->executions),
        summary->sustained_cases,
        summary->full_sync_cases);
}

static void print_architecture_description(
    SummaryResult summaries[ARCHITECTURE_COUNT][PROBABILITY_COUNT],
    int architecture_index)
{
    int sustained_cases = 0;
    int full_sync_cases = 0;
    double highest_mean_spikes = -1.0;
    double highest_mean_probability = 0.0;

    for (int i = 0; i < PROBABILITY_COUNT; i++)
    {
        const SummaryResult *summary = &summaries[architecture_index][i];
        double mean_spikes =
            average(summary->total_spike_sum, summary->executions);

        sustained_cases += summary->sustained_cases;
        full_sync_cases += summary->full_sync_cases;

        if (mean_spikes > highest_mean_spikes)
        {
            highest_mean_spikes = mean_spikes;
            highest_mean_probability = summary->probability;
        }
    }

    printf("%s:\n", summaries[architecture_index][0].architecture);
    printf("  casos sustentados: %d de %d\n",
           sustained_cases,
           PROBABILITY_COUNT * SEED_COUNT);
    printf("  casos com sincronizacao total: %d de %d\n",
           full_sync_cases,
           PROBABILITY_COUNT * SEED_COUNT);
    printf("  maior media de spikes: %.2f em p=%.2f\n",
           highest_mean_spikes,
           highest_mean_probability);
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
    FILE *raw_csv = fopen("results/experiments/sparse_ei/sparse_ei_robustness.csv", "w");
    FILE *summary_csv = NULL;

    if (raw_csv == NULL)
    {
        printf("Erro: nao foi possivel criar results/experiments/sparse_ei/sparse_ei_robustness.csv.\n");
        return 1;
    }

    summary_csv = fopen("results/experiments/sparse_ei/sparse_ei_robustness_summary.csv", "w");

    if (summary_csv == NULL)
    {
        printf(
            "Erro: nao foi possivel criar results/experiments/sparse_ei/sparse_ei_robustness_summary.csv.\n");
        fclose(raw_csv);
        return 1;
    }

    if (fprintf(
            raw_csv,
            "arquitetura,probabilidade,seed,conexoes,spikes_totais,"
            "pico_spikes,timesteps_ativos,ultimo_timestep_ativo,"
            "primeiro_timestep_sincronizacao_total,"
            "atividade_sustentada\n") < 0)
    {
        printf("Erro: nao foi possivel escrever cabecalho do CSV bruto.\n");
        fclose(raw_csv);
        fclose(summary_csv);
        return 1;
    }

    if (fprintf(
            summary_csv,
            "arquitetura,probabilidade,execucoes,media_conexoes,"
            "media_spikes_totais,media_pico_spikes,"
            "media_timesteps_ativos,casos_sustentados,"
            "casos_com_sincronizacao_total\n") < 0)
    {
        printf("Erro: nao foi possivel escrever cabecalho do CSV resumo.\n");
        fclose(raw_csv);
        fclose(summary_csv);
        return 1;
    }

    printf("=== Robustez EXC/INH em redes esparsas ===\n\n");
    printf(
        "arquitetura | p | media_spikes | sustentadas | "
        "sincronizacao_total\n");

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
                        probabilities[probability_index],
                        seeds[seed_index],
                        &result))
                {
                    fclose(raw_csv);
                    fclose(summary_csv);
                    return 1;
                }

                if (!write_raw_row(raw_csv, &result))
                {
                    printf("Erro: nao foi possivel escrever linha bruta.\n");
                    fclose(raw_csv);
                    fclose(summary_csv);
                    return 1;
                }

                update_summary(summary, &result);
            }

            if (!write_summary_row(summary_csv, summary))
            {
                printf("Erro: nao foi possivel escrever linha de resumo.\n");
                fclose(raw_csv);
                fclose(summary_csv);
                return 1;
            }

            print_summary_row(summary);
        }
    }

    fclose(raw_csv);
    fclose(summary_csv);

    printf("\n");
    print_architecture_description(summaries, ARCH_RANDOM_ALL_CONNECTIONS);
    printf("\n");
    print_architecture_description(summaries, ARCH_RANDOM_NO_INH_TO_INH);

    return 0;
}
