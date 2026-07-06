#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "network.h"
#include "recorder.h"
#include "stimulus.h"
#include "topology.h"

int main(void)
{
    FILE *csv = fopen("spikes.csv", "w");

    if (csv == NULL)
    {
        printf("Erro ao abrir o arquivo CSV.\n");
        return 1;
    }

    fprintf(csv, "tempo,neuronio\n");

    Network net;

    if (!network_init(&net, N_NEURONS))
    {
        printf("Erro ao criar a rede.\n");
        fclose(csv);
        return 1;
    }

    StimulusSchedule schedule;

    if (!stimulus_schedule_init(&schedule))
    {
        printf("Erro ao criar agenda de estimulos.\n");
        network_destroy(&net);
        fclose(csv);
        return 1;
    }

    if (!stimulus_schedule_add_pulse(&schedule, 0, 0, T_MAX, I_EXT))
    {
        printf("Erro ao adicionar pulso de entrada.\n");
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        fclose(csv);
        return 1;
    }

    srand(RANDOM_SEED);

    //aqui muda o tipo de topologia
    if (!topology_random_balanced(&net, 0.20, 0.20))
    {
        printf("Erro ao criar topologia.\n");
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        fclose(csv);
        return 1;
    }

    NeuronRecorder recorder;

    if (!neuron_recorder_open(&recorder, "neuron0.csv", 0))
    {
        printf("Erro ao abrir arquivo de monitoramento.\n");
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        fclose(csv);
        return 1;
    }

    if (!neuron_recorder_write_header(&recorder))
    {
        printf("Erro ao escrever cabecalho do recorder.\n");
        neuron_recorder_close(&recorder);
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        fclose(csv);
        return 1;
    }

    PopulationRecorder population_recorder;

    if (!population_recorder_open(
            &population_recorder,
            "population_metrics.csv"))
    {
        printf("Erro ao abrir arquivo de metricas da populacao.\n");
        neuron_recorder_close(&recorder);
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        fclose(csv);
        return 1;
    }

    if (!population_recorder_write_header(&population_recorder))
    {
        printf("Erro ao escrever cabecalho das metricas.\n");
        population_recorder_close(&population_recorder);
        neuron_recorder_close(&recorder);
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        fclose(csv);
        return 1;
    }

    // ===== DEBUG =====
    printf("\n=== TOPOLOGIA ===\n");

    for (int i = 0; i < net.size; i++)
    {
        const char *type_name =
            net.neurons[i].type == NEURON_INHIBITORY ? "INH" : "EXC";

        printf("Neuronio %d [%s] -> %d conexao(oes)\n",
               i,
               type_name,
               net.connections[i].count);

        for (int j = 0; j < net.connections[i].count; j++)
        {
            printf("    destino=%d  peso=%.2f  delay=%d\n",
                   net.connections[i].list[j].target,
                   net.connections[i].list[j].weight,
                   net.connections[i].list[j].delay);
        }
    }

    printf("=================\n\n");
    // =================

    for (int t = 0; t < T_MAX; t++)
    {
        network_clear_external_currents(&net);

        if (!stimulus_schedule_apply(&schedule, &net, t))
        {
            printf("Erro ao aplicar estimulos.\n");
            population_recorder_close(&population_recorder);
            neuron_recorder_close(&recorder);
            stimulus_schedule_destroy(&schedule);
            network_destroy(&net);
            fclose(csv);
            return 1;
        }

        network_update(&net);

        if (!neuron_recorder_record(&recorder, &net, t))
        {
            printf("Erro ao registrar dados do neuronio.\n");
            population_recorder_close(&population_recorder);
            neuron_recorder_close(&recorder);
            stimulus_schedule_destroy(&schedule);
            network_destroy(&net);
            fclose(csv);
            return 1;
        }

        if (!population_recorder_record(&population_recorder, &net, t))
        {
            printf("Erro ao registrar metricas da populacao.\n");
            population_recorder_close(&population_recorder);
            neuron_recorder_close(&recorder);
            stimulus_schedule_destroy(&schedule);
            network_destroy(&net);
            fclose(csv);
            return 1;
        }

        // Debug apenas nos primeiros passos
        if (t < 50)
        {
            printf("t=%3d | ", t);

            for (int i = 0; i < net.size; i++)
            {
                printf("V%d=%7.2f ", i, net.neurons[i].V);
            }

            printf("| ");

            for (int i = 0; i < net.size; i++)
            {
                printf("S%d=%d ", i, net.spikes[i]);
            }

            printf("\n");
        }

        for (int i = 0; i < net.size; i++)
        {
            if (net.spikes[i])
            {
                fprintf(csv, "%d,%d\n", t, i);
            }
        }
    }

    population_recorder_close(&population_recorder);
    neuron_recorder_close(&recorder);
    stimulus_schedule_destroy(&schedule);
    network_destroy(&net);

    fclose(csv);

    printf("\nSimulação finalizada!\n");

    return 0;
}
