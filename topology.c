#include <stdlib.h>

#include "topology.h"
#include "config.h"

void topology_chain(Network *net)
{
    for (int i = 0; i < net->size - 1; i++)
    {
        net->connections[i].list = malloc(sizeof(Connection));

        if (net->connections[i].list == NULL)
            return;

        net->connections[i].count = 1;

        net->connections[i].list[0].target = i + 1;
        net->connections[i].list[0].weight = W;
    }
}

void topology_ring(Network *net)
{
    for (int i = 0; i < net->size; i++)
    {
        net->connections[i].list = malloc(sizeof(Connection));

        if (net->connections[i].list == NULL)
            return;

        net->connections[i].count = 1;

        net->connections[i].list[0].target =
            (i + 1) % net->size;

        net->connections[i].list[0].weight = W;
    }
}
void topology_all_to_all(Network *net)
{
    for (int i = 0; i < net->size; i++)
    {
        net->connections[i].list = malloc((net->size - 1) * sizeof(Connection));

        if (net->connections[i].list == NULL)
            return;

        net->connections[i].count = net->size - 1;

        int k = 0;

        for (int j = 0; j < net->size; j++)
        {
            if (j == i)
                continue;

            net->connections[i].list[k].target = j;
            net->connections[i].list[k].weight = W;
            k++;
        }
    }
}

void topology_random(Network *net, double probability)
{
    int *targets = malloc(net->size * sizeof(int));

    for (int i = 0; i < net->size; i++)
    {
        int count = 0;

        for (int j = 0; j < net->size; j++)
        {
            if (i == j)
                continue;

            if ((double)rand() / RAND_MAX < probability)
                targets[count++] = j;
        }

        net->connections[i].count = count;

        if (count == 0)
        {
            net->connections[i].list = NULL;
            continue;
        }

        net->connections[i].list = malloc(count * sizeof(Connection));

        if (net->connections[i].list == NULL)
            break;

        for (int k = 0; k < count; k++)
        {
            net->connections[i].list[k].target = targets[k];
            net->connections[i].list[k].weight = W;
        }
    }

    free(targets);
}