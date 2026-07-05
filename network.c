#include <stdlib.h>

#include "network.h"
#include "config.h"

int network_init(Network *net, int size)
{
    net->size = size;
    net->step = 0;

    net->neurons = malloc(size * sizeof(LIFNeuron));
    net->connections = malloc(size * sizeof(ConnectionList));
    net->spikes = malloc(size * sizeof(int));

    // Corrente sináptica do passo atual
    net->syn_current = malloc(size * sizeof(double));

    // Corrente gerada neste passo
    net->next_current = malloc(size * sizeof(double));

    // Corrente externa
    net->ext_current = malloc(size * sizeof(double));

    if (net->neurons == NULL ||
        net->connections == NULL ||
        net->spikes == NULL ||
        net->syn_current == NULL ||
        net->next_current == NULL ||
        net->ext_current == NULL)
    {
        network_destroy(net);
        return 0;
    }

    for (int i = 0; i < size; i++)
    {
        lif_init(&net->neurons[i]);

        net->spikes[i] = 0;

        net->syn_current[i] = 0.0;
        net->next_current[i] = 0.0;
        net->ext_current[i] = 0.0;

        net->connections[i].list = NULL;
        net->connections[i].count = 0;
    }

    // Apenas o neurônio 0 recebe corrente externa
    net->ext_current[0] = I_EXT;

    return 1;
}

int network_update(Network *net)
{
    int total_spikes = 0;

    // Limpa os spikes do passo anterior
    for (int i = 0; i < net->size; i++)
        net->spikes[i] = 0;

    // Corrente total = externa + sináptica
    for (int i = 0; i < net->size; i++)
    {
        double I = net->ext_current[i] + net->syn_current[i];

        net->spikes[i] = lif_update(&net->neurons[i], I);

        if (net->spikes[i])
            total_spikes++;
    }

    // Gera corrente para o PRÓXIMO passo
    for (int i = 0; i < net->size; i++)
    {
        if (!net->spikes[i])
            continue;

        for (int j = 0; j < net->connections[i].count; j++)
        {
            Connection *c = &net->connections[i].list[j];

            net->next_current[c->target] += c->weight;
        }
    }

    // Atualiza a corrente sináptica
    for (int i = 0; i < net->size; i++)
    {
        net->syn_current[i] =
            net->syn_current[i] * SYN_DECAY +
            net->next_current[i];

        net->next_current[i] = 0.0;

        if (net->syn_current[i] > -1e-9 &&
            net->syn_current[i] <  1e-9)
        {
            net->syn_current[i] = 0.0;
        }
    }

    net->step++;

    return total_spikes;
}

void network_destroy(Network *net)
{
    if (net == NULL)
        return;

    if (net->connections != NULL)
    {
        for (int i = 0; i < net->size; i++)
            free(net->connections[i].list);

        free(net->connections);
    }

    free(net->neurons);
    free(net->spikes);
    free(net->syn_current);
    free(net->next_current);
    free(net->ext_current);
}