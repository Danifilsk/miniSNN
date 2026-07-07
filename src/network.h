#ifndef NETWORK_H
#define NETWORK_H

#include "neuron.h"
#include "connection.h"

typedef struct
{
    LIFParameters lif;
    double synaptic_decay;
    int max_synaptic_delay;
} NetworkConfig;

typedef struct
{
    // Vetor de neurônios
    LIFNeuron *neurons;

    // Lista de conexões de cada neurônio
    ConnectionList *connections;

    double *syn_current;   // corrente sinaptica disponivel para o passo
    // Corrente sináptica usada na atualização LIF do timestep atual
    double *used_syn_current;
    double *pending_current;  // correntes agendadas para timesteps futuros
    double *ext_current;   // corrente externa

    LIFParameters lif_parameters;
    double synaptic_decay;
    int max_synaptic_delay;
    int delay_cursor;

    // Spikes produzidos no passo atual
    int *spikes;

    // Tempo atual da simulação
    int step;

    // Número de neurônios
    int size;

} Network;

// Inicializa a rede
int network_init(Network *net, int size);

void network_config_default(NetworkConfig *out_config);

int network_config_is_valid(
    const NetworkConfig *config);

int network_init_with_config(
    Network *net,
    int size,
    const NetworkConfig *config);

// Executa um passo da simulação
int network_update(Network *net);

int network_connect(Network *net, int source, int target, double weight);

int network_connect_delayed(
    Network *net,
    int source,
    int target,
    double weight,
    int delay);

int network_set_neuron_type(
    Network *net,
    int neuron_id,
    NeuronType type);

void network_clear_connections(Network *net);

int network_set_external_current(Network *net, int neuron_id, double current);

int network_add_external_current(Network *net, int neuron_id, double current);

void network_clear_external_currents(Network *net);

// Libera toda a memória
void network_destroy(Network *net);

#endif
