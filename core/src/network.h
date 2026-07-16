#ifndef NETWORK_H
#define NETWORK_H

#include "neuron.h"
#include "connection.h"
#include "plasticity.h"
#include "homeostasis.h"
#include "reward.h"
#include "structural_plasticity.h"

typedef struct
{
    LIFParameters lif;
    double synaptic_decay;
    int max_synaptic_delay;
} NetworkConfig;

typedef struct Network
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

    PlasticityState *plasticity;
    HomeostasisState *homeostasis;
    RewardState *reward;
    StructuralPlasticityState *structural_plasticity;

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

int network_connect_ex(
    Network *net,
    int source,
    int target,
    double weight,
    int allow_self_connection);

int network_connect_delayed(
    Network *net,
    int source,
    int target,
    double weight,
    int delay);

int network_connect_delayed_ex(
    Network *net,
    int source,
    int target,
    double weight,
    int delay,
    int allow_self_connection);

int network_set_neuron_type(
    Network *net,
    int neuron_id,
    NeuronType type);

size_t network_connection_count(const Network *net);

int network_get_connection(
    const Network *net,
    size_t connection_id,
    int *out_source,
    Connection **out_connection);

int network_set_connection_weight(
    Network *net,
    size_t connection_id,
    double weight);

int network_set_plasticity_config(
    Network *net,
    const MiniSNNPlasticityConfig *config);

int network_set_homeostasis_config(
    Network *net,
    const MiniSNNHomeostasisConfig *config);

int network_reset_homeostasis(Network *net);

int network_set_reward_config(
    Network *net,
    const MiniSNNRewardConfig *config);

int network_reset_reward_learning(Network *net);

int network_set_structural_plasticity_config(
    Network *net,
    const MiniSNNStructuralPlasticityConfig *config);

int network_reset_structural_plasticity(
    Network *net,
    MiniSNNStructuralResetMode mode);

void network_clear_connections(Network *net);

int network_set_external_current(Network *net, int neuron_id, double current);

int network_add_external_current(Network *net, int neuron_id, double current);

void network_clear_external_currents(Network *net);

// Libera toda a memória
void network_destroy(Network *net);

#endif
