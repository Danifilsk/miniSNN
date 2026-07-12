#include <math.h>
#include <stdio.h>

#include "minisnn.h"

#define LONG_NEURONS 64
#define LONG_STEPS 50000

static int fail(const char *message)
{
    printf("FAIL: %s\n", message);
    return 1;
}

int main(void)
{
    MiniSNNConfig config = minisnn_default_config();
    MiniSNN *snn;
    long long total_spikes = 0;

    config.neuron_count = LONG_NEURONS;
    config.max_synaptic_delay = 8;
    snn = minisnn_create_with_config(&config);

    if (snn == NULL)
        return fail("long-run network creation failed");

    for (int source = 0; source < LONG_NEURONS; source++)
    {
        int target = (source + 1) % LONG_NEURONS;
        int delay = 1 + source % config.max_synaptic_delay;

        if (!minisnn_connect_delayed(snn, source, target, 40.0, delay))
        {
            minisnn_destroy(&snn);
            return fail("long-run delayed connection failed");
        }
    }

    for (int step = 0; step < LONG_STEPS; step++)
    {
        int spikes;

        minisnn_clear_inputs(snn);
        if (!minisnn_set_input(snn, 0, 20.0))
        {
            minisnn_destroy(&snn);
            return fail("long-run input failed");
        }

        spikes = minisnn_step(snn);
        if (spikes < 0 || spikes > LONG_NEURONS)
        {
            minisnn_destroy(&snn);
            return fail("long-run spike count invalid");
        }
        total_spikes += spikes;

        if (step % 1000 == 0 || step == LONG_STEPS - 1)
        {
            for (int neuron_id = 0; neuron_id < LONG_NEURONS; neuron_id++)
            {
                double voltage;
                double synaptic_current;

                if (!minisnn_get_voltage(snn, neuron_id, &voltage) ||
                    !minisnn_get_synaptic_current(snn, neuron_id, &synaptic_current) ||
                    !isfinite(voltage) ||
                    !isfinite(synaptic_current))
                {
                    minisnn_destroy(&snn);
                    return fail("long-run non-finite state");
                }
            }
        }
    }

    if (minisnn_current_step(snn) != LONG_STEPS || total_spikes <= 0)
    {
        minisnn_destroy(&snn);
        return fail("long-run final state mismatch");
    }

    minisnn_destroy(&snn);
    printf("Long-run validation OK: steps=%d spikes=%lld\n", LONG_STEPS, total_spikes);
    return 0;
}
