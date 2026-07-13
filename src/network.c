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
    net->plasticity = NULL;

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
        net->ext_current == NULL ||
        net->plasticity == NULL)
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
    net->plasticity = calloc(1, sizeof(*net->plasticity));

    if (net->neurons == NULL ||
        net->connections == NULL ||
        net->spikes == NULL ||
        net->syn_current == NULL ||
        net->used_syn_current == NULL ||
        net->pending_current == NULL ||
        net->ext_current == NULL ||
        net->plasticity == NULL)
    {
        network_destroy(net);
        return 0;
    }

    if (!plasticity_state_init(net->plasticity, size))
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

    /* O spike atual ja foi transmitido com o peso anterior ao STDP. */
    if (!plasticity_apply_step(
            net->plasticity,
            net->neurons,
            net->connections,
            net->spikes,
            net->lif_parameters.dt))
    {
        return -1;
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

static int network_connect_delayed_impl(
    Network *net,
    int source,
    int target,
    double weight,
    int delay,
    int allow_self_connection)
{
    int max_connections;

    if (net == NULL ||
        net->connections == NULL ||
        net->size <= 0 ||
        net->max_synaptic_delay <= 0)
    {
        return 0;
    }

    if (net->plasticity != NULL &&
        net->plasticity->config.enabled &&
        net->step > 0)
    {
        return 0;
    }

    if (source < 0 || source >= net->size ||
        target < 0 || target >= net->size)
    {
        return 0;
    }

    if (!allow_self_connection && source == target)
        return 0;

    if (!isfinite(weight))
        return 0;

    if (delay < 1 || delay > net->max_synaptic_delay)
        return 0;

    ConnectionList *connections = &net->connections[source];
    max_connections = net->size;

    if (connections->count < 0 ||
        connections->count >= max_connections)
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

    plasticity_state_invalidate_index(net->plasticity);

    return 1;
}

int network_connect(Network *net, int source, int target, double weight)
{
    return network_connect_ex(net, source, target, weight, 0);
}

int network_connect_ex(
    Network *net,
    int source,
    int target,
    double weight,
    int allow_self_connection)
{
    return network_connect_delayed_ex(
        net,
        source,
        target,
        weight,
        1,
        allow_self_connection);
}

int network_connect_delayed(
    Network *net,
    int source,
    int target,
    double weight,
    int delay)
{
    return network_connect_delayed_ex(
        net,
        source,
        target,
        weight,
        delay,
        0);
}

int network_connect_delayed_ex(
    Network *net,
    int source,
    int target,
    double weight,
    int delay,
    int allow_self_connection)
{
    return network_connect_delayed_impl(
        net,
        source,
        target,
        weight,
        delay,
        allow_self_connection);
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

    if (net->plasticity != NULL &&
        net->plasticity->config.enabled &&
        net->step > 0)
    {
        return 0;
    }

    net->neurons[neuron_id].type = type;
    plasticity_state_invalidate_index(net->plasticity);
    return 1;
}

size_t network_connection_count(const Network *net)
{
    size_t count = 0;

    if (net == NULL || net->connections == NULL || net->size <= 0)
        return 0;

    for (int source = 0; source < net->size; source++)
    {
        if (net->connections[source].count < 0)
            return 0;

        count += (size_t)net->connections[source].count;
    }

    return count;
}

int network_get_connection(
    const Network *net,
    size_t connection_id,
    int *out_source,
    Connection **out_connection)
{
    size_t offset = 0;

    if (net == NULL || net->connections == NULL ||
        out_source == NULL || out_connection == NULL)
    {
        return 0;
    }

    for (int source = 0; source < net->size; source++)
    {
        const ConnectionList *list = &net->connections[source];
        size_t count;

        if (list->count < 0 || (list->count > 0 && list->list == NULL))
            return 0;

        count = (size_t)list->count;

        if (connection_id < offset + count)
        {
            *out_source = source;
            *out_connection = &list->list[connection_id - offset];
            return 1;
        }

        offset += count;
    }

    return 0;
}

int network_set_connection_weight(
    Network *net,
    size_t connection_id,
    double weight)
{
    int source;
    Connection *connection;

    if (!isfinite(weight) ||
        !network_get_connection(
            net,
            connection_id,
            &source,
            &connection))
    {
        return 0;
    }

    (void)source;
    connection->weight = weight;
    return 1;
}

int network_set_plasticity_config(
    Network *net,
    const MiniSNNPlasticityConfig *config)
{
    if (net == NULL || net->plasticity == NULL ||
        net->neurons == NULL || net->connections == NULL)
    {
        return 0;
    }

    return plasticity_state_configure(
        net->plasticity,
        config,
        net->neurons,
        net->connections);
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

    plasticity_state_invalidate_index(net->plasticity);
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

    if (net->plasticity != NULL)
    {
        plasticity_state_destroy(net->plasticity);
        free(net->plasticity);
    }

    network_reset_fields(net);
}
