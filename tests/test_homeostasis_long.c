#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "minisnn.h"

#define NEURONS 30
#define STEPS 20000

static void fail(const char *message)
{
    fprintf(stderr, "Homeostasis long test failed: %s\n", message);
    exit(1);
}

int main(void)
{
    MiniSNNConfig network_config = minisnn_default_config();
    MiniSNNPlasticityConfig plasticity = minisnn_default_plasticity_config();
    MiniSNNHomeostasisConfig homeostasis = minisnn_default_homeostasis_config();
    MiniSNN *snn;
    size_t initial_connections;

    network_config.neuron_count = NEURONS;
    network_config.max_synaptic_delay = 4;
    snn = minisnn_create_with_config(&network_config);
    if (snn == NULL)
        fail("network creation");

    for (int i = 24; i < NEURONS; i++)
    {
        if (!minisnn_set_neuron_type(snn, i, MINISNN_NEURON_INHIBITORY))
            fail("set inhibitory type");
    }

    for (int source = 0; source < NEURONS; source++)
    {
        for (int target = 0; target < NEURONS; target++)
        {
            double weight;
            int delay;
            if (source == target || ((source * 17 + target * 31) % 5) != 0)
                continue;
            weight = source < 24 ? 80.0 : -120.0;
            delay = 1 + ((source + target) % 4);
            if (!minisnn_connect_delayed(snn, source, target, weight, delay))
                fail("connect network");
        }
    }
    initial_connections = minisnn_connection_count(snn);

    plasticity.enabled = 1;
    plasticity.a_plus = 0.05;
    plasticity.a_minus = 0.0525;
    plasticity.weight_max = 200.0;
    if (!minisnn_set_plasticity_config(snn, &plasticity))
        fail("configure plasticity");

    homeostasis.enabled = 1;
    homeostasis.intrinsic_enabled = 1;
    homeostasis.synaptic_scaling_enabled = 1;
    homeostasis.inhibitory_gain_enabled = 1;
    homeostasis.target_rate = 0.05;
    homeostasis.update_interval_steps = 20U;
    homeostasis.threshold_eta = 0.02;
    homeostasis.scaling_eta = 0.05;
    homeostasis.scaling_weight_max = 200.0;
    homeostasis.inhibitory_gain_eta = 0.01;
    if (!minisnn_set_homeostasis_config(snn, &homeostasis))
        fail("configure homeostasis");

    for (int step = 0; step < STEPS; step++)
    {
        minisnn_clear_inputs(snn);
        for (int source = 0; source < 3; source++)
        {
            if (!minisnn_set_input(snn, source, 20.0))
                fail("set input");
        }
        if (minisnn_step(snn) < 0)
            fail("simulation step");

        if ((step % 1000) == 0)
        {
            double gain;
            if (!minisnn_get_inhibitory_gain(snn, &gain) || !isfinite(gain) ||
                gain < homeostasis.inhibitory_gain_min ||
                gain > homeostasis.inhibitory_gain_max)
            {
                fail("gain bounds");
            }
            for (int i = 0; i < NEURONS; i++)
            {
                double rate;
                double threshold;
                if (!minisnn_get_neuron_rate_trace(snn, i, &rate) ||
                    !minisnn_get_neuron_effective_threshold(snn, i, &threshold) ||
                    !isfinite(rate) || !isfinite(threshold) ||
                    threshold < homeostasis.threshold_min ||
                    threshold > homeostasis.threshold_max)
                {
                    fail("neuron homeostasis bounds");
                }
            }
        }
    }

    if (minisnn_connection_count(snn) != initial_connections)
        fail("connection count changed");
    for (size_t i = 0; i < minisnn_connection_count(snn); i++)
    {
        MiniSNNConnectionInfo connection;
        if (!minisnn_get_connection(snn, i, &connection) ||
            !isfinite(connection.weight))
        {
            fail("finite connection weight");
        }
        if (connection.source_type == MINISNN_NEURON_INHIBITORY &&
            connection.weight != -120.0)
        {
            fail("raw inhibitory weight changed");
        }
        if (connection.source_type == MINISNN_NEURON_EXCITATORY &&
            (connection.weight < 0.0 || connection.weight > 200.0))
        {
            fail("excitatory weight bounds");
        }
    }

    minisnn_destroy(&snn);
    printf("Homeostasis long-run validation OK\n");
    return 0;
}
