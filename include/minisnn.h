#ifndef MINISNN_H
#define MINISNN_H

#include "minisnn_types.h"

typedef struct MiniSNN MiniSNN;

typedef struct
{
    int neuron_count;

    double dt;
    double tau;
    double v_rest;
    double v_reset;
    double v_threshold;
    double resistance;

    double synaptic_decay;
    int max_synaptic_delay;
} MiniSNNConfig;

/* Criacao e destruicao */
MiniSNN *minisnn_create(int neuron_count);

MiniSNNConfig minisnn_default_config(void);

MiniSNN *minisnn_create_with_config(
    const MiniSNNConfig *config);

/*
 * Libera a rede e define o ponteiro do usuario como NULL.
 * Deve ser seguro chamar minisnn_destroy(NULL) e repetir
 * minisnn_destroy(&net) depois de net ja ter virado NULL.
 */
void minisnn_destroy(MiniSNN **snn_ptr);

/* Informacoes gerais */
int minisnn_neuron_count(const MiniSNN *snn);
int minisnn_current_step(const MiniSNN *snn);

/* Configuracao */
int minisnn_set_neuron_type(
    MiniSNN *snn,
    int neuron_id,
    MiniSNNNeuronType type);

int minisnn_connect(
    MiniSNN *snn,
    int source,
    int target,
    double weight);

int minisnn_connect_ex(
    MiniSNN *snn,
    int source,
    int target,
    double weight,
    int allow_self_connection);

int minisnn_connect_delayed(
    MiniSNN *snn,
    int source,
    int target,
    double weight,
    int delay);

int minisnn_connect_delayed_ex(
    MiniSNN *snn,
    int source,
    int target,
    double weight,
    int delay,
    int allow_self_connection);

/* Entrada externa */
int minisnn_set_input(
    MiniSNN *snn,
    int neuron_id,
    double current);

int minisnn_add_input(
    MiniSNN *snn,
    int neuron_id,
    double current);

void minisnn_clear_inputs(MiniSNN *snn);

/*
 * Executa um timestep.
 * Retorna numero de spikes, ou -1 para rede invalida.
 */
int minisnn_step(MiniSNN *snn);

/* Consultas do estado mais recente */
int minisnn_get_spike(
    const MiniSNN *snn,
    int neuron_id,
    int *out_spike);

int minisnn_get_voltage(
    const MiniSNN *snn,
    int neuron_id,
    double *out_voltage);

/*
 * Retorna a corrente sinaptica efetivamente usada
 * no timestep mais recente.
 */
int minisnn_get_synaptic_current(
    const MiniSNN *snn,
    int neuron_id,
    double *out_current);

#endif
