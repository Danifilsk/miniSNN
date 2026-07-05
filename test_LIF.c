#include <stdio.h>

#include "neuron.h"
#include "config.h"

int main(void)
{
    LIFNeuron n;
    lif_init(&n);

    FILE *csv = fopen("lif.csv", "w");

    if (csv == NULL)
    {
        printf("Erro ao criar lif.csv\n");
        return 1;
    }

    fprintf(csv, "tempo,V,spike\n");

    int total_spikes = 0;

    for (int t = 0; t < T_MAX; t++)
    {
        if (lif_update(&n, I_EXT))
            total_spikes++;

        fprintf(csv, "%d,%lf,%d\n",
                t,
                n.V,
                n.spike);
    }

    fclose(csv);

    printf("Spikes totais: %d\n", total_spikes);

    return 0;
}

// para compilar: gcc teste_lif.c neuron.c -o teste_lif.exe
// para executar: ./teste_lif.exe