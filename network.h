#ifndef NETWORK_H
#define NETWORK_H

#include "neuron.h"
#include "connection.h"

typedef struct
{
    // Vetor de neurônios
    LIFNeuron *neurons;

    // Lista de conexões de cada neurônio
    ConnectionList *connections;

    double *syn_current;   // corrente usada neste passo
    double *next_current;  // corrente produzida pelos spikes deste passo
    double *ext_current;   // corrente externa

    // Spikes produzidos no passo atual
    int *spikes;

    // Tempo atual da simulação
    int step;

    // Número de neurônios
    int size;

} Network;

// Inicializa a rede
int network_init(Network *net, int size);

// Executa um passo da simulação
int network_update(Network *net);

// Libera toda a memória
void network_destroy(Network *net);

#endif