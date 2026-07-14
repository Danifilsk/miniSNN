#include <stdlib.h>

#include "config.h"
#include "minisnn.h"
#include "network.h"

struct MiniSNN
{
    Network net;
};

static int minisnn_valid_neuron_id(const MiniSNN *snn, int neuron_id)
{
    if (snn == NULL)
        return 0;

    if (neuron_id < 0 || neuron_id >= snn->net.size)
        return 0;

    return 1;
}

MiniSNN *minisnn_create(int neuron_count)
{
    MiniSNNConfig config = minisnn_default_config();

    config.neuron_count = neuron_count;
    return minisnn_create_with_config(&config);
}

MiniSNNConfig minisnn_default_config(void)
{
    MiniSNNConfig config;

    config.neuron_count = N_NEURONS;
    config.dt = DT;
    config.tau = TAU;
    config.v_rest = V_REST;
    config.v_reset = V_RESET;
    config.v_threshold = V_THRESH;
    config.resistance = R;
    config.synaptic_decay = SYN_DECAY;
    config.max_synaptic_delay = MAX_SYNAPTIC_DELAY;

    return config;
}

MiniSNN *minisnn_create_with_config(
    const MiniSNNConfig *config)
{
    MiniSNN *snn;
    NetworkConfig network_config;

    if (config == NULL)
        return NULL;

    network_config.lif.dt = config->dt;
    network_config.lif.tau = config->tau;
    network_config.lif.v_rest = config->v_rest;
    network_config.lif.v_reset = config->v_reset;
    network_config.lif.v_threshold = config->v_threshold;
    network_config.lif.resistance = config->resistance;
    network_config.synaptic_decay = config->synaptic_decay;
    network_config.max_synaptic_delay = config->max_synaptic_delay;

    if (config->neuron_count <= 0 ||
        !network_config_is_valid(&network_config))
    {
        return NULL;
    }

    snn = malloc(sizeof(*snn));

    if (snn == NULL)
        return NULL;

    if (!network_init_with_config(
            &snn->net,
            config->neuron_count,
            &network_config))
    {
        free(snn);
        return NULL;
    }

    return snn;
}

void minisnn_destroy(MiniSNN **snn_ptr)
{
    if (snn_ptr == NULL || *snn_ptr == NULL)
        return;

    network_destroy(&(*snn_ptr)->net);
    free(*snn_ptr);
    *snn_ptr = NULL;
}

int minisnn_neuron_count(const MiniSNN *snn)
{
    if (snn == NULL)
        return 0;

    return snn->net.size;
}

int minisnn_current_step(const MiniSNN *snn)
{
    if (snn == NULL)
        return -1;

    return snn->net.step;
}

size_t minisnn_connection_count(const MiniSNN *snn)
{
    if (snn == NULL)
        return 0;

    return network_connection_count(&snn->net);
}

int minisnn_get_connection(
    const MiniSNN *snn,
    size_t connection_id,
    MiniSNNConnectionInfo *out_connection)
{
    int source;
    Connection *connection;

    if (snn == NULL || out_connection == NULL ||
        !network_get_connection(
            &snn->net,
            connection_id,
            &source,
            &connection) ||
        connection->target < 0 || connection->target >= snn->net.size)
    {
        return 0;
    }

    out_connection->source = (size_t)source;
    out_connection->target = (size_t)connection->target;
    out_connection->source_type =
        (MiniSNNNeuronType)snn->net.neurons[source].type;
    out_connection->target_type =
        (MiniSNNNeuronType)snn->net.neurons[connection->target].type;
    out_connection->weight = connection->weight;
    out_connection->delay = (unsigned int)connection->delay;
    out_connection->plasticity_eligible =
        plasticity_connection_is_eligible(
            snn->net.plasticity,
            snn->net.neurons,
            source,
            connection);
    return 1;
}

int minisnn_get_connection_weight(
    const MiniSNN *snn,
    size_t connection_id,
    double *out_weight)
{
    MiniSNNConnectionInfo connection;

    if (out_weight == NULL ||
        !minisnn_get_connection(snn, connection_id, &connection))
    {
        return 0;
    }

    *out_weight = connection.weight;
    return 1;
}

int minisnn_set_connection_weight(
    MiniSNN *snn,
    size_t connection_id,
    double weight)
{
    if (snn == NULL)
        return 0;

    return network_set_connection_weight(&snn->net, connection_id, weight);
}

int minisnn_set_neuron_type(
    MiniSNN *snn,
    int neuron_id,
    MiniSNNNeuronType type)
{
    if (snn == NULL)
        return 0;

    return network_set_neuron_type(&snn->net, neuron_id, type);
}

int minisnn_connect(
    MiniSNN *snn,
    int source,
    int target,
    double weight)
{
    return minisnn_connect_ex(snn, source, target, weight, 0);
}

int minisnn_connect_ex(
    MiniSNN *snn,
    int source,
    int target,
    double weight,
    int allow_self_connection)
{
    if (snn == NULL)
        return 0;

    return network_connect_ex(
        &snn->net,
        source,
        target,
        weight,
        allow_self_connection);
}

int minisnn_connect_delayed(
    MiniSNN *snn,
    int source,
    int target,
    double weight,
    int delay)
{
    return minisnn_connect_delayed_ex(
        snn,
        source,
        target,
        weight,
        delay,
        0);
}

int minisnn_connect_delayed_ex(
    MiniSNN *snn,
    int source,
    int target,
    double weight,
    int delay,
    int allow_self_connection)
{
    if (snn == NULL)
        return 0;

    return network_connect_delayed_ex(
        &snn->net,
        source,
        target,
        weight,
        delay,
        allow_self_connection);
}

MiniSNNPlasticityConfig minisnn_default_plasticity_config(void)
{
    return plasticity_default_config();
}

int minisnn_set_plasticity_config(
    MiniSNN *snn,
    const MiniSNNPlasticityConfig *config)
{
    if (snn == NULL)
        return 0;

    return network_set_plasticity_config(&snn->net, config);
}

int minisnn_get_plasticity_config(
    const MiniSNN *snn,
    MiniSNNPlasticityConfig *out_config)
{
    if (snn == NULL || out_config == NULL || snn->net.plasticity == NULL)
        return 0;

    *out_config = snn->net.plasticity->config;
    return 1;
}

int minisnn_get_plasticity_stats(
    const MiniSNN *snn,
    MiniSNNPlasticityStats *out_stats)
{
    if (snn == NULL || out_stats == NULL || snn->net.plasticity == NULL)
        return 0;

    *out_stats = snn->net.plasticity->stats;
    return 1;
}

int minisnn_get_plasticity_traces(
    const MiniSNN *snn,
    int neuron_id,
    double *out_pre_trace,
    double *out_post_trace)
{
    if (!minisnn_valid_neuron_id(snn, neuron_id) ||
        out_pre_trace == NULL || out_post_trace == NULL ||
        snn->net.plasticity == NULL ||
        snn->net.plasticity->pre_trace == NULL ||
        snn->net.plasticity->post_trace == NULL)
    {
        return 0;
    }

    *out_pre_trace = snn->net.plasticity->pre_trace[neuron_id];
    *out_post_trace = snn->net.plasticity->post_trace[neuron_id];
    return 1;
}

MiniSNNHomeostasisConfig minisnn_default_homeostasis_config(void)
{
    return homeostasis_default_config();
}

int minisnn_set_homeostasis_config(
    MiniSNN *snn,
    const MiniSNNHomeostasisConfig *config)
{
    if (snn == NULL)
        return 0;

    return network_set_homeostasis_config(&snn->net, config);
}

int minisnn_get_homeostasis_config(
    const MiniSNN *snn,
    MiniSNNHomeostasisConfig *out_config)
{
    if (snn == NULL || out_config == NULL || snn->net.homeostasis == NULL)
        return 0;

    *out_config = snn->net.homeostasis->config;
    return 1;
}

int minisnn_reset_homeostasis(MiniSNN *snn)
{
    if (snn == NULL)
        return 0;

    return network_reset_homeostasis(&snn->net);
}

int minisnn_get_homeostasis_stats(
    const MiniSNN *snn,
    MiniSNNHomeostasisStats *out_stats)
{
    if (snn == NULL || out_stats == NULL || snn->net.homeostasis == NULL)
        return 0;

    *out_stats = snn->net.homeostasis->stats;
    return 1;
}

int minisnn_get_neuron_rate_trace(
    const MiniSNN *snn,
    int neuron_id,
    double *out_rate_trace)
{
    if (!minisnn_valid_neuron_id(snn, neuron_id) ||
        out_rate_trace == NULL || snn->net.homeostasis == NULL)
    {
        return 0;
    }

    *out_rate_trace = snn->net.homeostasis->config.enabled ?
        snn->net.homeostasis->rate_trace[neuron_id] : 0.0;
    return 1;
}

int minisnn_get_neuron_effective_threshold(
    const MiniSNN *snn,
    int neuron_id,
    double *out_threshold)
{
    if (!minisnn_valid_neuron_id(snn, neuron_id) || out_threshold == NULL ||
        snn->net.homeostasis == NULL)
    {
        return 0;
    }

    *out_threshold = homeostasis_effective_threshold(
        snn->net.homeostasis,
        neuron_id,
        snn->net.lif_parameters.v_threshold);
    return 1;
}

int minisnn_get_base_threshold(
    const MiniSNN *snn,
    double *out_threshold)
{
    if (snn == NULL || out_threshold == NULL)
        return 0;

    *out_threshold = snn->net.lif_parameters.v_threshold;
    return 1;
}

int minisnn_get_inhibitory_gain(
    const MiniSNN *snn,
    double *out_gain)
{
    if (snn == NULL || out_gain == NULL || snn->net.homeostasis == NULL)
        return 0;

    *out_gain = snn->net.homeostasis->config.enabled ?
        snn->net.homeostasis->inhibitory_gain : 1.0;
    return 1;
}

int minisnn_get_initial_incoming_exc_sum(
    const MiniSNN *snn,
    int neuron_id,
    double *out_sum)
{
    if (!minisnn_valid_neuron_id(snn, neuron_id) || out_sum == NULL ||
        snn->net.homeostasis == NULL)
    {
        return 0;
    }

    if (!snn->net.homeostasis->config.enabled)
    {
        *out_sum = 0.0;
        return 1;
    }

    *out_sum = snn->net.homeostasis->initial_incoming_exc_sum[neuron_id];
    return 1;
}

int minisnn_get_current_incoming_exc_sum(
    const MiniSNN *snn,
    int neuron_id,
    double *out_sum)
{
    if (!minisnn_valid_neuron_id(snn, neuron_id) || out_sum == NULL ||
        snn->net.homeostasis == NULL)
    {
        return 0;
    }

    if (!snn->net.homeostasis->config.enabled)
    {
        *out_sum = 0.0;
        return 1;
    }

    return homeostasis_current_incoming_sum(
        snn->net.homeostasis,
        neuron_id,
        snn->net.neurons,
        snn->net.connections,
        snn->net.plasticity,
        out_sum);
}

int minisnn_set_input(
    MiniSNN *snn,
    int neuron_id,
    double current)
{
    if (snn == NULL)
        return 0;

    return network_set_external_current(&snn->net, neuron_id, current);
}

int minisnn_add_input(
    MiniSNN *snn,
    int neuron_id,
    double current)
{
    if (snn == NULL)
        return 0;

    return network_add_external_current(&snn->net, neuron_id, current);
}

void minisnn_clear_inputs(MiniSNN *snn)
{
    if (snn == NULL)
        return;

    network_clear_external_currents(&snn->net);
}

int minisnn_step(MiniSNN *snn)
{
    if (snn == NULL)
        return -1;

    return network_update(&snn->net);
}

int minisnn_get_spike(
    const MiniSNN *snn,
    int neuron_id,
    int *out_spike)
{
    if (!minisnn_valid_neuron_id(snn, neuron_id) ||
        out_spike == NULL ||
        snn->net.spikes == NULL)
    {
        return 0;
    }

    *out_spike = snn->net.spikes[neuron_id];
    return 1;
}

int minisnn_get_voltage(
    const MiniSNN *snn,
    int neuron_id,
    double *out_voltage)
{
    if (!minisnn_valid_neuron_id(snn, neuron_id) ||
        out_voltage == NULL ||
        snn->net.neurons == NULL)
    {
        return 0;
    }

    *out_voltage = snn->net.neurons[neuron_id].V;
    return 1;
}

int minisnn_get_synaptic_current(
    const MiniSNN *snn,
    int neuron_id,
    double *out_current)
{
    if (!minisnn_valid_neuron_id(snn, neuron_id) ||
        out_current == NULL ||
        snn->net.used_syn_current == NULL)
    {
        return 0;
    }

    *out_current = snn->net.used_syn_current[neuron_id];
    return 1;
}
