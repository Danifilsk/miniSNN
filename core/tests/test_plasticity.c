#include <math.h>
#include <stdio.h>

#include "network.h"

#define TOLERANCE 1e-10

static int close_enough(double actual, double expected)
{
    return fabs(actual - expected) <= TOLERANCE;
}

static MiniSNNPlasticityConfig test_config(void)
{
    MiniSNNPlasticityConfig config = plasticity_default_config();

    config.enabled = 1;
    config.rule = MINISNN_PLASTICITY_STDP_PAIR_TRACE;
    config.a_plus = 0.5;
    config.a_minus = 0.25;
    config.tau_plus = 2.0;
    config.tau_minus = 2.0;
    config.trace_increment = 1.0;
    config.weight_min = 0.0;
    config.weight_max = 20.0;
    return config;
}

static int init_pair_model(
    Network *net,
    double weight,
    NeuronType source_type,
    MiniSNNNeuronModel model)
{
    NetworkConfig network_config;

    network_config_default(&network_config);
    network_config.neuron_model = model;
    if (model == MINISNN_NEURON_MODEL_ADEX)
        network_config.adex.dt = 1.0;
    else if (model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY)
        network_config.hodgkin_huxley.dt = 1.0;
    else
        network_config.lif.dt = 1.0;

    if (!network_init_with_config(net, 2, &network_config) ||
        !network_set_neuron_type(net, 0, source_type) ||
        !network_connect(net, 0, 1, weight))
    {
        network_destroy(net);
        return 0;
    }

    return 1;
}

static int init_pair(Network *net, double weight, NeuronType source_type)
{
    return init_pair_model(
        net, weight, source_type, MINISNN_NEURON_MODEL_LIF);
}

static int apply_spikes(Network *net, int first, int second)
{
    net->spikes[0] = first;
    net->spikes[1] = second;
    return plasticity_apply_step(
        net->plasticity,
        net->neurons,
        net->connections,
        net->spikes,
        neuron_model_dt(&net->model_config));
}

static int test_ltp_interval(int interval)
{
    Network net;
    MiniSNNPlasticityConfig config = test_config();
    double expected = 10.0 + 0.5 * exp(-(double)interval / 2.0);
    int ok = init_pair(&net, 10.0, NEURON_EXCITATORY) &&
             network_set_plasticity_config(&net, &config) &&
             apply_spikes(&net, 1, 0);

    for (int step = 1; ok && step < interval; step++)
        ok = apply_spikes(&net, 0, 0);

    if (ok)
        ok = apply_spikes(&net, 0, 1);

    ok = ok && close_enough(net.connections[0].list[0].weight, expected);
    network_destroy(&net);
    return ok;
}

static int test_ltd_interval(int interval)
{
    Network net;
    MiniSNNPlasticityConfig config = test_config();
    double expected = 10.0 - 0.25 * exp(-(double)interval / 2.0);
    int ok = init_pair(&net, 10.0, NEURON_EXCITATORY) &&
             network_set_plasticity_config(&net, &config) &&
             apply_spikes(&net, 0, 1);

    for (int step = 1; ok && step < interval; step++)
        ok = apply_spikes(&net, 0, 0);

    if (ok)
        ok = apply_spikes(&net, 1, 0);

    ok = ok && close_enough(net.connections[0].list[0].weight, expected);
    network_destroy(&net);
    return ok;
}

static int test_simultaneous_and_disabled(void)
{
    Network net;
    MiniSNNPlasticityConfig config = test_config();
    int ok = init_pair(&net, 10.0, NEURON_EXCITATORY) &&
             network_set_plasticity_config(&net, &config) &&
             apply_spikes(&net, 1, 1) &&
             close_enough(net.connections[0].list[0].weight, 10.0);

    config.enabled = 0;
    ok = ok && network_set_plasticity_config(&net, &config) &&
         apply_spikes(&net, 1, 0) &&
         apply_spikes(&net, 0, 1) &&
         close_enough(net.connections[0].list[0].weight, 10.0) &&
         net.plasticity->stats.total_absolute_change == 0.0;

    network_destroy(&net);
    return ok;
}

static int test_advanced_model_spike_events(void)
{
    const MiniSNNNeuronModel models[] = {
        MINISNN_NEURON_MODEL_ADEX,
        MINISNN_NEURON_MODEL_HODGKIN_HUXLEY
    };
    MiniSNNPlasticityConfig config = test_config();

    for (size_t i = 0; i < sizeof(models) / sizeof(models[0]); i++)
    {
        Network net;
        double ltp_expected = 10.0 + 0.5 * exp(-0.5);
        double ltd_expected = 10.0 - 0.25 * exp(-0.5);
        int ok = init_pair_model(
                &net, 10.0, NEURON_EXCITATORY, models[i]) &&
            network_set_plasticity_config(&net, &config) &&
            apply_spikes(&net, 1, 0) &&
            apply_spikes(&net, 0, 1) &&
            close_enough(net.connections[0].list[0].weight, ltp_expected);
        network_destroy(&net);
        if (!ok)
            return 0;

        ok = init_pair_model(
                &net, 10.0, NEURON_EXCITATORY, models[i]) &&
            network_set_plasticity_config(&net, &config) &&
            apply_spikes(&net, 0, 1) &&
            apply_spikes(&net, 1, 0) &&
            close_enough(net.connections[0].list[0].weight, ltd_expected);
        network_destroy(&net);
        if (!ok)
            return 0;
    }
    return 1;
}

static int test_weight_clamps(void)
{
    Network net;
    MiniSNNPlasticityConfig config = test_config();
    int ok = init_pair(&net, 19.9, NEURON_EXCITATORY) &&
             network_set_plasticity_config(&net, &config) &&
             apply_spikes(&net, 1, 0) &&
             apply_spikes(&net, 0, 1) &&
             close_enough(net.connections[0].list[0].weight, 20.0) &&
             net.plasticity->stats.clamp_max_events == 1ULL;

    network_destroy(&net);

    ok = ok && init_pair(&net, 0.1, NEURON_EXCITATORY) &&
         network_set_plasticity_config(&net, &config) &&
         apply_spikes(&net, 0, 1) &&
         apply_spikes(&net, 1, 0) &&
         close_enough(net.connections[0].list[0].weight, 0.0) &&
         net.plasticity->stats.clamp_min_events == 1ULL;

    network_destroy(&net);
    return ok;
}

static int test_inhibitory_connection(void)
{
    Network net;
    MiniSNNPlasticityConfig config = test_config();
    int ok = init_pair(&net, -4.0, NEURON_INHIBITORY) &&
             network_set_plasticity_config(&net, &config) &&
             apply_spikes(&net, 1, 0) &&
             apply_spikes(&net, 0, 1) &&
             close_enough(net.connections[0].list[0].weight, -4.0) &&
             net.plasticity->stats.eligible_connections == 0U;

    network_destroy(&net);
    return ok;
}

static int test_self_connection(void)
{
    Network net;
    NetworkConfig network_config;
    MiniSNNPlasticityConfig config = test_config();
    double decay = exp(-0.5);
    double expected = 10.0 + (0.5 - 0.25) * decay;
    int ok;

    network_config_default(&network_config);
    network_config.lif.dt = 1.0;
    ok = network_init_with_config(&net, 1, &network_config) &&
         network_connect_ex(&net, 0, 0, 10.0, 1) &&
         network_set_plasticity_config(&net, &config);

    if (ok)
    {
        net.spikes[0] = 1;
        ok = plasticity_apply_step(
            net.plasticity,
            net.neurons,
            net.connections,
            net.spikes,
            1.0);
    }

    if (ok)
    {
        net.spikes[0] = 1;
        ok = plasticity_apply_step(
            net.plasticity,
            net.neurons,
            net.connections,
            net.spikes,
            1.0);
    }

    ok = ok && close_enough(net.connections[0].list[0].weight, expected) &&
         net.plasticity->stats.potentiation_events == 1ULL &&
         net.plasticity->stats.depression_events == 1ULL;
    network_destroy(&net);
    return ok;
}

static int test_traces_and_reset(void)
{
    Network net;
    MiniSNNPlasticityConfig config = test_config();
    double decay = exp(-0.5);
    int ok = init_pair(&net, 10.0, NEURON_EXCITATORY) &&
             network_set_plasticity_config(&net, &config) &&
             close_enough(net.plasticity->pre_trace[0], 0.0) &&
             close_enough(net.plasticity->post_trace[1], 0.0) &&
             apply_spikes(&net, 1, 0) &&
             close_enough(net.plasticity->pre_trace[0], 1.0) &&
             close_enough(net.plasticity->post_trace[0], 1.0) &&
             close_enough(net.plasticity->pre_trace[1], 0.0) &&
             apply_spikes(&net, 0, 0) &&
             close_enough(net.plasticity->pre_trace[0], decay) &&
             isfinite(net.plasticity->pre_trace[0]) &&
             network_set_plasticity_config(&net, &config) &&
             close_enough(net.plasticity->pre_trace[0], 0.0) &&
             close_enough(net.plasticity->post_trace[0], 0.0) &&
             net.plasticity->stats.potentiation_events == 0ULL;

    network_destroy(&net);
    return ok;
}

static int test_transmission_uses_old_weight(void)
{
    Network net;
    NetworkConfig network_config;
    MiniSNNPlasticityConfig config = test_config();
    double expected_new_weight = 5.0 - 0.25 * exp(-0.5);
    int ok;

    network_config_default(&network_config);
    network_config.lif.dt = 1.0;
    ok = network_init_with_config(&net, 2, &network_config) &&
         network_connect_delayed(&net, 0, 1, 5.0, 2) &&
         network_set_plasticity_config(&net, &config);

    if (ok)
    {
        net.plasticity->post_trace[1] = 1.0;
        ok = network_set_external_current(&net, 0, 1000.0) &&
             network_update(&net) == 1;
    }

    ok = ok && close_enough(net.pending_current[2 * net.size + 1], 5.0) &&
         close_enough(net.connections[0].list[0].weight, expected_new_weight);

    if (ok)
        ok = network_update(&net) == 1;

    ok = ok && close_enough(net.syn_current[1], 5.0) &&
         close_enough(
             net.pending_current[3 * net.size + 1],
             expected_new_weight);
    network_destroy(&net);
    return ok;
}

static int test_incoming_index_rebuild_after_connection(void)
{
    Network net;
    NetworkConfig network_config;
    MiniSNNPlasticityConfig config = test_config();
    double expected = 5.0 + 0.5 * exp(-0.5);
    int ok;

    network_config_default(&network_config);
    network_config.lif.dt = 1.0;
    ok = network_init_with_config(&net, 2, &network_config) &&
        network_set_plasticity_config(&net, &config) &&
        net.plasticity->index_valid &&
        network_connect(&net, 0, 1, 5.0) &&
        !net.plasticity->index_valid;

    if (ok)
    {
        ok = apply_spikes(&net, 1, 0) &&
             apply_spikes(&net, 0, 1) &&
             net.plasticity->index_valid &&
             net.plasticity->stats.eligible_connections == 1U &&
             close_enough(net.connections[0].list[0].weight, expected);
    }

    network_destroy(&net);
    return ok;
}

int main(void)
{
    int intervals[] = {1, 2, 5};

    for (size_t i = 0; i < sizeof(intervals) / sizeof(intervals[0]); i++)
    {
        if (!test_ltp_interval(intervals[i]) ||
            !test_ltd_interval(intervals[i]))
        {
            fprintf(stderr, "Falha no intervalo STDP %d.\n", intervals[i]);
            return 1;
        }
    }

    if (!test_simultaneous_and_disabled() ||
        !test_advanced_model_spike_events() ||
        !test_weight_clamps() ||
        !test_inhibitory_connection() ||
        !test_self_connection() ||
        !test_traces_and_reset() ||
        !test_transmission_uses_old_weight() ||
        !test_incoming_index_rebuild_after_connection())
    {
        fprintf(stderr, "Falha na validacao numerica STDP.\n");
        return 1;
    }

    printf("STDP numerical validation OK\n");
    return 0;
}
