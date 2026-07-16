#include <math.h>
#include <stdio.h>

#include "minisnn.h"
#include "network.h"

#define REPEAT_COUNT 250
#define DENSE_NEURONS 128

static int fail(const char *message)
{
    printf("FAIL: %s\n", message);
    return 0;
}

static int check_partial_and_invalid_lifecycle(void)
{
    Network net;
    NetworkConfig config;

    network_config_default(&config);
    config.max_synaptic_delay = 0;

    if (network_init_with_config(&net, 8, &config))
        return fail("invalid partial initialization succeeded");

    network_destroy(&net);
    network_destroy(&net);

    if (network_update(&net) != -1)
        return fail("partially initialized network accepted update");

    return 1;
}

static int check_repeated_internal_lifecycle(void)
{
    for (int iteration = 0; iteration < REPEAT_COUNT; iteration++)
    {
        Network net;

        if (!network_init(&net, 16))
            return fail("repeated network_init failed");

        for (int target = 1; target < 16; target++)
        {
            int delay = 1 + target % net.max_synaptic_delay;

            if (!network_connect_delayed(&net, 0, target, 10.0 + target, delay))
            {
                network_destroy(&net);
                return fail("reallocating connection list failed");
            }
        }

        if (!network_set_external_current(&net, 0, 20.0) ||
            network_update(&net) < 0)
        {
            network_destroy(&net);
            return fail("repeated internal update failed");
        }

        network_destroy(&net);
        network_destroy(&net);
    }

    return 1;
}

static int check_repeated_public_lifecycle(void)
{
    for (int iteration = 0; iteration < REPEAT_COUNT; iteration++)
    {
        MiniSNN *snn = minisnn_create(8);

        if (snn == NULL)
            return fail("repeated minisnn_create failed");

        for (int target = 1; target < 8; target++)
        {
            if (!minisnn_connect_delayed(snn, 0, target, 50.0, target))
            {
                minisnn_destroy(&snn);
                return fail("public delayed connection failed");
            }
        }

        if (!minisnn_set_input(snn, 0, 20.0) || minisnn_step(snn) < 0)
        {
            minisnn_destroy(&snn);
            return fail("public repeated update failed");
        }

        minisnn_destroy(&snn);
        minisnn_destroy(&snn);

        if (snn != NULL)
            return fail("public destroy did not clear pointer");
    }

    return 1;
}

static int check_dense_connections(void)
{
    MiniSNN *snn = minisnn_create(DENSE_NEURONS);
    int expected = DENSE_NEURONS * (DENSE_NEURONS - 1);
    int created = 0;

    if (snn == NULL)
        return fail("dense network creation failed");

    for (int source = 0; source < DENSE_NEURONS; source++)
    {
        for (int target = 0; target < DENSE_NEURONS; target++)
        {
            if (source == target)
                continue;

            if (!minisnn_connect_delayed(
                    snn,
                    source,
                    target,
                    source < DENSE_NEURONS - 16 ? 5.0 : -7.0,
                    1 + (source + target) % 8))
            {
                minisnn_destroy(&snn);
                return fail("dense connection creation failed");
            }
            created++;
        }
    }

    if (created != expected)
    {
        minisnn_destroy(&snn);
        return fail("dense connection count mismatch");
    }

    if (!minisnn_set_input(snn, 0, 20.0) || minisnn_step(snn) < 0)
    {
        minisnn_destroy(&snn);
        return fail("dense network update failed");
    }

    for (int neuron_id = 0; neuron_id < DENSE_NEURONS; neuron_id++)
    {
        double voltage;
        double current;

        if (!minisnn_get_voltage(snn, neuron_id, &voltage) ||
            !minisnn_get_synaptic_current(snn, neuron_id, &current) ||
            !isfinite(voltage) ||
            !isfinite(current))
        {
            minisnn_destroy(&snn);
            return fail("dense network produced invalid state");
        }
    }

    minisnn_destroy(&snn);
    return 1;
}

int main(void)
{
    if (!check_partial_and_invalid_lifecycle() ||
        !check_repeated_internal_lifecycle() ||
        !check_repeated_public_lifecycle() ||
        !check_dense_connections())
    {
        return 1;
    }

    printf("Network memory stress validation OK\n");
    return 0;
}
