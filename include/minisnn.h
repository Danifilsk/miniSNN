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

/*
 * As conexoes sao enumeradas por source crescente e, dentro de cada source,
 * pela ordem em que foram inseridas.
 */
size_t minisnn_connection_count(const MiniSNN *snn);

int minisnn_get_connection(
    const MiniSNN *snn,
    size_t connection_id,
    MiniSNNConnectionInfo *out_connection);

int minisnn_get_connection_weight(
    const MiniSNN *snn,
    size_t connection_id,
    double *out_weight);

int minisnn_set_connection_weight(
    MiniSNN *snn,
    size_t connection_id,
    double weight);

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

/* Plasticidade sinaptica */
MiniSNNPlasticityConfig minisnn_default_plasticity_config(void);

int minisnn_set_plasticity_config(
    MiniSNN *snn,
    const MiniSNNPlasticityConfig *config);

int minisnn_get_plasticity_config(
    const MiniSNN *snn,
    MiniSNNPlasticityConfig *out_config);

int minisnn_get_plasticity_stats(
    const MiniSNN *snn,
    MiniSNNPlasticityStats *out_stats);

int minisnn_get_plasticity_traces(
    const MiniSNN *snn,
    int neuron_id,
    double *out_pre_trace,
    double *out_post_trace);

/* Homeostase neural simplificada */
MiniSNNHomeostasisConfig minisnn_default_homeostasis_config(void);

int minisnn_set_homeostasis_config(
    MiniSNN *snn,
    const MiniSNNHomeostasisConfig *config);

int minisnn_get_homeostasis_config(
    const MiniSNN *snn,
    MiniSNNHomeostasisConfig *out_config);

int minisnn_reset_homeostasis(MiniSNN *snn);

int minisnn_get_homeostasis_stats(
    const MiniSNN *snn,
    MiniSNNHomeostasisStats *out_stats);

int minisnn_get_neuron_rate_trace(
    const MiniSNN *snn,
    int neuron_id,
    double *out_rate_trace);

int minisnn_get_neuron_effective_threshold(
    const MiniSNN *snn,
    int neuron_id,
    double *out_threshold);

int minisnn_get_base_threshold(
    const MiniSNN *snn,
    double *out_threshold);

int minisnn_get_inhibitory_gain(
    const MiniSNN *snn,
    double *out_gain);

int minisnn_get_initial_incoming_exc_sum(
    const MiniSNN *snn,
    int neuron_id,
    double *out_sum);

int minisnn_get_current_incoming_exc_sum(
    const MiniSNN *snn,
    int neuron_id,
    double *out_sum);

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
