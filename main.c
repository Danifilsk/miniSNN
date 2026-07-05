#include <stdio.h>

#include "config.h"
#include "network.h"
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

    //aqui muda o tipo de topologia
    topology_random(&net, 0.20);

    // ===== DEBUG =====
    printf("\n=== TOPOLOGIA ===\n");

    for (int i = 0; i < 5 && i < net.size; i++)
    {
        printf("Neuronio %d -> %d conexao(oes)\n",
               i,
               net.connections[i].count);

        for (int j = 0; j < net.connections[i].count; j++)
        {
            printf("    destino=%d  peso=%.2f\n",
                   net.connections[i].list[j].target,
                   net.connections[i].list[j].weight);
        }
    }

    printf("=================\n\n");
    // =================

    for (int t = 0; t < T_MAX; t++)
    {
        network_update(&net);

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

    network_destroy(&net);

    fclose(csv);

    printf("\nSimulação finalizada!\n");

    return 0;
}