#include <stdio.h>
#include <stdlib.h>

#include "minisnn.h"

#define STEPS 1000
#define CSV_PATH "results/api/api_single_neuron.csv"

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
    int total_spikes = 0;
    int first_spike = -1;

    config.neuron_count = 1;

    if (!ensure_results_directory())
    {
        printf("Erro ao criar diretorio results/api.\n");
        return 1;
    }

    brain = minisnn_create_with_config(&config);

    if (brain == NULL)
    {
        printf("Erro ao criar rede.\n");
        return 1;
    }

    csv = fopen(CSV_PATH, "w");

    if (csv == NULL)
    {
        printf("Erro ao criar %s.\n", CSV_PATH);
        minisnn_destroy(&brain);
        return 1;
    }

    if (fprintf(csv, "tempo,voltagem,spike,corrente_sinaptica\n") < 0)
    {
        printf("Erro ao escrever cabecalho.\n");
        fclose(csv);
        minisnn_destroy(&brain);
        return 1;
    }

    if (!minisnn_set_input(brain, 0, 20.0))
    {
        printf("Erro ao aplicar entrada.\n");
        fclose(csv);
        minisnn_destroy(&brain);
        return 1;
    }

    for (int step = 0; step < STEPS; step++)
    {
        int spike;
        double voltage;
        double synaptic_current;

        if (minisnn_step(brain) < 0)
        {
            printf("Erro ao executar timestep.\n");
            fclose(csv);
            minisnn_destroy(&brain);
            return 1;
        }

        if (!minisnn_get_spike(brain, 0, &spike) ||
            !minisnn_get_voltage(brain, 0, &voltage) ||
            !minisnn_get_synaptic_current(brain, 0, &synaptic_current))
        {
            printf("Erro ao consultar estado.\n");
            fclose(csv);
            minisnn_destroy(&brain);
            return 1;
        }

        if (spike)
        {
            total_spikes++;

            if (first_spike < 0)
                first_spike = step;
        }

        if (fprintf(
                csv,
                "%d,%.2f,%d,%.2f\n",
                step,
                voltage,
                spike,
                synaptic_current) < 0)
        {
            printf("Erro ao escrever CSV.\n");
            fclose(csv);
            minisnn_destroy(&brain);
            return 1;
        }
    }

    fclose(csv);
    minisnn_destroy(&brain);

    if (first_spike < 0)
    {
        printf("Erro: neuronio nunca disparou.\n");
        return 1;
    }

    printf("=== API: neuronio unico ===\n");
    printf("spikes totais: %d\n", total_spikes);
    printf("primeiro spike: %d\n", first_spike);
    printf("arquivo gerado: %s\n", CSV_PATH);

    return 0;
}
