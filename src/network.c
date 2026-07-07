#include <math.h>
#include <stdlib.h>

#include "network.h"
#include "config.h"

static void network_reset_fields(Network *net)
{
    if (net == NULL)
        return;

    net->neurons = NULL;
    net->connections = NULL;
    net->spikes = NULL;
    net->syn_current = NULL;
    net->used_syn_current = NULL;
    net->pending_current = NULL;
    net->ext_current = NULL;

    net->lif_parameters.dt = 0.0;
    net->lif_parameters.tau = 0.0;
    net->lif_parameters.v_rest = 0.0;
    net->lif_parameters.v_reset = 0.0;
    net->lif_parameters.v_threshold = 0.0;
    net->lif_parameters.resistance = 0.0;
    net->synaptic_decay = 0.0;

    net->size = 0;
    net->step = 0;
    net->max_synaptic_delay = 0;
    net->delay_cursor = 0;
}

static int network_is_valid_for_update(Network *net)
{
    if (net == NULL ||
        net->size <= 0 ||
        net->max_synaptic_delay <= 0)
    {
        return 0;
    }

    if (!lif_parameters_are_valid(&net->lif_parameters) ||
        !isfinite(net->synaptic_decay) ||
        net->synaptic_decay < 0.0 ||
        net->synaptic_decay > 1.0)
    {
        return 0;
    }

    if (net->neurons == NULL ||
        net->connections == NULL ||
        net->spikes == NULL ||
        net->syn_current == NULL ||
        net->used_syn_current == NULL ||
        net->pending_current == NULL ||
        net->ext_current == NULL)
    {
        return 0;
    }

    return 1;
}

int network_init(Network *net, int size)
{
    NetworkConfig config;

    network_config_default(&config);
    return network_init_with_config(net, size, &config);
}

void network_config_default(NetworkConfig *out_config)
{
    if (out_config == NULL)
        return;

    lif_parameters_default(&out_config->lif);
    out_config->synaptic_decay = SYN_DECAY;
    out_config->max_synaptic_delay = MAX_SYNAPTIC_DELAY;
}

int network_config_is_valid(
    const NetworkConfig *config)
{
    if (config == NULL)
        return 0;

    if (!lif_parameters_are_valid(&config->lif))
        return 0;

    if (!isfinite(config->synaptic_decay) ||
        config->synaptic_decay < 0.0 ||
        config->synaptic_decay > 1.0)
    {
        return 0;
    }

    if (config->max_synaptic_delay < 1)
        return 0;

    return 1;
}

int network_init_with_config(
    Network *net,
    int size,
    const NetworkConfig *config)
{
    if (net == NULL)
        return 0;

    network_reset_fields(net);

    if (size <= 0 || !network_config_is_valid(config))
        return 0;

    net->size = size;
    net->step = 0;
    net->lif_parameters = config->lif;
    net->synaptic_decay = config->synaptic_decay;
    net->max_synaptic_delay = config->max_synaptic_delay;
    net->delay_cursor = 0;

    net->neurons = malloc(size * sizeof(LIFNeuron));
    net->connections = malloc(size * sizeof(ConnectionList));

    if (net->connections != NULL)
    {
        for (int i = 0; i < size; i++)
        {
            net->connections[i].list = NULL;
            net->connections[i].count = 0;
        }
    }

    net->spikes = malloc(size * sizeof(int));

    // Corrente sináptica do passo atual
    net->syn_current = malloc(size * sizeof(double));
    net->used_syn_current = malloc(size * sizeof(double));

    net->pending_current =
        malloc((size_t)size *
               (size_t)net->max_synaptic_delay *
               sizeof(double));

    // Corrente externa
    net->ext_current = malloc(size * sizeof(double));

    if (net->neurons == NULL ||
        net->connections == NULL ||
        net->spikes == NULL ||
        net->syn_current == NULL ||
        net->used_syn_current == NULL ||
        net->pending_current == NULL ||
        net->ext_current == NULL)
    {
        network_destroy(net);
        return 0;
    }

    for (int i = 0; i < size; i++)
    {
        lif_init_with_parameters(&net->neurons[i], &net->lif_parameters);

        net->spikes[i] = 0;

        net->syn_current[i] = 0.0;
        net->used_syn_current[i] = 0.0;
        net->ext_current[i] = 0.0;

        net->connections[i].list = NULL;
        net->connections[i].count = 0;
    }

    for (int i = 0; i < size * net->max_synaptic_delay; i++)
        net->pending_current[i] = 0.0;

    return 1;
}

int network_update(Network *net)
{
    int total_spikes = 0;
    int next_slot;

    if (!network_is_valid_for_update(net))
        return -1;

    // Limpa os spikes do passo anterior
    for (int i = 0; i < net->size; i++)
        net->spikes[i] = 0;

    // Corrente total = externa + sináptica
    for (int i = 0; i < net->size; i++)
    {
        net->used_syn_current[i] = net->syn_current[i];

        double I = net->ext_current[i] + net->used_syn_current[i];

        net->spikes[i] =
            lif_update_with_parameters(&net->neurons[i], I, &net->lif_parameters);

        if (net->spikes[i])
            total_spikes++;
    }

    // Gera corrente para o PRÓXIMO passo
    for (int i = 0; i < net->size; i++)
    {
        if (!net->spikes[i])
            continue;

        if (net->connections[i].list == NULL)
            continue;

        for (int j = 0; j < net->connections[i].count; j++)
        {
            Connection *c = &net->connections[i].list[j];

            if (c->target < 0 || c->target >= net->size)
                continue;

            if (c->delay < 1 || c->delay > net->max_synaptic_delay)
                continue;

            int delivery_slot =
                (net->delay_cursor + c->delay) %
                net->max_synaptic_delay;

            net->pending_current[delivery_slot * net->size + c->target] +=
                c->weight;
        }
    }

    next_slot =
        (net->delay_cursor + 1) %
        net->max_synaptic_delay;

    // Atualiza a corrente sináptica
    for (int i = 0; i < net->size; i++)
    {
        net->syn_current[i] =
            net->syn_current[i] * net->synaptic_decay +
            net->pending_current[next_slot * net->size + i];

        net->pending_current[next_slot * net->size + i] = 0.0;

        if (net->syn_current[i] > -1e-9 &&
            net->syn_current[i] <  1e-9)
        {
            net->syn_current[i] = 0.0;
        }
    }

    net->delay_cursor = next_slot;
    net->step++;

    return total_spikes;
}

int network_connect(Network *net, int source, int target, double weight)
{
    return network_connect_delayed(net, source, target, weight, 1);
}

int network_connect_delayed(
    Network *net,
    int source,
    int target,
    double weight,
    int delay)
{
    if (net == NULL || net->connections == NULL || net->size <= 0)
        return 0;

    if (source < 0 || source >= net->size ||
        target < 0 || target >= net->size)
    {
        return 0;
    }

    if (source == target || !isfinite(weight))
        return 0;

    if (delay < 1 || delay > net->max_synaptic_delay)
        return 0;

    ConnectionList *connections = &net->connections[source];

    if (connections->count < 0 ||
        connections->count >= net->size - 1)
    {
        return 0;
    }

    if (connections->count > 0 && connections->list == NULL)
        return 0;

    for (int i = 0; i < connections->count; i++)
    {
        if (connections->list[i].target == target)
            return 0;
    }

    int new_count = connections->count + 1;
    Connection *new_list = realloc(
        connections->list,
        (size_t)new_count * sizeof(Connection));

    if (new_list == NULL)
        return 0;

    new_list[connections->count].target = target;
    new_list[connections->count].weight = weight;
    new_list[connections->count].delay = delay;

    connections->list = new_list;
    connections->count = new_count;

    return 1;
}

int network_set_neuron_type(
    Network *net,
    int neuron_id,
    NeuronType type)
{
    if (net == NULL || net->neurons == NULL)
        return 0;

    if (neuron_id < 0 || neuron_id >= net->size)
        return 0;

    if (type != NEURON_EXCITATORY &&
        type != NEURON_INHIBITORY)
    {
        return 0;
    }

    net->neurons[neuron_id].type = type;
    return 1;
}

void network_clear_connections(Network *net)
{
    if (net == NULL || net->connections == NULL)
        return;

    for (int i = 0; i < net->size; i++)
    {
        free(net->connections[i].list);
        net->connections[i].list = NULL;
        net->connections[i].count = 0;
    }
}

int network_set_external_current(Network *net, int neuron_id, double current)
{
    if (net == NULL || net->ext_current == NULL)
        return 0;

    if (neuron_id < 0 || neuron_id >= net->size)
        return 0;

    if (!isfinite(current))
        return 0;

    net->ext_current[neuron_id] = current;
    return 1;
}

int network_add_external_current(Network *net, int neuron_id, double current)
{
    if (net == NULL || net->ext_current == NULL)
        return 0;

    if (neuron_id < 0 || neuron_id >= net->size)
        return 0;

    if (!isfinite(current))
        return 0;

    double updated_current = net->ext_current[neuron_id] + current;

    if (!isfinite(updated_current))
        return 0;

    net->ext_current[neuron_id] = updated_current;
    return 1;
}

void network_clear_external_currents(Network *net)
{
    if (net == NULL || net->ext_current == NULL)
        return;

    for (int i = 0; i < net->size; i++)
        net->ext_current[i] = 0.0;
}

void network_destroy(Network *net)
{
    if (net == NULL)
        return;

    if (net->connections != NULL)
    {
        network_clear_connections(net);
        free(net->connections);
    }

    free(net->neurons);
    free(net->spikes);
    free(net->syn_current);
    free(net->used_syn_current);
    free(net->pending_current);
    free(net->ext_current);

    network_reset_fields(net);
}
