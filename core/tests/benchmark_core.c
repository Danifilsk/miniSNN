#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "minisnn.h"

static int parse_positive_int(const char *text, int *out_value)
{
    char *end;
    long value;

    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || *text == '\0' || *end != '\0' ||
        value <= 0 || value > INT_MAX)
    {
        return 0;
    }

    *out_value = (int)value;
    return 1;
}

int main(int argc, char **argv)
{
    MiniSNN *snn;
    int neurons;
    int steps;
    long long spikes_total = 0;

    if (argc != 3 ||
        !parse_positive_int(argv[1], &neurons) ||
        !parse_positive_int(argv[2], &steps))
    {
        printf("Uso: %s <neuronios> <passos>\n", argv[0]);
        return 1;
    }

    snn = minisnn_create(neurons);
    if (snn == NULL)
    {
        printf("FAIL: creation\n");
        return 1;
    }

    for (int source = 0; source < neurons; source++)
    {
        if (!minisnn_connect_delayed(
                snn,
                source,
                (source + 1) % neurons,
                40.0,
                1 + source % 8))
        {
            minisnn_destroy(&snn);
            printf("FAIL: connection\n");
            return 1;
        }
    }

    for (int step = 0; step < steps; step++)
    {
        int spikes;

        minisnn_clear_inputs(snn);
        for (int source = 0; source < neurons && source < 3; source++)
        {
            if (!minisnn_set_input(snn, source, 20.0))
            {
                minisnn_destroy(&snn);
                printf("FAIL: input\n");
                return 1;
            }
        }

        spikes = minisnn_step(snn);
        if (spikes < 0)
        {
            minisnn_destroy(&snn);
            printf("FAIL: step\n");
            return 1;
        }
        spikes_total += spikes;
    }

    minisnn_destroy(&snn);
    printf("connections=%d spikes=%lld\n", neurons, spikes_total);
    return 0;
}
