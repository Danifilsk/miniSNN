#include <math.h>
#include <stdio.h>

#include "minisnn.h"

#define NEURON_COUNT 20
#define LONG_STEPS 10000
#define INHIBITORY_START 16
#define EXCITATORY_WEIGHT 100.0
#define INHIBITORY_WEIGHT -100.0

static int inspect_state(MiniSNN *snn, size_t expected_connections)
{
    if (minisnn_connection_count(snn) != expected_connections)
        return 0;

    for (size_t id = 0; id < expected_connections; id++)
    {
        MiniSNNConnectionInfo connection;

        if (!minisnn_get_connection(snn, id, &connection) ||
            !isfinite(connection.weight))
        {
            return 0;
        }

        if (connection.source_type == MINISNN_NEURON_EXCITATORY)
        {
            if (connection.weight < 0.0 || connection.weight > 200.0)
                return 0;
        }
        else if (connection.weight != INHIBITORY_WEIGHT ||
                 connection.plasticity_eligible)
        {
            return 0;
        }
    }

    for (int neuron = 0; neuron < NEURON_COUNT; neuron++)
    {
        double pre_trace;
        double post_trace;

        if (!minisnn_get_plasticity_traces(
                snn,
                neuron,
                &pre_trace,
                &post_trace) ||
            !isfinite(pre_trace) ||
            !isfinite(post_trace))
        {
            return 0;
        }
    }

    return 1;
}

int main(void)
{
    MiniSNNConfig network_config = minisnn_default_config();
    MiniSNNPlasticityConfig plasticity_config =
        minisnn_default_plasticity_config();
    MiniSNNPlasticityStats stats;
    MiniSNN *snn;
    size_t expected_connections =
        (size_t)NEURON_COUNT * (size_t)(NEURON_COUNT - 1);

    network_config.neuron_count = NEURON_COUNT;
    snn = minisnn_create_with_config(&network_config);
    if (snn == NULL)
        return 1;

    for (int neuron = INHIBITORY_START; neuron < NEURON_COUNT; neuron++)
    {
        if (!minisnn_set_neuron_type(
                snn,
                neuron,
                MINISNN_NEURON_INHIBITORY))
        {
            minisnn_destroy(&snn);
            return 1;
        }
    }

    for (int source = 0; source < NEURON_COUNT; source++)
    {
        double weight = source < INHIBITORY_START ?
            EXCITATORY_WEIGHT :
            INHIBITORY_WEIGHT;

        for (int target = 0; target < NEURON_COUNT; target++)
        {
            if (source != target &&
                !minisnn_connect_delayed(snn, source, target, weight, 1))
            {
                minisnn_destroy(&snn);
                return 1;
            }
        }
    }

    plasticity_config.enabled = 1;
    plasticity_config.a_plus = 0.1;
    plasticity_config.a_minus = 0.105;
    plasticity_config.weight_min = 0.0;
    plasticity_config.weight_max = 200.0;

    if (!minisnn_set_plasticity_config(snn, &plasticity_config))
    {
        minisnn_destroy(&snn);
        return 1;
    }

    for (int step = 0; step < LONG_STEPS; step++)
    {
        minisnn_clear_inputs(snn);

        for (int source = 0; source < 3; source++)
        {
            if (!minisnn_set_input(snn, source, 20.0))
            {
                minisnn_destroy(&snn);
                return 1;
            }
        }

        if (minisnn_step(snn) < 0 ||
            ((step + 1) % 1000 == 0 &&
             !inspect_state(snn, expected_connections)))
        {
            minisnn_destroy(&snn);
            return 1;
        }
    }

    if (!minisnn_get_plasticity_stats(snn, &stats) ||
        stats.eligible_connections !=
            (size_t)INHIBITORY_START * (size_t)(NEURON_COUNT - 1) ||
        !isfinite(stats.total_signed_change) ||
        !isfinite(stats.total_absolute_change) ||
        !isfinite(stats.max_absolute_change))
    {
        minisnn_destroy(&snn);
        return 1;
    }

    minisnn_destroy(&snn);
    printf("STDP long-run validation OK\n");
    return 0;
}
