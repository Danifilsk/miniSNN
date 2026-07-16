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
    int last_active_step;
    int active_neurons;
    int source_spiked[INPUT_SOURCE_COUNT];
    int neuron_spiked[POPULATION_SIZE];
    double mean_spikes;
} TransitionResult;

static void reset_result(TransitionResult *result, double inhibitory_weight)
{
    result->inhibitory_weight = inhibitory_weight;
    result->total_spikes = 0;
    result->peak_spikes = 0;
    result->active_timesteps = 0;
    result->first_recruitment_step = -1;
    result->last_active_step = -1;
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
    TransitionResult *result)
{
    result->total_spikes += step_spikes;

    if (step_spikes > result->peak_spikes)
        result->peak_spikes = step_spikes;

    if (step_spikes > 0)
    {
        result->active_timesteps++;
        result->last_active_step = step;
    }

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

static int finalize_result(TransitionResult *result)
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

static int run_transition_case(
    double inhibitory_weight,
    TransitionResult *result)
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

static int write_csv_row(FILE *csv, const TransitionResult *result)
{
    if (fprintf(
            csv,
            "%.2f,%d,%d,%d,%.2f,%d,%d,%d\n",
            result->inhibitory_weight,
            result->total_spikes,
            result->peak_spikes,
            result->active_timesteps,
            result->mean_spikes,
            result->first_recruitment_step,
            result->last_active_step,
            result->active_neurons) < 0)
    {
        return 0;
    }

    return 1;
}

static void update_summary(
    const TransitionResult *result,
    const TransitionResult **max_activity,
    const TransitionResult **min_activity,
    const TransitionResult **first_below_100)
{
    if (*max_activity == NULL ||
        result->total_spikes > (*max_activity)->total_spikes)
    {
        *max_activity = result;
    }

    if (*min_activity == NULL ||
        result->total_spikes < (*min_activity)->total_spikes)
    {
        *min_activity = result;
    }

    if (*first_below_100 == NULL && result->total_spikes < 100)
        *first_below_100 = result;
}

int main(void)
{
    double inhibitory_weights[] =
    {
        -250.0,
        -275.0,
        -300.0,
        -325.0,
        -350.0,
        -375.0,
        -400.0,
        -425.0,
        -450.0,
        -475.0,
        -500.0
    };
    int weight_count =
        (int)(sizeof(inhibitory_weights) / sizeof(inhibitory_weights[0]));
    TransitionResult results[
        sizeof(inhibitory_weights) / sizeof(inhibitory_weights[0])];
    const TransitionResult *max_activity = NULL;
    const TransitionResult *min_activity = NULL;
    const TransitionResult *first_below_100 = NULL;
    FILE *csv = fopen("results/experiments/inhibition/inhibition_transition.csv", "w");

    if (csv == NULL)
    {
        printf("Erro: nao foi possivel criar results/experiments/inhibition/inhibition_transition.csv.\n");
        return 1;
    }

    if (fprintf(
            csv,
            "peso_inibitorio,spikes_totais,pico_spikes,"
            "timesteps_ativos,media_spikes,primeiro_recrutamento,"
            "ultimo_timestep_ativo,neuronios_ativos\n") < 0)
    {
        printf("Erro: nao foi possivel escrever cabecalho do CSV.\n");
        fclose(csv);
        return 1;
    }

    printf("=== Transicao de inibicao ===\n");
    printf(
        "peso | spikes | pico | passos_ativos | "
        "ultimo_passo_ativo | neuronios_ativos\n");

    for (int i = 0; i < weight_count; i++)
    {
        if (!run_transition_case(inhibitory_weights[i], &results[i]))
        {
            fclose(csv);
            return 1;
        }

        if (!write_csv_row(csv, &results[i]))
        {
            printf("Erro: nao foi possivel escrever linha do CSV.\n");
            fclose(csv);
            return 1;
        }

        update_summary(
            &results[i],
            &max_activity,
            &min_activity,
            &first_below_100);

        printf("%6.2f | %6d | %4d | %14d | %18d | %16d\n",
               results[i].inhibitory_weight,
               results[i].total_spikes,
               results[i].peak_spikes,
               results[i].active_timesteps,
               results[i].last_active_step,
               results[i].active_neurons);
    }

    fclose(csv);

    printf("\n");
    printf("Peso com maior atividade: %.2f (%d spikes)\n",
           max_activity->inhibitory_weight,
           max_activity->total_spikes);
    printf("Peso com menor atividade: %.2f (%d spikes)\n",
           min_activity->inhibitory_weight,
           min_activity->total_spikes);

    if (first_below_100 != NULL)
    {
        printf("Primeiro peso com menos de 100 spikes: %.2f (%d spikes)\n",
               first_below_100->inhibitory_weight,
               first_below_100->total_spikes);
    }
    else
    {
        printf("Primeiro peso com menos de 100 spikes: nenhum\n");
    }

    return 0;
}
