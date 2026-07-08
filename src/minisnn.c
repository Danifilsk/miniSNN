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
