#include <stdio.h>

#include "config.h"
#include "network.h"

#define POPULATION_SIZE 20
#define INHIBITORY_COUNT 4
#define EXPERIMENT_STEPS 1000
#define INPUT_SOURCE_COUNT 3

typedef struct
{
    double inhibitory_weight;
    int total_spikes;
    int peak_spikes;
    int active_timesteps;
    int first_recruitment_step;
    int active_neurons;
    int source_spiked[INPUT_SOURCE_COUNT];
    int neuron_spiked[POPULATION_SIZE];
    double mean_spikes;
} SweepResult;

static void reset_result(SweepResult *result, double inhibitory_weight)
{
    result->inhibitory_weight = inhibitory_weight;
    result->total_spikes = 0;
    result->peak_spikes = 0;
    result->active_timesteps = 0;
    result->first_recruitment_step = -1;
    result->active_neurons = 0;
    result->mean_spikes = 0.0;

    for (int i = 0; i < INPUT_SOURCE_COUNT; i++)
        result->source_spiked[i] = 0;

    for (int i = 0; i < POPULATION_SIZE; i++)
        result->neuron_spiked[i] = 0;
}

static int configure_neuron_types(Network *net)
{
    for (int i = 0; i < POPULATION_SIZE; i++)
    {
        NeuronType type = NEURON_EXCITATORY;

        if (i >= POPULATION_SIZE - INHIBITORY_COUNT)
            type = NEURON_INHIBITORY;

        if (!network_set_neuron_type(net, i, type))
            return 0;
    }

    return 1;
}

static int connect_all_to_all(Network *net, double inhibitory_weight)
{
    for (int source = 0; source < POPULATION_SIZE; source++)
    {
        double weight = W_EXC;

        if (source >= POPULATION_SIZE - INHIBITORY_COUNT)
            weight = inhibitory_weight;

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

static void update_spike_metrics(
    const Network *net,
    int step,
    int step_spikes,
    SweepResult *result)
{
    result->total_spikes += step_spikes;

    if (step_spikes > result->peak_spikes)
        result->peak_spikes = step_spikes;

    if (step_spikes > 0)
        result->active_timesteps++;

    for (int i = 0; i < INPUT_SOURCE_COUNT; i++)
    {
        if (net->spikes[i])
            result->source_spiked[i] = 1;
    }

    for (int neuron_id = 0; neuron_id < POPULATION_SIZE; neuron_id++)
    {
        if (net->spikes[neuron_id])
            result->neuron_spiked[neuron_id] = 1;
    }

    if (result->first_recruitment_step < 0)
    {
        for (int neuron_id = INPUT_SOURCE_COUNT;
             neuron_id < POPULATION_SIZE;
             neuron_id++)
        {
            if (net->spikes[neuron_id])
            {
                result->first_recruitment_step = step;
                break;
            }
        }
    }
}

static int finalize_result(SweepResult *result)
{
    for (int i = 0; i < INPUT_SOURCE_COUNT; i++)
    {
        if (!result->source_spiked[i])
        {
            printf("Erro: fonte %d nunca disparou para peso %.2f.\n",
                   i,
                   result->inhibitory_weight);
            return 0;
        }
    }

    for (int neuron_id = 0; neuron_id < POPULATION_SIZE; neuron_id++)
    {
        if (result->neuron_spiked[neuron_id])
            result->active_neurons++;
    }

    result->mean_spikes =
        (double)result->total_spikes / (double)EXPERIMENT_STEPS;

    return 1;
}

static int run_sweep_case(double inhibitory_weight, SweepResult *result)
{
    Network net;

    reset_result(result, inhibitory_weight);

    if (!network_init(&net, POPULATION_SIZE))
    {
        printf("Erro: falha ao criar rede para peso %.2f.\n",
               inhibitory_weight);
        return 0;
    }

    if (!configure_neuron_types(&net))
    {
        printf("Erro: falha ao configurar tipos para peso %.2f.\n",
               inhibitory_weight);
        network_destroy(&net);
        return 0;
    }

    if (!connect_all_to_all(&net, inhibitory_weight))
    {
        printf("Erro: falha ao criar conexoes para peso %.2f.\n",
               inhibitory_weight);
        network_destroy(&net);
        return 0;
    }

    for (int step = 0; step < EXPERIMENT_STEPS; step++)
    {
        int step_spikes;

        if (!apply_input_currents(&net))
        {
            printf("Erro: falha ao aplicar estimulo para peso %.2f.\n",
                   inhibitory_weight);
            network_destroy(&net);
            return 0;
        }

        step_spikes = network_update(&net);

        update_spike_metrics(&net, step, step_spikes, result);
    }

    network_destroy(&net);

    return finalize_result(result);
}

static int write_csv_row(FILE *csv, const SweepResult *result)
{
    if (fprintf(
            csv,
            "%.2f,%d,%d,%d,%.2f,%d,%d\n",
            result->inhibitory_weight,
            result->total_spikes,
            result->peak_spikes,
            result->active_timesteps,
            result->mean_spikes,
            result->first_recruitment_step,
            result->active_neurons) < 0)
    {
        return 0;
    }

    return 1;
}

int main(void)
{
    double inhibitory_weights[] =
    {
        -250.0,
        -500.0,
        -750.0,
        -1000.0,
        -1250.0,
        -1500.0
    };
    int weight_count =
        (int)(sizeof(inhibitory_weights) / sizeof(inhibitory_weights[0]));
    FILE *csv = fopen("results/experiments/inhibition/inhibition_sweep.csv", "w");

    if (csv == NULL)
    {
        printf("Erro: nao foi possivel criar results/experiments/inhibition/inhibition_sweep.csv.\n");
        return 1;
    }

    if (fprintf(
            csv,
            "peso_inibitorio,spikes_totais,pico_spikes,"
            "timesteps_ativos,media_spikes,primeiro_recrutamento,"
            "neuronios_ativos\n") < 0)
    {
        printf("Erro: nao foi possivel escrever cabecalho do CSV.\n");
        fclose(csv);
        return 1;
    }

    printf("=== Varredura de inibicao ===\n");
    printf("peso_inibitorio | spikes_totais | pico | media | neuronios_ativos\n");

    for (int i = 0; i < weight_count; i++)
    {
        SweepResult result;

        if (!run_sweep_case(inhibitory_weights[i], &result))
        {
            fclose(csv);
            return 1;
        }

        if (!write_csv_row(csv, &result))
        {
            printf("Erro: nao foi possivel escrever linha do CSV.\n");
            fclose(csv);
            return 1;
        }

        printf("%15.2f | %13d | %4d | %5.2f | %16d\n",
               result.inhibitory_weight,
               result.total_spikes,
               result.peak_spikes,
               result.mean_spikes,
               result.active_neurons);
    }

    fclose(csv);
    return 0;
}
