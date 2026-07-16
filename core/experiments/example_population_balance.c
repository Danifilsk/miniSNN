#include <stdio.h>

#include "config.h"
#include "network.h"
#include "recorder.h"

#define POPULATION_SIZE 20
#define EXPERIMENT_STEPS 1000
#define INHIBITORY_COUNT 4
#define INPUT_SOURCE_COUNT 3

typedef struct
{
    int total_spikes;
    int max_spikes_per_step;
    int active_timesteps;
    int first_population_recruitment_step;
    int source_spiked[INPUT_SOURCE_COUNT];
    double mean_spikes_total;
} PopulationExperimentResult;

static void reset_result(PopulationExperimentResult *result)
{
    result->total_spikes = 0;
    result->max_spikes_per_step = 0;
    result->active_timesteps = 0;
    result->first_population_recruitment_step = -1;
    result->mean_spikes_total = 0.0;

    for (int i = 0; i < INPUT_SOURCE_COUNT; i++)
        result->source_spiked[i] = 0;
}

static void close_resources(
    Network *net,
    PopulationRecorder *population_recorder,
    FILE *spike_file,
    int net_initialized)
{
    population_recorder_close(population_recorder);

    if (spike_file != NULL)
        fclose(spike_file);

    if (net_initialized)
        network_destroy(net);
}

static int configure_neuron_types(Network *net, int balanced)
{
    for (int i = 0; i < POPULATION_SIZE; i++)
    {
        NeuronType type = NEURON_EXCITATORY;

        if (balanced && i >= POPULATION_SIZE - INHIBITORY_COUNT)
            type = NEURON_INHIBITORY;

        if (!network_set_neuron_type(net, i, type))
            return 0;
    }

    return 1;
}

static int connect_all_to_all(Network *net, int balanced)
{
    for (int source = 0; source < POPULATION_SIZE; source++)
    {
        double weight = W_EXC;

        if (balanced && source >= POPULATION_SIZE - INHIBITORY_COUNT)
            weight = W_INH;

        for (int target = 0; target < POPULATION_SIZE; target++)
        {
            if (target == source)
                continue;

            if (!network_connect_delayed(net, source, target, weight, 1))
                return 0;
        }
    }

    return 1;
}

static int apply_input_currents(Network *net)
{
    network_clear_external_currents(net);

    for (int i = 0; i < INPUT_SOURCE_COUNT; i++)
    {
        if (!network_set_external_current(net, i, I_EXT))
            return 0;
    }

    return 1;
}

static int record_spikes(FILE *spike_file, const Network *net, int step)
{
    for (int neuron_id = 0; neuron_id < POPULATION_SIZE; neuron_id++)
    {
        if (net->spikes[neuron_id])
        {
            if (fprintf(spike_file, "%d,%d\n", step, neuron_id) < 0)
                return 0;
        }
    }

    return 1;
}

static int run_scenario(
    const char *label,
    int balanced,
    const char *spikes_filename,
    const char *metrics_filename,
    PopulationExperimentResult *result)
{
    Network net;
    PopulationRecorder population_recorder = {NULL};
    FILE *spike_file = NULL;
    int net_initialized = 0;

    reset_result(result);

    if (!network_init(&net, POPULATION_SIZE))
    {
        printf("Erro: falha ao criar rede do cenario %s.\n", label);
        return 0;
    }

    net_initialized = 1;

    if (!configure_neuron_types(&net, balanced))
    {
        printf("Erro: falha ao configurar tipos no cenario %s.\n", label);
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    if (!connect_all_to_all(&net, balanced))
    {
        printf("Erro: falha ao criar conexoes no cenario %s.\n", label);
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    spike_file = fopen(spikes_filename, "w");

    if (spike_file == NULL)
    {
        printf("Erro: falha ao criar raster no cenario %s.\n", label);
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    if (fprintf(spike_file, "tempo,neuronio\n") < 0)
    {
        printf("Erro: falha ao escrever cabecalho do raster no cenario %s.\n",
               label);
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    if (!population_recorder_open(&population_recorder, metrics_filename))
    {
        printf("Erro: falha ao criar metricas no cenario %s.\n", label);
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    if (!population_recorder_write_header(&population_recorder))
    {
        printf("Erro: falha ao escrever cabecalho de metricas no cenario %s.\n",
               label);
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    for (int step = 0; step < EXPERIMENT_STEPS; step++)
    {
        int step_spikes;

        if (!apply_input_currents(&net))
        {
            printf("Erro: falha ao aplicar estimulo no cenario %s.\n", label);
            close_resources(
                &net,
                &population_recorder,
                spike_file,
                net_initialized);
            return 0;
        }

        step_spikes = network_update(&net);

        if (step_spikes < 0)
        {
            printf("Erro: atividade invalida no cenario %s.\n", label);
            close_resources(
                &net,
                &population_recorder,
                spike_file,
                net_initialized);
            return 0;
        }

        result->total_spikes += step_spikes;

        if (step_spikes > result->max_spikes_per_step)
            result->max_spikes_per_step = step_spikes;

        if (step_spikes > 0)
            result->active_timesteps++;

        for (int i = 0; i < INPUT_SOURCE_COUNT; i++)
        {
            if (net.spikes[i])
                result->source_spiked[i] = 1;
        }

        if (result->first_population_recruitment_step < 0)
        {
            for (int neuron_id = INPUT_SOURCE_COUNT;
                 neuron_id < POPULATION_SIZE;
                 neuron_id++)
            {
                if (net.spikes[neuron_id])
                {
                    result->first_population_recruitment_step = step;
                    break;
                }
            }
        }

        if (!record_spikes(spike_file, &net, step))
        {
            printf("Erro: falha ao registrar raster no cenario %s.\n", label);
            close_resources(
                &net,
                &population_recorder,
                spike_file,
                net_initialized);
            return 0;
        }

        if (!population_recorder_record(&population_recorder, &net, step))
        {
            printf("Erro: falha ao registrar metricas no cenario %s.\n", label);
            close_resources(
                &net,
                &population_recorder,
                spike_file,
                net_initialized);
            return 0;
        }
    }

    for (int i = 0; i < INPUT_SOURCE_COUNT; i++)
    {
        if (!result->source_spiked[i])
        {
            printf("Erro: fonte %d nunca disparou no cenario %s.\n",
                   i,
                   label);
            close_resources(
                &net,
                &population_recorder,
                spike_file,
                net_initialized);
            return 0;
        }
    }

    if (result->first_population_recruitment_step < 0)
    {
        printf("Erro: populacao nao foi recrutada no cenario %s.\n", label);
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    result->mean_spikes_total =
        (double)result->total_spikes / (double)EXPERIMENT_STEPS;

    close_resources(&net, &population_recorder, spike_file, net_initialized);
    return 1;
}

static void print_result(
    const char *label,
    const PopulationExperimentResult *result)
{
    printf("%s:\n", label);
    printf("  spikes totais: %d\n", result->total_spikes);
    printf("  maior atividade por timestep: %d\n",
           result->max_spikes_per_step);
    printf("  timesteps ativos: %d\n", result->active_timesteps);
    printf("  media de spikes_total: %.2f\n", result->mean_spikes_total);
    printf("  primeiro recrutamento da populacao: %d\n",
           result->first_population_recruitment_step);
}

int main(void)
{
    PopulationExperimentResult exc_only;
    PopulationExperimentResult balanced;

    if (!run_scenario(
            "EXC-only",
            0,
            "results/experiments/population_balance/population_exc_only_spikes.csv",
            "results/experiments/population_balance/population_exc_only_metrics.csv",
            &exc_only))
    {
        return 1;
    }

    if (!run_scenario(
            "EXC/INH",
            1,
            "results/experiments/population_balance/population_balanced_spikes.csv",
            "results/experiments/population_balance/population_balanced_metrics.csv",
            &balanced))
    {
        return 1;
    }

    printf("=== Experimento de atividade populacional ===\n\n");
    print_result("EXC-only", &exc_only);
    printf("\n");
    print_result("EXC/INH", &balanced);

    return 0;
}
