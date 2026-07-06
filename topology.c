#include <math.h>
#include <stdlib.h>

#include "topology.h"
#include "config.h"

static int topology_network_is_valid(Network *net, int min_size)
{
    return net != NULL &&
           net->size >= min_size &&
           net->neurons != NULL &&
           net->connections != NULL;
}

static int topology_set_all_excitatory(Network *net)
{
    for (int i = 0; i < net->size; i++)
    {
        if (!network_set_neuron_type(net, i, NEURON_EXCITATORY))
            return 0;
    }

    return 1;
}

static int topology_prepare_excitatory(Network *net, int min_size)
{
    if (!topology_network_is_valid(net, min_size))
        return 0;

    network_clear_connections(net);

    if (!topology_set_all_excitatory(net))
    {
        network_clear_connections(net);
        return 0;
    }

    return 1;
}

int topology_chain(Network *net)
{
    if (!topology_prepare_excitatory(net, 1))
        return 0;

    for (int i = 0; i < net->size - 1; i++)
    {
        if (!network_connect(net, i, i + 1, W))
        {
            network_clear_connections(net);
            return 0;
        }
    }

    return 1;
}

int topology_ring(Network *net)
{
    if (!topology_prepare_excitatory(net, 2))
        return 0;

    for (int i = 0; i < net->size; i++)
    {
        if (!network_connect(net, i, (i + 1) % net->size, W))
        {
            network_clear_connections(net);
            return 0;
        }
    }

    return 1;
}

int topology_all_to_all(Network *net)
{
    if (!topology_prepare_excitatory(net, 1))
        return 0;

    for (int i = 0; i < net->size; i++)
    {
        for (int j = 0; j < net->size; j++)
        {
            if (j == i)
                continue;

            if (!network_connect(net, i, j, W))
            {
                network_clear_connections(net);
                return 0;
            }
        }
    }

    return 1;
}

int topology_random(Network *net, double probability)
{
    if (!topology_prepare_excitatory(net, 1))
        return 0;

    if (probability < 0.0)
        probability = 0.0;

    if (probability > 1.0)
        probability = 1.0;

    for (int i = 0; i < net->size; i++)
    {
        for (int j = 0; j < net->size; j++)
        {
            if (i == j)
                continue;

            if ((double)rand() / RAND_MAX < probability)
            {
                if (!network_connect(net, i, j, W))
                {
                    network_clear_connections(net);
                    return 0;
                }
            }
        }
    }

    return 1;
}

int topology_random_balanced(
    Network *net,
    double probability,
    double inhibitory_fraction)
{
    int inhibitory_count;
    int *indices;

    if (!topology_network_is_valid(net, 1))
        return 0;

    if (!isfinite(probability) ||
        probability < 0.0 ||
        probability > 1.0)
    {
        return 0;
    }

    if (!isfinite(inhibitory_fraction) ||
        inhibitory_fraction < 0.0 ||
        inhibitory_fraction > 1.0)
    {
        return 0;
    }

    network_clear_connections(net);

    if (!topology_set_all_excitatory(net))
    {
        network_clear_connections(net);
        return 0;
    }

    inhibitory_count =
        (int)(inhibitory_fraction * (double)net->size + 0.5);

    if (inhibitory_count < 0)
        inhibitory_count = 0;

    if (inhibitory_count > net->size)
        inhibitory_count = net->size;

    indices = malloc((size_t)net->size * sizeof(int));

    if (indices == NULL)
    {
        network_clear_connections(net);
        return 0;
    }

    for (int i = 0; i < net->size; i++)
        indices[i] = i;

    srand(RANDOM_SEED);

    for (int i = net->size - 1; i > 0; i--)
    {
        int j = rand() % (i + 1);
        int temp = indices[i];

        indices[i] = indices[j];
        indices[j] = temp;
    }

    for (int i = 0; i < inhibitory_count; i++)
    {
        if (!network_set_neuron_type(
                net,
                indices[i],
                NEURON_INHIBITORY))
        {
            free(indices);
            network_clear_connections(net);
            topology_set_all_excitatory(net);
            return 0;
        }
    }

    free(indices);

    for (int i = 0; i < net->size; i++)
    {
        double weight =
            net->neurons[i].type == NEURON_INHIBITORY ? W_INH : W_EXC;

        for (int j = 0; j < net->size; j++)
        {
            if (i == j)
                continue;

            if (probability >= 1.0 ||
                (double)rand() / (double)RAND_MAX < probability)
            {
                if (!network_connect(net, i, j, weight))
                {
                    network_clear_connections(net);
                    return 0;
                }
            }
        }
    }

    return 1;
}
