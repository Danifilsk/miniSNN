#include <math.h>
#include <stdio.h>

#include "minisnn.h"

static int run_model(MiniSNNNeuronModel model, int steps, double current)
{
    MiniSNNConfig config = minisnn_default_config();
    MiniSNN *first;
    MiniSNN *second;
    int spikes_first = 0;
    int spikes_second = 0;

    config.neuron_count = 2;
    config.neuron_model = model;
    config.dt = model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY ? 0.01 : 0.1;
    first = minisnn_create_with_config(&config);
    second = minisnn_create_with_config(&config);
    if (first == NULL || second == NULL)
        return 0;
    for (int step = 0; step < steps; step++)
    {
        double voltage_a;
        double voltage_b;
        minisnn_clear_inputs(first);
        minisnn_clear_inputs(second);
        if (!minisnn_set_input(first, 0, current) ||
            !minisnn_set_input(second, 0, current))
            return 0;
        {
            int a = minisnn_step(first);
            int b = minisnn_step(second);
            if (a < 0 || a != b)
                return 0;
            spikes_first += a;
            spikes_second += b;
        }
        if (!minisnn_get_voltage(first, 0, &voltage_a) ||
            !minisnn_get_voltage(second, 0, &voltage_b) ||
            !isfinite(voltage_a) || voltage_a != voltage_b)
            return 0;
    }
    minisnn_destroy(&first);
    minisnn_destroy(&second);
    return spikes_first == spikes_second && spikes_first > 0;
}

int main(void)
{
    if (!run_model(MINISNN_NEURON_MODEL_ADEX, 100000, 500.0) ||
        !run_model(MINISNN_NEURON_MODEL_HODGKIN_HUXLEY, 50000, 10.0))
        return 1;
    printf("Advanced neuron model long-run validation OK\n");
    return 0;
}
