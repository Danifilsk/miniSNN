#include <stdio.h>
#include <stdlib.h>

#include "minisnn.h"

#define NEURON_COUNT 5
#define STEPS 1000
#define CSV_PATH "results/api/api_chain_spikes.csv"

static int ensure_results_directory(void)
{
    if (system("if not exist results mkdir results") != 0)
        return 0;

    if (system("if not exist results\\api mkdir results\\api") != 0)
        return 0;

    return 1;
}

int main(void)
{
    MiniSNNConfig config = minisnn_default_config();
    MiniSNN *brain;
    FILE *csv = NULL;
    int spike_counts[NEURON_COUNT] = {0};
    int first_spikes[NEURON_COUNT];

    for (int i = 0; i < NEURON_COUNT; i++)
        first_spikes[i] = -1;

    if (!ensure_results_directory())
    {
        printf("Erro ao criar diretorio results/api.\n");
        return 1;
    }

    config.neuron_count = NEURON_COUNT;

    brain = minisnn_create_with_config(&config);

    if (brain == NULL)
    {
        printf("Erro ao criar rede.\n");
        return 1;
    }

    for (int source = 0; source < NEURON_COUNT - 1; source++)
    {
        if (!minisnn_connect_delayed(brain, source, source + 1, 200.0, 1))
        {
            printf("Erro ao criar conexao da cadeia.\n");
            minisnn_destroy(&brain);
            return 1;
        }
    }

    if (!minisnn_set_input(brain, 0, 20.0))
    {
        printf("Erro ao aplicar entrada.\n");
        minisnn_destroy(&brain);
        return 1;
    }

    csv = fopen(CSV_PATH, "w");

    if (csv == NULL)
    {
        printf("Erro ao criar %s.\n", CSV_PATH);
        minisnn_destroy(&brain);
        return 1;
    }

    if (fprintf(csv, "tempo,neuronio\n") < 0)
    {
        printf("Erro ao escrever cabecalho.\n");
        fclose(csv);
        minisnn_destroy(&brain);
        return 1;
    }

    for (int step = 0; step < STEPS; step++)
    {
        if (minisnn_step(brain) < 0)
        {
            printf("Erro ao executar timestep.\n");
            fclose(csv);
            minisnn_destroy(&brain);
            return 1;
        }

        for (int neuron_id = 0; neuron_id < NEURON_COUNT; neuron_id++)
        {
            int spike;

            if (!minisnn_get_spike(brain, neuron_id, &spike))
            {
                printf("Erro ao consultar spike.\n");
                fclose(csv);
                minisnn_destroy(&brain);
                return 1;
            }

            if (!spike)
                continue;

            spike_counts[neuron_id]++;

            if (first_spikes[neuron_id] < 0)
                first_spikes[neuron_id] = step;

            if (fprintf(csv, "%d,%d\n", step, neuron_id) < 0)
            {
                printf("Erro ao escrever raster.\n");
                fclose(csv);
                minisnn_destroy(&brain);
                return 1;
            }
        }
    }

    fclose(csv);
    minisnn_destroy(&brain);

    if (first_spikes[0] < 0)
    {
        printf("Erro: neuronio 0 nunca disparou.\n");
        return 1;
    }

    printf("=== API: cadeia de neuronios ===\n");

    for (int neuron_id = 0; neuron_id < NEURON_COUNT; neuron_id++)
    {
        printf(
            "neuronio %d | spikes: %d | primeiro spike: %d\n",
            neuron_id,
            spike_counts[neuron_id],
            first_spikes[neuron_id]);
    }

    printf("arquivo gerado: %s\n", CSV_PATH);

    return 0;
}
